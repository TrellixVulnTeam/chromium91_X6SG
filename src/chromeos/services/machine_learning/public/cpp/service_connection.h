// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_

#include "base/component_export.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {
namespace machine_learning {

// Encapsulates a connection to the Chrome OS ML Service daemon via its Mojo
// interface.
//
// Usage for BindMachineLearningService:
//   mojo::Remote<mojom::MachineLearningService> ml_service;
//   chromeos::machine_learning::ServiceConnection::GetInstance()
//       ->BindMachineLearningService(
//             ml_service.BindNewPipeAndPassReceiver());
//   // Use ml_service to LoadBuiltinModel(), LoadFlatBufferModel() etc. e.g
//   ml_service->LoadBuiltinModel(...);
//
// Usage for GetMachineLearningService:
//   chromeos::machine_learning::ServiceConnection::GetInstance()
//       ->GetMachineLearningService()
//       .LoadBuiltinModel(...);
//
// Sequencing: BindMachineLearningService can be called from any sequence, while
// GetMachineLearningService must be called from the sequence that the instance
// is created on.
class COMPONENT_EXPORT(CHROMEOS_MLSERVICE) ServiceConnection {
 public:
  static ServiceConnection* GetInstance();
  // Overrides the result of GetInstance() for use in tests.
  // Does not take ownership of |fake_service_connection|.
  // Note: Caller is responsible for calling Initialize() on
  // `fake_service_connection`.
  static void UseFakeServiceConnectionForTesting(
      ServiceConnection* fake_service_connection);

  // Gets the primordial top-level machine learning service interface.
  // Must be called from the sequence that the instance is created on.
  virtual mojom::MachineLearningService& GetMachineLearningService() = 0;

  // Binds the receiver to a Clone of the primordial top-level interface.
  // May be called from any sequence.
  virtual void BindMachineLearningService(
      mojo::PendingReceiver<mojom::MachineLearningService> receiver) = 0;

  // Call this once at startup (e.g. PostBrowserStart) on the sequence that
  // should own the Mojo connection to MachineLearningService (e.g. UI thread).
  virtual void Initialize() = 0;

 protected:
  ServiceConnection() = default;
  virtual ~ServiceConnection() {}
};

}  // namespace machine_learning
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_
