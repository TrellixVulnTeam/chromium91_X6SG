// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "platform/public/single_thread_executor.h"

#include <atomic>
#include <functional>
#include <string>

#include "platform/base/exception.h"
#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {

TEST(SingleThreadExecutorTest, ConsructorDestructorWorks) {
  SingleThreadExecutor executor;
}

TEST(SingleThreadExecutorTest, CanExecute) {
  absl::CondVar cond;
  std::atomic_bool done = false;
  SingleThreadExecutor executor;
  executor.Execute([&done, &cond]() {
    done = true;
    cond.SignalAll();
  });
  absl::Mutex mutex;
  {
    absl::MutexLock lock(&mutex);
    if (!done) {
      cond.WaitWithTimeout(&mutex, absl::Seconds(1));
    }
  }
  EXPECT_TRUE(done);
}

TEST(SingleThreadExecutorTest, CanExecuteNamedTask) {
  absl::CondVar cond;
  std::atomic_bool done = false;
  SingleThreadExecutor executor;
  executor.Execute("my task", [&done, &cond]() {
    done = true;
    cond.SignalAll();
  });
  absl::Mutex mutex;
  {
    absl::MutexLock lock(&mutex);
    if (!done) {
      cond.WaitWithTimeout(&mutex, absl::Seconds(1));
    }
  }
  EXPECT_TRUE(done);
}

TEST(SingleThreadExecutorTest, JobsExecuteInOrder) {
  std::vector<int> results;
  SingleThreadExecutor executor;

  for (int i = 0; i < 10; ++i) {
    executor.Execute([i, &results]() { results.push_back(i); });
  }

  absl::CondVar cond;
  std::atomic_bool done = false;
  executor.Execute([&done, &cond]() {
    done = true;
    cond.SignalAll();
  });
  absl::Mutex mutex;
  {
    absl::MutexLock lock(&mutex);
    if (!done) {
      cond.WaitWithTimeout(&mutex, absl::Seconds(1));
    }
  }
  EXPECT_TRUE(done);
  EXPECT_EQ(results, (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

TEST(SingleThreadExecutorTest, CanSubmit) {
  SingleThreadExecutor executor;
  Future<bool> future;
  bool submitted =
      executor.Submit<bool>([]() { return ExceptionOr<bool>{true}; }, &future);
  EXPECT_TRUE(submitted);
  EXPECT_TRUE(future.Get().result());
}

struct ThreadCheckTestClass {
  SingleThreadExecutor executor;
  int value ABSL_GUARDED_BY(executor) = 0;

  void incValue() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor) { value++; }
  int getValue() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor) { return value; }
};

TEST(SingleThreadExecutorTest, ThreadCheck_ExecuteRunnable) {
  ThreadCheckTestClass test_class;

  test_class.executor.Execute(
      [&test_class]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
        test_class.incValue();
      });
}

TEST(SingleThreadExecutorTest, ThreadCheck_SubmitCallable) {
  ThreadCheckTestClass test_class;
  test_class.executor.Execute(
      [&test_class]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
        test_class.incValue();
      });
  Future<int> future;

  bool submitted = test_class.executor.Submit<int>(
      [&test_class]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(test_class.executor) {
        return ExceptionOr<int>{test_class.getValue()};
      },
      &future);

  EXPECT_TRUE(submitted);
  EXPECT_EQ(future.Get().result(), 1);
}

}  // namespace nearby
}  // namespace location
