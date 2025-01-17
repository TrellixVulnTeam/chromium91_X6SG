// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/onboarding_view.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ash/system/phonehub/phone_hub_ui_controller.h"
#include "ash/system/phonehub/phone_status_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/scoped_observation.h"

namespace chromeos {
namespace phonehub {
class PhoneHubManager;
}  // namespace phonehub
}  // namespace chromeos

namespace views {
class ImageView;
}

namespace ash {

class PhoneHubContentView;
class TrayBubbleWrapper;

// This class represents the Phone Hub tray button in the status area and
// controls the bubble that is shown when the tray button is clicked.
class ASH_EXPORT PhoneHubTray : public TrayBackgroundView,
                                public OnboardingView::Delegate,
                                public PhoneStatusView::Delegate,
                                public PhoneHubUiController::Observer {
 public:
  explicit PhoneHubTray(Shelf* shelf);
  PhoneHubTray(const PhoneHubTray&) = delete;
  ~PhoneHubTray() override;
  PhoneHubTray& operator=(const PhoneHubTray&) = delete;

  // Sets the PhoneHubManager that provides the data to drive the UI.
  void SetPhoneHubManager(
      chromeos::phonehub::PhoneHubManager* phone_hub_manager);

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void AnchorUpdated() override;
  void Initialize() override;
  void CloseBubble() override;
  void ShowBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  const char* GetClassName() const override;

  // PhoneStatusView::Delegate:
  bool CanOpenConnectedDeviceSettings() override;
  void OpenConnectedDevicesSettings() override;

  // OnboardingView::Delegate:
  void HideStatusHeaderView() override;

  views::View* content_view_for_testing() { return content_view_; }

  PhoneHubUiController* ui_controller_for_testing() {
    return ui_controller_.get();
  }

 private:
  // TrayBubbleView::Delegate:
  std::u16string GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // PhoneHubUiController::Observer:
  void OnPhoneHubUiStateChanged() override;

  // Updates the visibility of the tray in the shelf based on the feature is
  // enabled.
  void UpdateVisibility();

  // Icon of the tray. Unowned.
  views::ImageView* icon_;

  // Controls the main content view displayed in the bubble based on the current
  // PhoneHub state.
  std::unique_ptr<PhoneHubUiController> ui_controller_;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  // The header status view on top of the bubble.
  views::View* phone_status_view_ = nullptr;

  // The main content view of the bubble, which changes depending on the state.
  // Unowned.
  PhoneHubContentView* content_view_ = nullptr;

  base::ScopedObservation<PhoneHubUiController, PhoneHubUiController::Observer>
      observed_phone_hub_ui_controller_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_TRAY_H_
