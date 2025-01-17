// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/audio_input_host_impl.h"

#include "base/check.h"
#include "base/optional.h"
#include "chromeos/services/assistant/platform/audio_devices.h"
#include "chromeos/services/assistant/public/cpp/assistant_client.h"
#include "chromeos/services/assistant/public/cpp/features.h"

namespace chromeos {
namespace assistant {

namespace {

using MojomLidState = chromeos::libassistant::mojom::LidState;

MojomLidState ConvertLidState(chromeos::PowerManagerClient::LidState state) {
  switch (state) {
    case chromeos::PowerManagerClient::LidState::CLOSED:
      return MojomLidState::kClosed;
    case chromeos::PowerManagerClient::LidState::OPEN:
      return MojomLidState::kOpen;
    case chromeos::PowerManagerClient::LidState::NOT_PRESENT:
      // If there is no lid, it can't be closed.
      return MojomLidState::kOpen;
  }
}

}  // namespace

chromeos::assistant::AudioInputHostImpl::AudioInputHostImpl(
    mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
        pending_remote,
    CrasAudioHandler* cras_audio_handler,
    chromeos::PowerManagerClient* power_manager_client,
    const std::string& locale)
    : remote_(std::move(pending_remote)),
      power_manager_client_(power_manager_client),
      power_manager_client_observer_(this),
      audio_devices_(cras_audio_handler, locale) {
  DCHECK(power_manager_client_);

  audio_devices_observation_.Observe(&audio_devices_);
  power_manager_client_observer_.Observe(power_manager_client_);
  power_manager_client_->GetSwitchStates(
      base::BindOnce(&AudioInputHostImpl::OnInitialLidStateReceived,
                     weak_factory_.GetWeakPtr()));
}

AudioInputHostImpl::~AudioInputHostImpl() = default;

void AudioInputHostImpl::SetMicState(bool mic_open) {
  remote_->SetMicOpen(mic_open);
}

void AudioInputHostImpl::SetDeviceId(
    const base::Optional<std::string>& device_id) {
  remote_->SetDeviceId(device_id);
}

void AudioInputHostImpl::OnConversationTurnStarted() {
  remote_->OnConversationTurnStarted();
  // Inform power manager of a wake notification when Libassistant
  // recognized hotword and started a conversation. We intentionally
  // avoid using |NotifyUserActivity| because it is not suitable for
  // this case according to the Platform team.
  power_manager_client_->NotifyWakeNotification();
}

void AudioInputHostImpl::OnHotwordEnabled(bool enable) {
  remote_->SetHotwordEnabled(enable);
}

void AudioInputHostImpl::SetHotwordDeviceId(
    const base::Optional<std::string>& device_id) {
  remote_->SetHotwordDeviceId(device_id);
}

void AudioInputHostImpl::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks timestamp) {
  // Lid switch event still gets fired during system suspend, which enables
  // us to stop DSP recording correctly when user closes lid after the device
  // goes to sleep.
  remote_->SetLidState(ConvertLidState(state));
}

void AudioInputHostImpl::OnInitialLidStateReceived(
    base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
  if (switch_states.has_value())
    remote_->SetLidState(ConvertLidState(switch_states->lid_state));
}

}  // namespace assistant
}  // namespace chromeos
