// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/fake_shill_device_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/network/cellular_metrics_logger.h"
#include "chromeos/network/network_device_handler_impl.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

const char kDefaultCellularDevicePath[] = "stub_cellular_device";
const char kUnknownCellularDevicePath[] = "unknown_cellular_device";
const char kDefaultWifiDevicePath[] = "stub_wifi_device";
const char kResultFailure[] = "failure";
const char kResultSuccess[] = "success";
const char kDefaultPin[] = "1111";

}  // namespace

class NetworkDeviceHandlerTest : public testing::Test {
 public:
  NetworkDeviceHandlerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}
  ~NetworkDeviceHandlerTest() override = default;

  void SetUp() override {
    shill_clients::InitializeFakes();
    fake_device_client_ = ShillDeviceClient::Get();
    fake_device_client_->GetTestInterface()->ClearDevices();

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    NetworkDeviceHandlerImpl* device_handler = new NetworkDeviceHandlerImpl;
    device_handler->Init(network_state_handler_.get());
    network_device_handler_.reset(device_handler);

    // Add devices after handlers have been initialized.
    ShillDeviceClient::TestInterface* device_test =
        fake_device_client_->GetTestInterface();
    device_test->AddDevice(kDefaultCellularDevicePath, shill::kTypeCellular,
                           "cellular1");
    device_test->AddDevice(kDefaultWifiDevicePath, shill::kTypeWifi, "wifi1");

    base::ListValue test_ip_configs;
    test_ip_configs.AppendString("ip_config1");
    device_test->SetDeviceProperty(kDefaultWifiDevicePath,
                                   shill::kIPConfigsProperty, test_ip_configs,
                                   /*notify_changed=*/true);

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_state_handler_->Shutdown();
    network_device_handler_.reset();
    network_state_handler_.reset();
    shill_clients::Shutdown();
  }

  base::OnceClosure GetSuccessCallback() {
    return base::BindOnce(&NetworkDeviceHandlerTest::SuccessCallback,
                          base::Unretained(this));
  }

  network_handler::ErrorCallback GetErrorCallback() {
    return base::BindOnce(&NetworkDeviceHandlerTest::ErrorCallback,
                          base::Unretained(this));
  }

  void ErrorCallback(const std::string& error_name,
                     std::unique_ptr<base::DictionaryValue> error_data) {
    VLOG(1) << "ErrorCallback: " << error_name;
    result_ = error_name;
  }

  void SuccessCallback() { result_ = kResultSuccess; }

  void GetPropertiesCallback(const std::string& device_path,
                             base::Optional<base::Value> properties) {
    if (!properties) {
      result_ = kResultFailure;
      return;
    }
    result_ = kResultSuccess;
    properties_ = base::DictionaryValue::From(
        std::make_unique<base::Value>(std::move(*properties)));
  }

  void StringSuccessCallback(const std::string& result) {
    VLOG(1) << "StringSuccessCallback: " << result;
    result_ = result;
  }

  void GetDeviceProperties(const std::string& device_path,
                           const std::string& expected_result) {
    network_device_handler_->GetDeviceProperties(
        device_path,
        base::BindOnce(&NetworkDeviceHandlerTest::GetPropertiesCallback,
                       base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(expected_result, result_);
  }

  void ExpectDeviceProperty(const std::string& device_path,
                            const std::string& property_name,
                            const std::string& expected_value) {
    GetDeviceProperties(device_path, kResultSuccess);
    std::string value;
    ASSERT_TRUE(
        properties_->GetStringWithoutPathExpansion(property_name, &value));
    ASSERT_EQ(value, expected_value);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::string result_;
  ShillDeviceClient* fake_device_client_ = nullptr;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<base::DictionaryValue> properties_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkDeviceHandlerTest);
};

TEST_F(NetworkDeviceHandlerTest, GetDeviceProperties) {
  GetDeviceProperties(kDefaultWifiDevicePath, kResultSuccess);
  std::string type;
  properties_->GetString(shill::kTypeProperty, &type);
  EXPECT_EQ(shill::kTypeWifi, type);
}

TEST_F(NetworkDeviceHandlerTest, SetDeviceProperty) {
  // Set the shill::kScanIntervalProperty to true. The call
  // should succeed and the value should be set.
  network_device_handler_->SetDeviceProperty(
      kDefaultCellularDevicePath, shill::kScanIntervalProperty, base::Value(1),
      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  // GetDeviceProperties should return the value set by SetDeviceProperty.
  GetDeviceProperties(kDefaultCellularDevicePath, kResultSuccess);

  int interval = 0;
  EXPECT_TRUE(properties_->GetIntegerWithoutPathExpansion(
      shill::kScanIntervalProperty, &interval));
  EXPECT_EQ(1, interval);

  // Repeat the same with value false.
  network_device_handler_->SetDeviceProperty(
      kDefaultCellularDevicePath, shill::kScanIntervalProperty, base::Value(2),
      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  GetDeviceProperties(kDefaultCellularDevicePath, kResultSuccess);

  EXPECT_TRUE(properties_->GetIntegerWithoutPathExpansion(
      shill::kScanIntervalProperty, &interval));
  EXPECT_EQ(2, interval);

  // Set property on an invalid path.
  network_device_handler_->SetDeviceProperty(
      kUnknownCellularDevicePath, shill::kScanIntervalProperty, base::Value(1),
      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);

  // Setting a owner-protected device property through SetDeviceProperty must
  // fail.
  network_device_handler_->SetDeviceProperty(
      kDefaultCellularDevicePath, shill::kCellularAllowRoamingProperty,
      base::Value(true), GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(kResultSuccess, result_);
}

TEST_F(NetworkDeviceHandlerTest, CellularAllowRoaming) {
  // Start with disabled data roaming.
  ShillDeviceClient::TestInterface* device_test =
      fake_device_client_->GetTestInterface();
  device_test->SetDeviceProperty(kDefaultCellularDevicePath,
                                 shill::kCellularAllowRoamingProperty,
                                 base::Value(false), /*notify_changed=*/true);

  network_device_handler_->SetCellularAllowRoaming(true);
  base::RunLoop().RunUntilIdle();

  // Roaming should be enabled now.
  GetDeviceProperties(kDefaultCellularDevicePath, kResultSuccess);

  bool allow_roaming;
  EXPECT_TRUE(properties_->GetBooleanWithoutPathExpansion(
      shill::kCellularAllowRoamingProperty, &allow_roaming));
  EXPECT_TRUE(allow_roaming);

  network_device_handler_->SetCellularAllowRoaming(false);
  base::RunLoop().RunUntilIdle();

  // Roaming should be disable again.
  GetDeviceProperties(kDefaultCellularDevicePath, kResultSuccess);

  EXPECT_TRUE(properties_->GetBooleanWithoutPathExpansion(
      shill::kCellularAllowRoamingProperty, &allow_roaming));
  EXPECT_FALSE(allow_roaming);
}

TEST_F(NetworkDeviceHandlerTest,
       ResetUsbEthernetMacAddressSourceForSecondaryUsbDevices) {
  ShillDeviceClient::TestInterface* device_test =
      fake_device_client_->GetTestInterface();

  constexpr char kSource[] = "some_source1";

  constexpr char kUsbEthernetDevicePath1[] = "usb_ethernet_device1";
  device_test->AddDevice(kUsbEthernetDevicePath1, shill::kTypeEthernet, "eth1");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath1, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath1,
                                 shill::kLinkUpProperty, base::Value(true),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath1,
                                 shill::kUsbEthernetMacAddressSourceProperty,
                                 base::Value("source_to_override1"),
                                 /*notify_changed=*/true);
  constexpr char kUsbEthernetDevicePath2[] = "usb_ethernet_device2";
  device_test->AddDevice(kUsbEthernetDevicePath2, shill::kTypeEthernet, "eth2");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath2, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath2,
                                 shill::kLinkUpProperty, base::Value(true),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath2,
                                 shill::kUsbEthernetMacAddressSourceProperty,
                                 base::Value(kSource),
                                 /*notify_changed=*/true);
  constexpr char kUsbEthernetDevicePath3[] = "usb_ethernet_device3";
  device_test->AddDevice(kUsbEthernetDevicePath3, shill::kTypeEthernet, "eth3");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath3, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath3,
                                 shill::kLinkUpProperty, base::Value(true),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath3,
                                 shill::kUsbEthernetMacAddressSourceProperty,
                                 base::Value("source_to_override2"),
                                 /*notify_changed=*/true);

  network_device_handler_->SetUsbEthernetMacAddressSource(kSource);
  base::RunLoop().RunUntilIdle();

  // Expect to reset source property for eth1 and eth3 since eth2 already has
  // needed source value.
  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath1, shill::kUsbEthernetMacAddressSourceProperty,
      "usb_adapter_mac"));
  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath2, shill::kUsbEthernetMacAddressSourceProperty,
      kSource));
  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath3, shill::kUsbEthernetMacAddressSourceProperty,
      "usb_adapter_mac"));
}

TEST_F(NetworkDeviceHandlerTest, UsbEthernetMacAddressSourceNotSupported) {
  ShillDeviceClient::TestInterface* device_test =
      fake_device_client_->GetTestInterface();

  constexpr char kSourceToOverride[] = "source_to_override";
  constexpr char kUsbEthernetDevicePath[] = "usb_ethernet_device1";
  device_test->AddDevice(kUsbEthernetDevicePath, shill::kTypeEthernet, "eth1");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath, shill::kLinkUpProperty,
                                 base::Value(true),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath,
                                 shill::kUsbEthernetMacAddressSourceProperty,
                                 base::Value(kSourceToOverride),
                                 /*notify_changed=*/true);
  device_test->SetUsbEthernetMacAddressSourceError(
      kUsbEthernetDevicePath, shill::kErrorResultNotSupported);

  network_device_handler_->SetUsbEthernetMacAddressSource("some_source1");
  base::RunLoop().RunUntilIdle();

  // Expect to do not change MAC address source property, because eth1 does not
  // support |some_source1|.
  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath, shill::kUsbEthernetMacAddressSourceProperty,
      kSourceToOverride));

  constexpr char kSource2[] = "some_source2";
  device_test->SetUsbEthernetMacAddressSourceError(kUsbEthernetDevicePath, "");
  network_device_handler_->SetUsbEthernetMacAddressSource(kSource2);
  base::RunLoop().RunUntilIdle();

  // Expect to change MAC address source property, because eth1 supports
  // |some_source2|.
  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath, shill::kUsbEthernetMacAddressSourceProperty,
      kSource2));
}

TEST_F(NetworkDeviceHandlerTest, UsbEthernetMacAddressSource) {
  ShillDeviceClient::TestInterface* device_test =
      fake_device_client_->GetTestInterface();

  constexpr char kUsbEthernetDevicePath1[] = "ubs_ethernet_device1";
  device_test->AddDevice(kUsbEthernetDevicePath1, shill::kTypeEthernet, "eth1");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath1, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);

  constexpr char kUsbEthernetDevicePath2[] = "usb_ethernet_device2";
  device_test->AddDevice(kUsbEthernetDevicePath2, shill::kTypeEthernet, "eth2");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath2, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath2,
                                 shill::kAddressProperty,
                                 base::Value("abcdef123456"),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath2,
                                 shill::kLinkUpProperty, base::Value(true),
                                 /*notify_changed=*/true);
  device_test->SetUsbEthernetMacAddressSourceError(
      kUsbEthernetDevicePath2, shill::kErrorResultNotSupported);

  constexpr char kUsbEthernetDevicePath3[] = "usb_ethernet_device3";
  device_test->AddDevice(kUsbEthernetDevicePath3, shill::kTypeEthernet, "eth3");
  device_test->SetDeviceProperty(
      kUsbEthernetDevicePath3, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypeUsb), /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath3,
                                 shill::kAddressProperty,
                                 base::Value("123456abcdef"),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kUsbEthernetDevicePath3,
                                 shill::kLinkUpProperty, base::Value(true),
                                 /*notify_changed=*/true);

  constexpr char kPciEthernetDevicePath[] = "pci_ethernet_device";
  device_test->AddDevice(kPciEthernetDevicePath, shill::kTypeEthernet, "eth4");
  device_test->SetDeviceProperty(
      kPciEthernetDevicePath, shill::kDeviceBusTypeProperty,
      base::Value(shill::kDeviceBusTypePci), /*notify_changed=*/true);

  // Expect property change on eth3, because:
  //   1) eth2 device is connected to the internet, but does not support MAC
  //      address change;
  //   2) eth3 device is connected to the internet and supports MAC address
  //      change.
  constexpr char kSource1[] = "some_source1";
  network_device_handler_->SetUsbEthernetMacAddressSource(kSource1);
  base::RunLoop().RunUntilIdle();

  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath3, shill::kUsbEthernetMacAddressSourceProperty,
      kSource1));

  // Expect property change on eth3, because device is connected to the
  // internet.
  const char* kSource2 = shill::kUsbEthernetMacAddressSourceBuiltinAdapterMac;
  network_device_handler_->SetUsbEthernetMacAddressSource(kSource2);
  base::RunLoop().RunUntilIdle();

  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath3, shill::kUsbEthernetMacAddressSourceProperty,
      kSource2));

  // Expect property change back to "usb_adapter_mac" on eth3, because device
  // is not connected to the internet.
  device_test->SetDeviceProperty(kUsbEthernetDevicePath3,
                                 shill::kLinkUpProperty, base::Value(false),
                                 /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();

  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath3, shill::kUsbEthernetMacAddressSourceProperty,
      "usb_adapter_mac"));

  // Expect property change back to "usb_adapter_mac" on eth1, because both
  // builtin PCI eth4 and eth1 have the same MAC address and connected to the
  // internet.
  device_test->SetDeviceProperty(kUsbEthernetDevicePath1,
                                 shill::kLinkUpProperty, base::Value(true),
                                 /*notify_changed=*/true);
  device_test->SetDeviceProperty(kPciEthernetDevicePath, shill::kLinkUpProperty,
                                 base::Value(true),
                                 /*notify_changed=*/true);
  base::RunLoop().RunUntilIdle();

  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath1, shill::kUsbEthernetMacAddressSourceProperty,
      "usb_adapter_mac"));

  // Expect property change on eth1, because device is connected to the
  // internet and builtin PCI eth4 and eth1 have different MAC addresses.
  constexpr char kSource3[] = "some_source3";
  network_device_handler_->SetUsbEthernetMacAddressSource(kSource3);
  base::RunLoop().RunUntilIdle();

  ASSERT_NO_FATAL_FAILURE(ExpectDeviceProperty(
      kUsbEthernetDevicePath1, shill::kUsbEthernetMacAddressSourceProperty,
      kSource3));
}

TEST_F(NetworkDeviceHandlerTest, RequirePin) {
  base::HistogramTester histogram_tester;

  // Test that the success callback gets called.
  network_device_handler_->RequirePin(kDefaultCellularDevicePath, true,
                                      kDefaultPin, GetSuccessCallback(),
                                      GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinLockSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kSimPinLockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->RequirePin(kUnknownCellularDevicePath, true,
                                      kDefaultPin, GetSuccessCallback(),
                                      GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);

  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinLockSuccessHistogram, 2);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kSimPinLockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorUnknown, 1);
}

TEST_F(NetworkDeviceHandlerTest, EnterPin) {
  base::HistogramTester histogram_tester;

  // Test that the success callback gets called.
  network_device_handler_->EnterPin(kDefaultCellularDevicePath, kDefaultPin,
                                    GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinUnlockSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->EnterPin(kUnknownCellularDevicePath, kDefaultPin,
                                    GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinUnlockSuccessHistogram, 2);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kSimPinUnlockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorUnknown, 1);
}

TEST_F(NetworkDeviceHandlerTest, UnblockPin) {
  base::HistogramTester histogram_tester;
  const char kPuk[] = "12345678";
  const char kPin[] = "1234";

  // Test that the success callback gets called.
  network_device_handler_->UnblockPin(kDefaultCellularDevicePath, kPin, kPuk,
                                      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinUnblockSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->UnblockPin(kUnknownCellularDevicePath, kPin, kPuk,
                                      GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorDeviceMissing, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinUnblockSuccessHistogram, 2);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kSimPinUnblockSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorUnknown, 1);
}

TEST_F(NetworkDeviceHandlerTest, ChangePin) {
  base::HistogramTester histogram_tester;
  const char kNewPin[] = "1234";
  const char kIncorrectPin[] = "9999";

  fake_device_client_->GetTestInterface()->SetSimLocked(
      kDefaultCellularDevicePath, true);

  // Test that the success callback gets called.
  network_device_handler_->ChangePin(
      kDefaultCellularDevicePath, FakeShillDeviceClient::kDefaultSimPin,
      kNewPin, GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinChangeSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kSimPinChangeSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kSuccess, 1);

  // Test that the shill error propagates to the error callback.
  network_device_handler_->ChangePin(kDefaultCellularDevicePath, kIncorrectPin,
                                     kNewPin, GetSuccessCallback(),
                                     GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(NetworkDeviceHandler::kErrorIncorrectPin, result_);
  histogram_tester.ExpectTotalCount(
      CellularMetricsLogger::kSimPinChangeSuccessHistogram, 2);
  histogram_tester.ExpectBucketCount(
      CellularMetricsLogger::kSimPinChangeSuccessHistogram,
      CellularMetricsLogger::SimPinOperationResult::kErrorUnknown, 1);
}

TEST_F(NetworkDeviceHandlerTest, AddWifiWakeOnPacketOfTypes) {
  std::vector<std::string> valid_packet_types = {shill::kWakeOnTCP,
                                                 shill::kWakeOnUDP};

  network_device_handler_->AddWifiWakeOnPacketOfTypes(
      valid_packet_types, GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
}

TEST_F(NetworkDeviceHandlerTest, AddAndRemoveWifiWakeOnPacketOfTypes) {
  std::vector<std::string> valid_packet_types = {shill::kWakeOnTCP,
                                                 shill::kWakeOnUDP};
  std::vector<std::string> remove_packet_types = {shill::kWakeOnTCP};

  network_device_handler_->AddWifiWakeOnPacketOfTypes(
      valid_packet_types, GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);

  network_device_handler_->RemoveWifiWakeOnPacketOfTypes(
      remove_packet_types, GetSuccessCallback(), GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kResultSuccess, result_);
}

}  // namespace chromeos
