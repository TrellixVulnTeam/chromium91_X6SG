// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/seller_worklet.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8.h"

namespace auction_worklet {

namespace {

// Converts `auction_config` back to JSON format, and appends to args.
// Returns true if conversion succeeded.
//
// The resulting object will look something like this (based on example from
// explainer):
//
// {
//  'seller': 'https://www.example-ssp.com/',
//  'decisionLogicUrl': 'https://www.example-ssp.com/seller.js',
//  'trustedScoringSignalsUrl': ...,
//  'interestGroupBuyers': ['www.example-dsp.com', 'buyer2.com', ...],
//  'auctionSignals': {...},
//  'sellerSignals': {...},
//  'perBuyerSignals': {'www.example-dsp.com': {...},
//                      'www.another-buyer.com': {...},
//                       ...}
// }
bool AppendAuctionConfig(AuctionV8Helper* const v8_helper,
                         v8::Local<v8::Context> context,
                         const blink::mojom::AuctionAdConfig& auction_config,
                         std::vector<v8::Local<v8::Value>>* args) {
  // TODO(morlovich): Unclear on .Serialize vs .host() conventions.
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Object> auction_config_value = v8::Object::New(isolate);
  gin::Dictionary auction_config_dict(isolate, auction_config_value);
  if (!auction_config_dict.Set("seller", auction_config.seller.Serialize()) ||
      !auction_config_dict.Set("decisionLogicUrl",
                               auction_config.decision_logic_url.spec())) {
    return false;
  }

  if (auction_config.interest_group_buyers) {
    if (auction_config.interest_group_buyers->is_all_buyers()) {
      if (!auction_config_dict.Set("interestGroupBuyers", std::string("*")))
        return false;
    } else {
      std::vector<v8::Local<v8::Value>> interest_group_buyers;
      for (const url::Origin& buyer :
           auction_config.interest_group_buyers->get_buyers()) {
        v8::Local<v8::String> v8_buyer;
        if (!v8_helper->CreateUtf8String(buyer.host()).ToLocal(&v8_buyer))
          return false;
        interest_group_buyers.push_back(v8_buyer);
      }
      auction_config_dict.Set("interestGroupBuyers", interest_group_buyers);
    }
  }

  if (auction_config.auction_signals.has_value() &&
      !v8_helper->InsertJsonValue(context, "auctionSignals",
                                  auction_config.auction_signals.value(),
                                  auction_config_value)) {
    return false;
  }

  if (auction_config.seller_signals.has_value() &&
      !v8_helper->InsertJsonValue(context, "sellerSignals",
                                  auction_config.seller_signals.value(),
                                  auction_config_value)) {
    return false;
  }

  if (auction_config.per_buyer_signals.has_value()) {
    v8::Local<v8::Object> per_buyer_value = v8::Object::New(isolate);
    for (const auto& kv : auction_config.per_buyer_signals.value()) {
      if (!v8_helper->InsertJsonValue(context, kv.first.host(), kv.second,
                                      per_buyer_value)) {
        return false;
      }
    }
    auction_config_dict.Set("perBuyerSignals", per_buyer_value);
  }

  args->push_back(std::move(auction_config_value));
  return true;
}

}  // namespace

SellerWorklet::ScoreResult::ScoreResult() = default;

SellerWorklet::ScoreResult::ScoreResult(double score)
    : success(true), score(score) {
  DCHECK_GT(score, 0);
}

SellerWorklet::Report::Report() = default;

SellerWorklet::Report::Report(std::string signals_for_winner, GURL report_url)
    : success(true),
      signals_for_winner(std::move(signals_for_winner)),
      report_url(std::move(report_url)) {}

SellerWorklet::SellerWorklet(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& script_source_url,
    AuctionV8Helper* v8_helper,
    LoadWorkletCallback load_worklet_callback)
    : v8_helper_(v8_helper) {
  DCHECK(load_worklet_callback);
  worklet_loader_ = std::make_unique<WorkletLoader>(
      url_loader_factory, script_source_url, v8_helper,
      base::BindOnce(&SellerWorklet::OnDownloadComplete, base::Unretained(this),
                     std::move(load_worklet_callback)));
}

SellerWorklet::~SellerWorklet() = default;

SellerWorklet::ScoreResult SellerWorklet::ScoreAd(
    const std::string& ad_metadata_json,
    double bid,
    const blink::mojom::AuctionAdConfig& auction_config,
    const std::string& browser_signal_top_window_hostname,
    const url::Origin& browser_signal_interest_group_owner,
    const std::string& browser_signal_ad_render_fingerprint,
    base::TimeDelta browser_signal_bidding_duration) {
  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_);
  v8::Isolate* isolate = v8_helper_->isolate();
  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> args;
  if (!v8_helper_->AppendJsonValue(context, ad_metadata_json, &args))
    return ScoreResult();

  args.push_back(gin::ConvertToV8(isolate, bid));

  if (!AppendAuctionConfig(v8_helper_, context, auction_config, &args))
    return ScoreResult();

  // Placeholder for trustedScoringSignals, which isn't wired up yet.
  args.push_back(v8::Null(isolate));

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                browser_signal_top_window_hostname) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          browser_signal_interest_group_owner.Serialize()) ||
      !browser_signals_dict.Set("adRenderFingerprint",
                                browser_signal_ad_render_fingerprint) ||
      !browser_signals_dict.Set(
          "biddingDurationMsec",
          browser_signal_bidding_duration.InMilliseconds())) {
    return ScoreResult();
  }
  args.push_back(browser_signals);

  v8::Local<v8::Value> score_ad_result;
  double score;
  if (!v8_helper_
           ->RunScript(context, worklet_script_->Get(isolate), "scoreAd", args)
           .ToLocal(&score_ad_result) ||
      !gin::ConvertFromV8(isolate, score_ad_result, &score)) {
    return ScoreResult();
  }

  if (score <= 0 || std::isnan(score) || !std::isfinite(score))
    return ScoreResult();

  return ScoreResult(score);
}

SellerWorklet::Report SellerWorklet::ReportResult(
    const blink::mojom::AuctionAdConfig& auction_config,
    const std::string& browser_signal_top_window_hostname,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    const std::string& browser_signal_ad_render_fingerprint,
    double browser_signal_bid,
    double browser_signal_desirability) {
  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_);
  v8::Isolate* isolate = v8_helper_->isolate();

  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  ReportBindings report_bindings(v8_helper_, global_template);

  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  v8::Local<v8::Context> context = v8_helper_->CreateContext(global_template);
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> args;
  if (!AppendAuctionConfig(v8_helper_, context, auction_config, &args))
    return Report();

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                browser_signal_top_window_hostname) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          browser_signal_interest_group_owner.Serialize()) ||
      !browser_signals_dict.Set("renderUrl",
                                browser_signal_render_url.spec()) ||
      !browser_signals_dict.Set("adRenderFingerprint",
                                browser_signal_ad_render_fingerprint) ||
      !browser_signals_dict.Set("bid", browser_signal_bid) ||
      !browser_signals_dict.Set("desirability", browser_signal_desirability)) {
    return Report();
  }
  args.push_back(browser_signals);

  v8::Local<v8::Value> signals_for_winner_value;
  if (!v8_helper_
           ->RunScript(context, worklet_script_->Get(isolate), "reportResult",
                       args)
           .ToLocal(&signals_for_winner_value)) {
    return Report();
  }

  // Consider lack of error but no return value type, or a return value that
  // can't be converted to JSON a valid result.
  std::string signals_for_winner;
  if (!v8_helper_->ExtractJson(context, signals_for_winner_value,
                               &signals_for_winner)) {
    signals_for_winner = "null";
  }

  return Report(signals_for_winner, report_bindings.report_url());
}

void SellerWorklet::OnDownloadComplete(
    LoadWorkletCallback load_worklet_callback,
    std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script) {
  worklet_loader_.reset();
  worklet_script_ = std::move(worklet_script);
  std::move(load_worklet_callback).Run(worklet_script_ != nullptr);
}

}  // namespace auction_worklet
