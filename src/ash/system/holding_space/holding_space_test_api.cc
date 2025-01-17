// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_test_api.h"

#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_item_screen_capture_view.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/status_area_widget.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Finds all descendents of `parent` matching a specific class.
template <typename T>
void FindDescendentsOfClass(views::View* parent,
                            std::vector<views::View*>* descendents) {
  if (parent) {
    for (auto* child : parent->children()) {
      if (child->GetClassName() == T::kViewClassName)
        descendents->push_back(child);
      FindDescendentsOfClass<T>(child, descendents);
    }
  }
}

// Performs a tap on the specified `view`.
void TapOn(const views::View* view) {
  ui::test::EventGenerator event_generator(Shell::GetRootWindowForNewWindows());
  event_generator.MoveTouch(view->GetBoundsInScreen().CenterPoint());
  event_generator.PressTouch();
  event_generator.ReleaseTouch();
}

}  // namespace

// HoldingSpaceTestApi ---------------------------------------------------------

HoldingSpaceTestApi::HoldingSpaceTestApi()
    : holding_space_tray_(Shelf::ForWindow(Shell::GetRootWindowForNewWindows())
                              ->shelf_widget()
                              ->status_area_widget()
                              ->holding_space_tray()) {
  holding_space_tray_->set_use_zero_previews_update_delay_for_testing(true);
  // Holding space tests perform drag/drop so we need to disable blocking.
  auto* drag_drop_controller = ShellTestApi().drag_drop_controller();
  drag_drop_controller->set_should_block_during_drag_drop(false);
}

HoldingSpaceTestApi::~HoldingSpaceTestApi() {
  if (!Shell::HasInstance())
    return;

  holding_space_tray_->set_use_zero_previews_update_delay_for_testing(false);
  // Enable blocking during drag/drop that was disabled for holding space tests.
  auto* drag_drop_controller = ShellTestApi().drag_drop_controller();
  drag_drop_controller->set_should_block_during_drag_drop(true);
}

// static
aura::Window* HoldingSpaceTestApi::GetRootWindowForNewWindows() {
  return Shell::GetRootWindowForNewWindows();
}

void HoldingSpaceTestApi::Show() {
  if (!IsShowing())
    TapOn(holding_space_tray_);
}

void HoldingSpaceTestApi::Close() {
  if (IsShowing())
    TapOn(holding_space_tray_);
}

bool HoldingSpaceTestApi::IsShowing() {
  return holding_space_tray_ && holding_space_tray_->GetBubbleView() &&
         holding_space_tray_->GetBubbleView()->GetVisible();
}

bool HoldingSpaceTestApi::IsShowingInShelf() {
  return holding_space_tray_ && holding_space_tray_->GetVisible();
}

views::View* HoldingSpaceTestApi::GetDownloadsSectionHeader() {
  return holding_space_tray_->GetBubbleView()
             ? holding_space_tray_->GetBubbleView()->GetViewByID(
                   kHoldingSpaceDownloadsSectionHeaderId)
             : nullptr;
}

std::vector<views::View*> HoldingSpaceTestApi::GetDownloadChips() {
  std::vector<views::View*> download_chips;
  if (holding_space_tray_->GetBubbleView()) {
    FindDescendentsOfClass<HoldingSpaceItemChipView>(
        holding_space_tray_->GetBubbleView()->GetViewByID(
            kHoldingSpaceRecentFilesBubbleId),
        &download_chips);
  }
  return download_chips;
}

std::vector<views::View*> HoldingSpaceTestApi::GetPinnedFileChips() {
  std::vector<views::View*> pinned_file_chips;
  if (holding_space_tray_->GetBubbleView()) {
    FindDescendentsOfClass<HoldingSpaceItemChipView>(
        holding_space_tray_->GetBubbleView()->GetViewByID(
            kHoldingSpacePinnedFilesBubbleId),
        &pinned_file_chips);
  }
  return pinned_file_chips;
}

std::vector<views::View*> HoldingSpaceTestApi::GetScreenCaptureViews() {
  std::vector<views::View*> screen_capture_views;
  if (holding_space_tray_->GetBubbleView()) {
    FindDescendentsOfClass<HoldingSpaceItemScreenCaptureView>(
        holding_space_tray_->GetBubbleView()->GetViewByID(
            kHoldingSpaceRecentFilesBubbleId),
        &screen_capture_views);
  }
  return screen_capture_views;
}

views::View* HoldingSpaceTestApi::GetTray() {
  return holding_space_tray_;
}

views::View* HoldingSpaceTestApi::GetTrayDropTargetOverlay() {
  return holding_space_tray_->GetViewByID(kHoldingSpaceTrayDropTargetOverlayId);
}

views::View* HoldingSpaceTestApi::GetDefaultTrayIcon() {
  return holding_space_tray_->GetViewByID(kHoldingSpaceTrayDefaultIconId);
}

views::View* HoldingSpaceTestApi::GetPreviewsTrayIcon() {
  return holding_space_tray_->GetViewByID(kHoldingSpaceTrayPreviewsIconId);
}

views::View* HoldingSpaceTestApi::GetPinnedFilesBubble() {
  if (!holding_space_tray_->GetBubbleView())
    return nullptr;
  return holding_space_tray_->GetBubbleView()->GetViewByID(
      kHoldingSpacePinnedFilesBubbleId);
}

bool HoldingSpaceTestApi::PinnedFilesBubbleShown() const {
  if (!holding_space_tray_->GetBubbleView())
    return false;

  views::View* bubble = holding_space_tray_->GetBubbleView()->GetViewByID(
      kHoldingSpacePinnedFilesBubbleId);
  return bubble && bubble->GetVisible();
}

bool HoldingSpaceTestApi::RecentFilesBubbleShown() const {
  if (!holding_space_tray_->GetBubbleView())
    return false;

  views::View* bubble = holding_space_tray_->GetBubbleView()->GetViewByID(
      kHoldingSpaceRecentFilesBubbleId);
  return bubble && bubble->GetVisible();
}

}  // namespace ash
