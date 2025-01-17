// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace page_info {

#if defined(OS_ANDROID)
const base::Feature kPageInfoDiscoverability{"PageInfoDiscoverability",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPageInfoHistory{"PageInfoHistory",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPageInfoV2{"PageInfoV2", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if !defined(OS_ANDROID)
const base::Feature kPageInfoV2Desktop{"PageInfoV2Desktop",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace page_info
