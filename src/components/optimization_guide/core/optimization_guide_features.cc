// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/variations/hashing.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"

namespace optimization_guide {
namespace features {

// Enables the syncing of the Optimization Hints component, which provides
// hints for what Previews can be applied on a page load.
const base::Feature kOptimizationHints {
  "OptimizationHints",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // !defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_ANDROID)
};

// Feature flag that contains a feature param that specifies the field trials
// that are allowed to be sent up to the Optimization Guide Server.
const base::Feature kOptimizationHintsFieldTrials{
    "OptimizationHintsFieldTrials", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables fetching from a remote Optimization Guide Service.
const base::Feature kRemoteOptimizationGuideFetching {
  "OptimizationHintsFetching",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // !defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_ANDROID)
};

const base::Feature kRemoteOptimizationGuideFetchingAnonymousDataConsent{
    "OptimizationHintsFetchingAnonymousDataConsent",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables performance info in the context menu and fetching from a remote
// Optimization Guide Service.
const base::Feature kContextMenuPerformanceInfoAndRemoteHintFetching{
    "ContextMenuPerformanceInfoAndRemoteHintFetching",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the prediction of optimization targets.
const base::Feature kOptimizationTargetPrediction{
    "OptimizationTargetPrediction", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the downloading of models.
const base::Feature kOptimizationGuideModelDownloading {
  "OptimizationGuideModelDownloading",
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // BUILD_WITH_TFLITE_LIB
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // !BUILD_WITH_TFLITE_LIB
};

// Enables page content to be annotated.
const base::Feature kPageContentAnnotations{"PageContentAnnotations",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// This feature flag does not turn off any behavior, it is only used for
// experiment parameters.
const base::Feature kPageTextExtraction{
    "OptimizationGuidePageContentExtraction", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the model file to be loaded for each execution, then unloaded on
// completion.
const base::Feature kLoadModelFileForEachExecution{
    "LoadModelFileForEachExecution", base::FEATURE_DISABLED_BY_DEFAULT};

// The default value here is a bit of a guess.
// TODO(crbug/1163244): This should be tuned once metrics are available.
base::TimeDelta PageTextExtractionOutstandingRequestsGracePeriod() {
  return base::TimeDelta::FromMilliseconds(GetFieldTrialParamByFeatureAsInt(
      kPageTextExtraction, "outstanding_requests_grace_period_ms", 1000));
}

bool ShouldBatchUpdateHintsForActiveTabsAndTopHosts() {
  if (base::FeatureList::IsEnabled(kRemoteOptimizationGuideFetching)) {
    return GetFieldTrialParamByFeatureAsBool(kRemoteOptimizationGuideFetching,
                                             "batch_update_hints_for_top_hosts",
                                             true);
  }
  return false;
}

size_t MaxHintsFetcherTopHostBlocklistSize() {
  // The blocklist will be limited to the most engaged hosts and will hold twice
  // (2*N) as many hosts that the HintsFetcher request hints for. The extra N
  // hosts on the blocklist are meant to cover the case that the engagement
  // scores on some of the top N host engagement scores decay and they fall out
  // of the top N.
  return GetFieldTrialParamByFeatureAsInt(kRemoteOptimizationGuideFetching,
                                          "top_host_blacklist_size_multiplier",
                                          3) *
         MaxHostsForOptimizationGuideServiceHintsFetch();
}

size_t MaxHostsForOptimizationGuideServiceHintsFetch() {
  return GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "max_hosts_for_optimization_guide_service_hints_fetch", 30);
}

size_t MaxUrlsForOptimizationGuideServiceHintsFetch() {
  return GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "max_urls_for_optimization_guide_service_hints_fetch", 30);
}

size_t MaxHostsForRecordingSuccessfullyCovered() {
  return GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "max_hosts_for_recording_successfully_covered", 200);
}

double MinTopHostEngagementScoreThreshold() {
  // The default initial site engagement score for a navigation is 3.0, 1.5
  // points for a navigation from the omnibox and 1.5 points for the first
  // navigation of the day.
  return GetFieldTrialParamByFeatureAsDouble(
      kRemoteOptimizationGuideFetching,
      "min_top_host_engagement_score_threshold", 2.0);
}

base::TimeDelta StoredFetchedHintsFreshnessDuration() {
  return base::TimeDelta::FromDays(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "max_store_duration_for_featured_hints_in_days", 7));
}

base::TimeDelta DurationApplyLowEngagementScoreThreshold() {
  return base::TimeDelta::FromDays(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "duration_apply_low_engagement_score_threshold_in_days", 30));
}

std::string GetOptimizationGuideServiceAPIKey() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOptimizationGuideServiceAPIKey)) {
    return command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceAPIKey);
  }

  return google_apis::GetAPIKey();
}

GURL GetOptimizationGuideServiceGetHintsURL() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOptimizationGuideServiceGetHintsURL)) {
    // Assume the command line switch is correct and return it.
    return GURL(command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceGetHintsURL));
  }

  std::string url = base::GetFieldTrialParamValueByFeature(
      kRemoteOptimizationGuideFetching, "optimization_guide_service_url");
  if (url.empty() || !GURL(url).SchemeIs(url::kHttpsScheme)) {
    if (!url.empty())
      LOG(WARNING)
          << "Empty or invalid optimization_guide_service_url provided: "
          << url;
    return GURL(kOptimizationGuideServiceGetHintsDefaultURL);
  }

  return GURL(url);
}

GURL GetOptimizationGuideServiceGetModelsURL() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kOptimizationGuideServiceGetModelsURL)) {
    // Assume the command line switch is correct and return it.
    return GURL(command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceGetModelsURL));
  }

  GURL get_models_url(kOptimizationGuideServiceGetModelsDefaultURL);
  CHECK(get_models_url.SchemeIs(url::kHttpsScheme));
  return get_models_url;
}

bool IsOptimizationHintsEnabled() {
  return base::FeatureList::IsEnabled(kOptimizationHints);
}

bool IsRemoteFetchingEnabled() {
  return base::FeatureList::IsEnabled(kRemoteOptimizationGuideFetching);
}

bool IsRemoteFetchingForAnonymousDataConsentEnabled() {
  return base::FeatureList::IsEnabled(
      kRemoteOptimizationGuideFetchingAnonymousDataConsent);
}

bool IsRemoteFetchingExplicitlyAllowedForPerformanceInfo() {
  return base::FeatureList::IsEnabled(
      kContextMenuPerformanceInfoAndRemoteHintFetching);
}

int MaxServerBloomFilterByteSize() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kOptimizationHints, "max_bloom_filter_byte_size", 250 * 1024 /* 250KB */);
}

base::Optional<net::EffectiveConnectionType>
GetMaxEffectiveConnectionTypeForNavigationHintsFetch() {
  std::string param_value = base::GetFieldTrialParamValueByFeature(
      kRemoteOptimizationGuideFetching,
      "max_effective_connection_type_for_navigation_hints_fetch");

  // Use a default value.
  if (param_value.empty())
    return net::EFFECTIVE_CONNECTION_TYPE_4G;

  return net::GetEffectiveConnectionTypeForName(param_value);
}

base::TimeDelta GetHostHintsFetchRefreshDuration() {
  return base::TimeDelta::FromHours(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching, "hints_fetch_refresh_duration_in_hours",
      72));
}

base::TimeDelta GetActiveTabsFetchRefreshDuration() {
  return base::TimeDelta::FromHours(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "active_tabs_fetch_refresh_duration_in_hours", 1));
}

base::TimeDelta GetActiveTabsStalenessTolerance() {
  // 90 days initially chosen since that's how long local history lasts for.
  return base::TimeDelta::FromDays(GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "active_tabs_staleness_tolerance_in_days", 90));
}

size_t MaxConcurrentPageNavigationFetches() {
  // If overridden, this needs to be large enough where we do not thrash the
  // inflight page navigations since if we approach the limit here, we will
  // abort the oldest page navigation fetch that is in flight.
  return GetFieldTrialParamByFeatureAsInt(
      kRemoteOptimizationGuideFetching,
      "max_concurrent_page_navigation_fetches", 20);
}

int ActiveTabsHintsFetchRandomMinDelaySecs() {
  return GetFieldTrialParamByFeatureAsInt(kRemoteOptimizationGuideFetching,
                                          "fetch_random_min_delay_secs", 30);
}

int ActiveTabsHintsFetchRandomMaxDelaySecs() {
  return GetFieldTrialParamByFeatureAsInt(kRemoteOptimizationGuideFetching,
                                          "fetch_random_max_delay_secs", 60);
}

base::TimeDelta StoredHostModelFeaturesFreshnessDuration() {
  return base::TimeDelta::FromDays(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction,
      "max_store_duration_for_host_model_features_in_days", 7));
}

base::TimeDelta StoredModelsInactiveDuration() {
  return base::TimeDelta::FromDays(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "inactive_duration_for_models_in_days",
      30));
}

base::TimeDelta URLKeyedHintValidCacheDuration() {
  return base::TimeDelta::FromSeconds(GetFieldTrialParamByFeatureAsInt(
      kOptimizationHints, "max_url_keyed_hint_valid_cache_duration_in_seconds",
      60 * 60 /* 1 hour */));
}

size_t MaxHostsForOptimizationGuideServiceModelsFetch() {
  return GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction,
      "max_hosts_for_optimization_guide_service_models_fetch", 30);
}

size_t MaxHostModelFeaturesCacheSize() {
  return GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "max_host_model_features_cache_size", 100);
}

size_t MaxHostKeyedHintCacheSize() {
  size_t max_host_keyed_hint_cache_size = GetFieldTrialParamByFeatureAsInt(
      kOptimizationHints, "max_host_keyed_hint_cache_size", 30);
  return max_host_keyed_hint_cache_size;
}

size_t MaxURLKeyedHintCacheSize() {
  size_t max_url_keyed_hint_cache_size = GetFieldTrialParamByFeatureAsInt(
      kOptimizationHints, "max_url_keyed_hint_cache_size", 30);
  DCHECK_GE(max_url_keyed_hint_cache_size,
            MaxUrlsForOptimizationGuideServiceHintsFetch());
  return max_url_keyed_hint_cache_size;
}

bool ShouldPersistHintsToDisk() {
  return GetFieldTrialParamByFeatureAsBool(kOptimizationHints,
                                           "persist_hints_to_disk", true);
}

bool ShouldOverrideOptimizationTargetDecisionForMetricsPurposes(
    proto::OptimizationTarget optimization_target) {
  if (optimization_target != proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      kOptimizationTargetPrediction, "painful_page_load_metrics_only", false);
}

int PredictionModelFetchRandomMinDelaySecs() {
  return GetFieldTrialParamByFeatureAsInt(kOptimizationTargetPrediction,
                                          "fetch_random_min_delay_secs", 30);
}

int PredictionModelFetchRandomMaxDelaySecs() {
  return GetFieldTrialParamByFeatureAsInt(kOptimizationTargetPrediction,
                                          "fetch_random_max_delay_secs", 60);
}

base::TimeDelta PredictionModelFetchRetryDelay() {
  return base::TimeDelta::FromMinutes(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "fetch_retry_minutes", 2));
}

base::TimeDelta PredictionModelFetchInterval() {
  return base::TimeDelta::FromHours(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction, "fetch_interval_hours", 24));
}

base::flat_set<std::string> ExternalAppPackageNamesApprovedForFetch() {
  std::string value = base::GetFieldTrialParamValueByFeature(
      kRemoteOptimizationGuideFetching, "approved_external_app_packages");
  if (value.empty())
    return {};

  std::vector<std::string> app_packages_list = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return base::flat_set<std::string>(app_packages_list.begin(),
                                     app_packages_list.end());
}

base::flat_set<uint32_t> FieldTrialNameHashesAllowedForFetch() {
  std::string value = base::GetFieldTrialParamValueByFeature(
      kOptimizationHintsFieldTrials, "allowed_field_trial_names");
  if (value.empty())
    return {};

  std::vector<std::string> allowed_field_trial_names = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  base::flat_set<uint32_t> allowed_field_trial_name_hashes;
  for (const auto& allowed_field_trial_name : allowed_field_trial_names) {
    allowed_field_trial_name_hashes.insert(
        variations::HashName(allowed_field_trial_name));
  }
  return allowed_field_trial_name_hashes;
}

bool IsModelDownloadingEnabled() {
  return base::FeatureList::IsEnabled(kOptimizationGuideModelDownloading);
}

bool IsUnrestrictedModelDownloadingEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kOptimizationGuideModelDownloading, "unrestricted_model_downloading",
      false);
}

bool IsPageContentAnnotationEnabled() {
  return base::FeatureList::IsEnabled(kPageContentAnnotations);
}

uint64_t MaxSizeForPageContentTextDump() {
  return static_cast<uint64_t>(base::GetFieldTrialParamByFeatureAsInt(
      kPageContentAnnotations, "max_size_for_text_dump_in_bytes", 1024));
}

bool ShouldWriteContentAnnotationsToHistoryService() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kPageContentAnnotations, "write_to_history_service", true);
}

bool LoadModelFileForEachExecution() {
  return base::FeatureList::IsEnabled(kLoadModelFileForEachExecution);
}

}  // namespace features
}  // namespace optimization_guide
