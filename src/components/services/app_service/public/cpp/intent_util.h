// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_UTIL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_UTIL_H_

// Utility functions for App Service intent handling.

#include <string>

#include "base/macros.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "url/gurl.h"

namespace apps_util {

extern const char kIntentActionMain[];
extern const char kIntentActionView[];
extern const char kIntentActionSend[];
extern const char kIntentActionSendMultiple[];

// Create an intent struct from URL.
apps::mojom::IntentPtr CreateIntentFromUrl(const GURL& url);

// Create an intent struct from the filesystem urls and mime types
// of a list of files.
apps::mojom::IntentPtr CreateShareIntentFromFiles(
    const std::vector<GURL>& filesystem_urls,
    const std::vector<std::string>& mime_types);

// Create an intent struct from the filesystem urls, mime types
// of a list of files, and the share text and title.
apps::mojom::IntentPtr CreateShareIntentFromFiles(
    const std::vector<GURL>& filesystem_urls,
    const std::vector<std::string>& mime_types,
    const std::string& share_text,
    const std::string& share_title);

// Create an intent struct from the filesystem url, mime type
// and the drive share url for a Google Drive file.
apps::mojom::IntentPtr CreateShareIntentFromDriveFile(
    const GURL& filesystem_url,
    const std::string& mime_type,
    const GURL& drive_share_url,
    bool is_directory);

// Create an intent struct from share text and title.
apps::mojom::IntentPtr CreateShareIntentFromText(
    const std::string& share_text,
    const std::string& share_title);

// Create an intent struct from activity and start type.
apps::mojom::IntentPtr CreateIntentForActivity(const std::string& activity,
                                               const std::string& start_type,
                                               const std::string& category);

// Return true if |value| matches with the |condition_value|, based on the
// pattern match type in the |condition_value|.
bool ConditionValueMatches(
    const std::string& value,
    const apps::mojom::ConditionValuePtr& condition_value);

// Return true if |intent| matches with any of the values in |condition|.
bool IntentMatchesCondition(const apps::mojom::IntentPtr& intent,
                            const apps::mojom::ConditionPtr& condition);

// Return true if a |filter| matches an |intent|. This is true when intent
// matches all existing conditions in the filter.
bool IntentMatchesFilter(const apps::mojom::IntentPtr& intent,
                         const apps::mojom::IntentFilterPtr& filter);

// Return true if |value| matches |pattern| with simple glob syntax.
// In this syntax, you can use the '*' character to match against zero or
// more occurrences of the character immediately before. If the character
// before it is '.' it will match any character. The character '\' can be
// used as an escape. This essentially provides only the '*' wildcard part
// of a normal regexp.
// This function is transcribed from android's PatternMatcher#matchPattern.
// See
// https://android.googlesource.com/platform/frameworks/base.git/+/e93165456c3c28278f275566bd90bfbcf1a0e5f7/core/java/android/os/PatternMatcher.java#186
bool MatchGlob(const std::string& value, const std::string& pattern);

// Check if the intent only mean to share to Google Drive.
bool OnlyShareToDrive(const apps::mojom::IntentPtr& intent);

// Check the if the intent is valid, e.g. action matches content.
bool IsIntentValid(const apps::mojom::IntentPtr& intent);

// Converts |intent| to base::Value, e.g.:
// {
//    "action": "xx",
//    "url": "abc.com",
//    "mime_type": "text/plain",
//    "file_urls": "/abc, /a",
//    "activity_name": "yy",
//    "drive_share_url": "aa.com",
//    "share_text": "text",
//    "share_title": "title",
// }
base::Value ConvertIntentToValue(const apps::mojom::IntentPtr& intent);

// Gets the string value from base::DictionaryValue, e.g. { "key": "value" }
// returns "value".
base::Optional<std::string> GetStringValueFromDict(
    const base::DictionaryValue& dict,
    const std::string& key_name);

// Gets the apps::mojom::OptionalBool value from base::DictionaryValue, e.g. {
// "key": "value" } returns "value".
apps::mojom::OptionalBool GetBoolValueFromDict(
    const base::DictionaryValue& dict,
    const std::string& key_name);

// Gets GURL from base::DictionaryValue, e.g. { "url": "abc.com" } returns
// "abc.com".
base::Optional<GURL> GetGurlValueFromDict(const base::DictionaryValue& dict,
                                          const std::string& key_name);

// Gets std::vector<::GURL> from base::DictionaryValue, e.g. { "file_urls":
// "/abc, /a" } returns std::vector<::GURL>{"/abc, /a"}.
base::Optional<std::vector<::GURL>> GetFileUrlsFromDict(
    const base::DictionaryValue& dict,
    const std::string& key_name);

// Converts base::Value to Intent.
apps::mojom::IntentPtr ConvertValueToIntent(base::Value&& value);

}  // namespace apps_util

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_UTIL_H_
