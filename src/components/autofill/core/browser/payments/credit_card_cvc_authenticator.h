// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_CVC_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_CVC_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/full_card_request.h"

namespace autofill {

// Authenticates credit card unmasking through CVC verification.
class CreditCardCVCAuthenticator
    : public payments::FullCardRequest::ResultDelegate,
      public payments::FullCardRequest::UIDelegate {
 public:
  struct CVCAuthenticationResponse {
    CVCAuthenticationResponse();
    ~CVCAuthenticationResponse();

    CVCAuthenticationResponse& with_did_succeed(bool b) {
      did_succeed = b;
      return *this;
    }
    // Data pointed to by |c| must outlive this object.
    CVCAuthenticationResponse& with_card(const CreditCard* c) {
      card = c;
      return *this;
    }
    CVCAuthenticationResponse& with_cvc(const std::u16string s) {
      cvc = std::u16string(s);
      return *this;
    }
    CVCAuthenticationResponse& with_creation_options(
        base::Optional<base::Value> v) {
      creation_options = std::move(v);
      return *this;
    }
    CVCAuthenticationResponse& with_request_options(
        base::Optional<base::Value> v) {
      request_options = std::move(v);
      return *this;
    }
    CVCAuthenticationResponse& with_card_authorization_token(std::string s) {
      card_authorization_token = s;
      return *this;
    }
    bool did_succeed = false;
    const CreditCard* card = nullptr;
    std::u16string cvc = std::u16string();
    base::Optional<base::Value> creation_options = base::nullopt;
    base::Optional<base::Value> request_options = base::nullopt;
    std::string card_authorization_token = std::string();
  };
  class Requester {
   public:
    virtual ~Requester() = default;
    virtual void OnCVCAuthenticationComplete(
        const CVCAuthenticationResponse& response) = 0;

#if defined(OS_ANDROID)
    // Returns whether or not the user, while on the CVC prompt, should be
    // offered to switch to FIDO authentication for card unmasking. This will
    // always be false for Desktop since FIDO authentication is offered as a
    // separate prompt after the CVC prompt. On Android, however, this is
    // offered through a checkbox on the CVC prompt. This feature does not yet
    // exist on iOS.
    virtual bool ShouldOfferFidoAuth() const = 0;

    // This returns true only on Android when the user previously opted-in for
    // FIDO authentication through the settings page and this is the first card
    // downstream since. In this case, the opt-in checkbox is not shown and the
    // opt-in request is sent.
    virtual bool UserOptedInToFidoFromSettingsPageOnMobile() const = 0;
#endif
  };
  explicit CreditCardCVCAuthenticator(AutofillClient* client);
  ~CreditCardCVCAuthenticator() override;

  // Authentication
  void Authenticate(const CreditCard* card,
                    base::WeakPtr<Requester> requester,
                    PersonalDataManager* personal_data_manager,
                    const base::TimeTicks& form_parsed_timestamp);

  // payments::FullCardRequest::ResultDelegate
  void OnFullCardRequestSucceeded(
      const payments::FullCardRequest& full_card_request,
      const CreditCard& card,
      const std::u16string& cvc) override;
  void OnFullCardRequestFailed(
      payments::FullCardRequest::FailureType failure_type) override;

  // payments::FullCardRequest::UIDelegate
  void ShowUnmaskPrompt(const CreditCard& card,
                        AutofillClient::UnmaskCardReason reason,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(
      AutofillClient::PaymentsRpcResult result) override;
#if defined(OS_ANDROID)
  bool ShouldOfferFidoAuth() const override;
  bool UserOptedInToFidoFromSettingsPageOnMobile() const override;
#endif

  payments::FullCardRequest* GetFullCardRequest();

  base::WeakPtr<payments::FullCardRequest::UIDelegate>
  GetAsFullCardRequestUIDelegate();

 private:
  friend class AutofillAssistantTest;
  friend class AutofillManagerTest;
  friend class AutofillMetricsTest;
  friend class CreditCardAccessManagerTest;
  friend class CreditCardCVCAuthenticatorTest;

  // The associated autofill client. Weak reference.
  AutofillClient* const client_;

  // Responsible for getting the full card details, including the PAN and the
  // CVC.
  std::unique_ptr<payments::FullCardRequest> full_card_request_;

  // Weak pointer to object that is requesting authentication.
  base::WeakPtr<Requester> requester_;

  base::WeakPtrFactory<CreditCardCVCAuthenticator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CreditCardCVCAuthenticator);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_CVC_AUTHENTICATOR_H_
