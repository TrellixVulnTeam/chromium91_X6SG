// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace features {

// Uses the Resume method instead of the Catch-up method for animated images.
// - Catch-up behavior tries to keep animated images in pace with wall-clock
//   time. This might require decoding several animation frames if the
//   animation has fallen behind.
// - Resume behavior presents what would have been the next presented frame.
//   This means it might only decode one frame, resuming where it left off.
//   However, if the animation updates faster than the display's refresh rate,
//   it is possible to decode more than a single frame.
const base::Feature kAnimatedImageResume = {"AnimatedImageResume",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables impulse-style scroll animations in place of the default ones.
const base::Feature kImpulseScrollAnimations = {
    "ImpulseScrollAnimations",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Whether the compositor should attempt to sync with the scroll handlers before
// submitting a frame.
const base::Feature kSynchronizedScrolling = {
    "SynchronizedScrolling",
#if defined(OS_ANDROID)
    base::FEATURE_DISABLED_BY_DEFAULT};
#else
    base::FEATURE_ENABLED_BY_DEFAULT};
#endif

bool IsImplLatencyRecoveryEnabled() {
  // TODO(crbug.com/1142598): Latency recovery has been disabled by default
  // since M87. For now, only the flag is removed. If all goes well, remove the
  // code supporting latency recovery.
  return false;
}

bool IsMainLatencyRecoveryEnabled() {
  // TODO(crbug.com/1142598): Latency recovery has been disabled by default
  // since M87. For now, only the flag is removed. If all goes well, remove the
  // code supporting latency recovery.
  return false;
}

const base::Feature kRemoveMobileViewportDoubleTap{
    "RemoveMobileViewportDoubleTap", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kScrollUnification{"ScrollUnification",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSchedulerSmoothnessForAnimatedScrolls{
    "SmoothnessModeForAnimatedScrolls", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWheelEventRegions{"WheelEventRegions",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHudDisplayForPerformanceMetrics{
    "HudDisplayForPerformanceMetrics", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kJankInjectionAblationFeature{
    "JankInjectionAblation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDocumentTransition{"DocumentTransition",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

bool IsDocumentTransitionEnabled() {
  return base::FeatureList::IsEnabled(kDocumentTransition);
}

}  // namespace features
