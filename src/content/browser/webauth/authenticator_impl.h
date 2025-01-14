// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_service_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace base {
class OneShotTimer;
}

namespace device {

struct PlatformAuthenticatorInfo;
struct CtapGetAssertionRequest;
class FidoRequestHandlerBase;

enum class FidoReturnCode : uint8_t;

}  // namespace device

namespace url {
class Origin;
}

namespace content {

class AuthenticatorCommon;
class RenderFrameHost;

// Implementation of the public Authenticator interface.
class CONTENT_EXPORT AuthenticatorImpl
    : public FrameServiceBase<blink::mojom::Authenticator> {
 public:
  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::Authenticator> receiver);

 private:
  friend class AuthenticatorImplTest;
  friend class AuthenticatorImplRequestDelegateTest;

  AuthenticatorImpl(RenderFrameHost* render_frame_host,
                    mojo::PendingReceiver<blink::mojom::Authenticator> receiver,
                    std::unique_ptr<AuthenticatorCommon> authenticator_common);
  ~AuthenticatorImpl() override;

  AuthenticatorCommon* get_authenticator_common_for_testing() {
    return authenticator_common_.get();
  }

  // mojom:Authenticator
  void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      MakeCredentialCallback callback) override;
  void GetAssertion(blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
                    GetAssertionCallback callback) override;
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) override;
  void Cancel() override;

  std::unique_ptr<AuthenticatorCommon> authenticator_common_;

  // Owns pipes to this Authenticator from |render_frame_host_|.
  mojo::Receiver<blink::mojom::Authenticator> receiver_{this};

  base::WeakPtrFactory<AuthenticatorImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_
