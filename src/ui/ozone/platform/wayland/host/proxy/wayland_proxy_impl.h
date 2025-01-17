// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_PROXY_WAYLAND_PROXY_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_PROXY_WAYLAND_PROXY_IMPL_H_

#include <vector>

#include "ui/ozone/platform/wayland/host/proxy/wayland_proxy.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

namespace ui {
class WaylandConnection;
class WaylandShmBuffer;
}  // namespace ui

namespace wl {

class WaylandProxyImpl : public WaylandProxy, public ui::WaylandWindowObserver {
 public:
  explicit WaylandProxyImpl(ui::WaylandConnection* connection);
  ~WaylandProxyImpl() override;

  // WaylandProxy overrides:
  void SetDelegate(WaylandProxy::Delegate* delegate) override;
  wl_display* GetDisplay() override;
  wl_surface* GetWlSurfaceForAcceleratedWidget(
      gfx::AcceleratedWidget widget) override;
  wl_buffer* CreateShmBasedWlBuffer(const gfx::Size& buffer_size) override;
  void DestroyShmForWlBuffer(wl_buffer* buffer) override;
  void ScheduleDisplayFlush() override;
  ui::PlatformWindowType GetWindowType(gfx::AcceleratedWidget widget) override;
  gfx::Rect GetWindowBounds(gfx::AcceleratedWidget widget) override;
  bool WindowHasPointerFocus(gfx::AcceleratedWidget widget) override;
  bool WindowHasKeyboardFocus(gfx::AcceleratedWidget widget) override;

 private:
  // ui::WaylandWindowObserver overrides:
  void OnWindowAdded(ui::WaylandWindow* window) override;
  void OnWindowRemoved(ui::WaylandWindow* window) override;
  void OnWindowConfigured(ui::WaylandWindow* window) override;

  ui::WaylandConnection* const connection_;

  WaylandProxy::Delegate* delegate_ = nullptr;

  std::vector<ui::WaylandShmBuffer> shm_buffers_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_PROXY_WAYLAND_PROXY_IMPL_H_
