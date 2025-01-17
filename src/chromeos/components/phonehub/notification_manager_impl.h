// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_IMPL_H_

#include <unordered_map>

#include "chromeos/components/phonehub/notification.h"
#include "chromeos/components/phonehub/notification_manager.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {
namespace phonehub {

class MessageSender;
class UserActionRecorder;

class NotificationManagerImpl
    : public NotificationManager,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  NotificationManagerImpl(
      MessageSender* message_sender,
      UserActionRecorder* user_action_recorder,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);
  ~NotificationManagerImpl() override;

 private:
  // NotificationManager:
  void DismissNotification(int64_t notification_id) override;
  void SendInlineReply(int64_t notification_id,
                       const std::u16string& inline_reply_text) override;

  // MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  MessageSender* message_sender_;
  UserActionRecorder* user_action_recorder_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  multidevice_setup::mojom::FeatureState notifications_feature_status_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_MANAGER_IMPL_H_
