// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/memory_ablation_study.h"

#include <algorithm>
#include <cstring>

#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/timer/timer.h"
#include "crypto/random.h"

namespace chromeos {

namespace {

// The name of the Finch study that turns on the experiment.
const base::Feature kCrosMemoryAblationStudy{"CrosMemoryAblationStudy",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// The total amount of memory to ablate in MB.
const char kAblationSizeMb[] = "ablation-size-mb";

// Number of seconds to wait between allocation periods.
constexpr int kAllocateTimerIntervalSeconds = 10;

// Maximum number of MB to allocate at a time.
constexpr int kAllocateAmountMb = 10;

// Numbers of seconds to wait between reading the next region.
constexpr int kReadTimerIntervalSeconds = 30;

// Size in bytes of the uncompressible region.
constexpr int kUncompressibleRegionSize = 4096;

}  // namespace

MemoryAblationStudy::MemoryAblationStudy() {
  // This class does nothing if the study is disabled.
  if (!base::FeatureList::IsEnabled(kCrosMemoryAblationStudy)) {
    return;
  }

  remaining_allocation_mb_ = base::GetFieldTrialParamByFeatureAsInt(
      kCrosMemoryAblationStudy, kAblationSizeMb, /*default_value=*/0);
  if (remaining_allocation_mb_ <= 0)
    return;

  allocate_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kAllocateTimerIntervalSeconds),
      base::BindRepeating(&MemoryAblationStudy::Allocate,
                          base::Unretained(this)));
  read_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kReadTimerIntervalSeconds),
      base::BindRepeating(&MemoryAblationStudy::Read, base::Unretained(this)));
}

MemoryAblationStudy::~MemoryAblationStudy() {
  // Avoid compiler optimizations by aliasing |dummy_read_|.
  uint8_t dummy = dummy_read_;
  base::debug::Alias(&dummy);
}

void MemoryAblationStudy::Allocate() {
  CHECK_GT(remaining_allocation_mb_, 0);
  int32_t amount_to_allocate_mb =
      std::min(remaining_allocation_mb_, kAllocateAmountMb);

  // Do accounting first. Stop the timer if this is the last allocation.
  remaining_allocation_mb_ -= amount_to_allocate_mb;
  if (remaining_allocation_mb_ <= 0) {
    allocate_timer_.Stop();
  }

  // Generate the initial uncompressible region if necessary.
  if (uncompressible_region_.empty()) {
    uncompressible_region_.resize(kUncompressibleRegionSize);
    crypto::RandBytes(uncompressible_region_.data(), kUncompressibleRegionSize);
  }

  // Allocate the new region.
  size_t amount_to_allocate_bytes = amount_to_allocate_mb * 1024 * 1024;
  Region region;
  region.resize(amount_to_allocate_bytes);

  // Fill it with uncompressible bytes.
  DCHECK_EQ(amount_to_allocate_bytes % kUncompressibleRegionSize, 0u);
  uint8_t* ptr = region.data();
  for (size_t offset = 0; offset < amount_to_allocate_bytes;
       offset += kUncompressibleRegionSize) {
    memcpy(ptr + offset, uncompressible_region_.data(),
           kUncompressibleRegionSize);
  }

  regions_.push_back(std::move(region));
}

void MemoryAblationStudy::Read() {
  if (regions_.empty())
    return;

  last_region_read_ = ((last_region_read_ + 1) % regions_.size());
  Region& region = regions_[last_region_read_];
  uint8_t* ptr = region.data();
  for (size_t offset = 0; offset < region.size();
       offset += kUncompressibleRegionSize) {
    dummy_read_ += ptr[offset];
  }
}

}  // namespace chromeos
