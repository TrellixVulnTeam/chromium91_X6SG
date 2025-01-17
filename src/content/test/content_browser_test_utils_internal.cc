// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test_utils_internal.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/stack.h"
#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"

namespace content {

bool NavigateFrameToURL(FrameTreeNode* node, const GURL& url) {
  TestFrameNavigationObserver observer(node);
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = node->frame_tree_node_id();
  FrameTree* frame_tree = node->frame_tree();

  node->navigator().controller().LoadURLWithParams(params);
  observer.Wait();

  if (!observer.last_navigation_succeeded()) {
    DLOG(WARNING) << "Navigation did not succeed: " << url;
    return false;
  }

  // It's possible for JS handlers triggered during the navigation to remove
  // the node, so retrieve it by ID again to check if that occurred.
  node = frame_tree->FindByID(params.frame_tree_node_id);

  if (node && url != node->current_url()) {
    DLOG(WARNING) << "Expected URL " << url << " but observed "
                  << node->current_url();
    return false;
  }
  return true;
}

void SetShouldProceedOnBeforeUnload(Shell* shell, bool proceed, bool success) {
  ShellJavaScriptDialogManager* manager =
      static_cast<ShellJavaScriptDialogManager*>(
          shell->GetJavaScriptDialogManager(shell->web_contents()));
  manager->set_should_proceed_on_beforeunload(proceed, success);
}

RenderFrameHost* ConvertToRenderFrameHost(FrameTreeNode* frame_tree_node) {
  return frame_tree_node->current_frame_host();
}

bool NavigateToURLInSameBrowsingInstance(Shell* window, const GURL& url) {
  TestNavigationObserver observer(window->web_contents());
  // Using a PAGE_TRANSITION_LINK transition with a browser-initiated
  // navigation forces it to stay in the current BrowsingInstance, as normally
  // that transition is used by renderer-initiated navigations.
  window->LoadURLForFrame(url, std::string(),
                          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK));
  observer.Wait();

  if (!IsLastCommittedEntryOfPageType(window->web_contents(),
                                      PAGE_TYPE_NORMAL)) {
    NavigationEntry* last_entry =
        window->web_contents()->GetController().GetLastCommittedEntry();
    DLOG(WARNING) << "last_entry->GetPageType() = "
                  << (last_entry ? last_entry->GetPageType() : -1);
    return false;
  }

  if (window->web_contents()->GetLastCommittedURL() != url) {
    DLOG(WARNING) << "window->web_contents()->GetLastCommittedURL() = "
                  << window->web_contents()->GetLastCommittedURL()
                  << "; url = " << url;
    return false;
  }

  return true;
}

bool IsExpectedSubframeErrorTransition(SiteInstance* start_site_instance,
                                       SiteInstance* end_site_instance) {
  bool site_instances_are_equal = (start_site_instance == end_site_instance);
  bool is_error_page_site_instance =
      (static_cast<SiteInstanceImpl*>(end_site_instance)
           ->GetSiteInfo()
           .is_error_page());
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(
          /*in_main_frame=*/false)) {
    return site_instances_are_equal && !is_error_page_site_instance;
  } else {
    return !site_instances_are_equal && is_error_page_site_instance;
  }
}

RenderFrameHost* CreateSubframe(WebContentsImpl* web_contents,
                                std::string frame_id,
                                const GURL& url,
                                bool wait_for_navigation) {
  RenderFrameHostCreatedObserver subframe_created_observer(web_contents);
  TestNavigationObserver subframe_nav_observer(web_contents);
  if (url.is_empty()) {
    EXPECT_TRUE(ExecJs(web_contents, JsReplace(R"(
          var iframe = document.createElement('iframe');
          iframe.id = $1;
          document.body.appendChild(iframe);
      )",
                                               frame_id)));
  } else {
    EXPECT_TRUE(ExecJs(web_contents, JsReplace(R"(
          var iframe = document.createElement('iframe');
          iframe.id = $1;
          iframe.src = $2;
          document.body.appendChild(iframe);
      )",
                                               frame_id, url)));
  }
  subframe_created_observer.Wait();
  if (wait_for_navigation)
    subframe_nav_observer.Wait();
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  return root->child_at(root->child_count() - 1)->current_frame_host();
}

Shell* OpenBlankWindow(WebContentsImpl* web_contents) {
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(root, "last_opened_window = window.open()"));
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_NE(new_shell->web_contents(), web_contents);
  EXPECT_FALSE(
      new_shell->web_contents()->GetController().GetLastCommittedEntry());
  return new_shell;
}

Shell* OpenWindow(WebContentsImpl* web_contents, const GURL& url) {
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(
      ExecJs(root, JsReplace("last_opened_window = window.open($1)", url)));
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_NE(new_shell->web_contents(), web_contents);
  return new_shell;
}

FrameTreeVisualizer::FrameTreeVisualizer() = default;

FrameTreeVisualizer::~FrameTreeVisualizer() = default;

std::string FrameTreeVisualizer::DepictFrameTree(FrameTreeNode* root) {
  // Tracks the sites actually used in this depiction.
  std::map<std::string, SiteInstance*> legend;

  // Traversal 1: Assign names to current frames. This ensures that the first
  // call to the pretty-printer will result in a naming of the site instances
  // that feels natural and stable.
  base::stack<FrameTreeNode*> to_explore;
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    RenderFrameHost* current = node->render_manager()->current_frame_host();
    legend[GetName(current->GetSiteInstance())] = current->GetSiteInstance();
  }

  // Traversal 2: Assign names to the pending/speculative frames. For stability
  // of assigned names it's important to do this before trying to name the
  // proxies, which have a less well defined order.
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    RenderFrameHost* spec = node->render_manager()->speculative_frame_host();
    if (spec)
      legend[GetName(spec->GetSiteInstance())] = spec->GetSiteInstance();
  }

  // Traversal 3: Assign names to the proxies and add them to |legend| too.
  // Typically, only openers should have their names assigned this way.
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    // Sort the proxies by SiteInstance ID to avoid unordered_map ordering.
    std::vector<SiteInstance*> site_instances;
    for (const auto& proxy_pair :
         node->render_manager()->GetAllProxyHostsForTesting()) {
      site_instances.push_back(proxy_pair.second->GetSiteInstance());
    }
    std::sort(site_instances.begin(), site_instances.end(),
              [](SiteInstance* lhs, SiteInstance* rhs) {
                return lhs->GetId() < rhs->GetId();
              });

    for (SiteInstance* site_instance : site_instances)
      legend[GetName(site_instance)] = site_instance;
  }

  // Traversal 4: Now that all names are assigned, make a big loop to pretty-
  // print the tree. Each iteration produces exactly one line of format.
  std::string result;
  for (to_explore.push(root); !to_explore.empty();) {
    FrameTreeNode* node = to_explore.top();
    to_explore.pop();
    for (size_t i = node->child_count(); i-- != 0;) {
      to_explore.push(node->child_at(i));
    }

    // Draw the feeler line tree graphics by walking up to the root. A feeler
    // line is needed for each ancestor that is the last child of its parent.
    // This creates the ASCII art that looks like:
    //    Foo
    //      |--Foo
    //      |--Foo
    //      |    |--Foo
    //      |    +--Foo
    //      |         +--Foo
    //      +--Foo
    //           +--Foo
    //
    // TODO(nick): Make this more elegant.
    std::string line;
    if (node != root) {
      if (node->parent()->child_at(node->parent()->child_count() - 1) != node)
        line = "  |--";
      else
        line = "  +--";
      for (FrameTreeNode* up = node->parent()->frame_tree_node(); up != root;
           up = FrameTreeNode::From(up->parent())) {
        if (up->parent()->child_at(up->parent()->child_count() - 1) != up)
          line = "  |  " + line;
        else
          line = "     " + line;
      }
    }

    // Prefix one extra space of padding for two reasons. First, this helps the
    // diagram aligns nicely with the legend. Second, this makes it easier to
    // read the diffs that gtest spits out on EXPECT_EQ failure.
    line = " " + line;

    // Summarize the FrameTreeNode's state. Always show the site of the current
    // RenderFrameHost, and show any exceptional state of the node, like a
    // pending or speculative RenderFrameHost.
    RenderFrameHost* current = node->render_manager()->current_frame_host();
    RenderFrameHost* spec = node->render_manager()->speculative_frame_host();
    base::StringAppendF(&line, "Site %s",
                        GetName(current->GetSiteInstance()).c_str());
    if (spec) {
      base::StringAppendF(&line, " (%s speculative)",
                          GetName(spec->GetSiteInstance()).c_str());
    }

    // Show the SiteInstances of the RenderFrameProxyHosts of this node.
    const auto& proxy_host_map =
        node->render_manager()->GetAllProxyHostsForTesting();
    if (!proxy_host_map.empty()) {
      // Show a dashed line of variable length before the proxy list. Always at
      // least two dashes.
      line.append(" --");

      // To make proxy lists align vertically for the first three tree levels,
      // pad with dashes up to a first tab stop at column 19 (which works out to
      // text editor column 28 in the typical diagram fed to EXPECT_EQ as a
      // string literal). Lining the lists up vertically makes differences in
      // the proxy sets easier to spot visually. We choose not to use the
      // *actual* tree height here, because that would make the diagram's
      // appearance less stable as the tree's shape evolves.
      while (line.length() < 20) {
        line.append("-");
      }
      line.append(" proxies for");

      // Sort these alphabetically, to avoid hash_map ordering dependency.
      std::vector<std::string> sorted_proxy_hosts;
      for (const auto& proxy_pair : proxy_host_map) {
        sorted_proxy_hosts.push_back(
            GetName(proxy_pair.second->GetSiteInstance()));
      }
      std::sort(sorted_proxy_hosts.begin(), sorted_proxy_hosts.end());
      for (std::string& proxy_name : sorted_proxy_hosts) {
        base::StringAppendF(&line, " %s", proxy_name.c_str());
      }
    }
    if (node != root)
      result.append("\n");
    result.append(line);
  }

  // Finally, show a legend with details of the site instances.
  const char* prefix = "Where ";
  for (auto& legend_entry : legend) {
    SiteInstanceImpl* site_instance =
        static_cast<SiteInstanceImpl*>(legend_entry.second);
    std::string description = site_instance->GetSiteURL().spec();
    base::StringAppendF(&result, "\n%s%s = %s", prefix,
                        legend_entry.first.c_str(), description.c_str());
    // Highlight some exceptionable conditions.
    if (site_instance->active_frame_count() == 0)
      result.append(" (active_frame_count == 0)");
    if (!site_instance->GetProcess()->IsInitializedAndNotDead())
      result.append(" (no process)");
    prefix = "      ";
  }
  return result;
}

std::string FrameTreeVisualizer::GetName(SiteInstance* site_instance) {
  // Indices into the vector correspond to letters of the alphabet.
  size_t index =
      std::find(seen_site_instance_ids_.begin(), seen_site_instance_ids_.end(),
                site_instance->GetId()) -
      seen_site_instance_ids_.begin();
  if (index == seen_site_instance_ids_.size())
    seen_site_instance_ids_.push_back(site_instance->GetId());

  // Whosoever writes a test using >=26 site instances shall be a lucky ducky.
  if (index < 25)
    return base::StringPrintf("%c", 'A' + static_cast<char>(index));
  else
    return base::StringPrintf("Z%d", static_cast<int>(index - 25));
}

std::string DepictFrameTree(FrameTreeNode& root) {
  return FrameTreeVisualizer().DepictFrameTree(&root);
}

Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name) {
  return OpenPopup(opener, url, name, "", true);
}

Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name,
                 const std::string& features,
                 bool expect_return_from_window_open) {
  TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();

  ShellAddedObserver new_shell_observer;
  bool did_create_popup = false;
  std::string popup_script =
      "window.domAutomationController.send("
      "    !!window.open('" +
      url.spec() + "', '" + name + "', '" + features + "'));";
  bool did_execute_script =
      ExecuteScriptAndExtractBool(opener, popup_script, &did_create_popup);

  // Don't check the value of |did_create_popup| since there are valid reasons
  // for it to be false, e.g. |features| specifies 'noopener', or 'noreferrer'
  // or others.
  if (!did_execute_script ||
      !(did_create_popup || !expect_return_from_window_open)) {
    return nullptr;
  }

  observer.Wait();

  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_EQ(url,
            new_shell->web_contents()->GetMainFrame()->GetLastCommittedURL());
  return new_shell_observer.GetShell();
}

FileChooserDelegate::FileChooserDelegate(const base::FilePath& file,
                                         base::OnceClosure callback)
    : file_(file), callback_(std::move(callback)) {}

FileChooserDelegate::~FileChooserDelegate() = default;

void FileChooserDelegate::RunFileChooser(
    RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // Send the selected file to the renderer process.
  auto file_info = blink::mojom::FileChooserFileInfo::NewNativeFile(
      blink::mojom::NativeFileInfo::New(file_, std::u16string()));
  std::vector<blink::mojom::FileChooserFileInfoPtr> files;
  files.push_back(std::move(file_info));
  listener->FileSelected(std::move(files), base::FilePath(),
                         blink::mojom::FileChooserParams::Mode::kOpen);

  params_ = params.Clone();
  std::move(callback_).Run();
}

FrameTestNavigationManager::FrameTestNavigationManager(
    int filtering_frame_tree_node_id,
    WebContents* web_contents,
    const GURL& url)
    : TestNavigationManager(web_contents, url),
      filtering_frame_tree_node_id_(filtering_frame_tree_node_id) {}

bool FrameTestNavigationManager::ShouldMonitorNavigation(
    NavigationHandle* handle) {
  return TestNavigationManager::ShouldMonitorNavigation(handle) &&
         handle->GetFrameTreeNodeId() == filtering_frame_tree_node_id_;
}

UrlCommitObserver::UrlCommitObserver(FrameTreeNode* frame_tree_node,
                                     const GURL& url)
    : content::WebContentsObserver(frame_tree_node->current_frame_host()
                                       ->delegate()
                                       ->GetAsWebContents()),
      frame_tree_node_id_(frame_tree_node->frame_tree_node_id()),
      url_(url) {
}

UrlCommitObserver::~UrlCommitObserver() {}

void UrlCommitObserver::Wait() {
  run_loop_.Run();
}

void UrlCommitObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      !navigation_handle->IsErrorPage() &&
      navigation_handle->GetURL() == url_ &&
      navigation_handle->GetFrameTreeNodeId() == frame_tree_node_id_) {
    run_loop_.Quit();
  }
}

RenderProcessHostBadIpcMessageWaiter::RenderProcessHostBadIpcMessageWaiter(
    RenderProcessHost* render_process_host)
    : internal_waiter_(render_process_host,
                       "Stability.BadMessageTerminated.Content") {}

base::Optional<bad_message::BadMessageReason>
RenderProcessHostBadIpcMessageWaiter::Wait() {
  base::Optional<int> internal_result = internal_waiter_.Wait();
  if (!internal_result.has_value())
    return base::nullopt;
  return static_cast<bad_message::BadMessageReason>(internal_result.value());
}

ShowPopupWidgetWaiter::ShowPopupWidgetWaiter(WebContentsImpl* web_contents,
                                             RenderFrameHostImpl* frame_host)
    : frame_host_(frame_host) {
#if defined(OS_MAC) || defined(OS_ANDROID)
  web_contents_ = web_contents;
  web_contents_->set_show_popup_menu_callback_for_testing(base::BindOnce(
      &ShowPopupWidgetWaiter::ShowPopupMenu, base::Unretained(this)));
#endif
  frame_host_->SetCreateNewPopupCallbackForTesting(base::BindRepeating(
      &ShowPopupWidgetWaiter::DidCreatePopupWidget, base::Unretained(this)));
}

ShowPopupWidgetWaiter::~ShowPopupWidgetWaiter() {
  if (auto* rwhi = RenderWidgetHostImpl::FromID(process_id_, routing_id_)) {
    rwhi->popup_widget_host_receiver_for_testing().SwapImplForTesting(rwhi);
  }
  if (frame_host_)
    frame_host_->SetCreateNewPopupCallbackForTesting(base::NullCallback());
}

void ShowPopupWidgetWaiter::Wait() {
  run_loop_.Run();
}

void ShowPopupWidgetWaiter::Stop() {
#if defined(OS_MAC) || defined(OS_ANDROID)
  web_contents_->set_show_popup_menu_callback_for_testing(base::NullCallback());
#endif
  frame_host_->SetCreateNewPopupCallbackForTesting(base::NullCallback());
  frame_host_ = nullptr;
}

blink::mojom::PopupWidgetHost* ShowPopupWidgetWaiter::GetForwardingInterface() {
  DCHECK_NE(MSG_ROUTING_NONE, routing_id_);
  return RenderWidgetHostImpl::FromID(process_id_, routing_id_);
}

void ShowPopupWidgetWaiter::ShowPopup(const gfx::Rect& initial_rect,
                                      ShowPopupCallback callback) {
  GetForwardingInterface()->ShowPopup(initial_rect, std::move(callback));
  initial_rect_ = initial_rect;
  run_loop_.Quit();
}

void ShowPopupWidgetWaiter::DidCreatePopupWidget(
    RenderWidgetHostImpl* render_widget_host) {
  process_id_ = render_widget_host->GetProcess()->GetID();
  routing_id_ = render_widget_host->GetRoutingID();
  render_widget_host->popup_widget_host_receiver_for_testing()
      .SwapImplForTesting(this);
}

#if defined(OS_MAC) || defined(OS_ANDROID)
void ShowPopupWidgetWaiter::ShowPopupMenu(const gfx::Rect& bounds) {
  initial_rect_ = bounds;
  run_loop_.Quit();
}
#endif

DropMessageFilter::DropMessageFilter(uint32_t message_class,
                                     uint32_t drop_message_id)
    : BrowserMessageFilter(message_class), drop_message_id_(drop_message_id) {}

DropMessageFilter::~DropMessageFilter() = default;

bool DropMessageFilter::OnMessageReceived(const IPC::Message& message) {
  return message.type() == drop_message_id_;
}

ObserveMessageFilter::ObserveMessageFilter(uint32_t message_class,
                                           uint32_t watch_message_id)
    : BrowserMessageFilter(message_class),
      watch_message_id_(watch_message_id) {}

ObserveMessageFilter::~ObserveMessageFilter() = default;

void ObserveMessageFilter::Wait() {
  base::RunLoop loop;
  quit_closure_ = loop.QuitClosure();
  loop.Run();
}

bool ObserveMessageFilter::OnMessageReceived(const IPC::Message& message) {
  if (message.type() == watch_message_id_) {
    // Exit the Wait() method if it's being used, but in a fresh stack once the
    // message is actually handled.
    if (quit_closure_ && !received_) {
      base::ThreadPool::PostTask(
          FROM_HERE, base::BindOnce(&ObserveMessageFilter::QuitWait, this));
    }
    received_ = true;
  }
  return false;
}

void ObserveMessageFilter::QuitWait() {
  std::move(quit_closure_).Run();
}

UnresponsiveRendererObserver::UnresponsiveRendererObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

UnresponsiveRendererObserver::~UnresponsiveRendererObserver() = default;

RenderProcessHost* UnresponsiveRendererObserver::Wait(base::TimeDelta timeout) {
  if (!captured_render_process_host_) {
    base::OneShotTimer timer;
    timer.Start(FROM_HERE, timeout, run_loop_.QuitClosure());
    run_loop_.Run();
    timer.Stop();
  }
  return captured_render_process_host_;
}

void UnresponsiveRendererObserver::OnRendererUnresponsive(
    RenderProcessHost* render_process_host) {
  captured_render_process_host_ = render_process_host;
  run_loop_.Quit();
}

BeforeUnloadBlockingDelegate::BeforeUnloadBlockingDelegate(
    WebContentsImpl* web_contents)
    : web_contents_(web_contents) {
  web_contents_->SetDelegate(this);
}

BeforeUnloadBlockingDelegate::~BeforeUnloadBlockingDelegate() {
  if (!callback_.is_null())
    std::move(callback_).Run(true, std::u16string());

  web_contents_->SetDelegate(nullptr);
  web_contents_->SetJavaScriptDialogManagerForTesting(nullptr);
}

void BeforeUnloadBlockingDelegate::Wait() {
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
}

JavaScriptDialogManager*
BeforeUnloadBlockingDelegate::GetJavaScriptDialogManager(WebContents* source) {
  return this;
}

void BeforeUnloadBlockingDelegate::RunJavaScriptDialog(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  NOTREACHED();
}

void BeforeUnloadBlockingDelegate::RunBeforeUnloadDialog(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  callback_ = std::move(callback);
  run_loop_->Quit();
}

bool BeforeUnloadBlockingDelegate::HandleJavaScriptDialog(
    WebContents* web_contents,
    bool accept,
    const std::u16string* prompt_override) {
  NOTREACHED();
  return true;
}

namespace {
static constexpr int kEnableLogMessageId = 0;
static constexpr char kEnableLogMessage[] = R"({"id":0,"method":"Log.enable"})";
static constexpr int kDisableLogMessageId = 1;
static constexpr char kDisableLogMessage[] =
    R"({"id":1,"method":"Log.disable"})";
}  // namespace

DevToolsInspectorLogWatcher::DevToolsInspectorLogWatcher(
    WebContents* web_contents) {
  host_ = DevToolsAgentHost::GetOrCreateFor(web_contents);
  host_->AttachClient(this);

  host_->DispatchProtocolMessage(
      this, base::as_bytes(
                base::make_span(kEnableLogMessage, strlen(kEnableLogMessage))));

  run_loop_enable_log_.Run();
}

DevToolsInspectorLogWatcher::~DevToolsInspectorLogWatcher() {
  host_->DetachClient(this);
}

void DevToolsInspectorLogWatcher::DispatchProtocolMessage(
    DevToolsAgentHost* host,
    base::span<const uint8_t> message) {
  base::StringPiece message_str(reinterpret_cast<const char*>(message.data()),
                                message.size());
  auto parsed_message = base::JSONReader::Read(message_str);
  base::Optional<int> command_id = parsed_message->FindIntPath("id");
  if (command_id.has_value()) {
    switch (command_id.value()) {
      case kEnableLogMessageId:
        run_loop_enable_log_.Quit();
        break;
      case kDisableLogMessageId:
        run_loop_disable_log_.Quit();
        break;
      default:
        NOTREACHED();
    }
    return;
  }

  std::string* notification = parsed_message->FindStringPath("method");
  if (notification && *notification == "Log.entryAdded") {
    std::string* text = parsed_message->FindStringPath("params.entry.text");
    DCHECK(text);
    last_message_ = *text;
  }
}

void DevToolsInspectorLogWatcher::AgentHostClosed(DevToolsAgentHost* host) {}

void DevToolsInspectorLogWatcher::FlushAndStopWatching() {
  host_->DispatchProtocolMessage(
      this, base::as_bytes(base::make_span(kDisableLogMessage,
                                           strlen(kDisableLogMessage))));
  run_loop_disable_log_.Run();
}

FrameNavigateParamsCapturer::FrameNavigateParamsCapturer(FrameTreeNode* node)
    : WebContentsObserver(
          node->current_frame_host()->delegate()->GetAsWebContents()),
      frame_tree_node_id_(node->frame_tree_node_id()) {}

FrameNavigateParamsCapturer::~FrameNavigateParamsCapturer() = default;

void FrameNavigateParamsCapturer::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->GetFrameTreeNodeId() != frame_tree_node_id_ ||
      navigations_remaining_ == 0) {
    return;
  }

  --navigations_remaining_;
  transitions_.push_back(navigation_handle->GetPageTransition());
  urls_.push_back(navigation_handle->GetURL());
  navigation_types_.push_back(
      NavigationRequest::From(navigation_handle)->navigation_type());
  is_same_documents_.push_back(navigation_handle->IsSameDocument());
  did_replace_entries_.push_back(navigation_handle->DidReplaceEntry());
  is_renderer_initiateds_.push_back(navigation_handle->IsRendererInitiated());
  has_user_gestures_.push_back(navigation_handle->HasUserGesture());
  is_overriding_user_agents_.push_back(
      navigation_handle->GetIsOverridingUserAgent());
  if (!navigations_remaining_ &&
      (!web_contents()->IsLoading() || !wait_for_load_))
    loop_.Quit();
}

void FrameNavigateParamsCapturer::Wait() {
  loop_.Run();
}

void FrameNavigateParamsCapturer::DidStopLoading() {
  if (!navigations_remaining_)
    loop_.Quit();
}

RenderFrameHostCreatedObserver::RenderFrameHostCreatedObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

RenderFrameHostCreatedObserver::RenderFrameHostCreatedObserver(
    WebContents* web_contents,
    int expected_frame_count)
    : WebContentsObserver(web_contents),
      expected_frame_count_(expected_frame_count) {}

RenderFrameHostCreatedObserver::RenderFrameHostCreatedObserver(
    WebContents* web_contents,
    OnRenderFrameHostCreatedCallback on_rfh_created)
    : WebContentsObserver(web_contents),
      on_rfh_created_(std::move(on_rfh_created)) {}

RenderFrameHostCreatedObserver::~RenderFrameHostCreatedObserver() = default;

RenderFrameHost* RenderFrameHostCreatedObserver::Wait() {
  if (frames_created_ < expected_frame_count_)
    run_loop_.Run();

  return last_rfh_;
}

void RenderFrameHostCreatedObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  frames_created_++;
  last_rfh_ = render_frame_host;
  if (on_rfh_created_)
    on_rfh_created_.Run(render_frame_host);
  if (frames_created_ == expected_frame_count_)
    run_loop_.Quit();
}

BackForwardCache::DisabledReason RenderFrameHostDisabledForTestingReason() {
  static const BackForwardCache::DisabledReason reason =
      BackForwardCache::DisabledReason(
          {BackForwardCache::DisabledSource::kTesting, 0,
           "disabled for testing"});
  return reason;
}

void DisableForRenderFrameHostForTesting(
    content::RenderFrameHost* render_frame_host) {
  content::BackForwardCache::DisableForRenderFrameHost(
      render_frame_host, RenderFrameHostDisabledForTestingReason());
}

void DisableForRenderFrameHostForTesting(content::GlobalFrameRoutingId id) {
  content::BackForwardCache::DisableForRenderFrameHost(
      id, RenderFrameHostDisabledForTestingReason());
}

void UserAgentInjector::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  web_contents()->SetUserAgentOverride(user_agent_override_, false);
  navigation_handle->SetIsOverridingUserAgent(is_overriding_user_agent_);
}

}  // namespace content
