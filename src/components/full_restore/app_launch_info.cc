// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/app_launch_info.h"

#include <utility>

namespace full_restore {

AppLaunchInfo::AppLaunchInfo(const std::string& app_id,
                             int32_t window_id,
                             apps::mojom::LaunchContainer container,
                             WindowOpenDisposition disposition,
                             int64_t display_id,
                             std::vector<base::FilePath> launch_files,
                             apps::mojom::IntentPtr intent)
    : app_id(app_id),
      window_id(window_id),
      container(static_cast<int32_t>(container)),
      disposition(static_cast<int32_t>(disposition)),
      display_id(display_id),
      file_paths(std::move(launch_files)),
      intent(std::move(intent)) {}

AppLaunchInfo::AppLaunchInfo(const std::string& app_id, int32_t session_id)
    : app_id(app_id), window_id(session_id) {}

AppLaunchInfo::AppLaunchInfo(const std::string& app_id,
                             apps::mojom::LaunchContainer container,
                             WindowOpenDisposition disposition,
                             int64_t display_id,
                             std::vector<base::FilePath> launch_files,
                             apps::mojom::IntentPtr intent)
    : app_id(app_id),
      container(static_cast<int32_t>(container)),
      disposition(static_cast<int32_t>(disposition)),
      display_id(display_id),
      file_paths(std::move(launch_files)),
      intent(std::move(intent)) {}

AppLaunchInfo::AppLaunchInfo(const std::string& app_id,
                             int32_t event_flags,
                             int32_t arc_session_id,
                             int64_t display_id)
    : app_id(app_id),
      event_flag(event_flags),
      arc_session_id(arc_session_id),
      display_id(display_id) {}

AppLaunchInfo::AppLaunchInfo(const std::string& app_id,
                             int32_t event_flags,
                             apps::mojom::IntentPtr intent,
                             int32_t arc_session_id,
                             int64_t display_id)
    : app_id(app_id),
      event_flag(event_flags),
      arc_session_id(arc_session_id),
      display_id(display_id),
      intent(std::move(intent)) {}

AppLaunchInfo::~AppLaunchInfo() = default;

}  // namespace full_restore
