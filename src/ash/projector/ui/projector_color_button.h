// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_UI_PROJECTOR_COLOR_BUTTON_H_
#define ASH_PROJECTOR_UI_PROJECTOR_COLOR_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/projector/ui/projector_button.h"

namespace ash {

// A view that shows a button with a color view which is used for start/stop
// recording buttons and color picker buttons.
class ASH_EXPORT ProjectorColorButton : public ProjectorButton {
 public:
  ProjectorColorButton(views::Button::PressedCallback callback,
                       SkColor color,
                       int size,
                       float radius);
  ProjectorColorButton(const ProjectorColorButton&) = delete;
  ProjectorColorButton& operator=(const ProjectorColorButton&) = delete;
  ~ProjectorColorButton() override = default;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_UI_PROJECTOR_COLOR_BUTTON_H_
