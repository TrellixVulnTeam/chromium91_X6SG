// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_controller.h"

#include "ash/public/cpp/holding_space/holding_space_color_provider.h"
#include "ash/public/cpp/holding_space/holding_space_controller_observer.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/check.h"

namespace ash {

namespace {

HoldingSpaceController* g_instance = nullptr;

}  // namespace

HoldingSpaceController::HoldingSpaceController(
    std::unique_ptr<HoldingSpaceColorProvider> color_provider)
    : color_provider_(std::move(color_provider)) {
  CHECK(!g_instance);
  g_instance = this;

  SessionController::Get()->AddObserver(this);
}

HoldingSpaceController::~HoldingSpaceController() {
  CHECK_EQ(g_instance, this);

  SetClient(nullptr);
  SetModel(nullptr);
  g_instance = nullptr;

  SessionController::Get()->RemoveObserver(this);
}

// static
HoldingSpaceController* HoldingSpaceController::Get() {
  return g_instance;
}

void HoldingSpaceController::AddObserver(
    HoldingSpaceControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void HoldingSpaceController::RemoveObserver(
    HoldingSpaceControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void HoldingSpaceController::RegisterClientAndModelForUser(
    const AccountId& account_id,
    HoldingSpaceClient* client,
    HoldingSpaceModel* model) {
  clients_and_models_by_account_id_[account_id] = std::make_pair(client, model);
  if (account_id == active_user_account_id_) {
    SetClient(client);
    SetModel(model);
  }
}

void HoldingSpaceController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  active_user_account_id_ = account_id;

  auto client_and_model_it = clients_and_models_by_account_id_.find(account_id);
  if (client_and_model_it == clients_and_models_by_account_id_.end()) {
    SetClient(nullptr);
    SetModel(nullptr);
    return;
  }

  SetClient(client_and_model_it->second.first);
  SetModel(client_and_model_it->second.second);
}

void HoldingSpaceController::SetClient(HoldingSpaceClient* client) {
  client_ = client;
}

void HoldingSpaceController::SetModel(HoldingSpaceModel* model) {
  if (model_) {
    HoldingSpaceModel* const old_model = model_;
    model_ = nullptr;
    for (auto& observer : observers_)
      observer.OnHoldingSpaceModelDetached(old_model);
  }

  model_ = model;

  if (model_) {
    for (auto& observer : observers_)
      observer.OnHoldingSpaceModelAttached(model_);
  }
}

}  // namespace ash
