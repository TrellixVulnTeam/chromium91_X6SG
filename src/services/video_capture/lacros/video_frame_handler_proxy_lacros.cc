// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/lacros/video_frame_handler_proxy_lacros.h"

#include <memory>
#include <utility>

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/mojom/buffer_types.mojom.h"
#include "ui/gfx/mojom/native_handle_types.mojom.h"

namespace video_capture {

namespace {

mojom::ReadyFrameInBufferPtr ToVideoCaptureBuffer(
    crosapi::mojom::ReadyFrameInBufferPtr buffer) {
  auto video_capture_buffer = mojom::ReadyFrameInBuffer::New();
  video_capture_buffer->buffer_id = buffer->buffer_id;
  video_capture_buffer->frame_feedback_id = buffer->frame_feedback_id;

  mojo::PendingRemote<mojom::ScopedAccessPermission> access_permission;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<VideoFrameHandlerProxyLacros::AccessPermissionProxy>(
          std::move(buffer->access_permission)),
      access_permission.InitWithNewPipeAndPassReceiver());
  video_capture_buffer->access_permission = std::move(access_permission);

  const auto& buffer_info = buffer->frame_info;
  auto video_capture_buffer_info = media::mojom::VideoFrameInfo::New();
  video_capture_buffer_info->timestamp = buffer_info->timestamp;
  video_capture_buffer_info->pixel_format = buffer_info->pixel_format;
  video_capture_buffer_info->coded_size = buffer_info->coded_size;
  video_capture_buffer_info->visible_rect = buffer_info->visible_rect;

  media::VideoFrameMetadata media_frame_metadata;
  switch (buffer_info->rotation) {
    case crosapi::mojom::VideoRotation::kVideoRotation0:
      media_frame_metadata.transformation =
          media::VideoTransformation(media::VideoRotation::VIDEO_ROTATION_0);
      break;
    case crosapi::mojom::VideoRotation::kVideoRotation90:
      media_frame_metadata.transformation =
          media::VideoTransformation(media::VideoRotation::VIDEO_ROTATION_90);
      break;
    case crosapi::mojom::VideoRotation::kVideoRotation180:
      media_frame_metadata.transformation =
          media::VideoTransformation(media::VideoRotation::VIDEO_ROTATION_180);
      break;
    case crosapi::mojom::VideoRotation::kVideoRotation270:
      media_frame_metadata.transformation =
          media::VideoTransformation(media::VideoRotation::VIDEO_ROTATION_270);
      break;
    default:
      NOTREACHED() << "Unexpected rotation in video frame metadata";
  }
  media_frame_metadata.reference_time = buffer_info->reference_time;

  video_capture_buffer_info->metadata = std::move(media_frame_metadata);
  video_capture_buffer->frame_info = std::move(video_capture_buffer_info);

  return video_capture_buffer;
}

gfx::GpuMemoryBufferHandle ToGfxGpuMemoryBufferHandle(
    crosapi::mojom::GpuMemoryBufferHandlePtr buffer_handle) {
  gfx::GpuMemoryBufferHandle gfx_buffer_handle;
  gfx_buffer_handle.id = gfx::GpuMemoryBufferId(buffer_handle->id);
  gfx_buffer_handle.offset = buffer_handle->offset;
  gfx_buffer_handle.stride = buffer_handle->stride;

  if (buffer_handle->platform_handle) {
    auto& platform_handle = buffer_handle->platform_handle;
    if (platform_handle->is_shared_memory_handle()) {
      gfx_buffer_handle.region =
          std::move(platform_handle->get_shared_memory_handle());
    } else if (platform_handle->is_native_pixmap_handle()) {
      auto& native_pixmap_handle = platform_handle->get_native_pixmap_handle();
      gfx::NativePixmapHandle gfx_native_pixmap_handle;
      gfx_native_pixmap_handle.planes = std::move(native_pixmap_handle->planes);
      gfx_native_pixmap_handle.modifier = native_pixmap_handle->modifier;
      gfx_buffer_handle.native_pixmap_handle =
          std::move(gfx_native_pixmap_handle);
    }
  }
  return gfx_buffer_handle;
}

}  // namespace

VideoFrameHandlerProxyLacros::VideoFrameHandlerProxyLacros(
    mojo::PendingReceiver<crosapi::mojom::VideoFrameHandler> proxy_receiver,
    mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote)
    : handler_(std::move(handler_remote)) {
  receiver_.Bind(std::move(proxy_receiver));
}

VideoFrameHandlerProxyLacros::~VideoFrameHandlerProxyLacros() = default;

VideoFrameHandlerProxyLacros::AccessPermissionProxy::AccessPermissionProxy(
    mojo::PendingRemote<crosapi::mojom::ScopedAccessPermission> remote)
    : remote_(std::move(remote)) {}

VideoFrameHandlerProxyLacros::AccessPermissionProxy::~AccessPermissionProxy() =
    default;

void VideoFrameHandlerProxyLacros::OnNewBuffer(
    int buffer_id,
    crosapi::mojom::VideoBufferHandlePtr buffer_handle) {
  media::mojom::VideoBufferHandlePtr media_handle =
      media::mojom::VideoBufferHandle::New();

  if (buffer_handle->is_shared_buffer_handle()) {
    media_handle->set_shared_buffer_handle(
        buffer_handle->get_shared_buffer_handle()->Clone(
            mojo::SharedBufferHandle::AccessMode::READ_WRITE));
  } else if (buffer_handle->is_gpu_memory_buffer_handle()) {
    media_handle->set_gpu_memory_buffer_handle(ToGfxGpuMemoryBufferHandle(
        std::move(buffer_handle->get_gpu_memory_buffer_handle())));
  } else {
    NOTREACHED() << "Unexpected new buffer type";
  }
  handler_->OnNewBuffer(buffer_id, std::move(media_handle));
}

void VideoFrameHandlerProxyLacros::OnFrameReadyInBuffer(
    crosapi::mojom::ReadyFrameInBufferPtr buffer,
    std::vector<crosapi::mojom::ReadyFrameInBufferPtr> scaled_buffers) {
  mojom::ReadyFrameInBufferPtr video_capture_buffer =
      ToVideoCaptureBuffer(std::move(buffer));
  std::vector<mojom::ReadyFrameInBufferPtr> video_capture_scaled_buffers;
  for (auto& b : scaled_buffers)
    video_capture_scaled_buffers.push_back(ToVideoCaptureBuffer(std::move(b)));

  handler_->OnFrameReadyInBuffer(std::move(video_capture_buffer),
                                 std::move(video_capture_scaled_buffers));
}

void VideoFrameHandlerProxyLacros::OnBufferRetired(int buffer_id) {
  handler_->OnBufferRetired(buffer_id);
}

void VideoFrameHandlerProxyLacros::OnError(media::VideoCaptureError error) {
  handler_->OnError(error);
}

void VideoFrameHandlerProxyLacros::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  handler_->OnFrameDropped(reason);
}

void VideoFrameHandlerProxyLacros::OnLog(const std::string& message) {
  handler_->OnLog(message);
}

void VideoFrameHandlerProxyLacros::OnStarted() {
  handler_->OnStarted();
}

void VideoFrameHandlerProxyLacros::OnStartedUsingGpuDecode() {
  handler_->OnStartedUsingGpuDecode();
}

void VideoFrameHandlerProxyLacros::OnStopped() {
  handler_->OnStopped();
}

}  // namespace video_capture
