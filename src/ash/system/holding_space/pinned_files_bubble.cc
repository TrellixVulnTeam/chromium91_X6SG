// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/pinned_files_bubble.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/system/holding_space/pinned_files_section.h"

namespace ash {

PinnedFilesBubble::PinnedFilesBubble(HoldingSpaceItemViewDelegate* delegate)
    : HoldingSpaceTrayChildBubble(delegate) {
  SetID(kHoldingSpacePinnedFilesBubbleId);
}

PinnedFilesBubble::~PinnedFilesBubble() = default;

const char* PinnedFilesBubble::GetClassName() const {
  return "PinnedFilesBubble";
}

std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>>
PinnedFilesBubble::CreateSections() {
  std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> sections;
  sections.push_back(std::make_unique<PinnedFilesSection>(delegate()));
  return sections;
}

}  // namespace ash
