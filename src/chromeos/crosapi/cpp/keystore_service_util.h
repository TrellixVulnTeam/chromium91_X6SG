// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_KEYSTORE_SERVICE_UTIL_H_
#define CHROMEOS_CROSAPI_CPP_KEYSTORE_SERVICE_UTIL_H_

#include "base/component_export.h"
#include "base/optional.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"

namespace crosapi {
namespace keystore_service_util {

// The WebCrypto string for ECDSA.
COMPONENT_EXPORT(CROSAPI)
extern const char kWebCryptoEcdsa[];

// The WebCrypto string for PKCS1.
COMPONENT_EXPORT(CROSAPI)
extern const char kWebCryptoRsassaPkcs1v15[];

// The WebCrypto string for the P-256 named curve.
COMPONENT_EXPORT(CROSAPI)
extern const char kWebCryptoNamedCurveP256[];

// Converts a crosapi signing algorithm into a WebCrypto dictionary. Returns
// base::nullopt on error.
COMPONENT_EXPORT(CROSAPI)
base::Optional<base::DictionaryValue> DictionaryFromSigningAlgorithm(
    const crosapi::mojom::KeystoreSigningAlgorithmPtr& algorithm);

// Converts a WebCrypto dictionary into a crosapi signing algorithm. Returns
// base::nullopt on error.
COMPONENT_EXPORT(CROSAPI)
base::Optional<crosapi::mojom::KeystoreSigningAlgorithmPtr>
SigningAlgorithmFromDictionary(const base::DictionaryValue& dictionary);

}  // namespace keystore_service_util
}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_KEYSTORE_SERVICE_UTIL_H_
