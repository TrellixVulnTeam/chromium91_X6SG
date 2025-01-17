// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace base {
class Value;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

namespace language {

extern const char kFallbackInputMethodLocale[];

class LanguagePrefs {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit LanguagePrefs(PrefService* user_prefs);
  ~LanguagePrefs();

  // Return true iff the user is fluent in the given |language|.
  bool IsFluent(base::StringPiece language) const;
  // Mark that the user is fluent in the given |language|.
  void SetFluent(base::StringPiece language);
  // Remove the given |language| from the user's fluent languages.
  void ClearFluent(base::StringPiece language);
  // Reset the fluent languages to their defaults.
  void ResetFluentLanguagesToDefaults();
  // Get the default fluent languages for the user.
  static base::Value GetDefaultFluentLanguages();
  // Get the current list of fluent languages for the user formatted as Chrome
  // language codes.
  std::vector<std::string> GetFluentLanguages() const;
  // If the list of fluent languages is empty, reset it to defaults.
  void ResetEmptyFluentLanguagesToDefault();
  // Gets the language settings list containing combination of policy-forced and
  // user-selected languages. Language settings list follows the Chrome internal
  // format.
  void GetAcceptLanguagesList(std::vector<std::string>* languages) const;
  // Gets the user-selected language settings list. Languages are expected to be
  // in the Chrome internal format.
  void GetUserSelectedLanguagesList(std::vector<std::string>* languages) const;
  // Updates the user-selected language settings list. Languages are expected to
  // be in the Chrome internal format.
  void SetUserSelectedLanguagesList(const std::vector<std::string>& languages);
  // Returns true if the target language is forced through policy.
  bool IsForcedLanguage(const std::string& language);

 private:
  // Updates the language list containing combination of policy-forced and
  // user-selected languages.
  void GetDeduplicatedUserLanguages(std::string* deduplicated_languages_string);
  // Updates the pref corresponding to the language list containing combination
  // of policy-forced and user-selected languages.
  // Since languages may be removed from the policy while the browser is off,
  // having an additional policy solely for user-selected languages allows
  // Chrome to clear any removed policy languages from the accept languages pref
  // while retaining all user-selected languages.
  void UpdateAcceptLanguagesPref();
  // Initializes the user selected language pref to ensure backwards
  // compatibility.
  void InitializeSelectedLanguagesPref();

  size_t NumFluentLanguages() const;

  // Used for deduplication and reordering of languages.
  std::set<std::string> forced_languages_set_;

  PrefService* prefs_;  // Weak.
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(LanguagePrefs);
};

void ResetLanguagePrefs(PrefService* prefs);

// Given a comma separated list of locales, return the first.
std::string GetFirstLanguage(base::StringPiece language_list);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_
