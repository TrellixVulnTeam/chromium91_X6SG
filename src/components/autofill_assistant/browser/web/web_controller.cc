// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/web_controller.h"

#include <math.h>
#include <algorithm>
#include <ctime>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace autofill_assistant {
using autofill::ContentAutofillDriver;

namespace {

// Get the visual viewport as a list of values to fill into RectF, that is:
// left, top, right, bottom.
const char* const kGetVisualViewport =
    R"({ const v = window.visualViewport;
         [v.pageLeft,
          v.pageTop,
          v.pageLeft + v.width,
          v.pageTop + v.height] })";

// Scrolls to the specified node with top padding. The top padding can
// be specified through pixels or ratio. Pixels take precedence.
const char* const kScrollIntoViewWithPaddingScript =
    R"(function(node, topPaddingPixels, topPaddingRatio, container = null) {
    node.scrollIntoViewIfNeeded();

    let scrollable = window;
    let containerTop = 0;
    if (container){
      scrollable = container;
      containerTop = container.getBoundingClientRect().top;
    }

    const rect = node.getBoundingClientRect();
    let topPadding = topPaddingPixels;
    if (!topPadding){
      topPadding = window.innerHeight * topPaddingRatio;
    }

    scrollable.scrollBy({top: rect.top - topPadding - containerTop});
  })";

// Scroll the window or any scrollable container as needed for the element to
// appear, center if specified.
const char* const kScrollIntoViewScript =
    R"(function(center) {
      if (center) {
        this.scrollIntoView({block: "center", inline: "center"});
      } else {
        this.scrollIntoViewIfNeeded();
      }
  })";

// Javascript to select a value from a select box. Also fires a "change" event
// to trigger any listeners. Changing the index directly does not trigger this.
const char* const kSelectOptionScript =
    R"(function(re2, valueSourceAttribute, caseSensitive) {
      const regexp = RegExp(re2, caseSensitive ? '' : 'i');
      let found = false;
      for (let i = 0; i < this.options.length; ++i) {
        if (regexp.test(this.options[i][valueSourceAttribute])) {
          this.options.selectedIndex = i;
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
      const e = document.createEvent('HTMLEvents');
      e.initEvent('change', true, true);
      this.dispatchEvent(e);
      return true;
    })";

// Javascript to highlight an element.
const char* const kHighlightElementScript =
    R"(function() {
      this.style.boxShadow = '0px 0px 0px 3px white, ' +
          '0px 0px 0px 6px rgb(66, 133, 244)';
      return true;
    })";

// Javascript code to retrieve the 'value' attribute of a node.
const char* const kGetValueAttributeScript =
    "function () { return this.value; }";

// Javascript code to retrieve the nested |attribute| of a node.
// The function intentionally has no "has value" check, such that a bad access
// will return an error.
const char* const kGetElementAttributeScript =
    R"(function (attributes) {
        let it = this;
        for (let i = 0; i < attributes.length; ++i) {
          it = it[attributes[i]];
        }
        return it;
      })";

// Javascript code to select the current value.
const char* const kSelectFieldValueScript = "function() { this.select(); }";

// Javascript code to focus a field.
const char* const kFocusFieldScript = "function() { this.focus(); }";

// Javascript code to set the 'value' attribute of a node and then fire a
// "change" event to trigger any listeners.
const char* const kSetValueAttributeScript =
    R"(function (value) {
         this.value = value;
         const e = document.createEvent('HTMLEvents');
         e.initEvent('change', true, true);
         this.dispatchEvent(e);
       })";

// Javascript code to set an attribute of a node to a given value.
// The function intentionally has no "has value" check, such that a bad access
// will return an error.
const char* const kSetAttributeScript =
    R"(function (attribute, value) {
         let receiver = this;
         for (let i = 0; i < attribute.length - 1; i++) {
           receiver = receiver[attribute[i]];
         }
         receiver[attribute[attribute.length - 1]] = value;
       })";

// Javascript code to get the outerHTML of a node.
// TODO(crbug.com/806868): Investigate if using DOM.GetOuterHtml would be a
// better solution than injecting Javascript code.
const char* const kGetOuterHtmlScript =
    "function () { return this.outerHTML; }";

// Javascript code to get the outerHTML of each node in a list.
const char* const kGetOuterHtmlsScript =
    "function () { return this.map((e) => e.outerHTML); }";

const char* const kGetElementTagScript = "function () { return this.tagName; }";

// Javascript code to click on an element.
const char* const kClickElementScript =
    R"(function () {
      this.click();
    })";

// Javascript code that returns a promise that will succeed once the main
// document window has changed height.
//
// This ignores width changes, to filter out resizes caused by changes to the
// screen orientation.
const char* const kWaitForWindowHeightChange = R"(
new Promise((fulfill, reject) => {
  var lastWidth = window.innerWidth;
  var handler = function(event) {
    if (window.innerWidth != lastWidth) {
      lastWidth = window.innerWidth;
      return
    }
    window.removeEventListener('resize', handler)
    fulfill(true)
  }
  window.addEventListener('resize', handler)
})
)";

const char* const kSendChangeEventScript =
    R"(function () {
         const e = document.createEvent('HTMLEvents');
         e.initEvent('change', true, true);
         this.dispatchEvent(e);
       })";

const char* const kDispatchEventToDocumentScript =
    R"(const event = new Event('duplexweb');
       document.dispatchEvent(event);)";

// Converts a int that correspond to the DocumentReadyState enum into an
// equivalent quoted Javascript string.
std::string DocumentReadyStateToQuotedJsString(int state) {
  switch (static_cast<DocumentReadyState>(state)) {
    case DOCUMENT_UNKNOWN_READY_STATE:
      return "''";
    case DOCUMENT_UNINITIALIZED:
      return "'uninitialized'";
    case DOCUMENT_LOADING:
      return "'loading'";
    case DOCUMENT_LOADED:
      return "'loaded'";
    case DOCUMENT_INTERACTIVE:
      return "'interactive'";
    case DOCUMENT_COMPLETE:
      return "'complete'";

      // No default, to get a compilation error if a new enum value is left
      // unsupported.
  }

  // If the enum values aren't sequential, just add empty strings to fill in the
  // blanks.
  return "''";
}

// Appends to |out| the definition of a function that'll wait for a
// ready state, expressed as a DocumentReadyState enum value.
void AppendWaitForDocumentReadyStateFunction(DocumentReadyState min_ready_state,
                                             std::string* out) {
  // quoted_names covers all possible DocumentReadyState values.
  std::vector<std::string> quoted_names(DOCUMENT_MAX_READY_STATE + 1);
  for (int i = 0; i <= DOCUMENT_MAX_READY_STATE; i++) {
    quoted_names[i] = DocumentReadyStateToQuotedJsString(i);
  }
  base::StrAppend(
      out, {R"((function (minReadyStateNum) {
  return new Promise((fulfill, reject) => {
    let handler = function(event) {
      let readyState = document.readyState;
      let readyStates = [)",
            base::JoinString(quoted_names, ", "), R"(];
      let readyStateNum = readyStates.indexOf(readyState);
      if (readyStateNum == -1) readyStateNum = 0;
      if (readyStateNum >= minReadyStateNum) {
        document.removeEventListener('readystatechange', handler);
        fulfill(readyStateNum);
      }
    }
    document.addEventListener('readystatechange', handler)
    handler();
  })
}))",
            base::StringPrintf("(%d)", static_cast<int>(min_ready_state))});
}

void WrapCallbackNoWait(
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)> callback,
    const ClientStatus& status,
    DocumentReadyState state,
    base::TimeDelta ignored_time) {
  std::move(callback).Run(status, state);
}

void DecorateWebControllerStatus(
    WebControllerErrorInfoProto::WebAction web_action,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  ClientStatus copy = status;
  if (!status.ok()) {
    VLOG(1) << web_action << " failed with status: " << status;
    FillWebControllerErrorInfo(web_action, &copy);
  }
  std::move(callback).Run(copy);
}

template <typename T>
void DecorateControllerStatusWithValue(
    WebControllerErrorInfoProto::WebAction web_action,
    base::OnceCallback<void(const ClientStatus&, const T&)> callback,
    const ClientStatus& status,
    const T& result) {
  ClientStatus copy = status;
  if (!status.ok()) {
    VLOG(1) << web_action << " failed with status: " << status;
    FillWebControllerErrorInfo(web_action, &copy);
  }
  std::move(callback).Run(copy, result);
}

}  // namespace

// static
std::unique_ptr<WebController> WebController::CreateForWebContents(
    content::WebContents* web_contents) {
  return std::make_unique<WebController>(
      web_contents,
      std::make_unique<DevtoolsClient>(
          content::DevToolsAgentHost::GetOrCreateFor(web_contents)));
}

WebController::WebController(content::WebContents* web_contents,
                             std::unique_ptr<DevtoolsClient> devtools_client)
    : web_contents_(web_contents),
      devtools_client_(std::move(devtools_client)) {}

WebController::~WebController() {}

WebController::FillFormInputData::FillFormInputData() {}

WebController::FillFormInputData::~FillFormInputData() {}

void WebController::LoadURL(const GURL& url) {
#ifdef NDEBUG
  VLOG(3) << __func__ << " <redacted>";
#else
  DVLOG(3) << __func__ << " " << url;
#endif
  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
}

void WebController::OnJavaScriptResult(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << " Failed JavaScript with status: " << status;
  }
  std::move(callback).Run(status);
}

void WebController::OnJavaScriptResultForString(
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  std::string value;
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << "Failed JavaScript with status: " << status;
  }
  SafeGetStringValue(result->GetResult(), &value);
  std::move(callback).Run(status, value);
}

void WebController::OnJavaScriptResultForStringArray(
    base::OnceCallback<void(const ClientStatus&,
                            const std::vector<std::string>&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << "Failed JavaScript with status: " << status;
    std::move(callback).Run(status, {});
    return;
  }

  auto* remote_object = result->GetResult();
  if (!remote_object || !remote_object->HasValue() ||
      !remote_object->GetValue()->is_list()) {
    VLOG(1) << __func__ << "JavaScript result is not an array.";
    std::move(callback).Run(
        JavaScriptErrorStatus(reply_status, __FILE__, __LINE__,
                              /* exception= */ nullptr),
        {});
    return;
  }

  auto values = remote_object->GetValue()->GetList();
  std::vector<std::string> v;
  for (const base::Value& value : values) {
    if (!value.is_string()) {
      VLOG(1) << __func__
              << "JavaScript array content is not a string: " << value.type();
      std::move(callback).Run(
          JavaScriptErrorStatus(reply_status, __FILE__, __LINE__,
                                /* exception= */ nullptr),
          {});
      return;
    }

    v.push_back(value.GetString());
  }

  std::move(callback).Run(status, v);
}

void WebController::ScrollIntoView(
    bool center,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  AddRuntimeCallArgument(center, &argument);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kScrollIntoViewScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResult, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateWebControllerStatus,
                         WebControllerErrorInfoProto::SCROLL_INTO_VIEW,
                         std::move(callback))));
}

void WebController::CheckOnTop(
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  auto worker = std::make_unique<CheckOnTopWorker>(devtools_client_.get());
  auto* ptr = worker.get();
  pending_workers_.emplace_back(std::move(worker));
  ptr->Start(element,
             base::BindOnce(&WebController::OnCheckOnTop,
                            weak_ptr_factory_.GetWeakPtr(), ptr,
                            base::BindOnce(&DecorateWebControllerStatus,
                                           WebControllerErrorInfoProto::ON_TOP,
                                           std::move(callback))));
}

void WebController::OnCheckOnTop(
    CheckOnTopWorker* worker_to_release,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  base::EraseIf(pending_workers_, [worker_to_release](const auto& worker) {
    return worker.get() == worker_to_release;
  });
  if (!status.ok()) {
    VLOG(1) << __func__ << " Element is not on top: " << status;
  }
  std::move(callback).Run(status);
}

void WebController::WaitUntilElementIsStable(
    int max_rounds,
    base::TimeDelta check_interval,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback) {
  std::unique_ptr<ElementPositionGetter> getter =
      std::make_unique<ElementPositionGetter>(devtools_client_.get(),
                                              max_rounds, check_interval,
                                              element.node_frame_id());
  auto* ptr = getter.get();
  pending_workers_.emplace_back(std::move(getter));
  ptr->Start(element.container_frame_host, element.object_id(),
             base::BindOnce(&WebController::OnWaitUntilElementIsStable,
                            weak_ptr_factory_.GetWeakPtr(), ptr,
                            base::TimeTicks::Now(), std::move(callback)));
}

void WebController::OnWaitUntilElementIsStable(
    ElementPositionGetter* getter_to_release,
    base::TimeTicks wait_start_time,
    base::OnceCallback<void(const ClientStatus&, base::TimeDelta)> callback,
    const ClientStatus& status) {
  base::EraseIf(pending_workers_, [getter_to_release](const auto& worker) {
    return worker.get() == getter_to_release;
  });
  ClientStatus end_status = status;
  if (!status.ok()) {
    VLOG(1) << __func__ << " Element unstable.";
    FillWebControllerErrorInfo(
        WebControllerErrorInfoProto::WAIT_UNTIL_ELEMENT_IS_STABLE, &end_status);
  }
  std::move(callback).Run(end_status, base::TimeTicks::Now() - wait_start_time);
}

void WebController::ClickOrTapElement(
    const ElementFinder::Result& element,
    ClickType click_type,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  auto wrapped_callback = GetAssistantActionRunningStateRetainingCallback(
      element, std::move(callback));

  if (click_type == ClickType::JAVASCRIPT) {
    devtools_client_->GetRuntime()->CallFunctionOn(
        runtime::CallFunctionOnParams::Builder()
            .SetObjectId(element.object_id())
            .SetFunctionDeclaration(kClickElementScript)
            .Build(),
        element.node_frame_id(),
        base::BindOnce(
            &WebController::OnJavaScriptResult, weak_ptr_factory_.GetWeakPtr(),
            base::BindOnce(&DecorateWebControllerStatus,
                           WebControllerErrorInfoProto::CLICK_OR_TAP_ELEMENT,
                           std::move(wrapped_callback))));
    return;
  }

  std::unique_ptr<ElementPositionGetter> getter =
      std::make_unique<ElementPositionGetter>(
          devtools_client_.get(), /* max_rounds= */ 1,
          /* check_interval= */ base::TimeDelta::FromMilliseconds(0),
          element.node_frame_id());
  auto* ptr = getter.get();
  pending_workers_.emplace_back(std::move(getter));
  ptr->Start(
      element.container_frame_host, element.object_id(),
      base::BindOnce(
          &WebController::TapOrClickOnCoordinates,
          weak_ptr_factory_.GetWeakPtr(), ptr, element.node_frame_id(),
          click_type,
          base::BindOnce(&DecorateWebControllerStatus,
                         WebControllerErrorInfoProto::CLICK_OR_TAP_ELEMENT,
                         std::move(wrapped_callback))));
}

void WebController::TapOrClickOnCoordinates(
    ElementPositionGetter* getter_to_release,
    const std::string& node_frame_id,
    ClickType click_type,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  int x = getter_to_release->x();
  int y = getter_to_release->y();
  base::EraseIf(pending_workers_, [getter_to_release](const auto& worker) {
    return worker.get() == getter_to_release;
  });

  if (!status.ok()) {
    VLOG(1) << __func__ << " Failed to get element position.";
    std::move(callback).Run(status);
    return;
  }

  DCHECK(click_type == ClickType::TAP || click_type == ClickType::CLICK);
  if (click_type == ClickType::CLICK) {
    devtools_client_->GetInput()->DispatchMouseEvent(
        input::DispatchMouseEventParams::Builder()
            .SetX(x)
            .SetY(y)
            .SetClickCount(1)
            .SetButton(input::MouseButton::LEFT)
            .SetType(input::DispatchMouseEventType::MOUSE_PRESSED)
            .Build(),
        node_frame_id,
        base::BindOnce(&WebController::OnDispatchPressMouseEvent,
                       weak_ptr_factory_.GetWeakPtr(), node_frame_id,
                       std::move(callback), x, y));
    return;
  }

  std::vector<std::unique_ptr<::autofill_assistant::input::TouchPoint>>
      touch_points;
  touch_points.emplace_back(
      input::TouchPoint::Builder().SetX(x).SetY(y).Build());
  devtools_client_->GetInput()->DispatchTouchEvent(
      input::DispatchTouchEventParams::Builder()
          .SetType(input::DispatchTouchEventType::TOUCH_START)
          .SetTouchPoints(std::move(touch_points))
          .Build(),
      node_frame_id,
      base::BindOnce(&WebController::OnDispatchTouchEventStart,
                     weak_ptr_factory_.GetWeakPtr(), node_frame_id,
                     std::move(callback)));
}

void WebController::OnDispatchPressMouseEvent(
    const std::string& node_frame_id,
    base::OnceCallback<void(const ClientStatus&)> callback,
    int x,
    int y,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  if (!result) {
    VLOG(1) << __func__
            << " Failed to dispatch mouse left button pressed event.";
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  devtools_client_->GetInput()->DispatchMouseEvent(
      input::DispatchMouseEventParams::Builder()
          .SetX(x)
          .SetY(y)
          .SetClickCount(1)
          .SetButton(input::MouseButton::LEFT)
          .SetType(input::DispatchMouseEventType::MOUSE_RELEASED)
          .Build(),
      node_frame_id,
      base::BindOnce(&WebController::OnDispatchReleaseMouseEvent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnDispatchReleaseMouseEvent(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  if (!result) {
    VLOG(1) << __func__ << " Failed to dispatch release mouse event.";
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }
  std::move(callback).Run(OkClientStatus());
}

void WebController::OnDispatchTouchEventStart(
    const std::string& node_frame_id,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchTouchEventResult> result) {
  if (!result) {
    VLOG(1) << __func__ << " Failed to dispatch touch start event.";
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }

  std::vector<std::unique_ptr<::autofill_assistant::input::TouchPoint>>
      touch_points;
  devtools_client_->GetInput()->DispatchTouchEvent(
      input::DispatchTouchEventParams::Builder()
          .SetType(input::DispatchTouchEventType::TOUCH_END)
          .SetTouchPoints(std::move(touch_points))
          .Build(),
      node_frame_id,
      base::BindOnce(&WebController::OnDispatchTouchEventEnd,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnDispatchTouchEventEnd(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<input::DispatchTouchEventResult> result) {
  if (!result) {
    VLOG(1) << __func__ << " Failed to dispatch touch end event.";
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }
  std::move(callback).Run(OkClientStatus());
}

void WebController::WaitForWindowHeightChange(
    base::OnceCallback<void(const ClientStatus&)> callback) {
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(kWaitForWindowHeightChange)
          .SetAwaitPromise(true)
          .Build(),
      /* node_frame_id= */ std::string(),
      base::BindOnce(&WebController::OnWaitForWindowHeightChange,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnWaitForWindowHeightChange(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  std::move(callback).Run(
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__));
}

void WebController::GetDocumentReadyState(
    const ElementFinder::Result& optional_frame_element,
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
        callback) {
  WaitForDocumentReadyState(
      optional_frame_element, DOCUMENT_UNKNOWN_READY_STATE,
      base::BindOnce(&WrapCallbackNoWait, std::move(callback)));
}

void WebController::WaitForDocumentReadyState(
    const ElementFinder::Result& optional_frame_element,
    DocumentReadyState min_ready_state,
    base::OnceCallback<void(const ClientStatus&,
                            DocumentReadyState,
                            base::TimeDelta)> callback) {
  // Note: An optional frame element will have an empty node_frame_id which
  // will be considered as operating in the main frame.
  std::string expression;
  AppendWaitForDocumentReadyStateFunction(min_ready_state, &expression);
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(expression)
          .SetReturnByValue(true)
          .SetAwaitPromise(true)
          .Build(),
      optional_frame_element.node_frame_id(),
      base::BindOnce(&WebController::OnWaitForDocumentReadyState,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     base::TimeTicks::Now()));
}

void WebController::OnWaitForDocumentReadyState(
    base::OnceCallback<void(const ClientStatus&,
                            DocumentReadyState,
                            base::TimeDelta)> callback,
    base::TimeTicks wait_start_time,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << " Failed to get document ready state.";
    FillWebControllerErrorInfo(
        WebControllerErrorInfoProto::WAIT_FOR_DOCUMENT_READY_STATE, &status);
  }

  int ready_state;
  SafeGetIntValue(result->GetResult(), &ready_state);
  std::move(callback).Run(status, static_cast<DocumentReadyState>(ready_state),
                          base::TimeTicks::Now() - wait_start_time);
}

void WebController::FindElement(const Selector& selector,
                                bool strict_mode,
                                ElementFinder::Callback callback) {
  RunElementFinder(selector,
                   strict_mode ? ElementFinder::ResultType::kExactlyOneMatch
                               : ElementFinder::ResultType::kAnyMatch,
                   std::move(callback));
}

void WebController::FindAllElements(const Selector& selector,
                                    ElementFinder::Callback callback) {
  RunElementFinder(selector, ElementFinder::ResultType::kMatchArray,
                   std::move(callback));
}

void WebController::RunElementFinder(const Selector& selector,
                                     ElementFinder::ResultType result_type,
                                     ElementFinder::Callback callback) {
  auto finder = std::make_unique<ElementFinder>(
      web_contents_, devtools_client_.get(), selector, result_type);

  auto* ptr = finder.get();
  pending_workers_.emplace_back(std::move(finder));
  ptr->Start(base::BindOnce(&WebController::OnFindElementResult,
                            weak_ptr_factory_.GetWeakPtr(), ptr,
                            std::move(callback)));
}

void WebController::OnFindElementResult(
    ElementFinder* finder_to_release,
    ElementFinder::Callback callback,
    const ClientStatus& status,
    std::unique_ptr<ElementFinder::Result> result) {
  base::EraseIf(pending_workers_, [finder_to_release](const auto& worker) {
    return worker.get() == finder_to_release;
  });
  std::move(callback).Run(status, std::move(result));
}

void WebController::FillAddressForm(
    const autofill::AutofillProfile* profile,
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  VLOG(3) << __func__ << " " << selector;
  auto data_to_autofill = std::make_unique<FillFormInputData>();
  data_to_autofill->profile = MakeUniqueFromProfile(*profile);
  GetElementFormAndFieldData(
      selector,
      base::BindOnce(&WebController::OnGetFormAndFieldDataForFilling,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(data_to_autofill), std::move(callback)));
}

void WebController::FillCardForm(
    std::unique_ptr<autofill::CreditCard> card,
    const std::u16string& cvc,
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  VLOG(3) << __func__ << " " << selector;
  auto data_to_autofill = std::make_unique<FillFormInputData>();
  data_to_autofill->card = std::move(card);
  data_to_autofill->cvc = cvc;
  GetElementFormAndFieldData(
      selector,
      base::BindOnce(&WebController::OnGetFormAndFieldDataForFilling,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(data_to_autofill), std::move(callback)));
}

void WebController::RetrieveElementFormAndFieldData(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&,
                            const autofill::FormData& form_data,
                            const autofill::FormFieldData& field_data)>
        callback) {
  DVLOG(3) << __func__ << " " << selector;
  GetElementFormAndFieldData(
      selector,
      base::BindOnce(&WebController::OnGetFormAndFieldDataForRetrieving,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::GetElementFormAndFieldData(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&,
                            ContentAutofillDriver* driver,
                            const autofill::FormData&,
                            const autofill::FormFieldData&)> callback) {
  FindElement(
      selector, /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForGetFormAndFieldData,
                     weak_ptr_factory_.GetWeakPtr(), selector,
                     std::move(callback)));
}

void WebController::OnFindElementForGetFormAndFieldData(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&,
                            ContentAutofillDriver* driver,
                            const autofill::FormData&,
                            const autofill::FormFieldData&)> callback,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinder::Result> element_result) {
  if (!element_status.ok()) {
    VLOG(1) << __func__
            << " Failed to find the element for getting Autofill data.";
    std::move(callback).Run(element_status, nullptr, autofill::FormData(),
                            autofill::FormFieldData());
    return;
  }

  ContentAutofillDriver* driver = ContentAutofillDriver::GetForRenderFrameHost(
      element_result->container_frame_host);
  if (driver == nullptr) {
    VLOG(1) << __func__ << " Failed to get the autofill driver.";
    std::move(callback).Run(UnexpectedErrorStatus(__FILE__, __LINE__), nullptr,
                            autofill::FormData(), autofill::FormFieldData());
    return;
  }

  base::Optional<std::string> css_selector =
      selector.ExtractSingleCssSelectorForAutofill();
  if (!css_selector) {
    std::move(callback).Run(ClientStatus(INVALID_SELECTOR), nullptr,
                            autofill::FormData(), autofill::FormFieldData());
    return;
  }

  driver->GetAutofillAgent()->GetElementFormAndFieldData(
      {*css_selector}, base::BindOnce(&WebController::OnGetFormAndFieldData,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(callback), driver));
}

void WebController::OnGetFormAndFieldData(
    base::OnceCallback<void(const ClientStatus&,
                            ContentAutofillDriver* driver,
                            const autofill::FormData&,
                            const autofill::FormFieldData&)> callback,
    ContentAutofillDriver* driver,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& form_field_data) {
  if (form_data.fields.empty()) {
    VLOG(1) << __func__ << " Failed to get form data.";
    std::move(callback).Run(UnexpectedErrorStatus(__FILE__, __LINE__), driver,
                            autofill::FormData(), autofill::FormFieldData());
    return;
  }
  std::move(callback).Run(OkClientStatus(), driver, form_data, form_field_data);
}

void WebController::OnGetFormAndFieldDataForFilling(
    std::unique_ptr<FillFormInputData> data_to_autofill,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& form_status,
    ContentAutofillDriver* driver,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& form_field) {
  if (!form_status.ok()) {
    std::move(callback).Run(form_status);
    return;
  }

  if (data_to_autofill->card) {
    driver->autofill_manager()->FillCreditCardForm(
        autofill::kNoQueryId, form_data, form_field, *data_to_autofill->card,
        data_to_autofill->cvc);
  } else {
    driver->autofill_manager()->FillProfileForm(*data_to_autofill->profile,
                                                form_data, form_field);
  }

  std::move(callback).Run(OkClientStatus());
}

void WebController::OnGetFormAndFieldDataForRetrieving(
    base::OnceCallback<void(const ClientStatus&,
                            const autofill::FormData& form_data,
                            const autofill::FormFieldData& field_data)>
        callback,
    const ClientStatus& form_status,
    ContentAutofillDriver* driver,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& form_field) {
  std::move(callback).Run(form_status, form_data, form_field);
}

void WebController::SelectOption(
    const std::string& re2,
    bool case_sensitive,
    SelectOptionProto::OptionComparisonAttribute option_comparison_attribute,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
#ifdef NDEBUG
  VLOG(3) << __func__ << " re2=(redacted)"
          << ", case_sensitive=" << case_sensitive
          << ", option_comparison_attribute=" << option_comparison_attribute;
#else
  DVLOG(3) << __func__ << " re2=" << re2
           << ", case_sensitive=" << case_sensitive
           << ", option_comparison_attribute=" << option_comparison_attribute;
#endif

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(re2, &arguments);
  switch (option_comparison_attribute) {
    case SelectOptionProto::VALUE:
      AddRuntimeCallArgument("value", &arguments);
      break;
    case SelectOptionProto::LABEL:
      AddRuntimeCallArgument("label", &arguments);
      break;
    case SelectOptionProto::NOT_SET:
      ClientStatus error(INVALID_ACTION);
      FillWebControllerErrorInfo(WebControllerErrorInfoProto::SELECT_OPTION,
                                 &error);
      std::move(callback).Run(error);
      return;
  }
  AddRuntimeCallArgument(case_sensitive, &arguments);

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kSelectOptionScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(&WebController::OnSelectOption,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&DecorateWebControllerStatus,
                                    WebControllerErrorInfoProto::SELECT_OPTION,
                                    std::move(callback))));
}

void WebController::OnSelectOption(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__ << " Failed to select option.";
    std::move(callback).Run(status);
    return;
  }
  bool found;
  if (!SafeGetBool(result->GetResult(), &found)) {
    std::move(callback).Run(
        UnexpectedDevtoolsErrorStatus(reply_status, __FILE__, __LINE__));
    return;
  }
  if (!found) {
    VLOG(1) << __func__ << " Failed to find option.";
    std::move(callback).Run(ClientStatus(OPTION_VALUE_NOT_FOUND));
    return;
  }
  std::move(callback).Run(OkClientStatus());
}

void WebController::HighlightElement(
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetFunctionDeclaration(std::string(kHighlightElementScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResult, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateWebControllerStatus,
                         WebControllerErrorInfoProto::HIGHLIGHT_ELEMENT,
                         std::move(callback))));
}

void WebController::ScrollToElementPosition(
    std::unique_ptr<ElementFinder::Result> container,
    const ElementFinder::Result& element,
    const TopPadding& top_padding,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgumentObjectId(element.object_id(), &arguments);
  AddRuntimeCallArgument(top_padding.pixels(), &arguments);
  AddRuntimeCallArgument(top_padding.ratio(), &arguments);
  if (container) {
    AddRuntimeCallArgumentObjectId(container->object_id(), &arguments);
  }

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kScrollIntoViewWithPaddingScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResult, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &DecorateWebControllerStatus,
              WebControllerErrorInfoProto::SCROLL_INTO_VIEW_WITH_PADDING,
              std::move(callback))));
}

void WebController::GetFieldValue(
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetFunctionDeclaration(std::string(kGetValueAttributeScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResultForString,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateControllerStatusWithValue<std::string>,
                         WebControllerErrorInfoProto::GET_FIELD_VALUE,
                         std::move(callback))));
}

void WebController::GetStringAttribute(
    const std::vector<std::string>& attributes,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  VLOG(3) << __func__ << " attributes=[" << base::JoinString(attributes, ",")
          << "]";

  if (attributes.empty()) {
    ClientStatus error_status = UnexpectedErrorStatus(__FILE__, __LINE__);
    FillWebControllerErrorInfo(
        WebControllerErrorInfoProto::GET_STRING_ATTRIBUTE, &error_status);
    std::move(callback).Run(error_status, "");
    return;
  }
  base::Value::ListStorage attribute_values;
  for (const std::string& attribute : attributes) {
    attribute_values.emplace_back(base::Value(attribute));
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(attribute_values, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kGetElementAttributeScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResultForString,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateControllerStatusWithValue<std::string>,
                         WebControllerErrorInfoProto::GET_STRING_ATTRIBUTE,
                         std::move(callback))));
}

void WebController::SelectFieldValue(
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetFunctionDeclaration(std::string(kSelectFieldValueScript))
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResult, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateWebControllerStatus,
                         WebControllerErrorInfoProto::SELECT_FIELD_VALUE,
                         std::move(callback))));
}

void WebController::SetValueAttribute(
    const std::string& value,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  AddRuntimeCallArgument(value, &argument);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kSetValueAttributeScript))
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResult, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateWebControllerStatus,
                         WebControllerErrorInfoProto::SET_VALUE_ATTRIBUTE,
                         std::move(callback))));
}

void WebController::SetAttribute(
    const std::vector<std::string>& attributes,
    const std::string& value,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " attributes=[" << base::JoinString(attributes, ",")
           << "], value=" << value;

  if (attributes.empty()) {
    ClientStatus error_status = UnexpectedErrorStatus(__FILE__, __LINE__);
    FillWebControllerErrorInfo(WebControllerErrorInfoProto::SET_ATTRIBUTE,
                               &error_status);
    std::move(callback).Run(error_status);
    return;
  }
  base::Value::ListStorage attribute_values;
  for (const std::string& attribute : attributes) {
    attribute_values.emplace_back(base::Value(attribute));
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  AddRuntimeCallArgument(attribute_values, &arguments);
  AddRuntimeCallArgument(value, &arguments);
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kSetAttributeScript))
          .Build(),
      element.node_frame_id(),
      base::BindOnce(&WebController::OnJavaScriptResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&DecorateWebControllerStatus,
                                    WebControllerErrorInfoProto::SET_ATTRIBUTE,
                                    std::move(callback))));
}

void WebController::SendTextInput(
    int key_press_delay_in_millisecond,
    const std::string& value,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  SendKeyboardInput(UTF8ToUnicode(value), key_press_delay_in_millisecond,
                    element, std::move(callback));
}

void WebController::SendKeyboardInput(
    const std::vector<UChar32>& codepoints,
    const int key_press_delay_in_millisecond,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::vector<KeyEvent> key_events;
  for (UChar32 codepoint : codepoints) {
    key_events.emplace_back(
        SendKeyboardInputWorker::KeyEventFromCodepoint(codepoint));
  }
  auto worker =
      std::make_unique<SendKeyboardInputWorker>(devtools_client_.get());
  auto* ptr = worker.get();
  pending_workers_.emplace_back(std::move(worker));
  ptr->Start(
      element.node_frame_id(), key_events, key_press_delay_in_millisecond,
      base::BindOnce(&DecorateWebControllerStatus,
                     WebControllerErrorInfoProto::SEND_KEYBOARD_INPUT,
                     base::BindOnce(&WebController::OnSendKeyboardInputDone,
                                    weak_ptr_factory_.GetWeakPtr(), ptr,
                                    std::move(callback))));
}

void WebController::SendKeyEvent(
    const KeyEvent& key_event,
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  auto worker =
      std::make_unique<SendKeyboardInputWorker>(devtools_client_.get());
  auto* ptr = worker.get();
  pending_workers_.emplace_back(std::move(worker));
  ptr->Start(
      element.node_frame_id(), {key_event}, 0,
      base::BindOnce(&DecorateWebControllerStatus,
                     WebControllerErrorInfoProto::SEND_KEY_EVENT,
                     base::BindOnce(&WebController::OnSendKeyboardInputDone,
                                    weak_ptr_factory_.GetWeakPtr(), ptr,
                                    std::move(callback))));
}

void WebController::OnSendKeyboardInputDone(
    SendKeyboardInputWorker* worker_to_release,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  base::EraseIf(pending_workers_, [worker_to_release](const auto& worker) {
    return worker.get() == worker_to_release;
  });
  std::move(callback).Run(status);
}

void WebController::FocusField(
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  auto wrapped_callback = GetAssistantActionRunningStateRetainingCallback(
      element, std::move(callback));

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetFunctionDeclaration(std::string(kFocusFieldScript))
          .Build(),
      element.node_frame_id(),
      base::BindOnce(&WebController::OnJavaScriptResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&DecorateWebControllerStatus,
                                    WebControllerErrorInfoProto::FOCUS_FIELD,
                                    std::move(wrapped_callback))));
}

void WebController::GetVisualViewport(
    base::OnceCallback<void(const ClientStatus&, const RectF&)> callback) {
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(std::string(kGetVisualViewport))
          .SetReturnByValue(true)
          .Build(),
      /* node_frame_id= */ std::string(),
      base::BindOnce(&WebController::OnGetVisualViewport,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetVisualViewport(
    base::OnceCallback<void(const ClientStatus&, const RectF&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok() || !result->GetResult()->HasValue() ||
      !result->GetResult()->GetValue()->is_list() ||
      result->GetResult()->GetValue()->GetList().size() != 4u) {
    VLOG(1) << __func__ << " Failed to get visual viewport: " << status;
    std::move(callback).Run(
        JavaScriptErrorStatus(reply_status, __FILE__, __LINE__, nullptr),
        RectF());
    return;
  }
  const auto& list = result->GetResult()->GetValue()->GetList();
  // Value::GetDouble() is safe to call without checking the value type; it'll
  // return 0.0 if the value has the wrong type.

  float left = static_cast<float>(list[0].GetDouble());
  float top = static_cast<float>(list[1].GetDouble());
  float width = static_cast<float>(list[2].GetDouble());
  float height = static_cast<float>(list[3].GetDouble());

  RectF rect;
  rect.left = left;
  rect.top = top;
  rect.right = left + width;
  rect.bottom = top + height;

  std::move(callback).Run(OkClientStatus(), rect);
}

void WebController::GetElementRect(
    const ElementFinder::Result& element,
    ElementRectGetter::ElementRectCallback callback) {
  std::unique_ptr<ElementRectGetter> getter =
      std::make_unique<ElementRectGetter>(devtools_client_.get());
  auto* ptr = getter.get();
  pending_workers_.emplace_back(std::move(getter));
  ptr->Start(
      // TODO(b/172041811): Ownership of element.
      std::make_unique<ElementFinder::Result>(element),
      base::BindOnce(&WebController::OnGetElementRect,
                     weak_ptr_factory_.GetWeakPtr(), ptr, std::move(callback)));
}

void WebController::OnGetElementRect(
    ElementRectGetter* getter_to_release,
    ElementRectGetter::ElementRectCallback callback,
    const ClientStatus& rect_status,
    const RectF& element_rect) {
  base::EraseIf(pending_workers_, [getter_to_release](const auto& worker) {
    return worker.get() == getter_to_release;
  });
  std::move(callback).Run(rect_status, element_rect);
}

void WebController::GetOuterHtml(
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetFunctionDeclaration(std::string(kGetOuterHtmlScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResultForString,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateControllerStatusWithValue<std::string>,
                         WebControllerErrorInfoProto::GET_OUTER_HTML,
                         std::move(callback))));
}

void WebController::GetOuterHtmls(
    const ElementFinder::Result& elements,
    base::OnceCallback<void(const ClientStatus&,
                            const std::vector<std::string>&)> callback) {
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(elements.object_id())
          .SetFunctionDeclaration(std::string(kGetOuterHtmlsScript))
          .SetReturnByValue(true)
          .Build(),
      elements.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResultForStringArray,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &DecorateControllerStatusWithValue<std::vector<std::string>>,
              WebControllerErrorInfoProto::GET_OUTER_HTML,
              std::move(callback))));
}

void WebController::GetElementTag(
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetFunctionDeclaration(std::string(kGetElementTagScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResultForString,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateControllerStatusWithValue<std::string>,
                         WebControllerErrorInfoProto::GET_ELEMENT_TAG,
                         std::move(callback))));
}

void WebController::SendChangeEvent(
    const ElementFinder::Result& element,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element.object_id())
          .SetFunctionDeclaration(std::string(kSendChangeEventScript))
          .SetReturnByValue(true)
          .Build(),
      element.node_frame_id(),
      base::BindOnce(
          &WebController::OnJavaScriptResult, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&DecorateWebControllerStatus,
                         WebControllerErrorInfoProto::SEND_CHANGE_EVENT,
                         std::move(callback))));
}

void WebController::DispatchJsEvent(
    base::OnceCallback<void(const ClientStatus&)> callback) const {
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(kDispatchEventToDocumentScript)
          .SetReturnByValue(true)
          .Build(),
      ElementFinder::Result().node_frame_id(),
      base::BindOnce(
          &WebController::OnDispatchJsEvent, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &DecorateWebControllerStatus,
              WebControllerErrorInfoProto::DISPATCH_EVENT_ON_DOCUMENT,
              std::move(callback))));
}

void WebController::OnDispatchJsEvent(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const DevtoolsClient::ReplyStatus& reply_status,
    std::unique_ptr<runtime::EvaluateResult> result) const {
  ClientStatus status =
      CheckJavaScriptResult(reply_status, result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    VLOG(1) << __func__
            << "Failed dispatching JavaScript event with status: " << status;
  }
  std::move(callback).Run(status);
}

base::WeakPtr<WebController> WebController::GetWeakPtr() const {
  return weak_ptr_factory_.GetWeakPtr();
}

WebController::ScopedAssistantActionStateRunning::
    ScopedAssistantActionStateRunning(
        content::WebContents* web_contents,
        content::RenderFrameHost* render_frame_host)
    : content::WebContentsObserver(web_contents),
      render_frame_host_(render_frame_host) {
  SetAssistantActionState(/* running= */ true);
}

WebController::ScopedAssistantActionStateRunning::
    ~ScopedAssistantActionStateRunning() {
  SetAssistantActionState(/* running= */ false);
}

void WebController::ScopedAssistantActionStateRunning::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host_ == render_frame_host)
    render_frame_host_ = nullptr;
}

void WebController::ScopedAssistantActionStateRunning::SetAssistantActionState(
    bool running) {
  if (render_frame_host_ == nullptr)
    return;

  ContentAutofillDriver* content_autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(render_frame_host_);
  if (content_autofill_driver != nullptr) {
    content_autofill_driver->GetAutofillAgent()->SetAssistantActionState(
        running);
  }
}

void WebController::RetainAssistantActionRunningStateAndExecuteCallback(
    std::unique_ptr<ScopedAssistantActionStateRunning> scoped_state,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& client_status) {
  // Deallocating the ScopedAssistantActionStateRunning sets the running state
  // to "not running" again.
  scoped_state.reset();

  std::move(callback).Run(client_status);
}

base::OnceCallback<void(const ClientStatus&)>
WebController::GetAssistantActionRunningStateRetainingCallback(
    const ElementFinder::Result& element_result,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  ContentAutofillDriver* content_autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(
          element_result.container_frame_host);
  if (content_autofill_driver == nullptr) {
    return callback;
  }

  auto scoped_assistant_action_state_running =
      std::make_unique<ScopedAssistantActionStateRunning>(
          web_contents_, element_result.container_frame_host);

  return base::BindOnce(
      &WebController::RetainAssistantActionRunningStateAndExecuteCallback,
      weak_ptr_factory_.GetWeakPtr(),
      std::move(scoped_assistant_action_state_running), std::move(callback));
}

}  // namespace autofill_assistant
