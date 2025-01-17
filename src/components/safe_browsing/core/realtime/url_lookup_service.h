// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_REALTIME_URL_LOOKUP_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_REALTIME_URL_LOOKUP_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/safe_browsing/core/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/realtime/url_lookup_service_base.h"
#include "url/gurl.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace variations {
class VariationsService;
}

class PrefService;

namespace safe_browsing {

class SafeBrowsingTokenFetcher;
class ReferrerChainProvider;

// This class implements the real time lookup feature for a given user/profile.
// It is separated from the base class for logic that is related to consumer
// users.(See: go/chrome-protego-enterprise-dd)
class RealTimeUrlLookupService : public RealTimeUrlLookupServiceBase {
 public:
  // A callback via which the client of this component indicates whether they
  // are configured to support token fetches.
  using ClientConfiguredForTokenFetchesCallback =
      base::RepeatingCallback<bool(bool user_has_enabled_enhanced_protection)>;

  // |cache_manager|, |sync_service|, and |pref_service| may be null in tests.
  // |token_fetcher| may also be null, but in that case the passed-in
  // |client_token_config_callback| should return false to ensure that access
  // token fetches are not actually invoked.
  RealTimeUrlLookupService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      VerdictCacheManager* cache_manager,
      base::RepeatingCallback<ChromeUserPopulation()>
          get_user_population_callback,
      PrefService* pref_service,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      const ClientConfiguredForTokenFetchesCallback&
          client_token_config_callback,
      bool is_off_the_record,
      variations::VariationsService* variations_service,
      ReferrerChainProvider* referrer_chain_provider);
  ~RealTimeUrlLookupService() override;

  // RealTimeUrlLookupServiceBase:
  bool CanPerformFullURLLookup() const override;
  bool CanCheckSubresourceURL() const override;
  bool CanCheckSafeBrowsingDb() const override;
  void Shutdown() override;

 private:
  // RealTimeUrlLookupServiceBase:
  GURL GetRealTimeLookupUrl() const override;
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const override;
  bool CanPerformFullURLLookupWithToken() const override;
  bool CanAttachReferrerChain() const override;
  int GetReferrerUserGestureLimit() const override;
  void GetAccessToken(const GURL& url,
                      RTLookupRequestCallback request_callback,
                      RTLookupResponseCallback response_callback) override;
  base::Optional<std::string> GetDMTokenString() const override;
  std::string GetMetricSuffix() const override;
  bool ShouldIncludeCredentials() const override;

  // Called when the access token is obtained from |token_fetcher_|.
  void OnGetAccessToken(const GURL& url,
                        RTLookupRequestCallback request_callback,
                        RTLookupResponseCallback response_callback,
                        base::TimeTicks get_token_start_time,
                        const std::string& access_token);

  // Unowned object used for getting preference settings.
  PrefService* pref_service_;

  // The token fetcher used for getting access token.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  // The callback via which the client of this component indicates whether they
  // are configured to support token fetches.
  ClientConfiguredForTokenFetchesCallback client_token_config_callback_;

  // A boolean indicates whether the profile associated with this
  // |url_lookup_service| is an off the record profile.
  bool is_off_the_record_;

  // Unowned. For checking whether real-time checks can be enabled in a given
  // location.
  variations::VariationsService* variations_;

  friend class RealTimeUrlLookupServiceTest;

  base::WeakPtrFactory<RealTimeUrlLookupService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RealTimeUrlLookupService);

};  // class RealTimeUrlLookupService

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_REALTIME_URL_LOOKUP_SERVICE_H_
