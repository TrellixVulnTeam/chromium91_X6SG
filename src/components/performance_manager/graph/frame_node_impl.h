// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/public/mojom/web_memory.mojom.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace performance_manager {

class FrameNodeImplDescriber;

namespace execution_context {
class ExecutionContextAccess;
}  // namespace execution_context

// Frame nodes form a tree structure, each FrameNode at most has one parent that
// is a FrameNode. Conceptually, a frame corresponds to a
// content::RenderFrameHost in the browser, and a content::RenderFrameImpl /
// blink::LocalFrame in a renderer.
//
// Note that a frame in a frame tree can be replaced with another, with the
// continuity of that position represented via the |frame_tree_node_id|. It is
// possible to have multiple "sibling" nodes that share the same
// |frame_tree_node_id|. Only one of these may contribute to the content being
// rendered, and this node is designated the "current" node in content
// terminology. A swap is effectively atomic but will take place in two steps
// in the graph: the outgoing frame will first be marked as not current, and the
// incoming frame will be marked as current. As such, the graph invariant is
// that there will be 0 or 1 |is_current| frames with a given
// |frame_tree_node_id|.
//
// This occurs when a frame is navigated and the existing frame can't be reused.
// In that case a "provisional" frame is created to start the navigation. Once
// the navigation completes (which may actually involve a redirect to another
// origin meaning the frame has to be destroyed and another one created in
// another process!) and commits, the frame will be swapped with the previously
// active frame.
class FrameNodeImpl
    : public PublicNodeImpl<FrameNodeImpl, FrameNode>,
      public TypedNodeBase<FrameNodeImpl, FrameNode, FrameNodeObserver>,
      public mojom::DocumentCoordinationUnit {
 public:
  static const char kDefaultPriorityReason[];
  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kFrame; }

  // Construct a frame node associated with a |process_node|, a |page_node| and
  // optionally with a |parent_frame_node|. For the main frame of |page_node|
  // the |parent_frame_node| parameter should be nullptr. |render_frame_id| is
  // the routing id of the frame (from RenderFrameHost::GetRoutingID).
  FrameNodeImpl(ProcessNodeImpl* process_node,
                PageNodeImpl* page_node,
                FrameNodeImpl* parent_frame_node,
                int frame_tree_node_id,
                int render_frame_id,
                const blink::LocalFrameToken& frame_token,
                int32_t browsing_instance_id,
                int32_t site_instance_id);

  ~FrameNodeImpl() override;

  void Bind(mojo::PendingReceiver<mojom::DocumentCoordinationUnit> receiver);

  // mojom::DocumentCoordinationUnit implementation.
  void SetNetworkAlmostIdle() override;
  void SetLifecycleState(LifecycleState state) override;
  void SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload) override;
  void SetIsAdFrame(bool is_ad_frame) override;
  void SetHadFormInteraction() override;
  void OnNonPersistentNotificationCreated() override;
  void OnFirstContentfulPaint(
      base::TimeDelta time_since_navigation_start) override;
  const RenderFrameHostProxy& GetRenderFrameHostProxy() const override;
  void OnWebMemoryMeasurementRequested(
      mojom::WebMemoryMeasurement::Mode mode,
      OnWebMemoryMeasurementRequestedCallback callback) override;

  // Partial FrameNodbase::TimeDelta time_since_navigatione implementation:
  bool IsMainFrame() const override;

  // Getters for const properties.
  FrameNodeImpl* parent_frame_node() const;
  PageNodeImpl* page_node() const;
  ProcessNodeImpl* process_node() const;
  int frame_tree_node_id() const;
  int render_frame_id() const;
  const blink::LocalFrameToken& frame_token() const;
  int32_t browsing_instance_id() const;
  int32_t site_instance_id() const;
  const RenderFrameHostProxy& render_frame_host_proxy() const;

  // Getters for non-const properties. These are not thread safe.
  const base::flat_set<FrameNodeImpl*>& child_frame_nodes() const;
  const base::flat_set<PageNodeImpl*>& opened_page_nodes() const;
  LifecycleState lifecycle_state() const;
  bool has_nonempty_beforeunload() const;
  const GURL& url() const;
  bool is_current() const;
  bool network_almost_idle() const;
  bool is_ad_frame() const;
  bool is_holding_weblock() const;
  bool is_holding_indexeddb_lock() const;
  const base::flat_set<WorkerNodeImpl*>& child_worker_nodes() const;
  const PriorityAndReason& priority_and_reason() const;
  bool had_form_interaction() const;
  bool is_audible() const;
  const base::Optional<gfx::Rect>& viewport_intersection() const;
  Visibility visibility() const;

  // Setters are not thread safe.
  void SetIsCurrent(bool is_current);
  void SetIsHoldingWebLock(bool is_holding_weblock);
  void SetIsHoldingIndexedDBLock(bool is_holding_indexeddb_lock);
  void SetIsAudible(bool is_audible);
  void SetViewportIntersection(const gfx::Rect& viewport_intersection);
  void SetVisibility(Visibility visibility);

  // Invoked when a navigation is committed in the frame.
  void OnNavigationCommitted(const GURL& url, bool same_document);

  // Invoked by |worker_node| when it starts/stops being a child of this frame.
  void AddChildWorker(WorkerNodeImpl* worker_node);
  void RemoveChildWorker(WorkerNodeImpl* worker_node);

  // Invoked to set the frame priority, and the reason behind it.
  void SetPriorityAndReason(const PriorityAndReason& priority_and_reason);

  base::WeakPtr<FrameNodeImpl> GetWeakPtrOnUIThread();
  base::WeakPtr<FrameNodeImpl> GetWeakPtr();

  void SeverOpenedPagesAndMaybeReparentForTesting() {
    SeverOpenedPagesAndMaybeReparent();
  }

  // Implementation details below this point.

  // Invoked by opened pages when this frame is set/cleared as their opener.
  // See PageNodeImpl::(Set|Clear)OpenerFrameNodeAndOpenedType.
  void AddOpenedPage(base::PassKey<PageNodeImpl> key, PageNodeImpl* page_node);
  void RemoveOpenedPage(base::PassKey<PageNodeImpl> key,
                        PageNodeImpl* page_node);

  // Used by the ExecutionContextRegistry mechanism.
  std::unique_ptr<NodeAttachedData>* GetExecutionContextStorage(
      base::PassKey<execution_context::ExecutionContextAccess> key) {
    return &execution_context_;
  }

 private:
  friend class ExecutionContextPriorityAccess;
  friend class FrameNodeImplDescriber;
  friend class ProcessNodeImpl;

  // Rest of FrameNode implementation. These are private so that users of the
  // impl use the private getters rather than the public interface.
  const FrameNode* GetParentFrameNode() const override;
  const PageNode* GetPageNode() const override;
  const ProcessNode* GetProcessNode() const override;
  int GetFrameTreeNodeId() const override;
  const blink::LocalFrameToken& GetFrameToken() const override;
  int32_t GetBrowsingInstanceId() const override;
  int32_t GetSiteInstanceId() const override;
  bool VisitChildFrameNodes(const FrameNodeVisitor& visitor) const override;
  const base::flat_set<const FrameNode*> GetChildFrameNodes() const override;
  bool VisitOpenedPageNodes(const PageNodeVisitor& visitor) const override;
  const base::flat_set<const PageNode*> GetOpenedPageNodes() const override;
  LifecycleState GetLifecycleState() const override;
  bool HasNonemptyBeforeUnload() const override;
  const GURL& GetURL() const override;
  bool IsCurrent() const override;
  bool GetNetworkAlmostIdle() const override;
  bool IsAdFrame() const override;
  bool IsHoldingWebLock() const override;
  bool IsHoldingIndexedDBLock() const override;
  const base::flat_set<const WorkerNode*> GetChildWorkerNodes() const override;
  bool VisitChildDedicatedWorkers(
      const WorkerNodeVisitor& visitor) const override;
  const PriorityAndReason& GetPriorityAndReason() const override;
  bool HadFormInteraction() const override;
  bool IsAudible() const override;
  const base::Optional<gfx::Rect>& GetViewportIntersection() const override;
  Visibility GetVisibility() const override;

  // Properties associated with a Document, which are reset when a
  // different-document navigation is committed in the frame.
  struct DocumentProperties {
    DocumentProperties();
    ~DocumentProperties();

    void Reset(FrameNodeImpl* frame_node, const GURL& url_in);

    ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
        GURL,
        const GURL&,
        &FrameNodeObserver::OnURLChanged>
        url;
    bool has_nonempty_beforeunload = false;

    // Network is considered almost idle when there are no more than 2 network
    // connections.
    ObservedProperty::NotifiesOnlyOnChanges<
        bool,
        &FrameNodeObserver::OnNetworkAlmostIdleChanged>
        network_almost_idle{false};

    // Indicates if a form in the frame has been interacted with.
    ObservedProperty::NotifiesOnlyOnChanges<
        bool,
        &FrameNodeObserver::OnHadFormInteractionChanged>
        had_form_interaction{false};
  };

  // Invoked by subframes on joining/leaving the graph.
  void AddChildFrame(FrameNodeImpl* frame_node);
  void RemoveChildFrame(FrameNodeImpl* frame_node);

  // NodeBase:
  void OnJoiningGraph() override;
  void OnBeforeLeavingGraph() override;
  void RemoveNodeAttachedData() override;

  // Helper function to sever all opened page relationships. This is called
  // before destroying the frame node in "OnBeforeLeavingGraph". Note that this
  // will reparent opened pages to this frame's parent so that tracking is
  // maintained.
  void SeverOpenedPagesAndMaybeReparent();

  // This is not quite the same as GetMainFrame, because there can be multiple
  // main frames while the main frame is navigating. This explicitly walks up
  // the tree to find the main frame that corresponds to this frame tree node,
  // even if it is not current.
  FrameNodeImpl* GetFrameTreeRoot() const;

  bool HasFrameNodeInAncestors(FrameNodeImpl* frame_node) const;
  bool HasFrameNodeInDescendants(FrameNodeImpl* frame_node) const;
  bool HasFrameNodeInTree(FrameNodeImpl* frame_node) const;

  // Returns the initial visibility of this frame. Should only be called when
  // the frame node joins the graph.
  Visibility GetInitialFrameVisibility() const;

  mojo::Receiver<mojom::DocumentCoordinationUnit> receiver_{this};

  FrameNodeImpl* const parent_frame_node_;
  PageNodeImpl* const page_node_;
  ProcessNodeImpl* const process_node_;
  // Can be used to tie together "sibling" frames, where a navigation is ongoing
  // in a new frame that will soon replace the existing one.
  const int frame_tree_node_id_;
  // The routing id of the frame.
  const int render_frame_id_;

  // This is the unique token for this frame instance as per e.g.
  // RenderFrameHost::GetFrameToken().
  const blink::LocalFrameToken frame_token_;

  // The unique ID of the BrowsingInstance this frame belongs to. Frames in the
  // same BrowsingInstance are allowed to script each other at least
  // asynchronously (if cross-site), and sometimes synchronously (if same-site,
  // and thus same SiteInstance).
  const int32_t browsing_instance_id_;
  // The unique ID of the SiteInstance this frame belongs to. Frames in the
  // same SiteInstance may sychronously script each other. Frames with the
  // same |site_instance_id_| will also have the same |browsing_instance_id_|.
  const int32_t site_instance_id_;
  // A proxy object that lets the underlying RFH be safely dereferenced on the
  // UI thread.
  const RenderFrameHostProxy render_frame_host_proxy_;

  base::flat_set<FrameNodeImpl*> child_frame_nodes_;

  // The set of pages that have been opened by this frame.
  base::flat_set<PageNodeImpl*> opened_page_nodes_;

  // Does *not* change when a navigation is committed.
  ObservedProperty::NotifiesOnlyOnChanges<
      LifecycleState,
      &FrameNodeObserver::OnFrameLifecycleStateChanged>
      lifecycle_state_{LifecycleState::kRunning};

  ObservedProperty::
      NotifiesOnlyOnChanges<bool, &FrameNodeObserver::OnIsAdFrameChanged>
          is_ad_frame_{false};

  // Locks held by a frame are tracked independently from navigation
  // (specifically, a few tasks must run in the Web Lock and IndexedDB
  // subsystems after a navigation for locks to be released).
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &FrameNodeObserver::OnFrameIsHoldingWebLockChanged>
      is_holding_weblock_{false};
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &FrameNodeObserver::OnFrameIsHoldingIndexedDBLockChanged>
      is_holding_indexeddb_lock_{false};

  ObservedProperty::
      NotifiesOnlyOnChanges<bool, &FrameNodeObserver::OnIsCurrentChanged>
          is_current_{false};

  // Properties associated with a Document, which are reset when a
  // different-document navigation is committed in the frame.
  //
  // TODO(fdoray): Cleanup this once there is a 1:1 mapping between
  // RenderFrameHost and Document https://crbug.com/936696.
  DocumentProperties document_;

  // The child workers of this frame.
  base::flat_set<WorkerNodeImpl*> child_worker_nodes_;

  // Frame priority information. Set via ExecutionContextPriorityDecorator.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      PriorityAndReason,
      const PriorityAndReason&,
      &FrameNodeObserver::OnPriorityAndReasonChanged>
      priority_and_reason_{PriorityAndReason(base::TaskPriority::LOWEST,
                                             kDefaultPriorityReason)};

  // Indicates if the frame is audible. This is tracked independently of a
  // document, and if a document swap occurs the audio stream monitor machinery
  // will keep this up to date.
  ObservedProperty::
      NotifiesOnlyOnChanges<bool, &FrameNodeObserver::OnIsAudibleChanged>
          is_audible_{false};

  // Tracks the intersection of this frame with the viewport.
  //
  // Note that the viewport intersection for the main frame is always invalid.
  // This is because the main frame always occupies the entirety of the viewport
  // so there is no point in tracking it. To avoid programming mistakes, it is
  // forbidden to query this property for the main frame.
  ObservedProperty::NotifiesOnlyOnChanges<
      base::Optional<gfx::Rect>,
      &FrameNodeObserver::OnViewportIntersectionChanged>
      viewport_intersection_;

  // Indicates if the frame is visible. This is initialized in
  // FrameNodeImpl::OnJoiningGraph() and then maintained by
  // FrameVisibilityDecorator.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      Visibility,
      Visibility,
      &FrameNodeObserver::OnFrameVisibilityChanged>
      visibility_{Visibility::kUnknown};

  // Inline storage for ExecutionContext.
  std::unique_ptr<NodeAttachedData> execution_context_;

  base::WeakPtr<FrameNodeImpl> weak_this_;
  base::WeakPtrFactory<FrameNodeImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  DISALLOW_COPY_AND_ASSIGN(FrameNodeImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_
