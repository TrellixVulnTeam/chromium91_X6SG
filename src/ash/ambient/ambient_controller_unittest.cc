// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/power/power_status.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"

namespace ash {

constexpr char kUser1[] = "user1@gmail.com";
constexpr char kUser2[] = "user2@gmail.com";

class AmbientControllerTest : public AmbientAshTestBase {
 public:
  AmbientControllerTest() : AmbientAshTestBase() {}
  ~AmbientControllerTest() override = default;

  // AmbientAshTestBase:
  void SetUp() override {
    AmbientAshTestBase::SetUp();
    GetSessionControllerClient()->set_show_lock_screen_views(true);
  }

  bool IsPrefObserved(const std::string& pref_name) {
    auto* pref_change_registrar =
        ambient_controller()->pref_change_registrar_.get();
    DCHECK(pref_change_registrar);
    return pref_change_registrar->IsObserved(pref_name);
  }

  bool WidgetsVisible() {
    const auto& views = GetContainerViews();
    return views.size() > 0 &&
           std::all_of(views.cbegin(), views.cend(), [](const auto* view) {
             return view->GetWidget()->IsVisible();
           });
  }

  bool AreSessionSpecificObserversBound() {
    auto* ctrl = ambient_controller();

    bool ui_model_bound = ctrl->ambient_ui_model_observer_.IsObserving();
    bool backend_model_bound =
        ctrl->ambient_backend_model_observer_.IsObserving();
    bool power_manager_bound =
        ctrl->power_manager_client_observer_.IsObserving();
    bool fingerprint_bound = ctrl->fingerprint_observer_receiver_.is_bound();
    EXPECT_EQ(ui_model_bound, backend_model_bound)
        << "observers should all have the same state";
    EXPECT_EQ(ui_model_bound, power_manager_bound)
        << "observers should all have the same state";
    EXPECT_EQ(ui_model_bound, fingerprint_bound)
        << "observers should all have the same state";
    return ui_model_bound;
  }
};

TEST_F(AmbientControllerTest, ShowAmbientScreenUponLock) {
  LockScreen();
  // Lockscreen will not immediately show Ambient mode.
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Ambient mode will show after inacivity and successfully loading first
  // image.
  FastForwardToLockScreenTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Clean up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest, NotShowAmbientWhenPrefNotEnabled) {
  SetAmbientModeEnabled(false);

  LockScreen();
  // Lockscreen will not immediately show Ambient mode.
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Ambient mode will not show after inacivity and successfully loading first
  // image.
  FastForwardToLockScreenTimeout();
  FastForwardTiny();

  EXPECT_TRUE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Clean up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest, HideAmbientScreen) {
  LockScreen();
  FastForwardToLockScreenTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  HideAmbientScreen();

  FastForwardTiny();
  EXPECT_TRUE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kHidden);

  // Clean up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest, CloseAmbientScreenUponUnlock) {
  LockScreen();
  FastForwardToLockScreenTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  UnlockScreen();

  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->IsShown());
  // The view should be destroyed along the widget.
  FastForwardTiny();
  EXPECT_TRUE(GetContainerViews().empty());
}

TEST_F(AmbientControllerTest, CloseAmbientScreenUponUnlockSecondaryUser) {
  // Simulate the login screen.
  ClearLogin();
  SimulateUserLogin(kUser1);
  SetAmbientModeEnabled(true);

  LockScreen();
  FastForwardToLockScreenTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  SimulateUserLogin(kUser2);
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->IsShown());
  // The view should be destroyed along the widget.
  FastForwardTiny();
  EXPECT_TRUE(GetContainerViews().empty());

  FastForwardToLockScreenTimeout();
  FastForwardTiny();
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->IsShown());
  // The view should be destroyed along the widget.
  FastForwardTiny();
  EXPECT_TRUE(GetContainerViews().empty());
}

TEST_F(AmbientControllerTest, NotShowAmbientWhenLockSecondaryUser) {
  // Simulate the login screen.
  ClearLogin();
  SimulateUserLogin(kUser1);
  SetAmbientModeEnabled(true);

  LockScreen();
  FastForwardToLockScreenTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());

  SimulateUserLogin(kUser2);
  SetAmbientModeEnabled(true);

  // Ambient mode should not show for second user even if that user has the pref
  // turned on.
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->IsShown());
  // The view should be destroyed along the widget.
  FastForwardTiny();
  EXPECT_TRUE(GetContainerViews().empty());

  LockScreen();
  FastForwardToLockScreenTimeout();
  FastForwardTiny();

  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->IsShown());
  // The view should be destroyed along the widget.
  EXPECT_TRUE(GetContainerViews().empty());
}

TEST_F(AmbientControllerTest, ShouldRequestAccessTokenWhenLockingScreen) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  std::string access_token = "access_token";
  IssueAccessToken(access_token, /*with_error=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Should close ambient widget already when unlocking screen.
  UnlockScreen();
  EXPECT_FALSE(IsAccessTokenRequestPending());
}

TEST_F(AmbientControllerTest, ShouldNotRequestAccessTokenWhenPrefNotEnabled) {
  SetAmbientModeEnabled(false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will not request a token.
  LockScreen();
  EXPECT_FALSE(IsAccessTokenRequestPending());

  UnlockScreen();
  EXPECT_FALSE(IsAccessTokenRequestPending());
}

TEST_F(AmbientControllerTest, ShouldReturnCachedAccessToken) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  std::string access_token = "access_token";
  IssueAccessToken(access_token, /*with_error=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Another token request will return cached token.
  base::OnceClosure closure = base::MakeExpectedRunClosure(FROM_HERE);
  base::RunLoop run_loop;
  ambient_controller()->RequestAccessToken(base::BindLambdaForTesting(
      [&](const std::string& gaia_id, const std::string& access_token_fetched) {
        EXPECT_EQ(access_token_fetched, access_token);

        std::move(closure).Run();
        run_loop.Quit();
      }));
  EXPECT_FALSE(IsAccessTokenRequestPending());
  run_loop.Run();

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldReturnEmptyAccessToken) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  std::string access_token = "access_token";
  IssueAccessToken(access_token, /*with_error=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Another token request will return cached token.
  base::OnceClosure closure = base::MakeExpectedRunClosure(FROM_HERE);
  base::RunLoop run_loop_1;
  ambient_controller()->RequestAccessToken(base::BindLambdaForTesting(
      [&](const std::string& gaia_id, const std::string& access_token_fetched) {
        EXPECT_EQ(access_token_fetched, access_token);

        std::move(closure).Run();
        run_loop_1.Quit();
      }));
  EXPECT_FALSE(IsAccessTokenRequestPending());
  run_loop_1.Run();

  base::RunLoop run_loop_2;
  // When token expired, another token request will get empty token.
  constexpr base::TimeDelta kTokenRefreshDelay =
      base::TimeDelta::FromSeconds(60);
  task_environment()->FastForwardBy(kTokenRefreshDelay);

  closure = base::MakeExpectedRunClosure(FROM_HERE);
  ambient_controller()->RequestAccessToken(base::BindLambdaForTesting(
      [&](const std::string& gaia_id, const std::string& access_token_fetched) {
        EXPECT_TRUE(access_token_fetched.empty());

        std::move(closure).Run();
        run_loop_2.Quit();
      }));
  EXPECT_FALSE(IsAccessTokenRequestPending());
  run_loop_2.Run();

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldRetryRefreshAccessTokenAfterFailure) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Token request automatically retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldRetryRefreshAccessTokenWithBackoffPolicy) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  base::TimeDelta delay1 = GetRefreshTokenDelay();
  task_environment()->FastForwardBy(delay1 * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  base::TimeDelta delay2 = GetRefreshTokenDelay();
  EXPECT_GT(delay2, delay1);

  task_environment()->FastForwardBy(delay2 * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldRetryRefreshAccessTokenOnlyThreeTimes) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // 1st retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // 2nd retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // 3rd retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*access_token=*/std::string(), /*with_error=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Will not retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest,
       CheckAcquireAndReleaseWakeLockWhenBatteryIsCharging) {
  // Simulate a device being connected to a charger initially.
  SetPowerStateCharging();

  // Lock screen to start ambient mode, and flush the loop to ensure
  // the acquire wake lock request has reached the wake lock provider.
  LockScreen();
  FastForwardToLockScreenTimeout();
  FastForwardTiny();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  HideAmbientScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Ambient screen showup again after inactivity.
  FastForwardToLockScreenTimeout();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Unlock screen to exit ambient mode.
  UnlockScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

TEST_F(AmbientControllerTest,
       CheckAcquireAndReleaseWakeLockWhenBatteryBatteryIsFullAndDischarging) {
  SetPowerStateDischarging();
  SetBatteryPercent(100.f);
  SetExternalPowerConnected();

  // Lock screen to start ambient mode, and flush the loop to ensure
  // the acquire wake lock request has reached the wake lock provider.
  LockScreen();
  FastForwardToLockScreenTimeout();
  FastForwardTiny();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  HideAmbientScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Ambient screen showup again after inactivity.
  FastForwardToLockScreenTimeout();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Unlock screen to exit ambient mode.
  UnlockScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

TEST_F(AmbientControllerTest,
       CheckAcquireAndReleaseWakeLockWhenBatteryStateChanged) {
  SetPowerStateDischarging();
  SetExternalPowerConnected();
  SetBatteryPercent(50.f);

  // Lock screen to start ambient mode.
  LockScreen();
  FastForwardToLockScreenTimeout();
  FastForwardTiny();

  EXPECT_TRUE(ambient_controller()->IsShown());
  // Should not acquire wake lock when device is not charging and with low
  // battery.
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Connect the device with a charger.
  SetPowerStateCharging();
  base::RunLoop().RunUntilIdle();

  // Should acquire the wake lock when battery is charging.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Simulates a full battery.
  SetBatteryPercent(100.f);

  // Should keep the wake lock as the charger is still connected.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Disconnects the charger again.
  SetPowerStateDischarging();
  base::RunLoop().RunUntilIdle();

  // Should keep the wake lock when battery is high.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  SetBatteryPercent(50.f);
  base::RunLoop().RunUntilIdle();

  // Should release the wake lock when battery is not charging and low.
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  SetBatteryPercent(100.f);
  base::RunLoop().RunUntilIdle();

  // Should take the wake lock when battery is not charging and high.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  SetExternalPowerDisconnected();
  base::RunLoop().RunUntilIdle();

  // Should release the wake lock when power is not connected.
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // An unbalanced release should do nothing.
  UnlockScreen();
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

// TODO(cowmoo): find a way to simulate events to trigger |UserActivityDetector|
TEST_F(AmbientControllerTest, ShouldDismissContainerViewOnEvents) {
  std::vector<std::unique_ptr<ui::Event>> events;

  for (auto mouse_event_type : {ui::ET_MOUSE_PRESSED, ui::ET_MOUSE_MOVED}) {
    events.emplace_back(std::make_unique<ui::MouseEvent>(
        mouse_event_type, gfx::Point(), gfx::Point(), base::TimeTicks(),
        ui::EF_NONE, ui::EF_NONE));
  }

  events.emplace_back(std::make_unique<ui::MouseWheelEvent>(
      gfx::Vector2d(), gfx::PointF(), gfx::PointF(), base::TimeTicks(),
      ui::EF_NONE, ui::EF_NONE));

  events.emplace_back(std::make_unique<ui::ScrollEvent>(
      ui::ET_SCROLL, gfx::PointF(), gfx::PointF(), base::TimeTicks(),
      ui::EF_NONE, /*x_offset=*/0.0f,
      /*y_offset=*/0.0f,
      /*x_offset_ordinal=*/0.0f,
      /*x_offset_ordinal=*/0.0f, /*finger_count=*/2));

  events.emplace_back(std::make_unique<ui::TouchEvent>(
      ui::ET_TOUCH_PRESSED, gfx::PointF(), gfx::PointF(), base::TimeTicks(),
      ui::PointerDetails()));

  for (const auto& event : events) {
    ShowAmbientScreen();
    FastForwardTiny();
    EXPECT_TRUE(WidgetsVisible());

    ambient_controller()->OnUserActivity(event.get());

    FastForwardTiny();
    EXPECT_TRUE(GetContainerViews().empty());

    // Clean up.
    CloseAmbientScreen();
  }
}

TEST_F(AmbientControllerTest, ShouldDismissAndThenComesBack) {
  LockScreen();
  FastForwardToLockScreenTimeout();
  FastForwardTiny();
  EXPECT_TRUE(WidgetsVisible());

  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_NONE, ui::EF_NONE);
  ambient_controller()->OnUserActivity(&mouse_event);
  FastForwardTiny();
  EXPECT_TRUE(GetContainerViews().empty());

  FastForwardToLockScreenTimeout();
  FastForwardTiny();
  EXPECT_TRUE(WidgetsVisible());
}

TEST_F(AmbientControllerTest, ShouldDismissContainerViewOnKeyEvent) {
  // Without user interaction, should show ambient mode.
  ambient_controller()->ShowUi();
  EXPECT_FALSE(WidgetsVisible());
  FastForwardTiny();
  EXPECT_TRUE(WidgetsVisible());
  CloseAmbientScreen();

  // When ambient is shown, OnUserActivity() should ignore key event.
  ambient_controller()->ShowUi();
  EXPECT_TRUE(ambient_controller()->IsShown());

  // General key press will exit ambient mode.
  // Simulate key press to close the widget.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_A, /*flags=*/0);
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest,
       ShouldDismissContainerViewOnKeyEventWhenLockScreenInBackground) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should not lock the device and enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());
  EXPECT_TRUE(ambient_controller()->IsShown());

  FastForwardToBackgroundLockScreenTimeout();
  EXPECT_TRUE(IsLocked());
  // Should not disrupt ongoing ambient mode.
  EXPECT_TRUE(ambient_controller()->IsShown());

  // General key press will exit ambient mode.
  // Simulate key press to close the widget.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_A, /*flags=*/0);
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest,
       ShouldShowAmbientScreenWithLockscreenWhenScreenIsDimmed) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should enter ambient mode when the screen is dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());
  EXPECT_TRUE(ambient_controller()->IsShown());

  FastForwardToBackgroundLockScreenTimeout();
  EXPECT_TRUE(IsLocked());
  // Should not disrupt ongoing ambient mode.
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Closes ambient for clean-up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest,
       ShouldShowAmbientScreenWithLockscreenWithNoisyPowerEvents) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should enter ambient mode when the screen is dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());

  FastForwardHalfLockScreenDelay();
  SetPowerStateCharging();

  FastForwardHalfLockScreenDelay();
  SetPowerStateCharging();

  EXPECT_TRUE(IsLocked());
  // Should not disrupt ongoing ambient mode.
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Closes ambient for clean-up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest,
       ShouldShowAmbientScreenWithoutLockscreenWhenScreenIsDimmed) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  // When power is discharging, we do not lock the screen with ambient
  // mode since we do not prevent the device go to sleep which will natually
  // lock the device.
  SetPowerStateDischarging();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should not lock the device but still enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());
  EXPECT_TRUE(ambient_controller()->IsShown());

  FastForwardToLockScreenTimeout();
  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());

  FastForwardToBackgroundLockScreenTimeout();
  EXPECT_FALSE(IsLocked());

  // Closes ambient for clean-up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldShowAmbientScreenWhenScreenIsDimmed) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(false);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should not lock the device but enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());

  FastForwardToBackgroundLockScreenTimeout();
  EXPECT_FALSE(IsLocked());

  // Closes ambient for clean-up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldHideAmbientScreenWhenDisplayIsOff) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(false);
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should not lock the device and enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Should dismiss ambient mode screen.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/true);
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Screen back on again, should not have ambient screen.
  SetScreenIdleStateAndWait(/*dimmed=*/false, /*off=*/false);
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest,
       ShouldHideAmbientScreenWhenDisplayIsOffThenComesBackWithLockScreen) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should not lock the device and enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardToLockScreenTimeout();
  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());

  FastForwardToBackgroundLockScreenTimeout();
  EXPECT_TRUE(IsLocked());

  // Should dismiss ambient mode screen.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/true);
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Screen back on again, should not have ambient screen, but still has lock
  // screen.
  SetScreenIdleStateAndWait(/*dimmed=*/false, /*off=*/false);
  EXPECT_TRUE(IsLocked());
  EXPECT_FALSE(ambient_controller()->IsShown());

  FastForwardToLockScreenTimeout();
  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest,
       ShouldHideAmbientScreenWhenDisplayIsOffAndNotStartWhenLockScreen) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateDischarging();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Should not lock the device and enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());

  // Should not lock the device because the device is not charging.
  FastForwardToBackgroundLockScreenTimeout();
  EXPECT_FALSE(IsLocked());

  // Should dismiss ambient mode screen.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/true);
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Lock screen will not start ambient mode.
  LockScreen();
  EXPECT_TRUE(IsLocked());

  FastForwardToLockScreenTimeout();
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->IsShown());

  // Screen back on again, should not have ambient screen, but still has lock
  // screen.
  SetScreenIdleStateAndWait(/*dimmed=*/false, /*off=*/false);
  EXPECT_TRUE(IsLocked());
  EXPECT_FALSE(ambient_controller()->IsShown());

  FastForwardToLockScreenTimeout();
  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest, HideCursor) {
  auto* cursor_manager = Shell::Get()->cursor_manager();
  LockScreen();

  cursor_manager->ShowCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  FastForwardToLockScreenTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShown);
  EXPECT_TRUE(ambient_controller()->IsShown());
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  // Clean up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest, ShowsOnMultipleDisplays) {
  UpdateDisplay("800x600,800x600");
  FastForwardTiny();

  ShowAmbientScreen();
  FastForwardToNextImage();

  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetNumDisplays(), 2);
  EXPECT_EQ(GetContainerViews().size(), 2u);
  // Check that each root controller has an ambient widget.
  for (auto* ctrl : RootWindowController::root_window_controllers())
    EXPECT_TRUE(ctrl->ambient_widget_for_testing() &&
                ctrl->ambient_widget_for_testing()->IsVisible());
}

// TODO(crbug.com/1195762): Test is disabled due to flakiness.
TEST_F(AmbientControllerTest, DISABLED_RespondsToDisplayAdded) {
  UpdateDisplay("800x600");
  ShowAmbientScreen();
  FastForwardToNextImage();

  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetNumDisplays(), 1);
  EXPECT_EQ(GetContainerViews().size(), 1u);

  UpdateDisplay("800x600,800x600");
  FastForwardTiny();

  EXPECT_TRUE(WidgetsVisible());
  EXPECT_EQ(screen->GetNumDisplays(), 2);
  EXPECT_EQ(GetContainerViews().size(), 2u);
  for (auto* ctrl : RootWindowController::root_window_controllers())
    EXPECT_TRUE(ctrl->ambient_widget_for_testing() &&
                ctrl->ambient_widget_for_testing()->IsVisible());
}

TEST_F(AmbientControllerTest, HandlesDisplayRemoved) {
  UpdateDisplay("800x600,800x600");
  FastForwardTiny();

  ShowAmbientScreen();
  FastForwardToNextImage();

  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetNumDisplays(), 2);
  EXPECT_EQ(GetContainerViews().size(), 2u);
  EXPECT_TRUE(WidgetsVisible());

  // Changing to one screen will destroy the widget on the non-primary screen.
  UpdateDisplay("800x600");
  FastForwardTiny();

  EXPECT_EQ(screen->GetNumDisplays(), 1);
  EXPECT_EQ(GetContainerViews().size(), 1u);
  EXPECT_TRUE(WidgetsVisible());
}

TEST_F(AmbientControllerTest, ClosesAmbientBeforeSuspend) {
  LockScreen();
  FastForwardToLockScreenTimeout();

  EXPECT_TRUE(ambient_controller()->IsShown());
  SimulateSystemSuspendAndWait(power_manager::SuspendImminent::Reason::
                                   SuspendImminent_Reason_LID_CLOSED);

  EXPECT_FALSE(ambient_controller()->IsShown());

  FastForwardToLockScreenTimeout();
  // Ambient mode should not resume until SuspendDone is received.
  EXPECT_FALSE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest, RestartsAmbientAfterSuspend) {
  LockScreen();
  FastForwardToLockScreenTimeout();

  EXPECT_TRUE(ambient_controller()->IsShown());

  SimulateSystemSuspendAndWait(
      power_manager::SuspendImminent::Reason::SuspendImminent_Reason_IDLE);

  EXPECT_FALSE(ambient_controller()->IsShown());

  // This call should be blocked by prior |SuspendImminent| until |SuspendDone|.
  ambient_controller()->ShowUi();
  EXPECT_FALSE(ambient_controller()->IsShown());

  SimulateSystemResumeAndWait();

  FastForwardToLockScreenTimeout();

  EXPECT_TRUE(ambient_controller()->IsShown());
}

TEST_F(AmbientControllerTest, ObservesPrefsWhenAmbientEnabled) {
  SetAmbientModeEnabled(false);

  // This pref is always observed.
  EXPECT_TRUE(IsPrefObserved(ambient::prefs::kAmbientModeEnabled));

  std::vector<std::string> other_prefs{
      ambient::prefs::kAmbientModeLockScreenInactivityTimeoutSeconds,
      ambient::prefs::kAmbientModeLockScreenBackgroundTimeoutSeconds,
      ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds};

  for (auto& pref_name : other_prefs)
    EXPECT_FALSE(IsPrefObserved(pref_name));

  SetAmbientModeEnabled(true);

  EXPECT_TRUE(IsPrefObserved(ambient::prefs::kAmbientModeEnabled));

  for (auto& pref_name : other_prefs)
    EXPECT_TRUE(IsPrefObserved(pref_name));
}

TEST_F(AmbientControllerTest, BindsObserversWhenAmbientEnabled) {
  auto* ctrl = ambient_controller();

  SetAmbientModeEnabled(false);

  // SessionObserver must always be observing to detect when user pref service
  // is started.
  EXPECT_TRUE(ctrl->session_observer_.IsObserving());

  EXPECT_FALSE(AreSessionSpecificObserversBound());

  SetAmbientModeEnabled(true);

  // Session observer should still be observing.
  EXPECT_TRUE(ctrl->session_observer_.IsObserving());

  EXPECT_TRUE(AreSessionSpecificObserversBound());
}

TEST_F(AmbientControllerTest, SwitchActiveUsersDoesNotDoubleBindObservers) {
  ClearLogin();
  SimulateUserLogin(kUser1);
  SetAmbientModeEnabled(true);

  TestSessionControllerClient* session = GetSessionControllerClient();

  // Observers are bound for primary user with Ambient mode enabled.
  EXPECT_TRUE(AreSessionSpecificObserversBound());
  EXPECT_TRUE(IsPrefObserved(ambient::prefs::kAmbientModeEnabled));

  // Observers are still bound when secondary user logs in.
  SimulateUserLogin(kUser2);
  EXPECT_TRUE(AreSessionSpecificObserversBound());
  EXPECT_TRUE(IsPrefObserved(ambient::prefs::kAmbientModeEnabled));

  // Observers are not re-bound for primary user when session is active.
  session->SwitchActiveUser(AccountId::FromUserEmail(kUser1));
  EXPECT_TRUE(AreSessionSpecificObserversBound());
  EXPECT_TRUE(IsPrefObserved(ambient::prefs::kAmbientModeEnabled));

  //  Switch back to secondary user.
  session->SwitchActiveUser(AccountId::FromUserEmail(kUser2));
}

TEST_F(AmbientControllerTest, BindsObserversWhenAmbientOn) {
  auto* ctrl = ambient_controller();

  LockScreen();

  // Start monitoring user activity on hidden ui.
  EXPECT_TRUE(ctrl->user_activity_observer_.IsObserving());
  // Do not monitor power status yet.
  EXPECT_FALSE(ctrl->power_status_observer_.IsObserving());

  FastForwardToLockScreenTimeout();

  EXPECT_TRUE(ctrl->user_activity_observer_.IsObserving());
  EXPECT_TRUE(ctrl->power_status_observer_.IsObserving());

  UnlockScreen();

  EXPECT_FALSE(ctrl->user_activity_observer_.IsObserving());
  EXPECT_FALSE(ctrl->power_status_observer_.IsObserving());
}

}  // namespace ash
