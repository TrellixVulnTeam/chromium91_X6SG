// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_disconnected_view.h"

#include <memory>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/interstitial_view_button.h"
#include "ash/system/phonehub/phone_hub_interstitial_view.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/ui_constants.h"
#include "chromeos/components/phonehub/connection_scheduler.h"
#include "chromeos/components/phonehub/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

using phone_hub_metrics::InterstitialScreenEvent;
using phone_hub_metrics::Screen;

PhoneDisconnectedView::PhoneDisconnectedView(
    chromeos::phonehub::ConnectionScheduler* connection_scheduler)
    : connection_scheduler_(connection_scheduler) {
  SetID(PhoneHubViewID::kDisconnectedView);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  content_view_ = AddChildView(std::make_unique<PhoneHubInterstitialView>(
      /*show_progress=*/false));

  // TODO(crbug.com/1127996): Replace PNG file with vector icon.
  gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PHONE_HUB_ERROR_STATE_IMAGE);
  content_view_->SetImage(*image);

  content_view_->SetTitle(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_PHONE_DISCONNECTED_DIALOG_TITLE));
  content_view_->SetDescription(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_PHONE_DISCONNECTED_DIALOG_DESCRIPTION));

  // Add "Learn more" and "Refresh" buttons.
  auto learn_more = std::make_unique<InterstitialViewButton>(
      base::BindRepeating(
          &PhoneDisconnectedView::ButtonPressed, base::Unretained(this),
          InterstitialScreenEvent::kLearnMore,
          base::BindRepeating(
              &NewWindowDelegate::NewTabWithUrl,
              base::Unretained(NewWindowDelegate::GetInstance()),
              GURL(chromeos::phonehub::kPhoneHubLearnMoreLink),
              /*from_user_interaction=*/true)),
      l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_PHONE_DISCONNECTED_DIALOG_LEARN_MORE_BUTTON),
      /*paint_background=*/false);
  learn_more->SetEnabledTextColors(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary));
  learn_more->SetID(PhoneHubViewID::kDisconnectedLearnMoreButton);
  content_view_->AddButton(std::move(learn_more));

  auto refresh = std::make_unique<InterstitialViewButton>(
      base::BindRepeating(
          &PhoneDisconnectedView::ButtonPressed, base::Unretained(this),
          InterstitialScreenEvent::kConfirm,
          base::BindRepeating(
              &chromeos::phonehub::ConnectionScheduler::ScheduleConnectionNow,
              base::Unretained(connection_scheduler_))),
      l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_PHONE_DISCONNECTED_DIALOG_REFRESH_BUTTON),
      /*paint_background=*/true);
  refresh->SetID(PhoneHubViewID::kDisconnectedRefreshButton);
  content_view_->AddButton(std::move(refresh));

  LogInterstitialScreenEvent(InterstitialScreenEvent::kShown);
}

PhoneDisconnectedView::~PhoneDisconnectedView() = default;

phone_hub_metrics::Screen PhoneDisconnectedView::GetScreenForMetrics() const {
  return Screen::kPhoneDisconnected;
}

void PhoneDisconnectedView::ButtonPressed(InterstitialScreenEvent event,
                                          base::RepeatingClosure callback) {
  LogInterstitialScreenEvent(event);
  std::move(callback).Run();
}

BEGIN_METADATA(PhoneDisconnectedView, views::View)
END_METADATA

}  // namespace ash
