// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_result.h"

class OAuth2AccessTokenFetcher;
class OAuth2AccessTokenConsumer;

namespace account_manager {

// An interface to talk to |AccountManager|.
// Implementations of this interface hide the in-process / out-of-process nature
// of this communication.
// Instances of this class are singletons, and are independent of a |Profile|.
// Use |GetAccountManagerFacade()| to get an instance of this class.
class COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE) AccountManagerFacade {
 public:
  // UMA histogram name.
  static const char kAccountAdditionSource[];

  // Observer interface to get notifications about changes in the account list.
  class Observer : public base::CheckedObserver {
   public:
    Observer();
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override;

    // Invoked when an account is added or updated.
    virtual void OnAccountUpserted(const Account& account) = 0;
    // Invoked when an account is removed.
    virtual void OnAccountRemoved(const Account& account) = 0;
  };

  // The source UI surface used for launching the account addition /
  // re-authentication dialog. This should be as specific as possible.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Note: Please update |AccountManagerAccountAdditionSource| in enums.xml
  // after adding new values.
  enum class AccountAdditionSource : int {
    // Settings > Add account button.
    kSettingsAddAccountButton = 0,
    // Settings > Sign in again button.
    kSettingsReauthAccountButton = 1,
    // Launched from an ARC application.
    kArc = 2,
    // Launched automatically from Chrome content area. As of now, this is
    // possible only when an account requires re-authentication.
    kContentArea = 3,
    // Print Preview dialog.
    kPrintPreviewDialog = 4,
    // Account Manager migration welcome screen.
    kAccountManagerMigrationWelcomeScreen = 5,
    // Onboarding.
    kOnboarding = 6,

    kMaxValue = kOnboarding
  };

  AccountManagerFacade();
  AccountManagerFacade(const AccountManagerFacade&) = delete;
  AccountManagerFacade& operator=(const AccountManagerFacade&) = delete;
  virtual ~AccountManagerFacade() = 0;

  // Registers an observer. Ensures the observer wasn't already registered.
  virtual void AddObserver(Observer* observer) = 0;
  // Unregisters an observer that was registered using AddObserver.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Gets the list of accounts in Account Manager. If the remote side doesn't
  // support this call, an empty list of accounts will be returned.
  virtual void GetAccounts(
      base::OnceCallback<void(const std::vector<Account>&)> callback) = 0;

  // If `account` is in an error state (for example, if the refresh token is
  // known to be invalid), `callback` will get the corresponding
  // GoogleServiceAuthError.  If there's no known persistent error for
  // `account`, `callback` will receive `GoogleServiceAuthError` with `NONE`
  // state (Note: fetching an access token might still fail in this case).
  virtual void GetPersistentErrorForAccount(
      const AccountKey& account,
      base::OnceCallback<void(const GoogleServiceAuthError&)> callback) = 0;

  // Launches account addition dialog.
  virtual void ShowAddAccountDialog(const AccountAdditionSource& source) = 0;

  // Launches account addition dialog and calls the `callback` with the result.
  // If `result` is `kSuccess`, the added account will be passed to the
  // callback. Otherwise `account` will be set to `base::nullopt`.
  virtual void ShowAddAccountDialog(
      const AccountAdditionSource& source,
      base::OnceCallback<void(const AccountAdditionResult& result)>
          callback) = 0;

  // Launches account reauthentication dialog for provided `email`.
  virtual void ShowReauthAccountDialog(const AccountAdditionSource& source,
                                       const std::string& email) = 0;

  // Launches OS Settings > Accounts.
  virtual void ShowManageAccountsSettings() = 0;

  // Creates an access token fetcher for `account`.
  // Currently, `account` must be a Gaia account.
  // The returned object should not outlive `AccountManagerFacade` itself.
  virtual std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const AccountKey& account,
      const std::string& oauth_consumer_name,
      OAuth2AccessTokenConsumer* consumer) = 0;
};

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_FACADE_H_
