// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HERMES_FAKE_HERMES_PROFILE_CLIENT_H_
#define CHROMEOS_DBUS_HERMES_FAKE_HERMES_PROFILE_CLIENT_H_

#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace chromeos {

// Fake implementation for HermesProfileClient.
class COMPONENT_EXPORT(HERMES_CLIENT) FakeHermesProfileClient
    : public HermesProfileClient,
      public HermesProfileClient::TestInterface {
 public:
  class Properties : public HermesProfileClient::Properties {
   public:
    explicit Properties(const PropertyChangedCallback& callback);
    ~Properties() override;

    // dbus::PropertySet:
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override;
    void GetAll() override;
    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override;
  };

  FakeHermesProfileClient();
  FakeHermesProfileClient(const FakeHermesProfileClient&) = delete;
  FakeHermesProfileClient& operator=(const FakeHermesProfileClient&) = delete;
  ~FakeHermesProfileClient() override;

  // HermesProfileClient::TestInterface:
  void ClearProfile(const dbus::ObjectPath& carrier_profile_path) override;
  void SetConnectedAfterEnable(bool connected_after_enable) override;

  // HermesProfileClient:
  void EnableCarrierProfile(const dbus::ObjectPath& object_path,
                            HermesResponseCallback callback) override;
  void DisableCarrierProfile(const dbus::ObjectPath& object_path,
                             HermesResponseCallback callback) override;
  HermesProfileClient::Properties* GetProperties(
      const dbus::ObjectPath& object_path) override;
  HermesProfileClient::TestInterface* GetTestInterface() override;

 private:
  void UpdateCellularDevice(HermesProfileClient::Properties* properties);
  void UpdateCellularServices(const std::string& iccid, bool connectable);
  void CallNotifyPropertyChanged(const dbus::ObjectPath& object_path,
                                 const std::string& property_name);
  void NotifyPropertyChanged(const dbus::ObjectPath& object_path,
                             const std::string& property_name);

  bool connected_after_enable_ = false;

  // Maps fake profile properties to their object paths.
  using PropertiesMap =
      std::map<const dbus::ObjectPath, std::unique_ptr<Properties>>;
  PropertiesMap properties_map_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_HERMES_FAKE_HERMES_PROFILE_CLIENT_H_
