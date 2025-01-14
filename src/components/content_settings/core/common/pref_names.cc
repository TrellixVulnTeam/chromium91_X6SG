// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/pref_names.h"

namespace prefs {

// CookieControlsMode enum value that decides when the cookie controls UI is
// enabled. This will block third-party cookies similar to
// kBlockThirdPartyCookies but with a new UI.
const char kCookieControlsMode[] = "profile.cookie_controls_mode";

// Version of the pattern format used to define content settings.
const char kContentSettingsVersion[] = "profile.content_settings.pref_version";

// Integer that specifies the index of the tab the user was on when they
// last visited the content settings window.
const char kContentSettingsWindowLastTabIndex[] =
    "content_settings_window.last_tab_index";

// Preferences that are exclusively used to store managed values for default
// content settings.
const char kManagedDefaultAdsSetting[] =
    "profile.managed_default_content_settings.ads";
const char kManagedDefaultCookiesSetting[] =
    "profile.managed_default_content_settings.cookies";
const char kManagedDefaultGeolocationSetting[] =
    "profile.managed_default_content_settings.geolocation";
const char kManagedDefaultImagesSetting[] =
    "profile.managed_default_content_settings.images";
const char kManagedDefaultInsecureContentSetting[] =
    "profile.managed_default_content_settings.insecure_content";
const char kManagedDefaultJavaScriptSetting[] =
    "profile.managed_default_content_settings.javascript";
const char kManagedDefaultNotificationsSetting[] =
    "profile.managed_default_content_settings.notifications";
const char kManagedDefaultMediaStreamSetting[] =
    "profile.managed_default_content_settings.media_stream";
const char kManagedDefaultPopupsSetting[] =
    "profile.managed_default_content_settings.popups";
const char kManagedDefaultSensorsSetting[] =
    "profile.managed_default_content_settings.sensors";
const char kManagedDefaultWebBluetoothGuardSetting[] =
    "profile.managed_default_content_settings.web_bluetooth_guard";
const char kManagedDefaultWebUsbGuardSetting[] =
    "profile.managed_default_content_settings.web_usb_guard";
const char kManagedDefaultFileHandlingGuardSetting[] =
    "profile.managed_default_content_settings.file_handling_guard";
const char kManagedDefaultFileSystemReadGuardSetting[] =
    "profile.managed_default_content_settings.file_system_read_guard";
const char kManagedDefaultFileSystemWriteGuardSetting[] =
    "profile.managed_default_content_settings.file_system_write_guard";
const char kManagedDefaultLegacyCookieAccessSetting[] =
    "profile.managed_default_content_settings.legacy_cookie_access";
const char kManagedDefaultSerialGuardSetting[] =
    "profile.managed_default_content_settings.serial_guard";
const char kManagedDefaultInsecurePrivateNetworkSetting[] =
    "profile.managed_default_content_settings.insecure_private_network";

// Preferences that are exclusively used to store managed
// content settings patterns.
const char kManagedAutoSelectCertificateForUrls[] =
    "profile.managed_auto_select_certificate_for_urls";
const char kManagedCookiesAllowedForUrls[] =
    "profile.managed_cookies_allowed_for_urls";
const char kManagedCookiesBlockedForUrls[] =
    "profile.managed_cookies_blocked_for_urls";
const char kManagedCookiesSessionOnlyForUrls[] =
    "profile.managed_cookies_sessiononly_for_urls";
const char kManagedImagesAllowedForUrls[] =
    "profile.managed_images_allowed_for_urls";
const char kManagedImagesBlockedForUrls[] =
    "profile.managed_images_blocked_for_urls";
const char kManagedInsecureContentAllowedForUrls[] =
    "profile.managed_insecure_content_allowed_for_urls";
const char kManagedInsecureContentBlockedForUrls[] =
    "profile.managed_insecure_content_blocked_for_urls";
const char kManagedJavaScriptAllowedForUrls[] =
    "profile.managed_javascript_allowed_for_urls";
const char kManagedJavaScriptBlockedForUrls[] =
    "profile.managed_javascript_blocked_for_urls";
const char kManagedNotificationsAllowedForUrls[] =
    "profile.managed_notifications_allowed_for_urls";
const char kManagedNotificationsBlockedForUrls[] =
    "profile.managed_notifications_blocked_for_urls";
const char kManagedPopupsAllowedForUrls[] =
    "profile.managed_popups_allowed_for_urls";
const char kManagedPopupsBlockedForUrls[] =
    "profile.managed_popups_blocked_for_urls";
const char kManagedSensorsAllowedForUrls[] =
    "profile.managed_sensors_allowed_for_urls";
const char kManagedSensorsBlockedForUrls[] =
    "profile.managed_sensors_blocked_for_urls";
const char kManagedWebUsbAllowDevicesForUrls[] =
    "profile.managed_web_usb_allow_devices_for_urls";
const char kManagedWebUsbAskForUrls[] = "profile.managed_web_usb_ask_for_urls";
const char kManagedWebUsbBlockedForUrls[] =
    "profile.managed_web_usb_blocked_for_urls";
const char kManagedFileHandlingAllowedForUrls[] =
    "profile.managed_file_handling_allowed_for_urls";
const char kManagedFileHandlingBlockedForUrls[] =
    "profile.managed_file_handling_blocked_for_urls";
const char kManagedFileSystemReadAskForUrls[] =
    "profile.managed_file_system_read_ask_for_urls";
const char kManagedFileSystemReadBlockedForUrls[] =
    "profile.managed_file_system_read_blocked_for_urls";
const char kManagedFileSystemWriteAskForUrls[] =
    "profile.managed_file_system_write_ask_for_urls";
const char kManagedFileSystemWriteBlockedForUrls[] =
    "profile.managed_file_system_write_blocked_for_urls";
const char kManagedLegacyCookieAccessAllowedForDomains[] =
    "profile.managed_legacy_cookie_access_allowed_for_domains";
const char kManagedSerialAskForUrls[] = "profile.managed_serial_ask_for_urls";
const char kManagedSerialBlockedForUrls[] =
    "profile.managed_serial_blocked_for_urls";
const char kManagedInsecurePrivateNetworkAllowedForUrls[] =
    "profile.managed_insecure_private_network_allowed_for_urls";

// Boolean indicating whether the quiet UI is enabled for notification
// permission requests.
const char kEnableQuietNotificationPermissionUi[] =
    "profile.content_settings.enable_quiet_permission_ui.notifications";

// Enum indicating by which method the quiet UI has been enabled for
// notification permission requests. This is stored as of M88 and will be
// backfilled if the quiet UI is enabled but this preference has no value.
const char kQuietNotificationPermissionUiEnablingMethod[] =
    "profile.content_settings.enable_quiet_permission_ui_enabling_method."
    "notifications";

// Time value indicating when the quiet notification UI was last disabled by the
// user. Only permission action history after this point is taken into account
// for adaptive quiet UI activation.
const char kQuietNotificationPermissionUiDisabledTime[] =
    "profile.content_settings.disable_quiet_permission_ui_time.notifications";

#if defined(OS_ANDROID)
// Enable vibration for web notifications.
const char kNotificationsVibrateEnabled[] = "notifications.vibrate_enabled";
#endif

}  // namespace prefs
