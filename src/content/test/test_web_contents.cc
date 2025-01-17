// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_web_contents.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/portal/portal.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/debug_urls.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/referrer_type_converters.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_view_host.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

RenderProcessHostFactory* GetMockProcessFactory() {
  static base::NoDestructor<MockRenderProcessHostFactory> factory;
  return factory.get();
}

}  // namespace

TestWebContents::TestWebContents(BrowserContext* browser_context)
    : WebContentsImpl(browser_context),
      delegate_view_override_(nullptr),
      web_preferences_changed_counter_(nullptr),
      pause_subresource_loading_called_(false),
      audio_group_id_(base::UnguessableToken::Create()),
      is_page_frozen_(false) {
  if (!RenderProcessHostImpl::get_render_process_host_factory_for_testing()) {
    // Most unit tests should prefer to create a generic MockRenderProcessHost
    // (instead of a real RenderProcessHostImpl).  Tests that need to use a
    // specific, custom RenderProcessHostFactory should set it before creating
    // the first TestWebContents.
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        GetMockProcessFactory());
  }
}

std::unique_ptr<TestWebContents> TestWebContents::Create(
    BrowserContext* browser_context,
    scoped_refptr<SiteInstance> instance) {
  std::unique_ptr<TestWebContents> test_web_contents(
      new TestWebContents(browser_context));
  test_web_contents->Init(CreateParams(browser_context, std::move(instance)));
  return test_web_contents;
}

TestWebContents* TestWebContents::Create(const CreateParams& params) {
  TestWebContents* test_web_contents =
      new TestWebContents(params.browser_context);
  test_web_contents->Init(params);
  return test_web_contents;
}

TestWebContents::~TestWebContents() {
}

TestRenderFrameHost* TestWebContents::GetMainFrame() {
  auto* instance = WebContentsImpl::GetMainFrame();
  DCHECK(instance->IsTestRenderFrameHost())
      << "You may want to instantiate RenderViewHostTestEnabler.";
  return static_cast<TestRenderFrameHost*>(instance);
}

TestRenderViewHost* TestWebContents::GetRenderViewHost() {
  auto* instance = WebContentsImpl::GetRenderViewHost();
  DCHECK(instance->IsTestRenderViewHost())
      << "You may want to instantiate RenderViewHostTestEnabler.";
  return static_cast<TestRenderViewHost*>(instance);
}

TestRenderFrameHost* TestWebContents::GetSpeculativePrimaryMainFrame() {
  return static_cast<TestRenderFrameHost*>(
      GetFrameTree()->root()->render_manager()->speculative_frame_host());
}

int TestWebContents::DownloadImage(const GURL& url,
                                   bool is_favicon,
                                   uint32_t preferred_size,
                                   uint32_t max_bitmap_size,
                                   bool bypass_cache,
                                   ImageDownloadCallback callback) {
  static int g_next_image_download_id = 0;
  ++g_next_image_download_id;
  pending_image_downloads_[url].emplace_back(g_next_image_download_id,
                                             std::move(callback));
  return g_next_image_download_id;
}

const GURL& TestWebContents::GetLastCommittedURL() {
  if (last_committed_url_.is_valid()) {
    return last_committed_url_;
  }
  return WebContentsImpl::GetLastCommittedURL();
}

const std::u16string& TestWebContents::GetTitle() {
  if (title_)
    return title_.value();

  return WebContentsImpl::GetTitle();
}

const std::string& TestWebContents::GetSaveFrameHeaders() {
  return save_frame_headers_;
}

const std::u16string& TestWebContents::GetSuggestedFileName() {
  return suggested_filename_;
}

bool TestWebContents::HasPendingDownloadImage(const GURL& url) {
  return !pending_image_downloads_[url].empty();
}

void TestWebContents::OnWebPreferencesChanged() {
  WebContentsImpl::OnWebPreferencesChanged();
  if (web_preferences_changed_counter_)
    ++*web_preferences_changed_counter_;
}

bool TestWebContents::IsPageFrozen() {
  return is_page_frozen_;
}

bool TestWebContents::TestDidDownloadImage(
    const GURL& url,
    int http_status_code,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  if (!HasPendingDownloadImage(url))
    return false;
  int id = pending_image_downloads_[url].front().first;
  ImageDownloadCallback callback =
      std::move(pending_image_downloads_[url].front().second);
  pending_image_downloads_[url].pop_front();
  WebContentsImpl::OnDidDownloadImage(/*rfh=*/nullptr, std::move(callback), id,
                                      url, http_status_code, bitmaps,
                                      original_bitmap_sizes);
  return true;
}

void TestWebContents::SetLastCommittedURL(const GURL& url) {
  last_committed_url_ = url;
}

void TestWebContents::SetTitle(const std::u16string& title) {
  title_ = title;
}

void TestWebContents::SetMainFrameMimeType(const std::string& mime_type) {
  static_cast<RenderViewHostImpl*>(GetRenderViewHost())
      ->SetContentsMimeType(mime_type);
}

const std::string& TestWebContents::GetContentsMimeType() {
  return static_cast<RenderViewHostImpl*>(GetRenderViewHost())
      ->contents_mime_type();
}

void TestWebContents::SetIsCurrentlyAudible(bool audible) {
  audio_stream_monitor()->set_is_currently_audible_for_testing(audible);
  OnAudioStateChanged();
}

void TestWebContents::TestDidReceiveMouseDownEvent() {
  blink::WebMouseEvent event;
  event.SetType(blink::WebInputEvent::Type::kMouseDown);
  // Use the first RenderWidgetHost from the frame tree to make sure that the
  // interaction doesn't get ignored.
  DCHECK(frame_tree_.Nodes().begin() != frame_tree_.Nodes().end());
  RenderWidgetHostImpl* render_widget_host = (*frame_tree_.Nodes().begin())
                                                 ->current_frame_host()
                                                 ->GetRenderWidgetHost();
  DidReceiveInputEvent(render_widget_host, event);
}

void TestWebContents::TestDidFinishLoad(const GURL& url) {
  OnDidFinishLoad(frame_tree_.root()->current_frame_host(), url);
}

void TestWebContents::TestDidFailLoadWithError(const GURL& url,
                                               int error_code) {
  GetMainFrame()->DidFailLoadWithError(url, error_code);
}

bool TestWebContents::CrossProcessNavigationPending() {
  // If we don't have a speculative RenderFrameHost then it means we did not
  // change SiteInstances so we must be in the same process.
  if (GetRenderManager()->speculative_render_frame_host_ == nullptr)
    return false;

  auto* current_instance =
      GetRenderManager()->current_frame_host()->GetSiteInstance();
  auto* speculative_instance =
      GetRenderManager()->speculative_frame_host()->GetSiteInstance();
  if (current_instance == speculative_instance)
    return false;
  return current_instance->GetProcess() != speculative_instance->GetProcess();
}

bool TestWebContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host,
    const base::Optional<blink::FrameToken>& opener_frame_token,
    RenderFrameProxyHost* proxy_host) {
  const auto proxy_routing_id =
      proxy_host ? proxy_host->GetRoutingID() : MSG_ROUTING_NONE;
  // This will go to a TestRenderViewHost.
  static_cast<RenderViewHostImpl*>(render_view_host)
      ->CreateRenderView(opener_frame_token, proxy_routing_id, false);
  return true;
}

std::unique_ptr<WebContents> TestWebContents::Clone() {
  std::unique_ptr<WebContentsImpl> contents =
      Create(GetBrowserContext(), SiteInstance::Create(GetBrowserContext()));
  contents->GetController().CopyStateFrom(&GetController(), true);
  return contents;
}

void TestWebContents::NavigateAndCommit(const GURL& url,
                                        ui::PageTransition transition) {
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateBrowserInitiated(url, this);
  // TODO(clamy): Browser-initiated navigations should not have a transition of
  // type ui::PAGE_TRANSITION_LINK however several tests expect this. They
  // should be rewritten to simulate renderer-initiated navigations in these
  // cases. Once that's done, the transtion can be set to
  // ui::PAGE_TRANSITION_TYPED which makes more sense in this context.
  // ui::PAGE_TRANSITION_TYPED is the default value for transition
  navigation->SetTransition(transition);
  navigation->Commit();
}

void TestWebContents::NavigateAndFail(const GURL& url, int error_code) {
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateBrowserInitiated(url, this);
  navigation->Fail(error_code);
}

void TestWebContents::TestSetIsLoading(bool value) {
  if (value) {
    DidStartLoading(GetMainFrame()->frame_tree_node(), true);
  } else {
    for (FrameTreeNode* node : frame_tree_.Nodes()) {
      RenderFrameHostImpl* current_frame_host =
          node->render_manager()->current_frame_host();
      DCHECK(current_frame_host);
      current_frame_host->ResetLoadingState();

      RenderFrameHostImpl* speculative_frame_host =
          node->render_manager()->speculative_frame_host();
      if (speculative_frame_host)
        speculative_frame_host->ResetLoadingState();
      node->ResetNavigationRequest(false);
    }
  }
}

void TestWebContents::CommitPendingNavigation() {
  NavigationEntry* entry = GetController().GetPendingEntry();
  DCHECK(entry);

  auto navigation = NavigationSimulator::CreateFromPending(this);
  navigation->Commit();
}

RenderViewHostDelegateView* TestWebContents::GetDelegateView() {
  if (delegate_view_override_)
    return delegate_view_override_;
  return WebContentsImpl::GetDelegateView();
}

void TestWebContents::SetOpener(WebContents* opener) {
  frame_tree_.root()->SetOpener(
      static_cast<WebContentsImpl*>(opener)->GetFrameTree()->root());
}

void TestWebContents::SetIsCrashed(base::TerminationStatus status,
                                   int error_code) {
  SetMainFrameProcessStatus(status, error_code);
}

void TestWebContents::AddPendingContents(
    std::unique_ptr<WebContentsImpl> contents,
    const GURL& target_url) {
  // This is normally only done in WebContentsImpl::CreateNewWindow.
  GlobalRoutingID key(
      contents->GetRenderViewHost()->GetProcess()->GetID(),
      contents->GetRenderViewHost()->GetWidget()->GetRoutingID());
  AddWebContentsDestructionObserver(contents.get());
  pending_contents_[key] = CreatedWindow(std::move(contents), target_url);
}

RenderFrameHostDelegate* TestWebContents::CreateNewWindow(
    RenderFrameHostImpl* opener,
    const mojom::CreateNewWindowParams& params,
    bool is_new_browsing_instance,
    bool has_user_gesture,
    SessionStorageNamespace* session_storage_namespace) {
  return nullptr;
}

RenderWidgetHostImpl* TestWebContents::CreateNewPopupWidget(
    AgentSchedulingGroupHost& agent_scheduling_group,
    int32_t route_id,
    mojo::PendingAssociatedReceiver<blink::mojom::PopupWidgetHost>
        blink_popup_widget_host,
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget) {
  return nullptr;
}

void TestWebContents::ShowCreatedWindow(RenderFrameHostImpl* opener,
                                        int route_id,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_rect,
                                        bool user_gesture) {}

void TestWebContents::ShowCreatedWidget(int process_id,
                                        int route_id,
                                        const gfx::Rect& initial_rect) {}

void TestWebContents::SaveFrameWithHeaders(
    const GURL& url,
    const Referrer& referrer,
    const std::string& headers,
    const std::u16string& suggested_filename,
    RenderFrameHost* rfh) {
  save_frame_headers_ = headers;
  suggested_filename_ = suggested_filename;
}

bool TestWebContents::GetPauseSubresourceLoadingCalled() {
  return pause_subresource_loading_called_;
}

void TestWebContents::ResetPauseSubresourceLoadingCalled() {
  pause_subresource_loading_called_ = false;
}

void TestWebContents::SetLastActiveTime(base::TimeTicks last_active_time) {
  last_active_time_ = last_active_time;
}

void TestWebContents::TestIncrementBluetoothConnectedDeviceCount() {
  IncrementBluetoothConnectedDeviceCount();
}

void TestWebContents::TestDecrementBluetoothConnectedDeviceCount() {
  DecrementBluetoothConnectedDeviceCount();
}

base::UnguessableToken TestWebContents::GetAudioGroupId() {
  return audio_group_id_;
}

const blink::PortalToken& TestWebContents::CreatePortal(
    std::unique_ptr<WebContents> web_contents) {
  auto portal =
      std::make_unique<Portal>(GetMainFrame(), std::move(web_contents));
  const blink::PortalToken& token = portal->portal_token();
  portal->CreateProxyAndAttachPortal();
  GetMainFrame()->OnPortalCreatedForTesting(std::move(portal));
  return token;
}

WebContents* TestWebContents::GetPortalContents(
    const blink::PortalToken& portal_token) {
  Portal* portal = GetMainFrame()->FindPortalByToken(portal_token);
  if (!portal)
    return nullptr;
  return portal->GetPortalContents();
}

void TestWebContents::SetPageFrozen(bool frozen) {
  is_page_frozen_ = frozen;
}

}  // namespace content
