// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/routine_properties.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace diagnostics {

TEST(RoutineTypeUtilTtest, RoutinePropertiesListUpToDate) {
  EXPECT_EQ(kRoutinePropertiesLength,
            static_cast<size_t>(mojom::RoutineType::kMaxValue) + 1);
  for (size_t i = 0; i < kRoutinePropertiesLength; i++) {
    EXPECT_EQ(static_cast<mojom::RoutineType>(i), kRoutineProperties[i].type);
  }
}

}  // namespace diagnostics
}  // namespace chromeos
