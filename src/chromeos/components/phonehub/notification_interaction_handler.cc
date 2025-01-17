// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_interaction_handler.h"

namespace chromeos {
namespace phonehub {

NotificationInteractionHandler::NotificationInteractionHandler() = default;
NotificationInteractionHandler::~NotificationInteractionHandler() = default;

void NotificationInteractionHandler::AddNotificationClickHandler(
    NotificationClickHandler* handler) {
  handler_list_.AddObserver(handler);
}

void NotificationInteractionHandler::RemoveNotificationClickHandler(
    NotificationClickHandler* handler) {
  handler_list_.RemoveObserver(handler);
}

void NotificationInteractionHandler::NotifyNotificationClicked(
    int64_t notification_id) {
  for (auto& handler : handler_list_)
    handler.HandleNotificationClick(notification_id);
}

}  // namespace phonehub
}  // namespace chromeos