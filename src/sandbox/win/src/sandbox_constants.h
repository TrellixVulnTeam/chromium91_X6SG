// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SANDBOX_CONSTANTS_H_
#define SANDBOX_WIN_SRC_SANDBOX_CONSTANTS_H_

namespace sandbox {
// Strings used as keys in base::Value snapshots of Policies.
extern const char kAppContainerCapabilities[];
extern const char kAppContainerInitialCapabilities[];
extern const char kAppContainerSid[];
extern const char kDesiredIntegrityLevel[];
extern const char kDesiredMitigations[];
extern const char kDisconnectCsrss[];
extern const char kHandlesToClose[];
extern const char kJobLevel[];
extern const char kLockdownLevel[];
extern const char kLowboxSid[];
extern const char kPlatformMitigations[];
extern const char kPolicyRules[];
extern const char kProcessIds[];

// Strings used as values in snapshots of Policies.
extern const char kDisabled[];
extern const char kEnabled[];
}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SANDBOX_CONSTANTS_H_
