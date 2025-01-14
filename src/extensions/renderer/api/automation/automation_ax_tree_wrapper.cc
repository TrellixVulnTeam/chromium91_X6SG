// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/no_destructor.h"
#include "components/crash/core/common/crash_key.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/api/automation/automation_api_util.h"
#include "extensions/renderer/api/automation/automation_internal_custom_bindings.h"
#include "ui/accessibility/ax_language_detection.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_tree_manager_map.h"

namespace extensions {

// Multiroot tree lookup.
// These maps support moving from a node to a descendant tree node via an app id
// (and vice versa).
std::map<std::string, std::pair<ui::AXTreeID, int32_t>>&
GetAppIDToChildTreeNodeMap() {
  static base::NoDestructor<
      std::map<std::string, std::pair<ui::AXTreeID, int32_t>>>
      app_id_to_child_tree_node_map;
  return *app_id_to_child_tree_node_map;
}

std::map<std::string, std::pair<ui::AXTreeID, int32_t>>&
GetAppIDToParentTreeNodeMap() {
  static base::NoDestructor<
      std::map<std::string, std::pair<ui::AXTreeID, int32_t>>>
      app_id_to_parent_tree_node_map;
  return *app_id_to_parent_tree_node_map;
}

AutomationAXTreeWrapper::AutomationAXTreeWrapper(
    ui::AXTreeID tree_id,
    AutomationInternalCustomBindings* owner)
    : tree_id_(tree_id), owner_(owner), event_generator_(&tree_) {
  tree_.AddObserver(this);
  ui::AXTreeManagerMap::GetInstance().AddTreeManager(tree_id, this);
  event_generator_.set_always_fire_load_complete(true);
}

AutomationAXTreeWrapper::~AutomationAXTreeWrapper() {
  // Stop observing so we don't get a callback for every node being deleted.
  event_generator_.SetTree(nullptr);
  tree_.RemoveObserver(this);
  ui::AXTreeManagerMap::GetInstance().RemoveTreeManager(tree_id_);
}

// static
AutomationAXTreeWrapper* AutomationAXTreeWrapper::GetParentOfTreeId(
    ui::AXTreeID tree_id) {
  std::map<ui::AXTreeID, AutomationAXTreeWrapper*>& child_tree_id_reverse_map =
      GetChildTreeIDReverseMap();
  const auto& iter = child_tree_id_reverse_map.find(tree_id);
  if (iter != child_tree_id_reverse_map.end())
    return iter->second;

  return nullptr;
}

bool AutomationAXTreeWrapper::OnAccessibilityEvents(
    const ExtensionMsg_AccessibilityEventBundleParams& event_bundle,
    bool is_active_profile) {
  TRACE_EVENT0("accessibility",
               "AutomationAXTreeWrapper::OnAccessibilityEvents");

  base::Optional<gfx::Rect> previous_accessibility_focused_global_bounds =
      owner_->GetAccessibilityFocusedLocation();

  std::map<ui::AXTreeID, AutomationAXTreeWrapper*>& child_tree_id_reverse_map =
      GetChildTreeIDReverseMap();
  const auto& child_tree_ids = tree_.GetAllChildTreeIds();

  // Invalidate any reverse child tree id mappings. Note that it is possible
  // there are no entries in this map for a given child tree to |this|, if this
  // is the first event from |this| tree or if |this| was destroyed and (and
  // then reset).
  base::EraseIf(child_tree_id_reverse_map, [child_tree_ids](auto& pair) {
    return child_tree_ids.count(pair.first);
  });

  // Unserialize all incoming data.
  for (const auto& update : event_bundle.updates) {
    deleted_node_ids_.clear();
    did_send_tree_change_during_unserialization_ = false;

    if (!tree_.Unserialize(update)) {
      static crash_reporter::CrashKeyString<4> crash_key(
          "ax-tree-wrapper-unserialize-failed");
      crash_key.Set("yes");
      event_generator_.ClearEvents();
      return false;
    }

    if (is_active_profile) {
      owner_->SendNodesRemovedEvent(&tree_, deleted_node_ids_);

      if (update.nodes.size() && did_send_tree_change_during_unserialization_) {
        owner_->SendTreeChangeEvent(
            api::automation::TREE_CHANGE_TYPE_SUBTREEUPDATEEND, &tree_,
            tree_.root());
      }
    }
  }

  // Refresh child tree id  mappings.
  for (const ui::AXTreeID& tree_id : tree_.GetAllChildTreeIds()) {
    DCHECK(!base::Contains(child_tree_id_reverse_map, tree_id));
    child_tree_id_reverse_map.insert(std::make_pair(tree_id, this));
  }

  // Exit early if this isn't the active profile.
  if (!is_active_profile) {
    event_generator_.ClearEvents();
    return true;
  }

  // Perform language detection first thing if we see a load complete event.
  // We have to run *before* we send the load complete event to javascript
  // otherwise code which runs immediately on load complete will not be able
  // to see the results of language detection.
  //
  // Currently language detection only runs once for initial load complete, any
  // content loaded after this will not have language detection performed for
  // it.
  for (const auto& targeted_event : event_generator_) {
    if (targeted_event.event_params.event ==
        ui::AXEventGenerator::Event::LOAD_COMPLETE) {
      tree_.language_detection_manager->DetectLanguages();
      tree_.language_detection_manager->LabelLanguages();

      // After initial language detection, enable language detection for future
      // content updates in order to support dynamic content changes.
      //
      // If the LanguageDetectionDynamic feature flag is not enabled then this
      // is a no-op.
      tree_.language_detection_manager->RegisterLanguageDetectionObserver();

      break;
    }
  }

  // Send all blur and focus events first.
  owner_->MaybeSendFocusAndBlur(this, event_bundle);

  // Send auto-generated AXEventGenerator events.
  for (const auto& targeted_event : event_generator_) {
    if (ShouldIgnoreGeneratedEvent(targeted_event.event_params.event))
      continue;
    ui::AXEvent generated_event;
    generated_event.id = targeted_event.node->id();
    generated_event.event_from = targeted_event.event_params.event_from;
    generated_event.event_from_action =
        targeted_event.event_params.event_from_action;
    generated_event.event_intents = targeted_event.event_params.event_intents;
    owner_->SendAutomationEvent(event_bundle.tree_id,
                                event_bundle.mouse_location, generated_event,
                                targeted_event.event_params.event);
  }
  event_generator_.ClearEvents();

  for (const auto& event : event_bundle.events) {
    if (event.event_type == ax::mojom::Event::kFocus ||
        event.event_type == ax::mojom::Event::kBlur)
      continue;

    // Send some events directly.
    if (!ShouldIgnoreAXEvent(event.event_type)) {
      owner_->SendAutomationEvent(event_bundle.tree_id,
                                  event_bundle.mouse_location, event);
    }
  }

  if (previous_accessibility_focused_global_bounds.has_value() &&
      previous_accessibility_focused_global_bounds !=
          owner_->GetAccessibilityFocusedLocation()) {
    owner_->SendAccessibilityFocusedLocationChange(event_bundle.mouse_location);
  }

  return true;
}

bool AutomationAXTreeWrapper::IsDesktopTree() const {
  return tree_.root() ? tree_.root()->data().role == ax::mojom::Role::kDesktop
                      : false;
}

bool AutomationAXTreeWrapper::IsInFocusChain(int32_t node_id) {
  if (tree()->data().focus_id != node_id)
    return false;

  if (IsDesktopTree())
    return true;

  AutomationAXTreeWrapper* descendant_tree = this;
  ui::AXTreeID descendant_tree_id = GetTreeID();
  AutomationAXTreeWrapper* ancestor_tree = descendant_tree;
  bool found = true;
  while ((ancestor_tree = ancestor_tree->GetParentTree())) {
    int32_t ancestor_tree_focus_id = ancestor_tree->tree()->data().focus_id;
    ui::AXNode* ancestor_tree_focused_node =
        ancestor_tree->tree()->GetFromId(ancestor_tree_focus_id);
    if (!ancestor_tree_focused_node)
      return false;

    if (ancestor_tree_focused_node->HasStringAttribute(
            ax::mojom::StringAttribute::kChildTreeNodeAppId)) {
      // |ancestor_tree_focused_node| points to a tree with multiple roots as
      // its child tree node. Ensure the node points back to
      // |ancestor_tree_focused_node| as its parent.
      ui::AXNode* parent_node = descendant_tree->GetParentTreeNodeForAppID(
          ancestor_tree_focused_node->GetStringAttribute(
              ax::mojom::StringAttribute::kChildTreeNodeAppId),
          owner_);
      if (parent_node != ancestor_tree_focused_node)
        return false;
    } else if (ui::AXTreeID::FromString(
                   ancestor_tree_focused_node->GetStringAttribute(
                       ax::mojom::StringAttribute::kChildTreeId)) !=
                   descendant_tree_id &&
               ancestor_tree->tree()->data().focused_tree_id !=
                   descendant_tree_id) {
      // Surprisingly, an ancestor frame can "skip" a child frame to point to a
      // descendant granchild, so we have to scan upwards.
      found = false;
      continue;
    }

    found = true;

    if (ancestor_tree->IsDesktopTree())
      return true;

    descendant_tree_id = ancestor_tree->GetTreeID();
    descendant_tree = ancestor_tree;
  }

  // We can end up here if the tree is detached from any desktop.  This can
  // occur in tabs-only mode. This is also the codepath for frames with inner
  // focus, but which are not focused by ancestor frames.
  return found;
}

ui::AXTree::Selection AutomationAXTreeWrapper::GetUnignoredSelection() {
  return tree()->GetUnignoredSelection();
}

ui::AXNode* AutomationAXTreeWrapper::GetUnignoredNodeFromId(int32_t id) {
  ui::AXNode* node = tree_.GetFromId(id);
  return (node && !node->IsIgnored()) ? node : nullptr;
}

void AutomationAXTreeWrapper::SetAccessibilityFocus(int32_t node_id) {
  accessibility_focused_id_ = node_id;
}

ui::AXNode* AutomationAXTreeWrapper::GetAccessibilityFocusedNode() {
  return accessibility_focused_id_ == ui::kInvalidAXNodeID
             ? nullptr
             : tree_.GetFromId(accessibility_focused_id_);
}

AutomationAXTreeWrapper* AutomationAXTreeWrapper::GetParentTree() {
  // Explicit parent tree from this tree's data.
  auto* ret = GetParentOfTreeId(tree()->data().tree_id);

  // If this tree has multiple roots, and no explicit parent tree, fallback to
  // any node with a parent tree node app id to find a parent tree.
  return ret ? ret : GetParentTreeFromAnyAppID();
}

AutomationAXTreeWrapper*
AutomationAXTreeWrapper::GetTreeWrapperWithUnignoredRoot() {
  // The desktop is always unignored.
  if (IsDesktopTree())
    return this;

  // Keep following these parent node id links upwards, since we want to ignore
  // these roots for the api in js.
  AutomationAXTreeWrapper* current = this;
  AutomationAXTreeWrapper* parent = this;
  while ((parent = current->GetParentTreeFromAnyAppID()))
    current = parent;

  return current;
}

AutomationAXTreeWrapper* AutomationAXTreeWrapper::GetParentTreeFromAnyAppID() {
  for (const std::string& app_id : all_parent_tree_node_app_ids_) {
    auto* wrapper = GetParentTreeWrapperForAppID(app_id, owner_);
    if (wrapper)
      return wrapper;
  }

  return nullptr;
}

void AutomationAXTreeWrapper::EventListenerAdded(
    api::automation::EventType event_type,
    ui::AXNode* node) {
  node_id_to_events_[node->id()].insert(event_type);
}

void AutomationAXTreeWrapper::EventListenerRemoved(
    api::automation::EventType event_type,
    ui::AXNode* node) {
  auto it = node_id_to_events_.find(node->id());
  if (it != node_id_to_events_.end())
    it->second.erase(event_type);
}

bool AutomationAXTreeWrapper::HasEventListener(
    api::automation::EventType event_type,
    ui::AXNode* node) {
  auto it = node_id_to_events_.find(node->id());
  if (it == node_id_to_events_.end())
    return false;

  return it->second.count(event_type);
}

// static
std::map<ui::AXTreeID, AutomationAXTreeWrapper*>&
AutomationAXTreeWrapper::GetChildTreeIDReverseMap() {
  static base::NoDestructor<std::map<ui::AXTreeID, AutomationAXTreeWrapper*>>
      child_tree_id_reverse_map;
  return *child_tree_id_reverse_map;
}

// static
ui::AXNode* AutomationAXTreeWrapper::GetParentTreeNodeForAppID(
    const std::string& app_id,
    const AutomationInternalCustomBindings* owner) {
  auto& map = GetAppIDToChildTreeNodeMap();
  auto it = map.find(app_id);
  if (it == map.end())
    return nullptr;

  AutomationAXTreeWrapper* wrapper =
      owner->GetAutomationAXTreeWrapperFromTreeID(it->second.first);
  if (!wrapper)
    return nullptr;

  return wrapper->tree()->GetFromId(it->second.second);
}

// static
AutomationAXTreeWrapper* AutomationAXTreeWrapper::GetParentTreeWrapperForAppID(
    const std::string& app_id,
    const AutomationInternalCustomBindings* owner) {
  auto& map = GetAppIDToChildTreeNodeMap();
  auto it = map.find(app_id);
  if (it == map.end())
    return nullptr;

  return owner->GetAutomationAXTreeWrapperFromTreeID(it->second.first);
}

// static
ui::AXNode* AutomationAXTreeWrapper::GetChildTreeNodeForAppID(
    const std::string& app_id,
    const AutomationInternalCustomBindings* owner) {
  auto& map = GetAppIDToParentTreeNodeMap();
  auto it = map.find(app_id);
  if (it == map.end())
    return nullptr;

  AutomationAXTreeWrapper* wrapper =
      owner->GetAutomationAXTreeWrapperFromTreeID(it->second.first);
  if (!wrapper)
    return nullptr;

  return wrapper->tree()->GetFromId(it->second.second);
}

void AutomationAXTreeWrapper::OnNodeDataChanged(
    ui::AXTree* tree,
    const ui::AXNodeData& old_node_data,
    const ui::AXNodeData& new_node_data) {
  if (old_node_data.GetStringAttribute(ax::mojom::StringAttribute::kName) !=
      new_node_data.GetStringAttribute(ax::mojom::StringAttribute::kName))
    text_changed_node_ids_.push_back(new_node_data.id);
}

void AutomationAXTreeWrapper::OnStringAttributeChanged(
    ui::AXTree* tree,
    ui::AXNode* node,
    ax::mojom::StringAttribute attr,
    const std::string& old_value,
    const std::string& new_value) {
  if (attr == ax::mojom::StringAttribute::kChildTreeNodeAppId) {
    if (new_value.empty()) {
      GetAppIDToChildTreeNodeMap().erase(old_value);
    } else {
      GetAppIDToChildTreeNodeMap()[new_value] = {tree->GetAXTreeID(),
                                                 node->data().id};
    }
  }

  if (attr == ax::mojom::StringAttribute::kParentTreeNodeAppId) {
    if (new_value.empty()) {
      GetAppIDToParentTreeNodeMap().erase(old_value);
      all_parent_tree_node_app_ids_.erase(old_value);
    } else {
      GetAppIDToParentTreeNodeMap()[new_value] = {tree->GetAXTreeID(),
                                                  node->data().id};
      all_parent_tree_node_app_ids_.insert(new_value);
    }
  }
}

void AutomationAXTreeWrapper::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                  ui::AXNode* node) {
  did_send_tree_change_during_unserialization_ |= owner_->SendTreeChangeEvent(
      api::automation::TREE_CHANGE_TYPE_NODEREMOVED, tree, node);
  deleted_node_ids_.push_back(node->id());
  node_id_to_events_.erase(node->id());

  if (node->data().HasStringAttribute(
          ax::mojom::StringAttribute::kChildTreeNodeAppId)) {
    GetAppIDToChildTreeNodeMap().erase(node->data().GetStringAttribute(
        ax::mojom::StringAttribute::kChildTreeNodeAppId));
  }

  if (node->data().HasStringAttribute(
          ax::mojom::StringAttribute::kParentTreeNodeAppId)) {
    const std::string& app_id = node->data().GetStringAttribute(
        ax::mojom::StringAttribute::kParentTreeNodeAppId);
    GetAppIDToParentTreeNodeMap().erase(app_id);
    all_parent_tree_node_app_ids_.erase(app_id);
  }
}

void AutomationAXTreeWrapper::OnNodeCreated(ui::AXTree* tree,
                                            ui::AXNode* node) {
  if (node->data().HasStringAttribute(
          ax::mojom::StringAttribute::kChildTreeNodeAppId)) {
    GetAppIDToChildTreeNodeMap()[node->data().GetStringAttribute(
        ax::mojom::StringAttribute::kChildTreeNodeAppId)] = {
        node->tree()->GetAXTreeID(), node->id()};
  }

  if (node->data().HasStringAttribute(
          ax::mojom::StringAttribute::kParentTreeNodeAppId)) {
    const std::string& app_id = node->data().GetStringAttribute(
        ax::mojom::StringAttribute::kParentTreeNodeAppId);
    GetAppIDToParentTreeNodeMap()[app_id] = {node->tree()->GetAXTreeID(),
                                             node->id()};
    all_parent_tree_node_app_ids_.insert(app_id);
  }
}

void AutomationAXTreeWrapper::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeObserver::Change>& changes) {
  DCHECK_EQ(&tree_, tree);
  for (const auto& change : changes) {
    ui::AXNode* node = change.node;
    switch (change.type) {
      case NODE_CREATED:
        did_send_tree_change_during_unserialization_ |=
            owner_->SendTreeChangeEvent(
                api::automation::TREE_CHANGE_TYPE_NODECREATED, tree, node);
        break;
      case SUBTREE_CREATED:
        did_send_tree_change_during_unserialization_ |=
            owner_->SendTreeChangeEvent(
                api::automation::TREE_CHANGE_TYPE_SUBTREECREATED, tree, node);
        break;
      case NODE_CHANGED:
        did_send_tree_change_during_unserialization_ |=
            owner_->SendTreeChangeEvent(
                api::automation::TREE_CHANGE_TYPE_NODECHANGED, tree, node);
        break;
      // Unhandled.
      case NODE_REPARENTED:
      case SUBTREE_REPARENTED:
        break;
    }
  }

  for (int id : text_changed_node_ids_) {
    did_send_tree_change_during_unserialization_ |= owner_->SendTreeChangeEvent(
        api::automation::TREE_CHANGE_TYPE_TEXTCHANGED, tree,
        tree->GetFromId(id));
  }
  text_changed_node_ids_.clear();
}

ui::AXNode* AutomationAXTreeWrapper::GetNodeFromTree(
    const ui::AXTreeID tree_id,
    const ui::AXNodeID node_id) const {
  AutomationAXTreeWrapper* tree_wrapper =
      owner_->GetAutomationAXTreeWrapperFromTreeID(tree_id);
  return tree_wrapper ? tree_wrapper->GetNodeFromTree(node_id) : nullptr;
}

ui::AXNode* AutomationAXTreeWrapper::GetNodeFromTree(
    const ui::AXNodeID node_id) const {
  return tree_.GetFromId(node_id);
}

ui::AXTreeID AutomationAXTreeWrapper::GetTreeID() const {
  return tree_id_;
}

ui::AXTreeID AutomationAXTreeWrapper::GetParentTreeID() const {
  AutomationAXTreeWrapper* parent_tree = GetParentOfTreeId(tree_id_);
  return parent_tree ? parent_tree->GetTreeID() : ui::AXTreeIDUnknown();
}

ui::AXNode* AutomationAXTreeWrapper::GetRootAsAXNode() const {
  return tree_.root();
}

ui::AXNode* AutomationAXTreeWrapper::GetParentNodeFromParentTreeAsAXNode()
    const {
  AutomationAXTreeWrapper* wrapper = const_cast<AutomationAXTreeWrapper*>(this);
  return owner_->GetParent(tree_.root(), &wrapper);
}

}  // namespace extensions
