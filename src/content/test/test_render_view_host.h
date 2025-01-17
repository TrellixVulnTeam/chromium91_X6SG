// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_VIEW_HOST_H_
#define CONTENT_TEST_TEST_RENDER_VIEW_HOST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/layout.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if defined(OS_MAC)
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#endif

// This file provides a testing framework for mocking out the RenderProcessHost
// layer. It allows you to test RenderViewHost, WebContentsImpl,
// NavigationController, and other layers above that without running an actual
// renderer process.
//
// To use, derive your test base class from RenderViewHostImplTestHarness.

namespace gfx {
class Rect;
}

namespace content {

class FrameTree;
class SiteInstance;
class TestRenderFrameHost;
class TestWebContents;

// TestRenderWidgetHostView ----------------------------------------------------

// Subclass the RenderViewHost's view so that we can call Show(), etc.,
// without having side-effects.
class TestRenderWidgetHostView : public RenderWidgetHostViewBase,
                                 public viz::HostFrameSinkClient {
 public:
  explicit TestRenderWidgetHostView(RenderWidgetHost* rwh);
  ~TestRenderWidgetHostView() override;

  // RenderWidgetHostView:
  void InitAsChild(gfx::NativeView parent_view) override {}
  void SetSize(const gfx::Size& size) override {}
  void SetBounds(const gfx::Rect& rect) override {}
  gfx::NativeView GetNativeView() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  ui::TextInputClient* GetTextInputClient() override;
  bool HasFocus() override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  void WasUnOccluded() override;
  void WasOccluded() override;
  gfx::Rect GetViewBounds() override;
#if defined(OS_MAC)
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override {}
  void SpeakSelection() override;
  void SetWindowFrameInScreen(const gfx::Rect& rect) override;
  void ShowSharePicker(
      const std::string& title,
      const std::string& text,
      const std::string& url,
      const std::vector<std::string>& file_paths,
      blink::mojom::ShareService::ShareCallback callback) override;
#endif  // defined(OS_MAC)

  // Advances the fallback surface to the first surface after navigation. This
  // ensures that stale surfaces are not presented to the user for an indefinite
  // period of time.
  void ResetFallbackToFirstNavigationSurface() override {}

  void TakeFallbackContentFrom(RenderWidgetHostView* view) override;
  void EnsureSurfaceSynchronizedForWebTest() override;

  // RenderWidgetHostViewBase:
  uint32_t GetCaptureSequenceNumber() const override;
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& bounds) override {}
  void Focus() override {}
  void SetIsLoading(bool is_loading) override {}
  void UpdateCursor(const WebCursor& cursor) override;
  void RenderProcessGone() override;
  void Destroy() override;
  void SetTooltipText(const std::u16string& tooltip_text) override {}
  gfx::Rect GetBoundsInRootWindow() override;
  blink::mojom::PointerLockResult LockMouse(bool) override;
  blink::mojom::PointerLockResult ChangeMouseLock(bool) override;
  void UnlockMouse() override;
  const viz::FrameSinkId& GetFrameSinkId() const override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() const override;
  viz::SurfaceId GetCurrentSurfaceId() const override;
  std::unique_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;

  bool is_showing() const { return is_showing_; }
  bool is_occluded() const { return is_occluded_; }

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;

  const WebCursor& last_cursor() const { return last_cursor_; }

 protected:
  // RenderWidgetHostViewBase:
  void UpdateBackgroundColor() override;
  base::Optional<DisplayFeature> GetDisplayFeature() override;
  void SetDisplayFeatureForTesting(
      const DisplayFeature* display_feature) override;

  viz::FrameSinkId frame_sink_id_;

 private:
  bool is_showing_;
  bool is_occluded_;
  ui::DummyTextInputClient text_input_client_;
  WebCursor last_cursor_;

  // Latest capture sequence number which is incremented when the caller
  // requests surfaces be synchronized via
  // EnsureSurfaceSynchronizedForWebTest().
  uint32_t latest_capture_sequence_number_ = 0u;

#if defined(USE_AURA)
  std::unique_ptr<aura::Window> window_;
#endif

  base::Optional<DisplayFeature> display_feature_;
};

// TestRenderViewHost ----------------------------------------------------------

// TODO(brettw) this should use a TestWebContents which should be generalized
// from the WebContentsImpl test. We will probably also need that class' version
// of CreateRenderViewForRenderManager when more complicated tests start using
// this.
//
// Note that users outside of content must use this class by getting
// the separate RenderViewHostTester interface via
// RenderViewHostTester::For(rvh) on the RenderViewHost they want to
// drive tests on.
//
// Users within content may directly static_cast from a
// RenderViewHost* to a TestRenderViewHost*.
//
// The reasons we do it this way rather than extending the parallel
// inheritance hierarchy we have for RenderWidgetHost/RenderViewHost
// vs. RenderWidgetHostImpl/RenderViewHostImpl are:
//
// a) Extending the parallel class hierarchy further would require
// more classes to use virtual inheritance.  This is a complexity that
// is better to avoid, especially when it would be introduced in the
// production code solely to facilitate testing code.
//
// b) While users outside of content only need to drive tests on a
// RenderViewHost, content needs a test version of the full
// RenderViewHostImpl so that it can test all methods on that concrete
// class (e.g. overriding a method such as
// RenderViewHostImpl::CreateRenderView).  This would have complicated
// the dual class hierarchy even further.
//
// The reason we do it this way instead of using composition is
// similar to (b) above, essentially it gets very tricky.  By using
// the split interface we avoid complexity within content and maintain
// reasonable utility for embedders.
class TestRenderViewHost
    : public RenderViewHostImpl,
      public RenderViewHostTester {
 public:
  TestRenderViewHost(FrameTree* frame_tree,
                     SiteInstance* instance,
                     std::unique_ptr<RenderWidgetHostImpl> widget,
                     RenderViewHostDelegate* delegate,
                     int32_t routing_id,
                     int32_t main_frame_routing_id,
                     bool swapped_out);
  // RenderViewHostImpl overrides.
  MockRenderProcessHost* GetProcess() override;
  bool CreateRenderView(
      const base::Optional<blink::FrameToken>& opener_frame_token,
      int proxy_route_id,
      bool window_was_created_with_opener) override;
  bool IsTestRenderViewHost() const override;

  // RenderViewHostTester implementation.
  void SimulateWasHidden() override;
  void SimulateWasShown() override;
  blink::web_pref::WebPreferences TestComputeWebPreferences() override;
  bool CreateTestRenderView() override;

  void TestOnUpdateStateWithFile(const base::FilePath& file_path);

  void TestStartDragging(const DropData& drop_data, SkBitmap bitmap = {});

  // If set, *delete_counter is incremented when this object destructs.
  void set_delete_counter(int* delete_counter) {
    delete_counter_ = delete_counter;
  }

  // The opener frame route id passed to CreateRenderView().
  const base::Optional<blink::FrameToken>& opener_frame_token() const {
    return opener_frame_token_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, FilterNavigate);

  ~TestRenderViewHost() override;

  void SendNavigateWithTransitionAndResponseCode(const GURL& url,
                                                 ui::PageTransition transition,
                                                 int response_code);

  // Calls OnNavigate on the RenderViewHost with the given information.
  // Sets the rest of the parameters in the message to the "typical" values.
  // This is a helper function for simulating the most common types of loads.
  void SendNavigateWithParameters(
      const GURL& url,
      ui::PageTransition transition,
      const GURL& original_request_url,
      int response_code,
      const base::FilePath* file_path_for_history_item);

  // See set_delete_counter() above. May be NULL.
  int* delete_counter_;

  // See opener_frame_token() above.
  base::Optional<blink::FrameToken> opener_frame_token_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHost);
};

// Adds methods to get straight at the impl classes.
class RenderViewHostImplTestHarness : public RenderViewHostTestHarness {
 public:
  RenderViewHostImplTestHarness();
  ~RenderViewHostImplTestHarness() override;

  // contents() is equivalent to static_cast<TestWebContents*>(web_contents())
  TestWebContents* contents();

  // RVH/RFH getters are shorthand for oft-used bits of web_contents().

  // test_rvh() is equivalent to any of the following:
  //   contents()->GetMainFrame()->GetRenderViewHost()
  //   contents()->GetRenderViewHost()
  //   static_cast<TestRenderViewHost*>(rvh())
  //
  // Since most functionality will eventually shift from RVH to RFH, you may
  // prefer to use the GetMainFrame() method in tests.
  TestRenderViewHost* test_rvh();

  // pending_test_rvh() is equivalent to all of the following:
  //   contents()->GetPendingMainFrame()->GetRenderViewHost() [if frame exists]
  //   contents()->GetPendingRenderViewHost()
  //   static_cast<TestRenderViewHost*>(pending_rvh())
  //
  // Since most functionality will eventually shift from RVH to RFH, you may
  // prefer to use the GetPendingMainFrame() method in tests.
  TestRenderViewHost* pending_test_rvh();

  // active_test_rvh() is equivalent to:
  //   contents()->GetPendingRenderViewHost() ?
  //        contents()->GetPendingRenderViewHost() :
  //        contents()->GetRenderViewHost();
  TestRenderViewHost* active_test_rvh();

  // main_test_rfh() is equivalent to contents()->GetMainFrame()
  // TODO(nick): Replace all uses with contents()->GetMainFrame()
  TestRenderFrameHost* main_test_rfh();

 private:
  typedef std::unique_ptr<ui::test::ScopedSetSupportedScaleFactors>
      ScopedSetSupportedScaleFactors;
  ScopedSetSupportedScaleFactors scoped_set_supported_scale_factors_;
  DISALLOW_COPY_AND_ASSIGN(RenderViewHostImplTestHarness);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_VIEW_HOST_H_
