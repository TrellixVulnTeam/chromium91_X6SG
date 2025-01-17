// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/render_pass_internal.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"

#include <stddef.h>

#include "components/viz/common/frame_sinks/copy_output_request.h"

namespace viz {
namespace {
const size_t kDefaultNumSharedQuadStatesToReserve = 32;
const size_t kDefaultNumQuadsToReserve = 128;
}  // namespace

RenderPassInternal::RenderPassInternal()
    : RenderPassInternal(kDefaultNumSharedQuadStatesToReserve,
                         kDefaultNumQuadsToReserve) {}
// Each layer usually produces one shared quad state, so the number of layers
// is a good hint for what to reserve here.
RenderPassInternal::RenderPassInternal(size_t num_layers)
    : RenderPassInternal(num_layers, kDefaultNumQuadsToReserve) {}
RenderPassInternal::RenderPassInternal(size_t shared_quad_state_list_size,
                                       size_t quad_list_size)
    : quad_list(quad_list_size),
      shared_quad_state_list(alignof(SharedQuadState),
                             sizeof(SharedQuadState),
                             shared_quad_state_list_size) {}

RenderPassInternal::~RenderPassInternal() = default;

SharedQuadState* RenderPassInternal::CreateAndAppendSharedQuadState() {
  return shared_quad_state_list.AllocateAndConstruct<SharedQuadState>();
}

void RenderPassInternal::ReplaceExistingQuadWithSolidColor(
    QuadList::Iterator at,
    SkColor color,
    SkBlendMode blend_mode) {
  const SharedQuadState* shared_quad_state = at->shared_quad_state;
  if (shared_quad_state->are_contents_opaque ||
      shared_quad_state->blend_mode != blend_mode) {
    auto* new_shared_quad_state =
        shared_quad_state_list.AllocateAndCopyFrom(shared_quad_state);
    new_shared_quad_state->are_contents_opaque = false;
    new_shared_quad_state->blend_mode = blend_mode;
    shared_quad_state = new_shared_quad_state;
  }

  const gfx::Rect rect = at->rect;
#if defined(USE_NEVA_MEDIA)
  auto* replacement = quad_list.ReplaceExistingElement<SolidColorDrawQuad>(at);
  replacement->SetAll(
      shared_quad_state, rect, /*visible_rect=*/rect,
      /*needs_blending=*/false, color,
      /*force_anti_aliasing_off=*/true);
  replacement->SetForceDrawTransparentColor(true);
#else
  quad_list.ReplaceExistingElement<SolidColorDrawQuad>(at)->SetAll(
    shared_quad_state, rect, /*visible_rect=*/rect,
    /*needs_blending=*/false, color,
    /*force_anti_aliasing_off=*/true);
#endif
}

}  // namespace viz
