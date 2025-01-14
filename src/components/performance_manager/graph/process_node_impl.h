// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/graph/node_attached_data.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/properties.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/public/mojom/v8_contexts.mojom.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {
class BackgroundTracingManager;
}  // namespace content

namespace performance_manager {

class FrameNodeImpl;
class ProcessNodeImpl;
class WorkerNodeImpl;

// A process node follows the lifetime of a RenderProcessHost.
// It may reference zero or one processes at a time, but during its lifetime, it
// may reference more than one process. This can happen if the associated
// renderer crashes, and an associated frame is then reloaded or re-navigated.
// The state of the process node goes through:
// 1. Created, no PID.
// 2. Process started, have PID - in the case where the associated render
//    process fails to start, this state may not occur.
// 3. Process died or failed to start, have exit status.
// 4. Back to 2.
class ProcessNodeImpl
    : public PublicNodeImpl<ProcessNodeImpl, ProcessNode>,
      public TypedNodeBase<ProcessNodeImpl, ProcessNode, ProcessNodeObserver>,
      public mojom::ProcessCoordinationUnit {
 public:
  using PassKey = base::PassKey<ProcessNodeImpl>;

  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kProcess; }

  ProcessNodeImpl(content::ProcessType process_type,
                  RenderProcessHostProxy render_process_proxy);

  ~ProcessNodeImpl() override;

  void Bind(mojo::PendingReceiver<mojom::ProcessCoordinationUnit> receiver);

  // mojom::ProcessCoordinationUnit implementation:
  void SetMainThreadTaskLoadIsLow(bool main_thread_task_load_is_low) override;
  void OnV8ContextCreated(
      mojom::V8ContextDescriptionPtr description,
      mojom::IframeAttributionDataPtr iframe_attribution_data) override;
  void OnV8ContextDetached(
      const blink::V8ContextToken& v8_context_token) override;
  void OnV8ContextDestroyed(
      const blink::V8ContextToken& v8_context_token) override;
  void OnRemoteIframeAttached(
      const blink::LocalFrameToken& parent_frame_token,
      const blink::RemoteFrameToken& remote_frame_token,
      mojom::IframeAttributionDataPtr iframe_attribution_data) override;
  void OnRemoteIframeDetached(
      const blink::LocalFrameToken& parent_frame_token,
      const blink::RemoteFrameToken& remote_frame_token) override;
  void FireBackgroundTracingTrigger(const std::string& trigger_name) override;

  void SetProcessExitStatus(int32_t exit_status);
  void SetProcess(base::Process process, base::Time launch_time);

  // Private implementation properties.
  void set_private_footprint_kb(uint64_t private_footprint_kb) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    private_footprint_kb_ = private_footprint_kb;
  }
  uint64_t private_footprint_kb() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return private_footprint_kb_;
  }
  uint64_t resident_set_kb() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return resident_set_kb_;
  }
  void set_resident_set_kb(uint64_t resident_set_kb) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    resident_set_kb_ = resident_set_kb;
  }

  const base::flat_set<FrameNodeImpl*>& frame_nodes() const;

  // Returns the render process id (equivalent to RenderProcessHost::GetID()),
  // or ChildProcessHost::kInvalidUniqueID if this is not a renderer.
  RenderProcessHostId GetRenderProcessId() const;

  // If this process is associated with only one page, returns that page.
  // Otherwise, returns nullptr.
  PageNodeImpl* GetPageNodeIfExclusive() const;

  content::ProcessType process_type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return process_type_;
  }
  // Use process_id() in preference to process().Pid(). It's always valid to
  // access, but will return kNullProcessId when the process is not valid. It
  // will also retain the process ID for a process that has exited.
  base::ProcessId process_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return process_id_;
  }
  const base::Process& process() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return process_.value();
  }
  base::Time launch_time() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return launch_time_;
  }
  base::Optional<int32_t> exit_status() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return exit_status_;
  }

  bool main_thread_task_load_is_low() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return main_thread_task_load_is_low_.value();
  }

  const RenderProcessHostProxy& render_process_host_proxy() const {
    return render_process_host_proxy_;
  }

  base::TaskPriority priority() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return priority_.value();
  }

  // Add |frame_node| to this process.
  void AddFrame(FrameNodeImpl* frame_node);
  // Removes |frame_node| from the set of frames hosted by this process. Invoked
  // when the frame is removed from the graph.
  void RemoveFrame(FrameNodeImpl* frame_node);

  // Add |worker_node| to this process.
  void AddWorker(WorkerNodeImpl* worker_node);
  // Removes |worker_node| from the set of workers hosted by this process.
  // Invoked when the worker is removed from the graph.
  void RemoveWorker(WorkerNodeImpl* worker_node);

  void set_priority(base::TaskPriority priority);

  void OnAllFramesInProcessFrozenForTesting() { OnAllFramesInProcessFrozen(); }
  static void FireBackgroundTracingTriggerOnUIForTesting(
      const std::string& trigger_name,
      content::BackgroundTracingManager* manager);

  base::WeakPtr<ProcessNodeImpl> GetWeakPtrOnUIThread();
  base::WeakPtr<ProcessNodeImpl> GetWeakPtr();

  static PassKey CreatePassKeyForTesting() { return PassKey(); }

 protected:
  void SetProcessImpl(base::Process process,
                      base::ProcessId process_id,
                      base::Time launch_time);

 private:
  friend class FrozenFrameAggregatorAccess;
  friend class ProcessMetricsDecoratorAccess;
  friend class ProcessPriorityAggregatorAccess;

  // ProcessNode implementation. These are private so that users of the impl use
  // the private getters rather than the public interface.
  content::ProcessType GetProcessType() const override;
  base::ProcessId GetProcessId() const override;
  const base::Process& GetProcess() const override;
  base::Time GetLaunchTime() const override;
  base::Optional<int32_t> GetExitStatus() const override;
  bool VisitFrameNodes(const FrameNodeVisitor& visitor) const override;
  base::flat_set<const FrameNode*> GetFrameNodes() const override;
  base::flat_set<const WorkerNode*> GetWorkerNodes() const override;
  bool GetMainThreadTaskLoadIsLow() const override;
  uint64_t GetPrivateFootprintKb() const override;
  uint64_t GetResidentSetKb() const override;
  RenderProcessHostId GetRenderProcessHostId() const override;
  const RenderProcessHostProxy& GetRenderProcessHostProxy() const override;
  base::TaskPriority GetPriority() const override;

  void OnAllFramesInProcessFrozen();

  // NodeBase:
  void OnBeforeLeavingGraph() override;
  void RemoveNodeAttachedData() override;

  mojo::Receiver<mojom::ProcessCoordinationUnit> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  uint64_t private_footprint_kb_ GUARDED_BY_CONTEXT(sequence_checker_) = 0u;
  uint64_t resident_set_kb_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  base::ProcessId process_id_ GUARDED_BY_CONTEXT(sequence_checker_) =
      base::kNullProcessId;
  ObservedProperty::NotifiesAlways<
      base::Process,
      &ProcessNodeObserver::OnProcessLifetimeChange>
      process_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::Time launch_time_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::Optional<int32_t> exit_status_ GUARDED_BY_CONTEXT(sequence_checker_);

  const content::ProcessType process_type_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // This is used during frame node initialization.
  // TODO(siggi): It seems unnecessary to initialize this at creation time?
  const RenderProcessHostProxy render_process_host_proxy_;

  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &ProcessNodeObserver::OnMainThreadTaskLoadIsLow>
      main_thread_task_load_is_low_ GUARDED_BY_CONTEXT(sequence_checker_){
          false};

  // Process priority information. This is aggregated from the priority of
  // all workers and frames in a given process.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      base::TaskPriority,
      base::TaskPriority,
      &ProcessNodeObserver::OnPriorityChanged>
      priority_ GUARDED_BY_CONTEXT(sequence_checker_){
          base::TaskPriority::LOWEST};

  base::flat_set<FrameNodeImpl*> frame_nodes_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::flat_set<WorkerNodeImpl*> worker_nodes_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Inline storage for FrozenFrameAggregator user data.
  InternalNodeAttachedDataStorage<sizeof(uintptr_t) + 8> frozen_frame_data_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Inline storage for ProcessPriorityAggregator user data.
  std::unique_ptr<NodeAttachedData> process_priority_data_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtr<ProcessNodeImpl> weak_this_;
  base::WeakPtrFactory<ProcessNodeImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  DISALLOW_COPY_AND_ASSIGN(ProcessNodeImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_
