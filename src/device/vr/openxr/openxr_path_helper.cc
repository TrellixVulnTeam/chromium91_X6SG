// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_path_helper.h"

#include "base/check.h"
#include "base/stl_util.h"
#include "device/vr/openxr/openxr_util.h"

namespace device {

OpenXRPathHelper::OpenXRPathHelper() {}

OpenXRPathHelper::~OpenXRPathHelper() = default;

XrResult OpenXRPathHelper::Initialize(XrInstance instance, XrSystemId system) {
  DCHECK(!initialized_);

  // Get the system properties, which is needed to determine the name of the
  // hardware being used. This helps disambiguate certain sets of controllers.
  XrSystemProperties system_properties = {XR_TYPE_SYSTEM_PROPERTIES};
  RETURN_IF_XR_FAILED(
      xrGetSystemProperties(instance, system, &system_properties));
  system_name_ = std::string(system_properties.systemName);

  // Create path declarations
  for (const auto& profile : kOpenXrControllerInteractionProfiles) {
    RETURN_IF_XR_FAILED(
        xrStringToPath(instance, profile.path,
                       &(declared_interaction_profile_paths_[profile.type])));
  }
  initialized_ = true;

  return XR_SUCCESS;
}

OpenXrInteractionProfileType OpenXRPathHelper::GetInputProfileType(
    XrPath interaction_profile) const {
  DCHECK(initialized_);
  for (auto it : declared_interaction_profile_paths_) {
    if (it.second == interaction_profile) {
      return it.first;
    }
  }
  return OpenXrInteractionProfileType::kCount;
}

std::vector<std::string> OpenXRPathHelper::GetInputProfiles(
    OpenXrInteractionProfileType interaction_profile) const {
  DCHECK(initialized_);

  for (auto& it : kOpenXrControllerInteractionProfiles) {
    if (it.type == interaction_profile) {
      const OpenXrSystemInputProfiles* active_system = nullptr;
      for (size_t system_index = 0; system_index < it.input_profile_size;
           system_index++) {
        const OpenXrSystemInputProfiles& system =
            it.system_input_profiles[system_index];
        if (system.system_name == nullptr) {
          active_system = &system;
        } else if (system_name_ == system.system_name) {
          active_system = &system;
          break;
        }
      }

      // Each interaction profile should always at least have a null system_name
      // entry.
      DCHECK(active_system);
      return std::vector<std::string>(
          active_system->input_profiles,
          active_system->input_profiles + active_system->profile_size);
    }
  }

  return {};
}

XrPath OpenXRPathHelper::GetInteractionProfileXrPath(
    OpenXrInteractionProfileType type) const {
  if (type == OpenXrInteractionProfileType::kCount) {
    return XR_NULL_PATH;
  }
  return declared_interaction_profile_paths_.at(type);
}

}  // namespace device
