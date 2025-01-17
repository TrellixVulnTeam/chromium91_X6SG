// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_H_

#include <map>

#include "base/containers/flat_map.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

namespace blink {

// The PermissionsPolicyFeatureDefault enum defines the default enable state for
// a feature when the feature is not declared in iframe 'allow' attribute.
// See |PermissionsPolicy::InheritedValueForFeature| for usage.
//
// The 2 possibilities map directly to Permissions Policy Allowlist semantics.
//
// The default values for each feature are set in GetDefaultFeatureList.
enum class PermissionsPolicyFeatureDefault {
  // Equivalent to ["self"]. If this default policy is in effect for a frame,
  // then the feature will be enabled for that frame, and any same-origin
  // child frames, but not for any cross-origin child frames.
  EnableForSelf,

  // Equivalent to ["*"]. If in effect for a frame, then the feature is
  // enabled for that frame and all of its children.
  EnableForAll
};

using PermissionsPolicyFeatureList =
    std::map<mojom::PermissionsPolicyFeature, PermissionsPolicyFeatureDefault>;

BLINK_COMMON_EXPORT const PermissionsPolicyFeatureList&
GetPermissionsPolicyFeatureList();

// TODO(iclelland): Generate, instead of this map, a set of bool flags, one
// for each feature, as all features are supposed to be represented here.
using PermissionsPolicyFeatureState =
    std::map<mojom::PermissionsPolicyFeature, bool>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_H_
