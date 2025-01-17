// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/account_manager/access_token_fetcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account_manager_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace crosapi {

AccessTokenFetcher::AccessTokenFetcher(
    ash::AccountManager* account_manager,
    mojom::AccountKeyPtr mojo_account_key,
    base::OnceCallback<void(AccessTokenFetcher*)> done_callback,
    mojo::PendingReceiver<mojom::AccessTokenFetcher> receiver)
    : done_callback_(std::move(done_callback)),
      receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &AccessTokenFetcher::OnMojoPipeError, base::Unretained(this)));

  base::Optional<account_manager::AccountKey> maybe_account_key =
      account_manager::FromMojoAccountKey(mojo_account_key);
  if (maybe_account_key.has_value() && maybe_account_key.value().IsValid()) {
    access_token_fetcher_ = account_manager->CreateAccessTokenFetcher(
        /*account_key=*/maybe_account_key.value(), /*consumer=*/this);
  }
  // else: access_token_fetcher_ will be `nullptr`. `Start` will handle this
  // case.
}

AccessTokenFetcher::~AccessTokenFetcher() = default;

void AccessTokenFetcher::Start(const std::vector<std::string>& scopes,
                               StartCallback callback) {
  DCHECK(callback_.is_null());
  DCHECK(callback);
  callback_ = std::move(callback);

  if (!access_token_fetcher_) {
    // `access_token_fetcher_` can be null only if `account_key` is invalid /
    // unknown.
    OnGetTokenFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::State::USER_NOT_SIGNED_UP));
    return;
  }

  access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(), scopes);
}

void AccessTokenFetcher::OnGetTokenSuccess(
    const TokenResponse& token_response) {
  mojom::AccessTokenResultPtr result =
      mojom::AccessTokenResult::NewAccessTokenInfo(mojom::AccessTokenInfo::New(
          token_response.access_token, token_response.expiration_time,
          token_response.id_token));
  std::move(callback_).Run(std::move(result));
  Finish();
}

void AccessTokenFetcher::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  mojom::AccessTokenResultPtr result = mojom::AccessTokenResult::NewError(
      account_manager::ToMojoGoogleServiceAuthError(error));
  std::move(callback_).Run(std::move(result));
  Finish();
}

void AccessTokenFetcher::OnMojoPipeError() {
  if (access_token_fetcher_)
    access_token_fetcher_->CancelRequest();

  if (callback_) {
    // We don't need to respond to callback. The Mojo pipe has been
    // disconnected.
    callback_.Reset();
  }

  Finish();
}

void AccessTokenFetcher::Finish() {
  DCHECK(callback_.is_null())
      << "Finish called before responding to pending request";

  // We cannot call `TriggerDeletion` directly because it will immediately start
  // deleting `this`, before this method has had a chance to return.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(done_callback_), this));
}

}  // namespace crosapi
