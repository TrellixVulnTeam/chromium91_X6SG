// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/attestation/attestation_flow_adaptive.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "chromeos/dbus/constants/attestation_constants.h"

namespace chromeos {
namespace attestation {

struct AttestationFlowAdaptive::GetCertificateParams {
  AttestationCertificateProfile certificate_profile;
  AccountId account_id;
  std::string request_origin;
  bool force_new_key;
  std::string key_name;
};

// Consructs the object with `AttestationFlowTypeDecider` and
// `AttestationFlowFactory`
AttestationFlowAdaptive::AttestationFlowAdaptive(
    std::unique_ptr<ServerProxy> server_proxy)
    : AttestationFlowAdaptive(std::move(server_proxy),
                              std::make_unique<AttestationFlowTypeDecider>(),
                              std::make_unique<AttestationFlowFactory>()) {}

// We don't really use the parent class to perform attestation flow. Just give
// it a null `ServerProxy`.
AttestationFlowAdaptive::AttestationFlowAdaptive(
    std::unique_ptr<ServerProxy> server_proxy,
    std::unique_ptr<AttestationFlowTypeDecider> type_decider,
    std::unique_ptr<AttestationFlowFactory> factory)
    : AttestationFlow(/*server_proxy=*/nullptr),
      server_proxy_(std::move(server_proxy)),
      attestation_flow_type_decider_(std::move(type_decider)),
      attestation_flow_factory_(std::move(factory)) {}

AttestationFlowAdaptive::~AttestationFlowAdaptive() = default;

void AttestationFlowAdaptive::GetCertificate(
    AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool force_new_key,
    const std::string& key_name,
    CertificateCallback callback) {
  GetCertificateParams params = {
      /*.certificate_profile=*/certificate_profile,
      /*.account_id=*/account_id,
      /*.request_origin=*/request_origin,
      /*.force_new_key=*/force_new_key,
      /*.key_name=*/key_name,
  };

  auto status_reporter = std::make_unique<AttestationFlowStatusReporter>();
  auto* raw_status_reporter = status_reporter.get();

  // Start the flow with checking if platform-side integrated attestation is an
  // valid option.
  attestation_flow_type_decider_->CheckType(
      server_proxy_.get(), raw_status_reporter,
      base::BindOnce(&AttestationFlowAdaptive::OnCheckAttestationFlowType,
                     weak_factory_.GetWeakPtr(), std::move(params),
                     std::move(status_reporter), std::move(callback)));
}

void AttestationFlowAdaptive::OnCheckAttestationFlowType(
    const GetCertificateParams& params,
    std::unique_ptr<AttestationFlowStatusReporter> status_reporter,
    CertificateCallback callback,
    bool is_integrated_flow_possible) {
  LOG_IF(WARNING, !is_integrated_flow_possible)
      << "Skipping the integrated attestation flow.";
  StartGetCertificate(params, std::move(status_reporter), std::move(callback),
                      is_integrated_flow_possible);
}

void AttestationFlowAdaptive::StartGetCertificate(
    const GetCertificateParams& params,
    std::unique_ptr<AttestationFlowStatusReporter> status_reporter,
    CertificateCallback callback,
    bool is_default_flow_valid) {
  InitializeAttestationFlowFactory();

  // Use the fallback if the inetgrated flow is not valid.
  if (!is_default_flow_valid) {
    AttestationFlow* fallback_attestation_flow =
        attestation_flow_factory_->GetFallback();
    fallback_attestation_flow->GetCertificate(
        params.certificate_profile, params.account_id, params.request_origin,
        params.force_new_key, params.key_name,
        base::BindOnce(
            &AttestationFlowAdaptive::OnGetCertificateWithFallbackFlow,
            weak_factory_.GetWeakPtr(), std::move(status_reporter),
            std::move(callback)));
    return;
  }
  AttestationFlow* default_attestation_flow =
      attestation_flow_factory_->GetDefault();
  default_attestation_flow->GetCertificate(
      params.certificate_profile, params.account_id, params.request_origin,
      params.force_new_key, params.key_name,
      base::BindOnce(&AttestationFlowAdaptive::OnGetCertificateWithDefaultFlow,
                     weak_factory_.GetWeakPtr(), params,
                     std::move(status_reporter), std::move(callback)));
}

void AttestationFlowAdaptive::InitializeAttestationFlowFactory() {
  if (!is_attestation_flow_initialized_) {
    is_attestation_flow_initialized_ = true;
    // At this point, we have confirmed if the default (platform-side) flow is a
    // valid option. Now hand over the ownership of `server_proxy_`.`
    attestation_flow_factory_->Initialize(std::move(server_proxy_));
  }
}

void AttestationFlowAdaptive::OnGetCertificateWithDefaultFlow(
    const GetCertificateParams& params,
    std::unique_ptr<AttestationFlowStatusReporter> status_reporter,
    CertificateCallback callback,
    AttestationStatus status,
    const std::string& pem_certificate_chain) {
  status_reporter->OnDefaultFlowStatus(/*success=*/status ==
                                       ATTESTATION_SUCCESS);
  if (status == ATTESTATION_SUCCESS) {
    std::move(callback).Run(status, pem_certificate_chain);
    return;
  }

  LOG(WARNING) << "Default attestation flow failed: " << status;
  StartGetCertificate(params, std::move(status_reporter), std::move(callback),
                      /*is_default_flow_valid=*/false);
}

void AttestationFlowAdaptive::OnGetCertificateWithFallbackFlow(
    std::unique_ptr<AttestationFlowStatusReporter> status_reporter,
    CertificateCallback callback,
    AttestationStatus status,
    const std::string& pem_certificate_chain) {
  status_reporter->OnFallbackFlowStatus(/*success=*/status ==
                                        ATTESTATION_SUCCESS);
  std::move(callback).Run(status, pem_certificate_chain);
}

}  // namespace attestation
}  // namespace chromeos
