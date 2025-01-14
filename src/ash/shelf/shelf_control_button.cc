// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_control_button.h"

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class ShelfControlButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  ShelfControlButtonHighlightPathGenerator() = default;

  ShelfControlButtonHighlightPathGenerator(
      const ShelfControlButtonHighlightPathGenerator&) = delete;
  ShelfControlButtonHighlightPathGenerator& operator=(
      const ShelfControlButtonHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  base::Optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    auto* shelf_config = ShelfConfig::Get();
    // Some control buttons have a slightly larger size to fill the shelf and
    // maximize the click target, but we still want their "visual" size to be
    // the same.
    gfx::RectF visual_bounds = rect;
    visual_bounds.ClampToCenteredSize(
        gfx::SizeF(shelf_config->control_size(), shelf_config->control_size()));
    if (Shell::Get()->IsInTabletMode() && shelf_config->is_in_app()) {
      visual_bounds.Inset(0,
                          shelf_config->in_app_control_button_height_inset());
    }
    return gfx::RRectF(visual_bounds, shelf_config->control_border_radius());
  }
};

}  // namespace

ShelfControlButton::ShelfControlButton(
    Shelf* shelf,
    ShelfButtonDelegate* shelf_button_delegate)
    : ShelfButton(shelf, shelf_button_delegate) {
  SetHasInkDropActionOnClick(true);
  SetInstallFocusRingOnFocus(true);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<ShelfControlButtonHighlightPathGenerator>());
  focus_ring()->SetColor(ShelfConfig::Get()->shelf_focus_border_color());
  SetFocusPainter(nullptr);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

ShelfControlButton::~ShelfControlButton() = default;

gfx::Point ShelfControlButton::GetCenterPoint() const {
  return GetLocalBounds().CenterPoint();
}

const char* ShelfControlButton::GetClassName() const {
  return "ash/ShelfControlButton";
}

gfx::Size ShelfControlButton::CalculatePreferredSize() const {
  return gfx::Size(ShelfConfig::Get()->control_size(),
                   ShelfConfig::Get()->control_size());
}

void ShelfControlButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ShelfButton::GetAccessibleNodeData(node_data);
  node_data->SetName(GetAccessibleName());
}

void ShelfControlButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintBackground(canvas, GetContentsBounds());
}

void ShelfControlButton::PaintBackground(gfx::Canvas* canvas,
                                         const gfx::Rect& bounds) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(ShelfConfig::Get()->GetShelfControlButtonColor());
  canvas->DrawRoundRect(bounds, ShelfConfig::Get()->control_border_radius(),
                        flags);
}

}  // namespace ash
