// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_NEARBY_CONNECTOR_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_NEARBY_CONNECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "chromeos/services/secure_channel/public/cpp/client/nearby_connector.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace secure_channel {

// Fake NearbyConnector implementation. When Connect() is called, parameters are
// queued up and can be completed using either FailQueuedCallback() or
// ConnectQueuedCallback(). Both of these functions take the parameters at the
// front of the queue and either cause the connection to fail or succeed. In the
// success case, a FakeConnection is returned which allows the client to
// interact with the connection.
class FakeNearbyConnector : public NearbyConnector {
 public:
  FakeNearbyConnector();
  ~FakeNearbyConnector() override;

  class FakeConnection : public mojom::NearbyMessageSender {
   public:
    FakeConnection(const std::vector<uint8_t>& bluetooth_public_address,
                   mojo::PendingReceiver<mojom::NearbyMessageSender>
                       message_sender_pending_receiver,
                   mojo::PendingRemote<mojom::NearbyMessageReceiver>
                       message_receiver_pending_remote);
    ~FakeConnection() override;

    void Disconnect();
    void ReceiveMessage(const std::string& message);

    void set_should_send_succeed(bool should_send_succeed) {
      should_send_succeed_ = should_send_succeed;
    }

    const std::vector<uint8_t>& bluetooth_public_address() const {
      return bluetooth_public_address_;
    }
    const std::vector<std::string>& sent_messages() { return sent_messages_; }

   private:
    // mojom::NearbyMessageSender:
    void SendMessage(const std::string& message,
                     SendMessageCallback callback) override;

    std::vector<uint8_t> bluetooth_public_address_;
    mojo::Receiver<mojom::NearbyMessageSender> message_sender_receiver_;
    mojo::Remote<mojom::NearbyMessageReceiver> message_receiver_remote_;

    std::vector<std::string> sent_messages_;
    bool should_send_succeed_ = true;
  };

  void FailQueuedCallback();
  FakeConnection* ConnectQueuedCallback();

  // Invoked when Connect() is called.
  base::OnceClosure on_connect_closure;

 private:
  struct ConnectArgs {
    ConnectArgs(
        const std::vector<uint8_t>& bluetooth_public_address,
        mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver,
        ConnectCallback callback);
    ~ConnectArgs();

    std::vector<uint8_t> bluetooth_public_address;
    mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver;
    ConnectCallback callback;
  };

  /// mojom::NearbyConnector:
  void Connect(
      const std::vector<uint8_t>& bluetooth_public_address,
      mojo::PendingRemote<mojom::NearbyMessageReceiver> message_receiver,
      ConnectCallback callback) override;

  base::queue<std::unique_ptr<ConnectArgs>> queued_connect_args_;
  std::vector<std::unique_ptr<FakeConnection>> fake_connections_;
};

}  // namespace secure_channel
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_FAKE_NEARBY_CONNECTOR_H_
