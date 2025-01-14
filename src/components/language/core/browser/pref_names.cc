// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/pref_names.h"

#include "build/chromeos_buildflags.h"

namespace language {
namespace prefs {

// The value to use for Accept-Languages HTTP header when making an HTTP
// request. This should not be set directly as it is a combination of
// kSelectedLanguages and kForcedLanguages. To update the list of preferred
// languages, set kSelectedLanguages and this pref will update automatically.
const char kAcceptLanguages[] = "intl.accept_languages";

// List which contains the user-selected languages.
const char kSelectedLanguages[] = "intl.selected_languages";

// List which contains the policy-forced languages.
const char kForcedLanguages[] = "intl.forced_languages";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A string pref (comma-separated list) set to the preferred language IDs
// (ex. "en-US,fr,ko").
const char kPreferredLanguages[] = "settings.language.preferred_languages";
const char kPreferredLanguagesSyncable[] =
    "settings.language.preferred_languages_syncable";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// The JSON representation of the user's language profile. Used as an input to
// the user language model (i.e. for determining which languages a user
// understands).
const char kUserLanguageProfile[] = "language_profile";

// Important: Refer to header file for how to use this.
const char kApplicationLocale[] = "intl.app_locale";

// Originally translate blocked languages from TranslatePrefs.
const char kFluentLanguages[] = "translate_blocked_languages";

}  // namespace prefs
}  // namespace language
