// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_WATCHDOG_TIMEOUT_H_
#define GPU_IPC_COMMON_GPU_WATCHDOG_TIMEOUT_H_

#include "base/time/time.h"
#include "build/build_config.h"

namespace gpu {

// TODO(magchen): crbug.com/949839. Move all constants back to
// gpu/ipc/service/gpu_watchdog_thread.h after the GPU watchdog V2 is fully
// launched.

#if defined(CYGPROFILE_INSTRUMENTATION)
constexpr base::TimeDelta kGpuWatchdogTimeout =
    base::TimeDelta::FromSeconds(30);
#elif defined(OS_MAC)
constexpr base::TimeDelta kGpuWatchdogTimeout =
    base::TimeDelta::FromSeconds(25);
#elif defined(OS_WIN)
constexpr base::TimeDelta kGpuWatchdogTimeout =
    base::TimeDelta::FromSeconds(30);
#elif defined(OS_FUCHSIA)
// Increased temporarily to investigate if this helps https://crbug.com/1185119
// (GPU process hangs when running blink web tests on Fuchsia).
constexpr base::TimeDelta kGpuWatchdogTimeout =
    base::TimeDelta::FromSeconds(30);
#else
constexpr base::TimeDelta kGpuWatchdogTimeout =
    base::TimeDelta::FromSeconds(15);
#endif

// It usually takes longer to finish a GPU task when the system just resumes
// from power suspension or when the Android app switches from the background to
// the foreground. This is the factor the original timeout will be multiplied.
constexpr int kRestartFactor = 2;

// It takes longer to initialize GPU process in Windows. See
// https://crbug.com/949839 for details.
#if defined(OS_WIN)
constexpr int kInitFactor = 2;
#else
constexpr int kInitFactor = 1;
#endif

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_WATCHDOG_TIMEOUT_H_
