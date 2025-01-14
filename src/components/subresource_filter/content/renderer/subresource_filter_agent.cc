// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/common/subresource_filter_utils.h"
#include "components/subresource_filter/content/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/content/renderer/web_document_subresource_filter_impl.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/frame/frame_ad_evidence.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/url_constants.h"

namespace subresource_filter {

SubresourceFilterAgent::SubresourceFilterAgent(
    content::RenderFrame* render_frame,
    UnverifiedRulesetDealer* ruleset_dealer,
    std::unique_ptr<AdResourceTracker> ad_resource_tracker)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<SubresourceFilterAgent>(render_frame),
      ruleset_dealer_(ruleset_dealer),
      ad_resource_tracker_(std::move(ad_resource_tracker)) {
  DCHECK(ruleset_dealer);
}

void SubresourceFilterAgent::Initialize() {
  const GURL& url = GetDocumentURL();
  // The initial empty document will always inherit activation.
  DCHECK(ShouldInheritActivation(url));

  // We must check for provisional here because in that case 2 RenderFrames will
  // be created for the same FrameTreeNode in the browser. The browser service
  // only expects us to call SendSubframeWasCreatedByAdScript() and
  // SendFrameIsAdSubframe() a single time each for a newly created RenderFrame,
  // so we must choose one. A provisional frame is created when a navigation is
  // performed cross-site and the navigation is done there to isolate it from
  // the previous frame tree. We choose to send this message from the initial
  // (non-provisional) "about:blank" frame that is created before the navigation
  // to match previous behaviour, and because this frame will always exist.
  // Whereas the provisional frame would only be created to perform the
  // navigation conditionally, so we ignore sending the IPC there.
  if (!IsMainFrame() && !IsProvisional()) {
    if (IsSubframeCreatedByAdScript())
      SendSubframeWasCreatedByAdScript();

    // As this is the initial empty document, we won't have received any message
    // from the browser and so we must calculate the ad status here.
    SetIsAdSubframeIfNecessary();
  }

  // `render_frame()` can be null in unit tests.
  if (render_frame()) {
    render_frame()->GetAssociatedInterfaceRegistry()->AddInterface(
        base::BindRepeating(
            &SubresourceFilterAgent::OnSubresourceFilterAgentRequest,
            base::Unretained(this)));

    if (IsMainFrame()) {
      // If a main frame has an activated opener, we will activate the
      // subresource filter for the initial empty document, which was created
      // before the constructor for `this`. This ensures that a popup's final
      // document is appropriately activated, even when the the initial
      // navigation is aborted and there are no further documents created.
      // TODO(dcheng): Navigation is an asynchronous operation, and the opener
      // frame may have been destroyed between the time the window is opened
      // and the RenderFrame in the window is constructed leading us to here.
      // To avoid that race condition the activation state would need to be
      // determined without the use of the opener frame.
      if (GetInheritedActivationState(render_frame()).activation_level !=
          mojom::ActivationLevel::kDisabled) {
        ConstructFilter(GetInheritedActivationStateForNewDocument(), url);
      }
    } else {
      // Child frames always have a parent, so the empty initial document can
      // always inherit activation.
      ConstructFilter(GetInheritedActivationStateForNewDocument(), url);
    }
  }
}

SubresourceFilterAgent::~SubresourceFilterAgent() {
  // Filter may outlive us, so reset the ad tracker.
  if (filter_for_last_created_document_)
    filter_for_last_created_document_->set_ad_resource_tracker(nullptr);
}

GURL SubresourceFilterAgent::GetDocumentURL() {
  return render_frame()->GetWebFrame()->GetDocument().Url();
}

bool SubresourceFilterAgent::IsMainFrame() {
  return render_frame()->IsMainFrame();
}

bool SubresourceFilterAgent::IsParentAdSubframe() {
  return render_frame()->GetWebFrame()->Parent()->IsAdSubframe();
}

bool SubresourceFilterAgent::IsProvisional() {
  return render_frame()->GetWebFrame()->IsProvisional();
}

bool SubresourceFilterAgent::IsSubframeCreatedByAdScript() {
  return render_frame()->GetWebFrame()->IsSubframeCreatedByAdScript();
}

bool SubresourceFilterAgent::HasDocumentLoader() {
  return render_frame()->GetWebFrame()->GetDocumentLoader();
}

void SubresourceFilterAgent::SetSubresourceFilterForCurrentDocument(
    std::unique_ptr<blink::WebDocumentSubresourceFilter> filter) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  DCHECK(web_frame->GetDocumentLoader());
  web_frame->GetDocumentLoader()->SetSubresourceFilter(filter.release());
}

void SubresourceFilterAgent::
    SignalFirstSubresourceDisallowedForCurrentDocument() {
  GetSubresourceFilterHost()->DidDisallowFirstSubresource();
}

void SubresourceFilterAgent::SendDocumentLoadStatistics(
    const mojom::DocumentLoadStatistics& statistics) {
  GetSubresourceFilterHost()->SetDocumentLoadStatistics(statistics.Clone());
}

void SubresourceFilterAgent::SendFrameIsAdSubframe() {
  GetSubresourceFilterHost()->FrameIsAdSubframe();
}

void SubresourceFilterAgent::SendSubframeWasCreatedByAdScript() {
  GetSubresourceFilterHost()->SubframeWasCreatedByAdScript();
}

bool SubresourceFilterAgent::IsAdSubframe() {
  return render_frame()->GetWebFrame()->IsAdSubframe();
}

void SubresourceFilterAgent::SetIsAdSubframe(
    blink::mojom::AdFrameType ad_frame_type) {
  render_frame()->GetWebFrame()->SetIsAdSubframe(ad_frame_type);
}

// static
mojom::ActivationState SubresourceFilterAgent::GetInheritedActivationState(
    content::RenderFrame* render_frame) {
  if (!render_frame)
    return mojom::ActivationState();

  blink::WebFrame* frame_to_inherit_from =
      render_frame->IsMainFrame() ? render_frame->GetWebFrame()->Opener()
                                  : render_frame->GetWebFrame()->Parent();

  if (!frame_to_inherit_from || !frame_to_inherit_from->IsWebLocalFrame())
    return mojom::ActivationState();

  blink::WebSecurityOrigin render_frame_origin =
      render_frame->GetWebFrame()->GetSecurityOrigin();
  blink::WebSecurityOrigin inherited_origin =
      frame_to_inherit_from->GetSecurityOrigin();

  // Only inherit from same-origin frames.
  if (render_frame_origin.IsSameOriginWith(inherited_origin)) {
    auto* agent =
        SubresourceFilterAgent::Get(content::RenderFrame::FromWebFrame(
            frame_to_inherit_from->ToWebLocalFrame()));
    if (agent && agent->filter_for_last_created_document_)
      return agent->filter_for_last_created_document_->activation_state();
  }

  return mojom::ActivationState();
}

void SubresourceFilterAgent::RecordHistogramsOnFilterCreation(
    const mojom::ActivationState& activation_state) {
  // Note: mojom::ActivationLevel used to be called mojom::ActivationState, the
  // legacy name is kept for the histogram.
  mojom::ActivationLevel activation_level = activation_state.activation_level;
  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.DocumentLoad.ActivationState",
                            activation_level);

  if (IsMainFrame()) {
    UMA_HISTOGRAM_BOOLEAN(
        "SubresourceFilter.MainFrameLoad.RulesetIsAvailableAnyActivationLevel",
        ruleset_dealer_->IsRulesetFileAvailable());
  }
  if (activation_level != mojom::ActivationLevel::kDisabled) {
    UMA_HISTOGRAM_BOOLEAN("SubresourceFilter.DocumentLoad.RulesetIsAvailable",
                          ruleset_dealer_->IsRulesetFileAvailable());
  }
}

void SubresourceFilterAgent::ResetInfoForNextDocument() {
  activation_state_for_next_document_ = mojom::ActivationState();
}

mojom::SubresourceFilterHost*
SubresourceFilterAgent::GetSubresourceFilterHost() {
  if (!subresource_filter_host_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &subresource_filter_host_);
  }
  return subresource_filter_host_.get();
}

void SubresourceFilterAgent::OnSubresourceFilterAgentRequest(
    mojo::PendingAssociatedReceiver<mojom::SubresourceFilterAgent> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void SubresourceFilterAgent::ActivateForNextCommittedLoad(
    mojom::ActivationStatePtr activation_state,
    blink::mojom::AdFrameType ad_frame_type) {
  activation_state_for_next_document_ = *activation_state;
  if (!IsMainFrame()) {
    SetIsAdSubframe(ad_frame_type);
  } else {
    DCHECK_EQ(ad_frame_type, blink::mojom::AdFrameType::kNonAd);
  }
}

void SubresourceFilterAgent::OnDestruct() {
  delete this;
}

void SubresourceFilterAgent::SetIsAdSubframeIfNecessary() {
  DCHECK(!IsAdSubframe());

  // TODO(alexmt): Store FrameAdEvidence on each frame, typically updated by the
  // browser but also populated here when the browser has not informed the
  // renderer.
  blink::FrameAdEvidence ad_evidence(IsParentAdSubframe());
  ad_evidence.set_created_by_ad_script(
      IsSubframeCreatedByAdScript()
          ? blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript
          : blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript);
  ad_evidence.set_is_complete();

  if (ad_evidence.IndicatesAdSubframe()) {
    blink::mojom::AdFrameType ad_frame_type =
        ad_evidence.parent_is_ad() ? blink::mojom::AdFrameType::kChildAd
                                   : blink::mojom::AdFrameType::kRootAd;
    SetIsAdSubframe(ad_frame_type);
    SendFrameIsAdSubframe();
  }
}

void SubresourceFilterAgent::DidCreateNewDocument() {
  // TODO(csharrison): Use WebURL and WebSecurityOrigin for efficiency here,
  // which requires changes to the unit tests.
  const GURL& url = GetDocumentURL();

  const mojom::ActivationState activation_state =
      ShouldInheritActivation(url) ? GetInheritedActivationStateForNewDocument()
                                   : activation_state_for_next_document_;

  ResetInfoForNextDocument();

  // Do not pollute the histograms with uninteresting main frame documents.
  const bool should_record_histograms =
      !IsMainFrame() || url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile();
  if (should_record_histograms) {
    RecordHistogramsOnFilterCreation(activation_state);
  }

  ConstructFilter(activation_state, url);
}

const mojom::ActivationState
SubresourceFilterAgent::GetInheritedActivationStateForNewDocument() {
  DCHECK(ShouldInheritActivation(GetDocumentURL()));
  return GetInheritedActivationState(render_frame());
}

void SubresourceFilterAgent::ConstructFilter(
    const mojom::ActivationState activation_state,
    const GURL& url) {
  // Filter may outlive us, so reset the ad tracker.
  if (filter_for_last_created_document_)
    filter_for_last_created_document_->set_ad_resource_tracker(nullptr);
  filter_for_last_created_document_.reset();

  if (activation_state.activation_level == mojom::ActivationLevel::kDisabled ||
      !ruleset_dealer_->IsRulesetFileAvailable())
    return;

  scoped_refptr<const MemoryMappedRuleset> ruleset =
      ruleset_dealer_->GetRuleset();
  if (!ruleset)
    return;

  base::OnceClosure first_disallowed_load_callback(
      base::BindOnce(&SubresourceFilterAgent::
                         SignalFirstSubresourceDisallowedForCurrentDocument,
                     AsWeakPtr()));
  auto filter = std::make_unique<WebDocumentSubresourceFilterImpl>(
      url::Origin::Create(url), activation_state, std::move(ruleset),
      std::move(first_disallowed_load_callback));
  filter->set_ad_resource_tracker(ad_resource_tracker_.get());
  filter_for_last_created_document_ = filter->AsWeakPtr();
  SetSubresourceFilterForCurrentDocument(std::move(filter));
}

void SubresourceFilterAgent::DidFailProvisionalLoad() {
  // TODO(engedy): Add a test with `frame-ancestor` violation to exercise this.
  ResetInfoForNextDocument();
}

void SubresourceFilterAgent::DidFinishLoad() {
  if (!filter_for_last_created_document_)
    return;
  const auto& statistics =
      filter_for_last_created_document_->filter().statistics();
  SendDocumentLoadStatistics(statistics);
}

void SubresourceFilterAgent::WillCreateWorkerFetchContext(
    blink::WebWorkerFetchContext* worker_fetch_context) {
  if (!filter_for_last_created_document_)
    return;
  if (!ruleset_dealer_->IsRulesetFileAvailable())
    return;
  base::File ruleset_file = ruleset_dealer_->DuplicateRulesetFile();
  if (!ruleset_file.IsValid())
    return;

  worker_fetch_context->SetSubresourceFilterBuilder(
      std::make_unique<WebDocumentSubresourceFilterImpl::BuilderImpl>(
          url::Origin::Create(GetDocumentURL()),
          filter_for_last_created_document_->filter().activation_state(),
          std::move(ruleset_file),
          base::BindOnce(&SubresourceFilterAgent::
                             SignalFirstSubresourceDisallowedForCurrentDocument,
                         AsWeakPtr())));
}

void SubresourceFilterAgent::OnOverlayPopupAdDetected() {
  GetSubresourceFilterHost()->OnAdsViolationTriggered(
      subresource_filter::mojom::AdsViolation::kOverlayPopupAd);
}
void SubresourceFilterAgent::OnLargeStickyAdDetected() {
  GetSubresourceFilterHost()->OnAdsViolationTriggered(
      subresource_filter::mojom::AdsViolation::kLargeStickyAd);
}

}  // namespace subresource_filter
