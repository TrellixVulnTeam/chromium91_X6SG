// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/modal_close_listener_host.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class ModalCloseListenerHostBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void InstallModalCloseWatcherAndSignal() {
    // Install a ModalCloseWatcher.
    std::string script =
        "let watcher = new ModalCloseWatcher(); "
        "watcher.onclose = () => window.document.title = 'SUCCESS';";
    EXPECT_TRUE(ExecuteScript(web_contents(), script));

    RenderFrameHostImpl* render_frame_host_impl =
        web_contents()->GetFrameTree()->root()->current_frame_host();
    EXPECT_TRUE(ModalCloseListenerHost::GetOrCreateForCurrentDocument(
                    render_frame_host_impl)
                    ->SignalIfActive());

    const std::u16string signaled_title = u"SUCCESS";
    TitleWatcher watcher(web_contents(), signaled_title);
    watcher.AlsoWaitForTitle(signaled_title);
    EXPECT_EQ(signaled_title, watcher.WaitAndGetTitle());
  }
};

IN_PROC_BROWSER_TEST_F(ModalCloseListenerHostBrowserTest,
                       SignalModalCloseWatcherIfActive) {
  NavigationController& controller = web_contents()->GetController();
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, controller.GetEntryCount());

  InstallModalCloseWatcherAndSignal();
}

IN_PROC_BROWSER_TEST_F(ModalCloseListenerHostBrowserTest,
                       SignalModalCloseWatcherIfActiveAfterReload) {
  NavigationController& controller = web_contents()->GetController();
  GURL main_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(1, controller.GetEntryCount());

  InstallModalCloseWatcherAndSignal();
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  InstallModalCloseWatcherAndSignal();
}

}  // namespace content
