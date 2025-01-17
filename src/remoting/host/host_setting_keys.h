// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_SETTING_KEYS_H_
#define REMOTING_HOST_HOST_SETTING_KEYS_H_

#include "remoting/host/host_settings.h"

namespace remoting {

// If setting is provided, the Mac host will capture audio from the audio device
// specified by the UID and stream it to the client. See AudioCapturerMac for
// more information.
constexpr HostSettingKey kMacAudioCaptureDeviceUid = "audio_capture_device_uid";

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_SETTING_KEYS_H_
