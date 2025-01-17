// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_detailed_view.h"

#include "ash/components/audio/cras_audio_handler.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/audio/mic_gain_slider_controller.h"
#include "ash/system/audio/mic_gain_slider_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"

namespace ash {
namespace {

std::u16string GetAudioDeviceName(const AudioDevice& device) {
  switch (device.type) {
    case AudioDeviceType::kFrontMic:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_FRONT_MIC);
    case AudioDeviceType::kHeadphone:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_HEADPHONE);
    case AudioDeviceType::kInternalSpeaker:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INTERNAL_SPEAKER);
    case AudioDeviceType::kInternalMic:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_INTERNAL_MIC);
    case AudioDeviceType::kRearMic:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_REAR_MIC);
    case AudioDeviceType::kUsb:
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_AUDIO_USB_DEVICE,
                                        base::UTF8ToUTF16(device.display_name));
    case AudioDeviceType::kBluetooth:
      FALLTHROUGH;
    case AudioDeviceType::kBluetoothNbMic:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_BLUETOOTH_DEVICE,
          base::UTF8ToUTF16(device.display_name));
    case AudioDeviceType::kHdmi:
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_AUDIO_HDMI_DEVICE,
                                        base::UTF8ToUTF16(device.display_name));
    case AudioDeviceType::kMic:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_MIC_JACK_DEVICE);
    default:
      return base::UTF8ToUTF16(device.display_name);
  }
}

}  // namespace

namespace tray {

AudioDetailedView::AudioDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  CreateItems();
  Update();
}

AudioDetailedView::~AudioDetailedView() = default;

void AudioDetailedView::Update() {
  UpdateAudioDevices();
  Layout();
}

const char* AudioDetailedView::GetClassName() const {
  return "AudioDetailedView";
}

void AudioDetailedView::AddAudioSubHeader(const gfx::VectorIcon& icon,
                                          int text_id) {
  TriView* header = AddScrollListSubHeader(icon, text_id);
  header->SetContainerVisible(TriView::Container::END, false);
}

void AudioDetailedView::CreateItems() {
  CreateScrollableList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_AUDIO);
  mic_gain_controller_ = std::make_unique<MicGainSliderController>();
}

void AudioDetailedView::UpdateAudioDevices() {
  output_devices_.clear();
  input_devices_.clear();
  AudioDeviceList devices;
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  audio_handler->GetAudioDevices(&devices);
  bool has_dual_internal_mic = audio_handler->HasDualInternalMic();
  bool is_front_or_rear_mic_active = false;
  for (const auto& device : devices) {
    // Don't display keyboard mic or aokr type.
    if (!device.is_for_simple_usage())
      continue;
    if (device.is_input) {
      // Do not expose the internal front and rear mic to UI.
      if (has_dual_internal_mic && audio_handler->IsFrontOrRearMic(device)) {
        if (device.active)
          is_front_or_rear_mic_active = true;
        continue;
      }
      input_devices_.push_back(device);
    } else {
      output_devices_.push_back(device);
    }
  }

  // Expose the dual internal mics as one device (internal mic) to user.
  if (has_dual_internal_mic) {
    // Create stub internal mic entry for UI rendering, which representing
    // both internal front and rear mics.
    AudioDevice internal_mic;
    internal_mic.is_input = true;
    internal_mic.stable_device_id_version = 2;
    internal_mic.type = AudioDeviceType::kInternalMic;
    internal_mic.active = is_front_or_rear_mic_active;
    input_devices_.push_back(internal_mic);
  }

  UpdateScrollableList();
}

void AudioDetailedView::UpdateScrollableList() {
  scroll_content()->RemoveAllChildViews(true);
  device_map_.clear();

  // Add audio output devices.
  const bool has_output_devices = output_devices_.size() > 0;
  if (has_output_devices) {
    AddAudioSubHeader(kSystemMenuAudioOutputIcon,
                      IDS_ASH_STATUS_TRAY_AUDIO_OUTPUT);
  }

  for (const auto& device : output_devices_) {
    HoverHighlightView* container =
        AddScrollListCheckableItem(GetAudioDeviceName(device), device.active);
    device_map_[container] = device;
  }

  if (has_output_devices) {
    scroll_content()->AddChildView(CreateListSubHeaderSeparator());
  }

  // Add audio input devices.
  const bool has_input_devices = input_devices_.size() > 0;
  if (has_input_devices) {
    AddAudioSubHeader(kSystemMenuAudioInputIcon,
                      IDS_ASH_STATUS_TRAY_AUDIO_INPUT);
  }

  for (const auto& device : input_devices_) {
    HoverHighlightView* container =
        AddScrollListCheckableItem(GetAudioDeviceName(device), device.active);
    device_map_[container] = device;

    AddScrollListChild(mic_gain_controller_->CreateMicGainSlider(
        device.id, device.IsInternalMic()));
  }

  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
}

void AudioDetailedView::HandleViewClicked(views::View* view) {
  AudioDeviceMap::iterator iter = device_map_.find(view);
  if (iter == device_map_.end())
    return;
  AudioDevice device = iter->second;
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  if (device.type == AudioDeviceType::kInternalMic &&
      audio_handler->HasDualInternalMic()) {
    audio_handler->SwitchToFrontOrRearMic();
  } else {
    audio_handler->SwitchToDevice(device, true,
                                  CrasAudioHandler::ACTIVATE_BY_USER);
  }
}

}  // namespace tray
}  // namespace ash
