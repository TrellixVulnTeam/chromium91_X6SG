// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_screen_ozone.h"

#include "ui/base/linux/linux_desktop.h"
#include "ui/base/x/x11_idle_query.h"
#include "ui/base/x/x11_screensaver_window_finder.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display_finder.h"
#include "ui/display/util/display_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/x11/x11_topmost_window_finder.h"
#include "ui/platform_window/x11/x11_window.h"
#include "ui/platform_window/x11/x11_window_manager.h"

namespace ui {

X11ScreenOzone::X11ScreenOzone()
    : window_manager_(X11WindowManager::GetInstance()),
      x11_display_manager_(std::make_unique<XDisplayManager>(this)) {
  DCHECK(window_manager_);
}

X11ScreenOzone::~X11ScreenOzone() {
  if (x11_display_manager_->IsXrandrAvailable())
    x11::Connection::Get()->RemoveEventObserver(this);
}

void X11ScreenOzone::Init() {
  if (x11_display_manager_->IsXrandrAvailable())
    x11::Connection::Get()->AddEventObserver(this);
  x11_display_manager_->Init();
}

const std::vector<display::Display>& X11ScreenOzone::GetAllDisplays() const {
  return x11_display_manager_->displays();
}

display::Display X11ScreenOzone::GetPrimaryDisplay() const {
  return x11_display_manager_->GetPrimaryDisplay();
}

display::Display X11ScreenOzone::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  if (widget == gfx::kNullAcceleratedWidget)
    return GetPrimaryDisplay();

  X11Window* window = window_manager_->GetWindow(widget);
  return window ? GetDisplayMatching(window->GetBounds()) : GetPrimaryDisplay();
}

gfx::Point X11ScreenOzone::GetCursorScreenPoint() const {
  base::Optional<gfx::Point> point_in_pixels;
  if (ui::X11EventSource::HasInstance()) {
    point_in_pixels = ui::X11EventSource::GetInstance()
                          ->GetRootCursorLocationFromCurrentEvent();
  }
  if (!point_in_pixels) {
    // This call is expensive so we explicitly only call it when
    // |point_in_pixels| is not set. We note that base::Optional::value_or()
    // would cause it to be called regardless.
    point_in_pixels = GetCursorLocation();
  }
  // TODO(danakj): Should this be rounded? Or kept as a floating point?
  return gfx::ToFlooredPoint(
      gfx::ConvertPointToDips(*point_in_pixels, GetXDisplayScaleFactor()));
}

gfx::AcceleratedWidget X11ScreenOzone::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  X11TopmostWindowFinder finder;
  return static_cast<gfx::AcceleratedWidget>(finder.FindWindowAt(point));
}

gfx::AcceleratedWidget X11ScreenOzone::GetLocalProcessWidgetAtPoint(
    const gfx::Point& point,
    const std::set<gfx::AcceleratedWidget>& ignore) const {
  X11TopmostWindowFinder finder;
  return static_cast<gfx::AcceleratedWidget>(
      finder.FindLocalProcessWindowAt(point, ignore));
}

display::Display X11ScreenOzone::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  auto displays = GetAllDisplays();
  if (displays.size() <= 1)
    return GetPrimaryDisplay();
  return *display::FindDisplayNearestPoint(displays, point);
}

display::Display X11ScreenOzone::GetDisplayMatching(
    const gfx::Rect& match_rect_in_pixels) const {
  gfx::Rect match_rect = gfx::ToEnclosingRect(
      gfx::ConvertRectToDips(match_rect_in_pixels, GetXDisplayScaleFactor()));
  const display::Display* matching_display =
      display::FindDisplayWithBiggestIntersection(
          x11_display_manager_->displays(), match_rect);
  return matching_display ? *matching_display : GetPrimaryDisplay();
}

void X11ScreenOzone::SetScreenSaverSuspended(bool suspend) {
  SuspendX11ScreenSaver(suspend);
}

bool X11ScreenOzone::IsScreenSaverActive() const {
  // Usually the screensaver is used to lock the screen.
  return ScreensaverWindowFinder::ScreensaverWindowExists();
}

base::TimeDelta X11ScreenOzone::CalculateIdleTime() const {
  IdleQueryX11 idle_query;
  return base::TimeDelta::FromSeconds(idle_query.IdleTime());
}

void X11ScreenOzone::AddObserver(display::DisplayObserver* observer) {
  x11_display_manager_->AddObserver(observer);
}

void X11ScreenOzone::RemoveObserver(display::DisplayObserver* observer) {
  x11_display_manager_->RemoveObserver(observer);
}

std::string X11ScreenOzone::GetCurrentWorkspace() {
  return x11_display_manager_->GetCurrentWorkspace();
}

base::Value X11ScreenOzone::GetGpuExtraInfoAsListValue(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  auto result = GetDesktopEnvironmentInfoAsListValue();
  StoreGpuExtraInfoIntoListValue(gpu_extra_info.system_visual,
                                 gpu_extra_info.rgba_visual, result);
  StorePlatformNameIntoListValue(result, "x11");
  return result;
}

void X11ScreenOzone::SetDeviceScaleFactor(float scale) {
  if (device_scale_factor_ == scale)
    return;

  device_scale_factor_ = scale;
  x11_display_manager_->DispatchDelayedDisplayListUpdate();
}

void X11ScreenOzone::OnEvent(const x11::Event& xev) {
  x11_display_manager_->OnEvent(xev);
}

gfx::Point X11ScreenOzone::GetCursorLocation() const {
  return x11_display_manager_->GetCursorLocation();
}

void X11ScreenOzone::OnXDisplayListUpdated() {
  float scale_factor =
      x11_display_manager_->GetPrimaryDisplay().device_scale_factor();
  gfx::SetFontRenderParamsDeviceScaleFactor(scale_factor);
}

float X11ScreenOzone::GetXDisplayScaleFactor() const {
  return display::Display::HasForceDeviceScaleFactor()
             ? display::Display::GetForcedDeviceScaleFactor()
             : device_scale_factor_;
}

}  // namespace ui
