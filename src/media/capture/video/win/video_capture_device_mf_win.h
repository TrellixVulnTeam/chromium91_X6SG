// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows specific implementation of VideoCaptureDevice.
// MediaFoundation is used for capturing. MediaFoundation provides its own
// threads for capturing.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_

#include <mfcaptureengine.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdint.h>
#include <strmif.h>
#include <wrl/client.h>

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/win/capability_list_win.h"
#include "media/capture/video/win/metrics.h"

interface IMFSourceReader;

namespace base {
class Location;
}  // namespace base

namespace media {

class MFVideoCallback;

class CAPTURE_EXPORT VideoCaptureDeviceMFWin : public VideoCaptureDevice {
 public:
  static bool GetPixelFormatFromMFSourceMediaSubtype(const GUID& guid,
                                                     bool use_hardware_format,
                                                     VideoPixelFormat* format);
  static VideoCaptureControlSupport GetControlSupport(
      Microsoft::WRL::ComPtr<IMFMediaSource> source);

  explicit VideoCaptureDeviceMFWin(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      Microsoft::WRL::ComPtr<IMFMediaSource> source,
      scoped_refptr<DXGIDeviceManager> dxgi_device_manager);
  explicit VideoCaptureDeviceMFWin(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      Microsoft::WRL::ComPtr<IMFMediaSource> source,
      scoped_refptr<DXGIDeviceManager> dxgi_device_manager,
      Microsoft::WRL::ComPtr<IMFCaptureEngine> engine);

  ~VideoCaptureDeviceMFWin() override;

  // Opens the device driver for this device.
  bool Init();

  // VideoCaptureDevice implementation.
  void AllocateAndStart(
      const VideoCaptureParams& params,
      std::unique_ptr<VideoCaptureDevice::Client> client) override;
  void StopAndDeAllocate() override;
  void TakePhoto(TakePhotoCallback callback) override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void OnUtilizationReport(int frame_feedback_id,
                           media::VideoCaptureFeedback feedback) override;

  // Captured new video data.
  void OnIncomingCapturedData(IMFMediaBuffer* buffer,
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp);
  void OnFrameDropped(VideoCaptureFrameDropReason reason);
  void OnEvent(IMFMediaEvent* media_event);

  using CreateMFPhotoCallbackCB =
      base::RepeatingCallback<scoped_refptr<IMFCaptureEngineOnSampleCallback>(
          VideoCaptureDevice::TakePhotoCallback callback,
          VideoCaptureFormat format)>;

  bool get_use_photo_stream_to_take_photo_for_testing() {
    return !photo_capabilities_.empty();
  }

  void set_create_mf_photo_callback_for_testing(CreateMFPhotoCallbackCB cb) {
    create_mf_photo_callback_ = cb;
  }

  void set_max_retry_count_for_testing(int max_retry_count) {
    max_retry_count_ = max_retry_count;
  }

  void set_retry_delay_in_ms_for_testing(int retry_delay_in_ms) {
    retry_delay_in_ms_ = retry_delay_in_ms;
  }

  void set_dxgi_device_manager_for_testing(
      scoped_refptr<DXGIDeviceManager> dxgi_device_manager) {
    dxgi_device_manager_ = std::move(dxgi_device_manager);
  }

  base::Optional<int> camera_rotation() const { return camera_rotation_; }

 private:
  HRESULT ExecuteHresultCallbackWithRetries(
      base::RepeatingCallback<HRESULT()> callback,
      MediaFoundationFunctionRequiringRetry which_function);
  HRESULT GetDeviceStreamCount(IMFCaptureSource* source, DWORD* count);
  HRESULT GetDeviceStreamCategory(
      IMFCaptureSource* source,
      DWORD stream_index,
      MF_CAPTURE_ENGINE_STREAM_CATEGORY* stream_category);
  HRESULT GetAvailableDeviceMediaType(IMFCaptureSource* source,
                                      DWORD stream_index,
                                      DWORD media_type_index,
                                      IMFMediaType** type);

  HRESULT FillCapabilities(IMFCaptureSource* source,
                           bool photo,
                           CapabilityList* capabilities);
  void OnError(VideoCaptureError error,
               const base::Location& from_here,
               HRESULT hr);
  void OnError(VideoCaptureError error,
               const base::Location& from_here,
               const char* message);
  void SendOnStartedIfNotYetSent();
  HRESULT WaitOnCaptureEvent(GUID capture_event_guid);
  HRESULT DeliverTextureToClient(ID3D11Texture2D* texture,
                                 base::TimeTicks reference_time,
                                 base::TimeDelta timestamp)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void OnIncomingCapturedDataInternal(
      IMFMediaBuffer* buffer,
      base::TimeTicks reference_time,
      base::TimeDelta timestamp,
      VideoCaptureFrameDropReason& frame_drop_reason);

  VideoFacingMode facing_mode_;
  CreateMFPhotoCallbackCB create_mf_photo_callback_;
  scoped_refptr<MFVideoCallback> video_callback_;
  bool is_initialized_;
  int max_retry_count_;
  int retry_delay_in_ms_;

  // Guards the below variables from concurrent access between methods running
  // on |sequence_checker_| and calls to OnIncomingCapturedData() and OnEvent()
  // made by MediaFoundation on threads outside of our control.
  base::Lock lock_;

  std::unique_ptr<VideoCaptureDevice::Client> client_;
  const Microsoft::WRL::ComPtr<IMFMediaSource> source_;
  Microsoft::WRL::ComPtr<IAMCameraControl> camera_control_;
  Microsoft::WRL::ComPtr<IAMVideoProcAmp> video_control_;
  Microsoft::WRL::ComPtr<IMFCaptureEngine> engine_;
  std::unique_ptr<CapabilityWin> selected_video_capability_;
  CapabilityList photo_capabilities_;
  std::unique_ptr<CapabilityWin> selected_photo_capability_;
  bool is_started_;
  bool has_sent_on_started_to_client_;
  // These flags keep the manual/auto mode between cycles of SetPhotoOptions().
  bool exposure_mode_manual_;
  bool focus_mode_manual_;
  bool white_balance_mode_manual_;
  base::queue<TakePhotoCallback> video_stream_take_photo_callbacks_;
  base::WaitableEvent capture_initialize_;
  base::WaitableEvent capture_error_;
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;
  base::Optional<int> camera_rotation_;

  media::VideoCaptureFeedback last_feedback_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceMFWin);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_MF_WIN_H_
