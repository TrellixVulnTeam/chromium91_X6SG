// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_CONSUMER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_CONSUMER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"

namespace password_manager {

// Callback interface for receiving a password reuse event.
class PasswordReuseDetectorConsumer
    : public base::SupportsWeakPtr<PasswordReuseDetectorConsumer> {
 public:
  PasswordReuseDetectorConsumer();
  virtual ~PasswordReuseDetectorConsumer();

  // Called when a password reuse check is finished. |reuse_found| indicates
  // whether a reuse was found. When a reuse is found, |password_length| is the
  // length of the re-used password, or the max length if multiple passwords
  // were matched. |reused_protected_password_hash| is the Gaia or enterprise
  // password that matches the reuse. |matching_reused_credentials| is the list
  // of MatchingReusedCredentials that contains the signon_realm which the
  // |password| is saved (may be empty if |reused_protected_password_hash| is
  // not null) on and the username, |saved_passwords| is the total number of
  // passwords (with unique domains) stored in Password Manager. When no reuse
  // is found, |password_length| is 0, |reused_protected_password_hash| is
  // nullopt, and |matching_reused_credentials| is empty.
  virtual void OnReuseCheckDone(
      bool is_reuse_found,
      size_t password_length,
      base::Optional<PasswordHashData> reused_protected_password_hash,
      const std::vector<MatchingReusedCredential>& matching_reused_credentials,
      int saved_passwords) = 0;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_CONSUMER_H_
