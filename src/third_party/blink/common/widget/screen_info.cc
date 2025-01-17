// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/widget/screen_info.h"

namespace blink {

ScreenInfo::ScreenInfo() = default;
ScreenInfo::ScreenInfo(const ScreenInfo& other) = default;
ScreenInfo::~ScreenInfo() = default;
ScreenInfo& ScreenInfo::operator=(const ScreenInfo& other) = default;

bool ScreenInfo::operator==(const ScreenInfo& other) const {
  return device_scale_factor == other.device_scale_factor &&
         display_color_spaces == other.display_color_spaces &&
         depth == other.depth &&
         depth_per_component == other.depth_per_component &&
         is_monochrome == other.is_monochrome &&
         display_frequency == other.display_frequency && rect == other.rect &&
         available_rect == other.available_rect &&
#if defined(USE_NEVA_MEDIA)
         additional_contents_scale == other.additional_contents_scale &&
#endif
         orientation_type == other.orientation_type &&
         orientation_angle == other.orientation_angle &&
         is_extended == other.is_extended && is_primary == other.is_primary &&
         is_internal == other.is_internal && display_id == other.display_id;
}

bool ScreenInfo::operator!=(const ScreenInfo& other) const {
  return !operator==(other);
}

}  // namespace blink
