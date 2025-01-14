// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/request_type.h"

#include "base/check.h"
#include "base/notreached.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permissions_client.h"

#if defined(OS_ANDROID)
#include "components/resources/android/theme_resources.h"
#else
#include "components/permissions/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#endif  // defined(OS_ANDROID)

namespace permissions {

namespace {

#if defined(OS_ANDROID)
int GetIconIdAndroid(RequestType type) {
  switch (type) {
    case RequestType::kAccessibilityEvents:
      return IDR_ANDROID_INFOBAR_ACCESSIBILITY_EVENTS;
    case RequestType::kArSession:
    case RequestType::kVrSession:
      return IDR_ANDROID_INFOBAR_VR_HEADSET;
    case RequestType::kCameraStream:
      return IDR_ANDROID_INFOBAR_MEDIA_STREAM_CAMERA;
    case RequestType::kClipboard:
      return IDR_ANDROID_INFOBAR_CLIPBOARD;
    case RequestType::kDiskQuota:
      return IDR_ANDROID_INFOBAR_FOLDER;
    case RequestType::kGeolocation:
      return IDR_ANDROID_INFOBAR_GEOLOCATION;
    case RequestType::kIdleDetection:
      return IDR_ANDROID_INFOBAR_IDLE_DETECTION;
    case RequestType::kMicStream:
      return IDR_ANDROID_INFOBAR_MEDIA_STREAM_MIC;
    case RequestType::kMidiSysex:
      return IDR_ANDROID_INFOBAR_MIDI;
    case RequestType::kMultipleDownloads:
      return IDR_ANDROID_INFOBAR_MULTIPLE_DOWNLOADS;
    case RequestType::kNfcDevice:
      return IDR_ANDROID_INFOBAR_NFC;
    case RequestType::kNotifications:
      return IDR_ANDROID_INFOBAR_NOTIFICATIONS;
    case RequestType::kProtectedMediaIdentifier:
      return IDR_ANDROID_INFOBAR_PROTECTED_MEDIA_IDENTIFIER;
    case RequestType::kStorageAccess:
      return IDR_ANDROID_INFOBAR_PERMISSION_COOKIE;
  }
  NOTREACHED();
  return 0;
}
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
const gfx::VectorIcon& GetIconIdDesktop(RequestType type) {
  switch (type) {
    case RequestType::kAccessibilityEvents:
      return kAccessibilityIcon;
    case RequestType::kArSession:
    case RequestType::kVrSession:
      return vector_icons::kVrHeadsetIcon;
    case RequestType::kCameraPanTiltZoom:
    case RequestType::kCameraStream:
      return vector_icons::kVideocamIcon;
    case RequestType::kClipboard:
      return vector_icons::kContentPasteIcon;
    case RequestType::kDiskQuota:
      return vector_icons::kFolderIcon;
    case RequestType::kFileHandling:
      return vector_icons::kDescriptionIcon;
    case RequestType::kFontAccess:
      return vector_icons::kFontDownloadIcon;
    case RequestType::kGeolocation:
      return vector_icons::kLocationOnIcon;
    case RequestType::kIdleDetection:
      return vector_icons::kDevicesIcon;
    case RequestType::kMicStream:
      return vector_icons::kMicIcon;
    case RequestType::kMidiSysex:
      return vector_icons::kMidiIcon;
    case RequestType::kMultipleDownloads:
      return vector_icons::kFileDownloadIcon;
    case RequestType::kNotifications:
      return vector_icons::kNotificationsIcon;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case RequestType::kProtectedMediaIdentifier:
      // This icon is provided by ChromePermissionsClient::GetOverrideIconId.
      NOTREACHED();
      return gfx::kNoneIcon;
#endif
    case RequestType::kRegisterProtocolHandler:
      return vector_icons::kProtocolHandlerIcon;
    case RequestType::kSecurityAttestation:
      return kUsbSecurityKeyIcon;
    case RequestType::kStorageAccess:
      return vector_icons::kCookieIcon;
    case RequestType::kWindowPlacement:
      return vector_icons::kSelectWindowIcon;
  }
  NOTREACHED();
  return gfx::kNoneIcon;
}
#endif  // !defined(OS_ANDROID)

}  // namespace

RequestType ContentSettingsTypeToRequestType(
    ContentSettingsType content_settings_type) {
  switch (content_settings_type) {
    case ContentSettingsType::ACCESSIBILITY_EVENTS:
      return RequestType::kAccessibilityEvents;
    case ContentSettingsType::AR:
      return RequestType::kArSession;
#if !defined(OS_ANDROID)
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      return RequestType::kCameraPanTiltZoom;
#endif
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return RequestType::kCameraStream;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      return RequestType::kClipboard;
#if !defined(OS_ANDROID)
    case ContentSettingsType::FILE_HANDLING:
      return RequestType::kFileHandling;
    case ContentSettingsType::FONT_ACCESS:
      return RequestType::kFontAccess;
#endif
    case ContentSettingsType::GEOLOCATION:
      return RequestType::kGeolocation;
    case ContentSettingsType::IDLE_DETECTION:
      return RequestType::kIdleDetection;
    case ContentSettingsType::MEDIASTREAM_MIC:
      return RequestType::kMicStream;
    case ContentSettingsType::MIDI_SYSEX:
      return RequestType::kMidiSysex;
    case ContentSettingsType::NOTIFICATIONS:
      return RequestType::kNotifications;
#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      return RequestType::kProtectedMediaIdentifier;
#endif
#if defined(OS_ANDROID)
    case ContentSettingsType::NFC:
      return RequestType::kNfcDevice;
#endif
    case ContentSettingsType::STORAGE_ACCESS:
      return RequestType::kStorageAccess;
    case ContentSettingsType::VR:
      return RequestType::kVrSession;
#if !defined(OS_ANDROID)
    case ContentSettingsType::WINDOW_PLACEMENT:
      return RequestType::kWindowPlacement;
#endif
    default:
      CHECK(false);
      return RequestType::kGeolocation;
  }
}

IconId GetIconId(RequestType type) {
  IconId override_id = PermissionsClient::Get()->GetOverrideIconId(type);
#if defined(OS_ANDROID)
  if (override_id)
    return override_id;
  return GetIconIdAndroid(type);
#else
  if (!override_id.is_empty())
    return override_id;
  return GetIconIdDesktop(type);
#endif
}

const char* PermissionKeyForRequestType(permissions::RequestType request_type) {
  switch (request_type) {
    case permissions::RequestType::kAccessibilityEvents:
      return "accessibility_events";
    case permissions::RequestType::kArSession:
      return "ar_session";
#if !defined(OS_ANDROID)
    case permissions::RequestType::kCameraPanTiltZoom:
      return "camera_pan_tilt_zoom";
#endif
    case permissions::RequestType::kCameraStream:
      return "camera_stream";
    case permissions::RequestType::kClipboard:
      return "clipboard";
    case permissions::RequestType::kDiskQuota:
      return "disk_quota";
#if !defined(OS_ANDROID)
    case permissions::RequestType::kFileHandling:
      return "file_handling";
    case permissions::RequestType::kFontAccess:
      return "font_access";
#endif
    case permissions::RequestType::kGeolocation:
      return "geolocation";
    case permissions::RequestType::kIdleDetection:
      return "idle_detection";
    case permissions::RequestType::kMicStream:
      return "mic_stream";
    case permissions::RequestType::kMidiSysex:
      return "midi_sysex";
    case permissions::RequestType::kMultipleDownloads:
      return "multiple_downloads";
#if defined(OS_ANDROID)
    case permissions::RequestType::kNfcDevice:
      return "nfc_device";
#endif
    case permissions::RequestType::kNotifications:
      return "notifications";
#if defined(OS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
    case permissions::RequestType::kProtectedMediaIdentifier:
      return "protected_media_identifier";
#endif
#if !defined(OS_ANDROID)
    case permissions::RequestType::kRegisterProtocolHandler:
      return "register_protocol_handler";
    case permissions::RequestType::kSecurityAttestation:
      return "security_attestation";
#endif
    case permissions::RequestType::kStorageAccess:
      return "storage_access";
    case permissions::RequestType::kVrSession:
      return "vr_session";
#if !defined(OS_ANDROID)
    case permissions::RequestType::kWindowPlacement:
      return "window_placement";
#endif
  }

  return nullptr;
}

}  // namespace permissions
