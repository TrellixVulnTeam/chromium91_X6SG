// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_RENDERER_PREFERENCES_RENDERER_PREFERENCES_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_RENDERER_PREFERENCES_RENDERER_PREFERENCES_MOJOM_TRAITS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "build/build_config.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/user_agent/user_agent_mojom_traits.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom-shared.h"
#include "ui/gfx/mojom/font_render_params_mojom_traits.h"

#if defined(OS_WIN)
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#endif

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::RendererPreferencesDataView,
                 ::blink::RendererPreferences> {
  static const bool& can_accept_load_drops(
      const ::blink::RendererPreferences& data) {
    return data.can_accept_load_drops;
  }

  static const bool& should_antialias_text(
      const ::blink::RendererPreferences& data) {
    return data.should_antialias_text;
  }

  static const gfx::FontRenderParams::Hinting& hinting(
      const ::blink::RendererPreferences& data) {
    return data.hinting;
  }

  static const bool& use_autohinter(const ::blink::RendererPreferences& data) {
    return data.use_autohinter;
  }

  static const bool& use_bitmaps(const ::blink::RendererPreferences& data) {
    return data.use_bitmaps;
  }

  static const gfx::FontRenderParams::SubpixelRendering& subpixel_rendering(
      const ::blink::RendererPreferences& data) {
    return data.subpixel_rendering;
  }

  static const bool& use_subpixel_positioning(
      const ::blink::RendererPreferences& data) {
    return data.use_subpixel_positioning;
  }

  static const uint32_t& focus_ring_color(
      const ::blink::RendererPreferences& data) {
    return data.focus_ring_color;
  }

  static const uint32_t& active_selection_bg_color(
      const ::blink::RendererPreferences& data) {
    return data.active_selection_bg_color;
  }

  static const uint32_t& active_selection_fg_color(
      const ::blink::RendererPreferences& data) {
    return data.active_selection_fg_color;
  }

  static const uint32_t& inactive_selection_bg_color(
      const ::blink::RendererPreferences& data) {
    return data.inactive_selection_bg_color;
  }

  static const uint32_t& inactive_selection_fg_color(
      const ::blink::RendererPreferences& data) {
    return data.inactive_selection_fg_color;
  }

  static const bool& browser_handles_all_top_level_requests(
      const ::blink::RendererPreferences& data) {
    return data.browser_handles_all_top_level_requests;
  }

  static base::Optional<base::TimeDelta> caret_blink_interval(
      const ::blink::RendererPreferences& data) {
    return data.caret_blink_interval;
  }

  static const bool& use_custom_colors(
      const ::blink::RendererPreferences& data) {
    return data.use_custom_colors;
  }

  static const bool& enable_referrers(
      const ::blink::RendererPreferences& data) {
    return data.enable_referrers;
  }

  static const bool& allow_cross_origin_auth_prompt(
      const ::blink::RendererPreferences& data) {
    return data.allow_cross_origin_auth_prompt;
  }

  static const bool& enable_do_not_track(
      const ::blink::RendererPreferences& data) {
    return data.enable_do_not_track;
  }

  static const bool& enable_encrypted_media(
      const ::blink::RendererPreferences& data) {
    return data.enable_encrypted_media;
  }

  static const std::string& webrtc_ip_handling_policy(
      const ::blink::RendererPreferences& data) {
    return data.webrtc_ip_handling_policy;
  }

  static const uint16_t& webrtc_udp_min_port(
      const ::blink::RendererPreferences& data) {
    return data.webrtc_udp_min_port;
  }

  static const uint16_t& webrtc_udp_max_port(
      const ::blink::RendererPreferences& data) {
    return data.webrtc_udp_max_port;
  }

  static const std::vector<std::string>& webrtc_local_ips_allowed_urls(
      const ::blink::RendererPreferences& data) {
    return data.webrtc_local_ips_allowed_urls;
  }

  static const bool& webrtc_allow_legacy_tls_protocols(
      const ::blink::RendererPreferences& data) {
    return data.webrtc_allow_legacy_tls_protocols;
  }

  static const ::blink::UserAgentOverride& user_agent_override(
      const ::blink::RendererPreferences& data) {
    return data.user_agent_override;
  }

  static const std::string& accept_languages(
      const ::blink::RendererPreferences& data) {
    return data.accept_languages;
  }

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  static const std::string& system_font_family_name(
      const ::blink::RendererPreferences& data) {
    return data.system_font_family_name;
  }
#endif

  ///@name USE_NEVA_APPRUNTIME
  ///@{
  static const std::string& application_id(
      const ::blink::RendererPreferences& data) {
    return data.application_id;
  }
  static const std::string& display_id(
      const ::blink::RendererPreferences& data) {
    return data.display_id;
  }
  static const std::string& media_codec_capability(
      const ::blink::RendererPreferences& data) {
    return data.media_codec_capability;
  }
  static const std::string& media_preferences(
      const ::blink::RendererPreferences& data) {
    return data.media_preferences;
  }
  static const std::string& board_type(
      const ::blink::RendererPreferences& data) {
    return data.board_type;
  }
  static const bool& allow_fake_bold_text(
      const ::blink::RendererPreferences& data) {
    return data.allow_fake_bold_text;
  }
  static const std::string& file_security_origin(
      const ::blink::RendererPreferences& data) {
    return data.file_security_origin;
  }
  static const bool& use_unlimited_media_policy(
      const ::blink::RendererPreferences& data) {
    return data.use_unlimited_media_policy;
  }
  static const bool& is_enact_browser(
      const ::blink::RendererPreferences& data) {
    return data.is_enact_browser;
  }
  ///@}

#if defined(USE_X11) || defined(USE_OZONE)
  static const bool& selection_clipboard_buffer_available(
      const ::blink::RendererPreferences& data) {
    return data.selection_clipboard_buffer_available;
  }
#endif

#if defined(OS_WIN)
  static const std::u16string& caption_font_family_name(
      const ::blink::RendererPreferences& data) {
    return data.caption_font_family_name;
  }
  static const int32_t& caption_font_height(
      const ::blink::RendererPreferences& data) {
    return data.caption_font_height;
  }
  static const std::u16string& small_caption_font_family_name(
      const ::blink::RendererPreferences& data) {
    return data.small_caption_font_family_name;
  }
  static const int32_t& small_caption_font_height(
      const ::blink::RendererPreferences& data) {
    return data.small_caption_font_height;
  }
  static const std::u16string& menu_font_family_name(
      const ::blink::RendererPreferences& data) {
    return data.menu_font_family_name;
  }
  static const int32_t& menu_font_height(
      const ::blink::RendererPreferences& data) {
    return data.menu_font_height;
  }
  static const std::u16string& status_font_family_name(
      const ::blink::RendererPreferences& data) {
    return data.status_font_family_name;
  }
  static const int32_t& status_font_height(
      const ::blink::RendererPreferences& data) {
    return data.status_font_height;
  }
  static const std::u16string& message_font_family_name(
      const ::blink::RendererPreferences& data) {
    return data.message_font_family_name;
  }
  static const int32_t& message_font_height(
      const ::blink::RendererPreferences& data) {
    return data.message_font_height;
  }
  static const int32_t& vertical_scroll_bar_width_in_dips(
      const ::blink::RendererPreferences& data) {
    return data.vertical_scroll_bar_width_in_dips;
  }
  static const int32_t& horizontal_scroll_bar_height_in_dips(
      const ::blink::RendererPreferences& data) {
    return data.horizontal_scroll_bar_height_in_dips;
  }
  static const int32_t& arrow_bitmap_height_vertical_scroll_bar_in_dips(
      const ::blink::RendererPreferences& data) {
    return data.arrow_bitmap_height_vertical_scroll_bar_in_dips;
  }
  static const int32_t& arrow_bitmap_width_horizontal_scroll_bar_in_dips(
      const ::blink::RendererPreferences& data) {
    return data.arrow_bitmap_width_horizontal_scroll_bar_in_dips;
  }
#endif

  static const bool& plugin_fullscreen_allowed(
      const ::blink::RendererPreferences& data) {
    return data.plugin_fullscreen_allowed;
  }

  static const bool& caret_browsing_enabled(
      const ::blink::RendererPreferences& data) {
    return data.caret_browsing_enabled;
  }

  static const std::vector<uint16_t>& explicitly_allowed_network_ports(
      const ::blink::RendererPreferences& data) {
    return data.explicitly_allowed_network_ports;
  }

  static bool Read(blink::mojom::RendererPreferencesDataView,
                   ::blink::RendererPreferences* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_RENDERER_PREFERENCES_RENDERER_PREFERENCES_MOJOM_TRAITS_H_
