// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/categorized_worker_pool.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequence_manager/task_time_observer.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/typed_macros.h"
#include "cc/base/math_util.h"
#include "cc/raster/task_category.h"
#include "ui/gfx/rendering_pipeline.h"

namespace content {
namespace {

// Task categories running at normal thread priority.
constexpr cc::TaskCategory kNormalThreadPriorityCategories[] = {
    cc::TASK_CATEGORY_NONCONCURRENT_FOREGROUND, cc::TASK_CATEGORY_FOREGROUND,
    cc::TASK_CATEGORY_BACKGROUND_WITH_NORMAL_THREAD_PRIORITY};

// Task categories running at background thread priority.
constexpr cc::TaskCategory kBackgroundThreadPriorityCategories[] = {
    cc::TASK_CATEGORY_BACKGROUND};

// Foreground task categories.
constexpr cc::TaskCategory kForegroundCategories[] = {
    cc::TASK_CATEGORY_NONCONCURRENT_FOREGROUND, cc::TASK_CATEGORY_FOREGROUND};

// Background task categories. Tasks in these categories cannot start running
// when a task with a category in |kForegroundCategories| is running or ready to
// run.
constexpr cc::TaskCategory kBackgroundCategories[] = {
    cc::TASK_CATEGORY_BACKGROUND,
    cc::TASK_CATEGORY_BACKGROUND_WITH_NORMAL_THREAD_PRIORITY};

// A thread which forwards to CategorizedWorkerPool::Run with the runnable
// categories.
class CategorizedWorkerPoolThread : public base::SimpleThread {
 public:
  CategorizedWorkerPoolThread(
      const std::string& name_prefix,
      const Options& options,
      CategorizedWorkerPool* pool,
      std::vector<cc::TaskCategory> categories,
      gfx::RenderingPipeline* pipeline,
      base::ConditionVariable* has_ready_to_run_tasks_cv)
      : SimpleThread(name_prefix, options),
        pool_(pool),
        categories_(categories),
        pipeline_(pipeline),
        has_ready_to_run_tasks_cv_(has_ready_to_run_tasks_cv) {}

  void SetBackgroundingCallback(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceCallback<void(base::PlatformThreadId)> callback) {
    DCHECK(!HasStartBeenAttempted());
    background_task_runner_ = std::move(task_runner);
    backgrounding_callback_ = std::move(callback);
  }

  // base::SimpleThread:
  void BeforeRun() override {
    if (backgrounding_callback_) {
      DCHECK(background_task_runner_);
      background_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(backgrounding_callback_), tid()));
    }
  }

  void Run() override {
    pool_->Run(categories_, pipeline_, has_ready_to_run_tasks_cv_);
  }

 private:
  CategorizedWorkerPool* const pool_;
  const std::vector<cc::TaskCategory> categories_;
  gfx::RenderingPipeline* pipeline_;
  base::ConditionVariable* const has_ready_to_run_tasks_cv_;

  base::OnceCallback<void(base::PlatformThreadId)> backgrounding_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;
};

}  // namespace

// A sequenced task runner which posts tasks to a CategorizedWorkerPool.
class CategorizedWorkerPool::CategorizedWorkerPoolSequencedTaskRunner
    : public base::SequencedTaskRunner {
 public:
  explicit CategorizedWorkerPoolSequencedTaskRunner(
      cc::TaskGraphRunner* task_graph_runner)
      : task_graph_runner_(task_graph_runner),
        namespace_token_(task_graph_runner->GenerateNamespaceToken()) {}

  // Overridden from base::TaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return PostNonNestableDelayedTask(from_here, std::move(task), delay);
  }

  // Overridden from base::SequencedTaskRunner:
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
    // for details.
    CHECK(task);
    base::AutoLock lock(lock_);

    // Remove completed tasks.
    DCHECK(completed_tasks_.empty());
    task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                              &completed_tasks_);

    tasks_.erase(tasks_.begin(), tasks_.begin() + completed_tasks_.size());

    tasks_.push_back(base::MakeRefCounted<ClosureTask>(std::move(task)));
    graph_.Reset();
    for (const auto& graph_task : tasks_) {
      int dependencies = 0;
      if (!graph_.nodes.empty())
        dependencies = 1;

      // Treat any tasks that are enqueued through the SequencedTaskRunner as
      // FOREGROUND priority. We don't have enough information to know the
      // actual priority of such tasks, so we run them as soon as possible.
      cc::TaskGraph::Node node(graph_task, cc::TASK_CATEGORY_FOREGROUND,
                               0u /* priority */, dependencies);
      if (dependencies) {
        graph_.edges.push_back(cc::TaskGraph::Edge(
            graph_.nodes.back().task.get(), node.task.get()));
      }
      graph_.nodes.push_back(std::move(node));
    }
    task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);
    completed_tasks_.clear();
    return true;
  }

  bool RunsTasksInCurrentSequence() const override { return true; }

 private:
  ~CategorizedWorkerPoolSequencedTaskRunner() override {
    {
      base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
      task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
    }
    task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                              &completed_tasks_);
  }

  // Lock to exclusively access all the following members that are used to
  // implement the SequencedTaskRunner interfaces.
  base::Lock lock_;

  cc::TaskGraphRunner* task_graph_runner_;
  // Namespace used to schedule tasks in the task graph runner.
  cc::NamespaceToken namespace_token_;
  // List of tasks currently queued up for execution.
  cc::Task::Vector tasks_;
  // Graph object used for scheduling tasks.
  cc::TaskGraph graph_;
  // Cached vector to avoid allocation when getting the list of complete
  // tasks.
  cc::Task::Vector completed_tasks_;
};

CategorizedWorkerPool::CategorizedWorkerPool()
    : namespace_token_(GenerateNamespaceToken()),
      has_task_for_normal_priority_thread_cv_(&lock_),
      has_task_for_background_priority_thread_cv_(&lock_),
      has_namespaces_with_finished_running_tasks_cv_(&lock_),
      shutdown_(false) {
  // Declare the two ConditionVariables which are used by worker threads to
  // sleep-while-idle as such to avoid throwing off //base heuristics.
  has_task_for_normal_priority_thread_cv_.declare_only_used_while_idle();
  has_task_for_background_priority_thread_cv_.declare_only_used_while_idle();
}

void CategorizedWorkerPool::Start(int num_normal_threads,
                                  gfx::RenderingPipeline* foreground_pipeline) {
  DCHECK(threads_.empty());

  // |num_normal_threads| normal threads and 1 background threads are created.
  const size_t num_threads = num_normal_threads + 1;
  threads_.reserve(num_threads);

  // Start |num_normal_threads| normal priority threads, which run foreground
  // work and background work that cannot run at background thread priority.
  std::vector<cc::TaskCategory> normal_thread_prio_categories(
      std::begin(kNormalThreadPriorityCategories),
      std::end(kNormalThreadPriorityCategories));

  for (int i = 0; i < num_normal_threads; i++) {
    auto thread = std::make_unique<CategorizedWorkerPoolThread>(
        base::StringPrintf("CompositorTileWorker%d", i + 1),
        base::SimpleThread::Options(), this, normal_thread_prio_categories,
        foreground_pipeline, &has_task_for_normal_priority_thread_cv_);
    thread->StartAsync();
    threads_.push_back(std::move(thread));
  }

  // Start a single thread running at background thread priority.
  std::vector<cc::TaskCategory> background_thread_prio_categories{
      std::begin(kBackgroundThreadPriorityCategories),
      std::end(kBackgroundThreadPriorityCategories)};

  base::SimpleThread::Options thread_options;
#if !defined(OS_MAC)
  thread_options.priority = base::ThreadPriority::BACKGROUND;
#endif

  auto thread = std::make_unique<CategorizedWorkerPoolThread>(
      "CompositorTileWorkerBackground", thread_options, this,
      background_thread_prio_categories,
      /*pipeline=*/nullptr, &has_task_for_background_priority_thread_cv_);
  if (backgrounding_callback_) {
    thread->SetBackgroundingCallback(std::move(background_task_runner_),
                                     std::move(backgrounding_callback_));
  }
  thread->StartAsync();
  threads_.push_back(std::move(thread));

  DCHECK_EQ(num_threads, threads_.size());
}

void CategorizedWorkerPool::Shutdown() {
  {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    WaitForTasksToFinishRunning(namespace_token_);
  }

  CollectCompletedTasks(namespace_token_, &completed_tasks_);
  // Shutdown raster threads.
  {
    base::AutoLock lock(lock_);

    DCHECK(!work_queue_.HasReadyToRunTasks());
    DCHECK(!work_queue_.HasAnyNamespaces());

    DCHECK(!shutdown_);
    shutdown_ = true;

    // Wake up all workers so they exit.
    has_task_for_normal_priority_thread_cv_.Broadcast();
    has_task_for_background_priority_thread_cv_.Broadcast();
  }
  while (!threads_.empty()) {
    threads_.back()->Join();
    threads_.pop_back();
  }
}

// Overridden from base::TaskRunner:
bool CategorizedWorkerPool::PostDelayedTask(const base::Location& from_here,
                                            base::OnceClosure task,
                                            base::TimeDelta delay) {
  base::AutoLock lock(lock_);

  // Remove completed tasks.
  DCHECK(completed_tasks_.empty());
  CollectCompletedTasksWithLockAcquired(namespace_token_, &completed_tasks_);

  base::EraseIf(tasks_, [this](const scoped_refptr<cc::Task>& e)
                            EXCLUSIVE_LOCKS_REQUIRED(lock_) {
                              return base::Contains(this->completed_tasks_, e);
                            });

  tasks_.push_back(base::MakeRefCounted<ClosureTask>(std::move(task)));
  graph_.Reset();
  for (const auto& graph_task : tasks_) {
    // Delayed tasks are assigned FOREGROUND category, ensuring that they run as
    // soon as possible once their delay has expired.
    graph_.nodes.push_back(
        cc::TaskGraph::Node(graph_task.get(), cc::TASK_CATEGORY_FOREGROUND,
                            0u /* priority */, 0u /* dependencies */));
  }

  ScheduleTasksWithLockAcquired(namespace_token_, &graph_);
  completed_tasks_.clear();
  return true;
}

void CategorizedWorkerPool::Run(
    const std::vector<cc::TaskCategory>& categories,
    gfx::RenderingPipeline* pipeline,
    base::ConditionVariable* has_ready_to_run_tasks_cv) {
  base::sequence_manager::TaskTimeObserver* observer =
      pipeline ? pipeline->AddSimpleThread(base::PlatformThread::CurrentId())
               : nullptr;
  base::AutoLock lock(lock_);

  while (true) {
    if (!RunTaskWithLockAcquired(categories, observer)) {
      // We are no longer running tasks, which may allow another category to
      // start running. Signal other worker threads.
      SignalHasReadyToRunTasksWithLockAcquired();

      // Exit when shutdown is set and no more tasks are pending.
      if (shutdown_)
        break;

      // Wait for more tasks.
      has_ready_to_run_tasks_cv->Wait();
      continue;
    }
  }
}

void CategorizedWorkerPool::FlushForTesting() {
  base::AutoLock lock(lock_);

  while (!work_queue_.HasFinishedRunningTasksInAllNamespaces()) {
    has_namespaces_with_finished_running_tasks_cv_.Wait();
  }
}

scoped_refptr<base::SequencedTaskRunner>
CategorizedWorkerPool::CreateSequencedTaskRunner() {
  return new CategorizedWorkerPoolSequencedTaskRunner(this);
}

void CategorizedWorkerPool::SetBackgroundingCallback(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::OnceCallback<void(base::PlatformThreadId)> callback) {
  // The callback must be set before the threads have been created.
  DCHECK(threads_.empty());
  backgrounding_callback_ = std::move(callback);
  background_task_runner_ = std::move(task_runner);
}

CategorizedWorkerPool::~CategorizedWorkerPool() = default;

cc::NamespaceToken CategorizedWorkerPool::GenerateNamespaceToken() {
  base::AutoLock lock(lock_);
  return work_queue_.GenerateNamespaceToken();
}

void CategorizedWorkerPool::ScheduleTasks(cc::NamespaceToken token,
                                          cc::TaskGraph* graph) {
  TRACE_EVENT2("disabled-by-default-cc.debug",
               "CategorizedWorkerPool::ScheduleTasks", "num_nodes",
               graph->nodes.size(), "num_edges", graph->edges.size());
  {
    base::AutoLock lock(lock_);
    ScheduleTasksWithLockAcquired(token, graph);
  }
}

void CategorizedWorkerPool::ScheduleTasksWithLockAcquired(
    cc::NamespaceToken token,
    cc::TaskGraph* graph) {
  DCHECK(token.IsValid());
  DCHECK(!cc::TaskGraphWorkQueue::DependencyMismatch(graph));
  DCHECK(!shutdown_);

  work_queue_.ScheduleTasks(token, graph);

  // There may be more work available, so wake up another worker thread.
  SignalHasReadyToRunTasksWithLockAcquired();
}

void CategorizedWorkerPool::WaitForTasksToFinishRunning(
    cc::NamespaceToken token) {
  TRACE_EVENT0("disabled-by-default-cc.debug",
               "CategorizedWorkerPool::WaitForTasksToFinishRunning");

  DCHECK(token.IsValid());

  {
    base::AutoLock lock(lock_);

    auto* task_namespace = work_queue_.GetNamespaceForToken(token);

    if (!task_namespace)
      return;

    while (!work_queue_.HasFinishedRunningTasksInNamespace(task_namespace))
      has_namespaces_with_finished_running_tasks_cv_.Wait();

    // There may be other namespaces that have finished running tasks, so wake
    // up another origin thread.
    has_namespaces_with_finished_running_tasks_cv_.Signal();
  }
}

void CategorizedWorkerPool::CollectCompletedTasks(
    cc::NamespaceToken token,
    cc::Task::Vector* completed_tasks) {
  TRACE_EVENT0("disabled-by-default-cc.debug",
               "CategorizedWorkerPool::CollectCompletedTasks");

  {
    base::AutoLock lock(lock_);
    CollectCompletedTasksWithLockAcquired(token, completed_tasks);
  }
}

void CategorizedWorkerPool::CollectCompletedTasksWithLockAcquired(
    cc::NamespaceToken token,
    cc::Task::Vector* completed_tasks) {
  DCHECK(token.IsValid());
  work_queue_.CollectCompletedTasks(token, completed_tasks);
}

bool CategorizedWorkerPool::RunTaskWithLockAcquired(
    const std::vector<cc::TaskCategory>& categories,
    base::sequence_manager::TaskTimeObserver* observer) {
  for (const auto& category : categories) {
    if (ShouldRunTaskForCategoryWithLockAcquired(category)) {
      RunTaskInCategoryWithLockAcquired(category, observer);
      return true;
    }
  }
  return false;
}

void CategorizedWorkerPool::RunTaskInCategoryWithLockAcquired(
    cc::TaskCategory category,
    base::sequence_manager::TaskTimeObserver* observer) {
  lock_.AssertAcquired();

  auto prioritized_task = work_queue_.GetNextTaskToRun(category);

  TRACE_EVENT(
      "toplevel", "TaskGraphRunner::RunTask", [&](perfetto::EventContext ctx) {
        ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
            ->set_chrome_raster_task()
            ->set_source_frame_number(prioritized_task.task->frame_number());
      });
  // There may be more work available, so wake up another worker thread.
  SignalHasReadyToRunTasksWithLockAcquired();

  {
    base::AutoUnlock unlock(lock_);
    base::TimeTicks start_time;
    if (observer) {
      start_time = base::TimeTicks::Now();
      observer->WillProcessTask(start_time);
    }
    prioritized_task.task->RunOnWorkerThread();
    if (observer) {
      observer->DidProcessTask(start_time, base::TimeTicks::Now());
    }
  }

  auto* task_namespace = prioritized_task.task_namespace;
  work_queue_.CompleteTask(std::move(prioritized_task));

  // If namespace has finished running all tasks, wake up origin threads.
  if (work_queue_.HasFinishedRunningTasksInNamespace(task_namespace))
    has_namespaces_with_finished_running_tasks_cv_.Signal();
}

bool CategorizedWorkerPool::ShouldRunTaskForCategoryWithLockAcquired(
    cc::TaskCategory category) {
  lock_.AssertAcquired();

  if (!work_queue_.HasReadyToRunTasksForCategory(category))
    return false;

  if (base::Contains(kBackgroundCategories, category)) {
    // Only run background tasks if there are no foreground tasks running or
    // ready to run.
    for (cc::TaskCategory foreground_category : kForegroundCategories) {
      if (work_queue_.NumRunningTasksForCategory(foreground_category) > 0 ||
          work_queue_.HasReadyToRunTasksForCategory(foreground_category)) {
        return false;
      }
    }

    // Enforce that only one background task runs at a time.
    for (cc::TaskCategory background_category : kBackgroundCategories) {
      if (work_queue_.NumRunningTasksForCategory(background_category) > 0)
        return false;
    }
  }

  // Enforce that only one nonconcurrent task runs at a time.
  if (category == cc::TASK_CATEGORY_NONCONCURRENT_FOREGROUND &&
      work_queue_.NumRunningTasksForCategory(
          cc::TASK_CATEGORY_NONCONCURRENT_FOREGROUND) > 0) {
    return false;
  }

  return true;
}

void CategorizedWorkerPool::SignalHasReadyToRunTasksWithLockAcquired() {
  lock_.AssertAcquired();

  for (cc::TaskCategory category : kNormalThreadPriorityCategories) {
    if (ShouldRunTaskForCategoryWithLockAcquired(category)) {
      has_task_for_normal_priority_thread_cv_.Signal();
      return;
    }
  }

  // Due to the early return in the previous loop, this only runs when there are
  // no tasks to run on normal priority threads.
  for (cc::TaskCategory category : kBackgroundThreadPriorityCategories) {
    if (ShouldRunTaskForCategoryWithLockAcquired(category)) {
      has_task_for_background_priority_thread_cv_.Signal();
      return;
    }
  }
}

CategorizedWorkerPool::ClosureTask::ClosureTask(base::OnceClosure closure)
    : closure_(std::move(closure)) {}

// Overridden from cc::Task:
void CategorizedWorkerPool::ClosureTask::RunOnWorkerThread() {
  std::move(closure_).Run();
}

CategorizedWorkerPool::ClosureTask::~ClosureTask() {}

}  // namespace content
