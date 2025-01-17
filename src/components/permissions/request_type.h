// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_REQUEST_TYPE_H_
#define COMPONENTS_PERMISSIONS_REQUEST_TYPE_H_

#include "base/optional.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

enum class ContentSettingsType;

namespace gfx {
struct VectorIcon;
}

namespace permissions {

// The type of the request that will be seen by the user. Values are only
// defined on the platforms where they are used and should be kept alphabetized.
enum class RequestType {
  kAccessibilityEvents,
  kArSession,
#if !defined(OS_ANDROID)
  kCameraPanTiltZoom,
#endif
  kCameraStream,
  kClipboard,
  kDiskQuota,
#if !defined(OS_ANDROID)
  kFileHandling,
  kFontAccess,
#endif
  kGeolocation,
  kIdleDetection,
  kMicStream,
  kMidiSysex,
  kMultipleDownloads,
#if defined(OS_ANDROID)
  kNfcDevice,
#endif
  kNotifications,
#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
  kProtectedMediaIdentifier,
#endif
#if !defined(OS_ANDROID)
  kRegisterProtocolHandler,
  kSecurityAttestation,
#endif
  kStorageAccess,
  kVrSession,
#if !defined(OS_ANDROID)
  kWindowPlacement,
#endif
};

#if defined(OS_ANDROID)
// On Android, icons are represented with an IDR_ identifier.
using IconId = int;
#else
// On desktop, we use a vector icon.
typedef const gfx::VectorIcon& IconId;
#endif

RequestType ContentSettingsTypeToRequestType(
    ContentSettingsType content_settings_type);

// Returns the icon to display.
IconId GetIconId(RequestType type);

// Returns a unique human-readable string that can be used in dictionaries that
// are keyed by the RequestType.
const char* PermissionKeyForRequestType(permissions::RequestType request_type);

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_REQUEST_TYPE_H_
