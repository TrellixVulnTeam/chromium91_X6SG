// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_inhibitor.h"

#include <memory>
#include <sstream>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

// Delay for first uninhibit retry attempt. Delay doubles for every
// subsequent attempt.
constexpr base::TimeDelta kUninhibitRetryDelay =
    base::TimeDelta::FromSeconds(2);

}  // namespace

CellularInhibitor::InhibitRequest::InhibitRequest(
    InhibitReason inhibit_reason,
    InhibitCallback inhibit_callback)
    : inhibit_reason(inhibit_reason),
      inhibit_callback(std::move(inhibit_callback)) {}

CellularInhibitor::InhibitRequest::~InhibitRequest() = default;

CellularInhibitor::InhibitLock::InhibitLock(base::OnceClosure unlock_callback)
    : unlock_callback_(std::move(unlock_callback)) {}

CellularInhibitor::InhibitLock::~InhibitLock() {
  std::move(unlock_callback_).Run();
}

// static
const base::TimeDelta CellularInhibitor::kInhibitPropertyChangeTimeout =
    base::TimeDelta::FromSeconds(5);

CellularInhibitor::CellularInhibitor() = default;

CellularInhibitor::~CellularInhibitor() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void CellularInhibitor::Init(NetworkStateHandler* network_state_handler,
                             NetworkDeviceHandler* network_device_handler) {
  network_state_handler_ = network_state_handler;
  network_device_handler_ = network_device_handler;

  network_state_handler_->AddObserver(this, FROM_HERE);
}

void CellularInhibitor::InhibitCellularScanning(InhibitReason reason,
                                                InhibitCallback callback) {
  inhibit_requests_.push(
      std::make_unique<InhibitRequest>(reason, std::move(callback)));
  ProcessRequests();
}

base::Optional<CellularInhibitor::InhibitReason>
CellularInhibitor::GetInhibitReason() const {
  if (state_ == State::kIdle)
    return base::nullopt;

  return inhibit_requests_.front()->inhibit_reason;
}

void CellularInhibitor::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CellularInhibitor::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool CellularInhibitor::HasObserver(Observer* observer) const {
  return observer_list_.HasObserver(observer);
}

void CellularInhibitor::NotifyInhibitStateChanged() {
  for (auto& observer : observer_list_)
    observer.OnInhibitStateChanged();
}

void CellularInhibitor::DeviceListChanged() {
  CheckScanningIfNeeded();
}

void CellularInhibitor::DevicePropertiesUpdated(const DeviceState* device) {
  CheckScanningIfNeeded();
  CheckInhibitPropertyIfNeeded();
}

const DeviceState* CellularInhibitor::GetCellularDevice() const {
  return network_state_handler_->GetDeviceStateByType(
      NetworkTypePattern::Cellular());
}

void CellularInhibitor::TransitionToState(State state) {
  State old_state = state_;
  state_ = state;

  bool was_inhibited = old_state != State::kIdle;
  bool is_inhibited = state_ != State::kIdle;

  std::stringstream ss;
  ss << "CellularInhibitor state: " << old_state << " => " << state_;

  if (!is_inhibited) {
    DCHECK(!GetInhibitReason());
    NET_LOG(EVENT) << ss.str();
  } else {
    NET_LOG(EVENT) << ss.str() << ", reason: " << *GetInhibitReason();
  }

  if (was_inhibited != is_inhibited)
    NotifyInhibitStateChanged();
}

void CellularInhibitor::ProcessRequests() {
  if (inhibit_requests_.empty())
    return;

  // Another inhibit request is already underway; wait until it has completed
  // before starting a new request.
  if (state_ != State::kIdle)
    return;

  uninhibit_attempts_so_far_ = 0;
  TransitionToState(State::kInhibiting);
  SetInhibitProperty();
}

void CellularInhibitor::OnInhibit(bool success) {
  DCHECK(state_ == State::kWaitForInhibit || state_ == State::kInhibiting);

  if (success) {
    TransitionToState(State::kInhibited);
    std::unique_ptr<InhibitLock> lock = std::make_unique<InhibitLock>(
        base::BindOnce(&CellularInhibitor::AttemptUninhibit,
                       weak_ptr_factory_.GetWeakPtr()));
    std::move(inhibit_requests_.front()->inhibit_callback).Run(std::move(lock));
    return;
  }

  std::move(inhibit_requests_.front()->inhibit_callback).Run(nullptr);
  PopRequestAndProcessNext();
}

void CellularInhibitor::AttemptUninhibit() {
  DCHECK(state_ == State::kInhibited);
  TransitionToState(State::kUninhibiting);
  SetInhibitProperty();
}

void CellularInhibitor::OnUninhibit(bool success) {
  DCHECK(state_ == State::kWaitForUninhibit || state_ == State::kUninhibiting);

  if (!success) {
    base::TimeDelta retry_delay =
        kUninhibitRetryDelay * (1 << uninhibit_attempts_so_far_);
    NET_LOG(DEBUG) << "Uninhibit Failed. Retrying in " << retry_delay;
    TransitionToState(State::kInhibited);
    uninhibit_attempts_so_far_++;

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CellularInhibitor::AttemptUninhibit,
                       weak_ptr_factory_.GetWeakPtr()),
        retry_delay);
    return;
  }

  TransitionToState(State::kWaitingForScanningToStart);
  CheckForScanningStarted();
}

void CellularInhibitor::CheckScanningIfNeeded() {
  if (state_ == State::kWaitingForScanningToStart)
    CheckForScanningStarted();

  if (state_ == State::kWaitingForScanningToStop)
    CheckForScanningStopped();
}

void CellularInhibitor::CheckForScanningStarted() {
  DCHECK(state_ == State::kWaitingForScanningToStart);

  if (!HasScanningStarted())
    return;

  TransitionToState(State::kWaitingForScanningToStop);
  CheckForScanningStopped();
}

bool CellularInhibitor::HasScanningStarted() {
  const DeviceState* cellular_device = GetCellularDevice();
  if (!cellular_device)
    return false;
  return !cellular_device->inhibited() && cellular_device->scanning();
}

void CellularInhibitor::CheckForScanningStopped() {
  DCHECK(state_ == State::kWaitingForScanningToStop);

  if (!HasScanningStopped())
    return;

  PopRequestAndProcessNext();
}

bool CellularInhibitor::HasScanningStopped() {
  const DeviceState* cellular_device = GetCellularDevice();
  if (!cellular_device)
    return false;
  return !cellular_device->scanning();
}

void CellularInhibitor::PopRequestAndProcessNext() {
  inhibit_requests_.pop();
  TransitionToState(State::kIdle);
  ProcessRequests();
}

void CellularInhibitor::SetInhibitProperty() {
  DCHECK(state_ == State::kInhibiting || state_ == State::kUninhibiting);
  const DeviceState* cellular_device = GetCellularDevice();
  if (!cellular_device) {
    ReturnSetInhibitPropertyResult(/*success=*/false);
    return;
  }

  bool new_inhibit_value = state_ == State::kInhibiting;

  // If the new value is already set, return early.
  if (cellular_device->inhibited() == new_inhibit_value) {
    ReturnSetInhibitPropertyResult(/*success=*/true);
    return;
  }

  network_device_handler_->SetDeviceProperty(
      cellular_device->path(), shill::kInhibitedProperty,
      base::Value(new_inhibit_value),
      base::BindOnce(&CellularInhibitor::OnSetPropertySuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CellularInhibitor::OnSetPropertyError,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*attempted_inhibit=*/new_inhibit_value));
}

void CellularInhibitor::OnSetPropertySuccess() {
  if (state_ == State::kInhibiting) {
    TransitionToState(State::kWaitForInhibit);
  } else if (state_ == State::kUninhibiting) {
    TransitionToState(State::kWaitForUninhibit);
  }
  set_inhibit_timer_.Start(
      FROM_HERE, kInhibitPropertyChangeTimeout,
      base::BindOnce(&CellularInhibitor::OnInhibitPropertyChangeTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
  CheckInhibitPropertyIfNeeded();
}

void CellularInhibitor::OnSetPropertyError(
    bool attempted_inhibit,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG(ERROR) << (attempted_inhibit ? "Inhibit" : "Uninhibit")
                 << "CellularScanning() failed: " << error_name;
  ReturnSetInhibitPropertyResult(/*success=*/false);
}

void CellularInhibitor::ReturnSetInhibitPropertyResult(bool success) {
  set_inhibit_timer_.Stop();
  if (state_ == State::kInhibiting || state_ == State::kWaitForInhibit) {
    OnInhibit(success);
    return;
  }
  if (state_ == State::kUninhibiting || state_ == State::kWaitForUninhibit) {
    OnUninhibit(success);
  }
}

void CellularInhibitor::CheckInhibitPropertyIfNeeded() {
  if (state_ != State::kWaitForInhibit && state_ != State::kWaitForUninhibit)
    return;

  const DeviceState* cellular_device = GetCellularDevice();
  if (!cellular_device)
    return;

  if (state_ == State::kWaitForInhibit && !cellular_device->inhibited())
    return;

  if (state_ == State::kWaitForUninhibit && cellular_device->inhibited())
    return;

  ReturnSetInhibitPropertyResult(/*success=*/true);
}

void CellularInhibitor::OnInhibitPropertyChangeTimeout() {
  NET_LOG(EVENT) << "Timeout waiting for inhibit property change state_"
                 << state_;
  ReturnSetInhibitPropertyResult(/*success=*/false);
}

std::ostream& operator<<(std::ostream& stream,
                         const CellularInhibitor::State& state) {
  switch (state) {
    case CellularInhibitor::State::kIdle:
      stream << "[Idle]";
      break;
    case CellularInhibitor::State::kInhibiting:
      stream << "[Inhibiting]";
      break;
    case CellularInhibitor::State::kWaitForInhibit:
      stream << "[Waiting for Inhibit property set]";
      break;
    case CellularInhibitor::State::kInhibited:
      stream << "[Inhibited]";
      break;
    case CellularInhibitor::State::kUninhibiting:
      stream << "[Uninhibiting]";
      break;
    case CellularInhibitor::State::kWaitForUninhibit:
      stream << "[Waiting for Inhibit property clear]";
      break;
    case CellularInhibitor::State::kWaitingForScanningToStart:
      stream << "[Waiting for scanning to start]";
      break;
    case CellularInhibitor::State::kWaitingForScanningToStop:
      stream << "[Waiting for scanning to stop]";
      break;
  }
  return stream;
}

}  // namespace chromeos

std::ostream& operator<<(
    std::ostream& stream,
    const chromeos::CellularInhibitor::InhibitReason& inhibit_reason) {
  switch (inhibit_reason) {
    case chromeos::CellularInhibitor::InhibitReason::kInstallingProfile:
      stream << "[Installing profile]";
      break;
    case chromeos::CellularInhibitor::InhibitReason::kRenamingProfile:
      stream << "[Renaming profile]";
      break;
    case chromeos::CellularInhibitor::InhibitReason::kRemovingProfile:
      stream << "[Removing profile]";
      break;
    case chromeos::CellularInhibitor::InhibitReason::kConnectingToProfile:
      stream << "[Connecting to profile]";
      break;
    case chromeos::CellularInhibitor::InhibitReason::kRefreshingProfileList:
      stream << "[Refreshing profile list]";
      break;
  }
  return stream;
}
