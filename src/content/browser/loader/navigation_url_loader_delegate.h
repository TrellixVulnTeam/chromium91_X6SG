// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_DELEGATE_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/navigation/navigation_policy.h"

namespace net {
class NetworkIsolationKey;
struct RedirectInfo;
}

namespace network {
struct URLLoaderCompletionStatus;
}

namespace content {

class NavigationEarlyHintsManager;
struct GlobalRequestID;
struct SubresourceLoaderParams;

// The delegate interface to NavigationURLLoader.
class CONTENT_EXPORT NavigationURLLoaderDelegate {
 public:
  // Conveys information related to Early Hints responses.
  struct CONTENT_EXPORT EarlyHints {
    EarlyHints();
    ~EarlyHints();

    EarlyHints(EarlyHints&& other);
    EarlyHints& operator=(EarlyHints&& other);

    EarlyHints(const EarlyHints& other) = delete;
    EarlyHints& operator=(const EarlyHints& other) = delete;

    // True when at least one preload Link header was received during a
    // main frame navigation.
    bool was_preload_link_header_received = false;
    // Non-null when at least one preload is actually requested.
    std::unique_ptr<NavigationEarlyHintsManager> manager;
  };

  // Called when the request is redirected. Call FollowRedirect to continue
  // processing the request.
  //
  // |network_isolation_key| is the NetworkIsolationKey associated with the
  // request that was redirected, not the one that will be used if the redirect
  // is followed.
  virtual void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      const net::NetworkIsolationKey& network_isolation_key,
      network::mojom::URLResponseHeadPtr response) = 0;

  // Called when the request receives its response. No further calls will be
  // made to the delegate. The response body can be retrieved by implementing an
  // URLLoaderClient and binding the |url_loader_client_endpoints|.
  // |navigation_data| is passed to the NavigationHandle.
  // |subresource_loader_params| is used in the network service only for passing
  // necessary info to create a custom subresource loader in the renderer
  // process if the navigated context is controlled by a request interceptor
  // like AppCache or ServiceWorker.
  //
  // |is_download| is true if the request must be downloaded, if it isn't
  // disallowed.
  //
  // |download_policy| specifies if downloading is disallowed.
  //
  // Invoking this method will delete the URLLoader, so it needs to take all
  // arguments by value.
  virtual void OnResponseStarted(
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      GlobalRequestID request_id,
      bool is_download,
      blink::NavigationDownloadPolicy download_policy,
      net::NetworkIsolationKey network_isolation_key,
      base::Optional<SubresourceLoaderParams> subresource_loader_params,
      EarlyHints early_hints) = 0;

  // Called if the request fails before receving a response. Specific
  // fields which are used: |status.error_code| holds the error code
  // for the failure; |status.extended_error_code| holds details if
  // available; |status.exists_in_cache| indicates a stale cache
  // entry; |status.ssl_info| is available when |status.error_code| is
  // a certificate error.
  virtual void OnRequestFailed(
      const network::URLLoaderCompletionStatus& status) = 0;

 protected:
  NavigationURLLoaderDelegate() {}
  virtual ~NavigationURLLoaderDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_DELEGATE_H_
