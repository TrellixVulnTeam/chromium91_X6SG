// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_INHIBITOR_H_
#define CHROMEOS_NETWORK_CELLULAR_INHIBITOR_H_

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class DeviceState;
class NetworkStateHandler;
class NetworkDeviceHandler;

// Updates the "Inhibited" property of the Cellular device.
//
// When some SIM-related operations are performed, properties of the Cellular
// device can change to a temporary value and then change back. To prevent churn
// in these properties, Shill provides the "Inhibited" property to inhibit any
// scans.
//
// This class is intended to be used when performing such actions to ensure that
// these transient states never occur.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularInhibitor
    : public NetworkStateHandlerObserver {
 public:
  CellularInhibitor();
  CellularInhibitor(const CellularInhibitor&) = delete;
  CellularInhibitor& operator=(const CellularInhibitor&) = delete;
  ~CellularInhibitor() override;

  void Init(NetworkStateHandler* network_state_handler,
            NetworkDeviceHandler* network_device_handler);

  // A lock object which ensures that all other Inhibit requests are blocked
  // during this it's lifetime. When a lock object is deleted, the Cellular
  // device is automatically uninhibited and any pending inhibit requests are
  // processed.
  class InhibitLock {
   public:
    explicit InhibitLock(base::OnceClosure unlock_callback);
    ~InhibitLock();

   private:
    base::OnceClosure unlock_callback_;
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when the inhibit state has changed; observers should use the
    // GetInhibitReason() function to determine the current state.
    virtual void OnInhibitStateChanged() = 0;
  };

  enum class InhibitReason {
    kInstallingProfile,
    kRenamingProfile,
    kRemovingProfile,
    kConnectingToProfile,
    kRefreshingProfileList
  };

  // Callback which returns InhibitLock on inhibit success or nullptr on
  // failure.
  using InhibitCallback =
      base::OnceCallback<void(std::unique_ptr<InhibitLock>)>;

  // Puts the Cellular device in Inhibited state and returns an InhibitLock
  // object which when destroyed automatically uninhibits the Cellular device. A
  // call to this method will block until the last issues lock is deleted.
  void InhibitCellularScanning(InhibitReason reason, InhibitCallback callback);

  // Returns the reason that cellular scanning is currently inhibited, or null
  // if it is not inhibited.
  base::Optional<InhibitReason> GetInhibitReason() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

 protected:
  void NotifyInhibitStateChanged();

 private:
  // Timeout after which an inhibit property change is considered to be failed.
  static const base::TimeDelta kInhibitPropertyChangeTimeout;
  friend class CellularInhibitorTest;

  struct InhibitRequest {
    InhibitRequest(InhibitReason inhibit_reason,
                   InhibitCallback inhibit_callback);
    InhibitRequest(const InhibitRequest&) = delete;
    InhibitRequest& operator=(const InhibitRequest&) = delete;
    ~InhibitRequest();

    InhibitReason inhibit_reason;
    InhibitCallback inhibit_callback;
  };

  enum class State {
    kIdle,
    kInhibiting,
    kWaitForInhibit,
    kInhibited,
    kUninhibiting,
    kWaitForUninhibit,
    kWaitingForScanningToStart,
    kWaitingForScanningToStop,
  };
  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  // NetworkStateHandlerObserver:
  void DeviceListChanged() override;
  void DevicePropertiesUpdated(const DeviceState* device) override;

  const DeviceState* GetCellularDevice() const;

  void TransitionToState(State state);
  void ProcessRequests();
  void OnInhibit(bool success);
  void AttemptUninhibit();
  void OnUninhibit(bool success);

  void CheckScanningIfNeeded();
  void CheckForScanningStarted();
  bool HasScanningStarted();
  void CheckForScanningStopped();
  bool HasScanningStopped();

  void CheckInhibitPropertyIfNeeded();
  void CheckForInhibit();
  void CheckForUninhibit();
  void OnInhibitPropertyChangeTimeout();

  void PopRequestAndProcessNext();

  void SetInhibitProperty();
  void OnSetPropertySuccess();
  void OnSetPropertyError(
      bool attempted_inhibit,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);
  void ReturnSetInhibitPropertyResult(bool success);

  NetworkStateHandler* network_state_handler_ = nullptr;
  NetworkDeviceHandler* network_device_handler_ = nullptr;

  State state_ = State::kIdle;
  base::queue<std::unique_ptr<InhibitRequest>> inhibit_requests_;

  size_t uninhibit_attempts_so_far_ = 0;
  base::OneShotTimer set_inhibit_timer_;

  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<CellularInhibitor> weak_ptr_factory_{this};
};

}  // namespace chromeos

std::ostream& operator<<(
    std::ostream& stream,
    const chromeos::CellularInhibitor::InhibitReason& inhibit_reason);

#endif  // CHROMEOS_NETWORK_CELLULAR_INHIBITOR_H_
