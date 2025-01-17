// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_util.h"

#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_result.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace account_manager {

namespace cm = crosapi::mojom;

namespace {

GoogleServiceAuthError::InvalidGaiaCredentialsReason
FromMojoInvalidGaiaCredentialsReason(
    crosapi::mojom::GoogleServiceAuthError::InvalidGaiaCredentialsReason
        mojo_reason) {
  switch (mojo_reason) {
    case cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::kUnknown:
      return GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN;
    case cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::
        kCredentialsRejectedByServer:
      return GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          CREDENTIALS_REJECTED_BY_SERVER;
    case cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::
        kCredentialsRejectedByClient:
      return GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          CREDENTIALS_REJECTED_BY_CLIENT;
    case cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::
        kCredentialsMissing:
      return GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          CREDENTIALS_MISSING;
    default:
      LOG(WARNING) << "Unknown "
                      "crosapi::mojom::GoogleServiceAuthError::"
                      "InvalidGaiaCredentialsReason: "
                   << mojo_reason;
      return GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN;
  }
}

crosapi::mojom::GoogleServiceAuthError::InvalidGaiaCredentialsReason
ToMojoInvalidGaiaCredentialsReason(
    GoogleServiceAuthError::InvalidGaiaCredentialsReason reason) {
  switch (reason) {
    case GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN:
      return cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::kUnknown;
    case GoogleServiceAuthError::InvalidGaiaCredentialsReason::
        CREDENTIALS_REJECTED_BY_SERVER:
      return cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          kCredentialsRejectedByServer;
    case GoogleServiceAuthError::InvalidGaiaCredentialsReason::
        CREDENTIALS_REJECTED_BY_CLIENT:
      return cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          kCredentialsRejectedByClient;
    case GoogleServiceAuthError::InvalidGaiaCredentialsReason::
        CREDENTIALS_MISSING:
      return cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          kCredentialsMissing;
    case GoogleServiceAuthError::InvalidGaiaCredentialsReason::NUM_REASONS:
      NOTREACHED();
      return cm::GoogleServiceAuthError::InvalidGaiaCredentialsReason::kUnknown;
  }
}

crosapi::mojom::GoogleServiceAuthError::State ToMojoGoogleServiceAuthErrorState(
    GoogleServiceAuthError::State state) {
  switch (state) {
    case GoogleServiceAuthError::State::NONE:
      return cm::GoogleServiceAuthError::State::kNone;
    case GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS:
      return cm::GoogleServiceAuthError::State::kInvalidGaiaCredentials;
    case GoogleServiceAuthError::State::USER_NOT_SIGNED_UP:
      return cm::GoogleServiceAuthError::State::kUserNotSignedUp;
    case GoogleServiceAuthError::State::CONNECTION_FAILED:
      return cm::GoogleServiceAuthError::State::kConnectionFailed;
    case GoogleServiceAuthError::State::SERVICE_UNAVAILABLE:
      return cm::GoogleServiceAuthError::State::kServiceUnavailable;
    case GoogleServiceAuthError::State::REQUEST_CANCELED:
      return cm::GoogleServiceAuthError::State::kRequestCanceled;
    case GoogleServiceAuthError::State::UNEXPECTED_SERVICE_RESPONSE:
      return cm::GoogleServiceAuthError::State::kUnexpectedServiceResponse;
    case GoogleServiceAuthError::State::SERVICE_ERROR:
      return cm::GoogleServiceAuthError::State::kServiceError;
    case GoogleServiceAuthError::State::NUM_STATES:
      NOTREACHED();
      return cm::GoogleServiceAuthError::State::kNone;
  }
}

base::Optional<account_manager::AccountAdditionResult::Status>
FromMojoAccountAdditionStatus(
    crosapi::mojom::AccountAdditionResult::Status mojo_status) {
  switch (mojo_status) {
    case cm::AccountAdditionResult::Status::kSuccess:
      return account_manager::AccountAdditionResult::Status::kSuccess;
    case cm::AccountAdditionResult::Status::kAlreadyInProgress:
      return account_manager::AccountAdditionResult::Status::kAlreadyInProgress;
    case cm::AccountAdditionResult::Status::kCancelledByUser:
      return account_manager::AccountAdditionResult::Status::kCancelledByUser;
    case cm::AccountAdditionResult::Status::kNetworkError:
      return account_manager::AccountAdditionResult::Status::kNetworkError;
    case cm::AccountAdditionResult::Status::kUnexpectedResponse:
      return account_manager::AccountAdditionResult::Status::
          kUnexpectedResponse;
    default:
      LOG(WARNING) << "Unknown crosapi::mojom::AccountAdditionResult::Status: "
                   << mojo_status;
      return base::nullopt;
  }
}

crosapi::mojom::AccountAdditionResult::Status ToMojoAccountAdditionStatus(
    account_manager::AccountAdditionResult::Status status) {
  switch (status) {
    case account_manager::AccountAdditionResult::Status::kSuccess:
      return cm::AccountAdditionResult::Status::kSuccess;
    case account_manager::AccountAdditionResult::Status::kAlreadyInProgress:
      return cm::AccountAdditionResult::Status::kAlreadyInProgress;
    case account_manager::AccountAdditionResult::Status::kCancelledByUser:
      return cm::AccountAdditionResult::Status::kCancelledByUser;
    case account_manager::AccountAdditionResult::Status::kNetworkError:
      return cm::AccountAdditionResult::Status::kNetworkError;
    case account_manager::AccountAdditionResult::Status::kUnexpectedResponse:
      return cm::AccountAdditionResult::Status::kUnexpectedResponse;
  }
}

}  // namespace

base::Optional<account_manager::Account> FromMojoAccount(
    const crosapi::mojom::AccountPtr& mojom_account) {
  const base::Optional<account_manager::AccountKey> account_key =
      FromMojoAccountKey(mojom_account->key);
  if (!account_key.has_value())
    return base::nullopt;

  account_manager::Account account;
  account.key = account_key.value();
  account.raw_email = mojom_account->raw_email;
  return account;
}

crosapi::mojom::AccountPtr ToMojoAccount(
    const account_manager::Account& account) {
  crosapi::mojom::AccountPtr mojom_account = crosapi::mojom::Account::New();
  mojom_account->key = ToMojoAccountKey(account.key);
  mojom_account->raw_email = account.raw_email;
  return mojom_account;
}

base::Optional<account_manager::AccountKey> FromMojoAccountKey(
    const crosapi::mojom::AccountKeyPtr& mojom_account_key) {
  const base::Optional<account_manager::AccountType> account_type =
      FromMojoAccountType(mojom_account_key->account_type);
  if (!account_type.has_value())
    return base::nullopt;

  account_manager::AccountKey account_key;
  account_key.id = mojom_account_key->id;
  account_key.account_type = account_type.value();
  return account_key;
}

crosapi::mojom::AccountKeyPtr ToMojoAccountKey(
    const account_manager::AccountKey& account_key) {
  crosapi::mojom::AccountKeyPtr mojom_account_key =
      crosapi::mojom::AccountKey::New();
  mojom_account_key->id = account_key.id;
  mojom_account_key->account_type = ToMojoAccountType(account_key.account_type);
  return mojom_account_key;
}

base::Optional<account_manager::AccountType> FromMojoAccountType(
    const crosapi::mojom::AccountType& account_type) {
  switch (account_type) {
    case crosapi::mojom::AccountType::kGaia:
      static_assert(static_cast<int>(crosapi::mojom::AccountType::kGaia) ==
                        static_cast<int>(account_manager::AccountType::kGaia),
                    "Underlying enum values must match");
      return account_manager::AccountType::kGaia;
    case crosapi::mojom::AccountType::kActiveDirectory:
      static_assert(
          static_cast<int>(crosapi::mojom::AccountType::kActiveDirectory) ==
              static_cast<int>(account_manager::AccountType::kActiveDirectory),
          "Underlying enum values must match");
      return account_manager::AccountType::kActiveDirectory;
    default:
      // Don't consider this as as error to preserve forwards compatibility with
      // lacros.
      LOG(WARNING) << "Unknown account type: " << account_type;
      return base::nullopt;
  }
}

crosapi::mojom::AccountType ToMojoAccountType(
    const account_manager::AccountType& account_type) {
  switch (account_type) {
    case account_manager::AccountType::kGaia:
      return crosapi::mojom::AccountType::kGaia;
    case account_manager::AccountType::kActiveDirectory:
      return crosapi::mojom::AccountType::kActiveDirectory;
  }
}

base::Optional<GoogleServiceAuthError> FromMojoGoogleServiceAuthError(
    const crosapi::mojom::GoogleServiceAuthErrorPtr& mojo_error) {
  switch (mojo_error->state) {
    case cm::GoogleServiceAuthError::State::kNone:
      return GoogleServiceAuthError::AuthErrorNone();
    case cm::GoogleServiceAuthError::State::kInvalidGaiaCredentials:
      return GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          FromMojoInvalidGaiaCredentialsReason(
              mojo_error->invalid_gaia_credentials_reason));
    case cm::GoogleServiceAuthError::State::kConnectionFailed:
      return GoogleServiceAuthError::FromConnectionError(
          mojo_error->network_error);
    case cm::GoogleServiceAuthError::State::kServiceError:
      return GoogleServiceAuthError::FromServiceError(
          mojo_error->error_message);
    case cm::GoogleServiceAuthError::State::kUnexpectedServiceResponse:
      return GoogleServiceAuthError::FromUnexpectedServiceResponse(
          mojo_error->error_message);
    case cm::GoogleServiceAuthError::State::kUserNotSignedUp:
      return GoogleServiceAuthError(
          GoogleServiceAuthError::State::USER_NOT_SIGNED_UP);
    case cm::GoogleServiceAuthError::State::kServiceUnavailable:
      return GoogleServiceAuthError(
          GoogleServiceAuthError::State::SERVICE_UNAVAILABLE);
    case cm::GoogleServiceAuthError::State::kRequestCanceled:
      return GoogleServiceAuthError(
          GoogleServiceAuthError::State::REQUEST_CANCELED);
    default:
      LOG(WARNING) << "Unknown crosapi::mojom::GoogleServiceAuthError::State: "
                   << mojo_error->state;
      return base::nullopt;
  }
}

crosapi::mojom::GoogleServiceAuthErrorPtr ToMojoGoogleServiceAuthError(
    GoogleServiceAuthError error) {
  crosapi::mojom::GoogleServiceAuthErrorPtr mojo_result =
      crosapi::mojom::GoogleServiceAuthError::New();
  mojo_result->error_message = error.error_message();
  if (error.state() == GoogleServiceAuthError::State::CONNECTION_FAILED) {
    mojo_result->network_error = error.network_error();
  }
  if (error.state() ==
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS) {
    mojo_result->invalid_gaia_credentials_reason =
        ToMojoInvalidGaiaCredentialsReason(
            error.GetInvalidGaiaCredentialsReason());
  }
  mojo_result->state = ToMojoGoogleServiceAuthErrorState(error.state());
  return mojo_result;
}

base::Optional<account_manager::AccountAdditionResult>
FromMojoAccountAdditionResult(
    const crosapi::mojom::AccountAdditionResultPtr& mojo_result) {
  base::Optional<account_manager::AccountAdditionResult::Status> status =
      FromMojoAccountAdditionStatus(mojo_result->status);
  if (!status.has_value()) {
    return base::nullopt;
  }
  account_manager::AccountAdditionResult result(status.value());
  result.status = status.value();
  if (mojo_result->account) {
    result.account = FromMojoAccount(mojo_result->account);
  }
  if (mojo_result->error) {
    result.error = FromMojoGoogleServiceAuthError(mojo_result->error);
  }
  return result;
}

crosapi::mojom::AccountAdditionResultPtr ToMojoAccountAdditionResult(
    account_manager::AccountAdditionResult result) {
  crosapi::mojom::AccountAdditionResultPtr mojo_result =
      crosapi::mojom::AccountAdditionResult::New();
  mojo_result->status = ToMojoAccountAdditionStatus(result.status);
  if (result.account.has_value()) {
    mojo_result->account =
        account_manager::ToMojoAccount(result.account.value());
  }
  if (result.error.has_value()) {
    mojo_result->error = ToMojoGoogleServiceAuthError(result.error.value());
  }
  return mojo_result;
}

}  // namespace account_manager
