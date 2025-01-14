// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/mock_devtools_observer.h"

#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

MockDevToolsObserver::MockDevToolsObserver() = default;
MockDevToolsObserver::~MockDevToolsObserver() = default;

mojo::PendingRemote<mojom::DevToolsObserver> MockDevToolsObserver::Bind() {
  mojo::PendingRemote<mojom::DevToolsObserver> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void MockDevToolsObserver::OnRawRequest(
    const std::string& devtools_request_id,
    const net::CookieAccessResultList& cookies_with_access_result,
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  raw_request_cookies_.insert(raw_request_cookies_.end(),
                              cookies_with_access_result.begin(),
                              cookies_with_access_result.end());
  got_raw_request_ = true;
  devtools_request_id_ = devtools_request_id;
  client_security_state_ = std::move(client_security_state);

  if (wait_for_raw_request_ &&
      raw_request_cookies_.size() >= wait_for_raw_request_goal_) {
    std::move(wait_for_raw_request_).Run();
  }
}

void MockDevToolsObserver::OnRawResponse(
    const std::string& devtools_request_id,
    const net::CookieAndLineAccessResultList& cookies_with_access_result,
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
    const base::Optional<std::string>& raw_response_headers,
    network::mojom::IPAddressSpace resource_address_space) {
  raw_response_cookies_.insert(raw_response_cookies_.end(),
                               cookies_with_access_result.begin(),
                               cookies_with_access_result.end());
  got_raw_response_ = true;
  devtools_request_id_ = devtools_request_id;
  resource_address_space_ = resource_address_space;

  raw_response_headers_ = raw_response_headers;

  if (wait_for_raw_response_ &&
      raw_response_cookies_.size() >= wait_for_raw_response_goal_) {
    std::move(wait_for_raw_response_).Run();
  }
}

void MockDevToolsObserver::OnPrivateNetworkRequest(
    const base::Optional<std::string>& devtools_request_id,
    const GURL& url,
    bool is_warning,
    network::mojom::IPAddressSpace resource_address_space,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  params_of_private_network_request_.emplace(devtools_request_id, url,
                                             is_warning, resource_address_space,
                                             std::move(client_security_state));
  wait_for_private_network_request_.Quit();
}

void MockDevToolsObserver::OnCorsPreflightRequest(
    const base::UnguessableToken& devtool_request_id,
    const network::ResourceRequest& request,
    const GURL& initiator_url,
    const std::string& initiator_devtool_request_id) {}

void MockDevToolsObserver::OnCorsPreflightResponse(
    const base::UnguessableToken& devtool_request_id,
    const GURL& url,
    network::mojom::URLResponseHeadPtr head) {}

void MockDevToolsObserver::OnCorsPreflightRequestCompleted(
    const base::UnguessableToken& devtool_request_id,
    const network::URLLoaderCompletionStatus& status) {}

void MockDevToolsObserver::OnTrustTokenOperationDone(
    const std::string& devtool_request_id,
    network::mojom::TrustTokenOperationResultPtr result) {}

void MockDevToolsObserver::OnCorsError(
    const base::Optional<std::string>& devtools_request_id,
    const base::Optional<::url::Origin>& initiator_origin,
    const GURL& url,
    const network::CorsErrorStatus& status) {
  params_of_cors_error_.emplace(devtools_request_id, initiator_origin, url,
                                status);
  wait_for_cors_error_.Quit();
}

void MockDevToolsObserver::Clone(
    mojo::PendingReceiver<DevToolsObserver> observer) {
  receivers_.Add(this, std::move(observer));
}

void MockDevToolsObserver::WaitUntilRawResponse(size_t goal) {
  if (raw_response_cookies_.size() < goal || !got_raw_response_) {
    wait_for_raw_response_goal_ = goal;
    base::RunLoop run_loop;
    wait_for_raw_response_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  EXPECT_EQ(goal, raw_response_cookies_.size());
}

void MockDevToolsObserver::WaitUntilRawRequest(size_t goal) {
  if (raw_request_cookies_.size() < goal || !got_raw_request_) {
    wait_for_raw_request_goal_ = goal;
    base::RunLoop run_loop;
    wait_for_raw_request_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  EXPECT_EQ(goal, raw_request_cookies_.size());
}

void MockDevToolsObserver::WaitUntilPrivateNetworkRequest() {
  wait_for_private_network_request_.Run();
}

void MockDevToolsObserver::WaitUntilCorsError() {
  wait_for_cors_error_.Run();
}

MockDevToolsObserver::OnPrivateNetworkRequestParams::
    OnPrivateNetworkRequestParams(
        const base::Optional<std::string>& devtools_request_id,
        const GURL& url,
        bool is_warning,
        network::mojom::IPAddressSpace resource_address_space,
        network::mojom::ClientSecurityStatePtr client_security_state)
    : devtools_request_id(devtools_request_id),
      url(url),
      is_warning(is_warning),
      resource_address_space(resource_address_space),
      client_security_state(std::move(client_security_state)) {}
MockDevToolsObserver::OnPrivateNetworkRequestParams::
    OnPrivateNetworkRequestParams(OnPrivateNetworkRequestParams&&) = default;
MockDevToolsObserver::OnPrivateNetworkRequestParams::
    ~OnPrivateNetworkRequestParams() = default;

MockDevToolsObserver::OnCorsErrorParams::OnCorsErrorParams(
    const base::Optional<std::string>& devtools_request_id,
    const base::Optional<::url::Origin>& initiator_origin,
    const GURL& url,
    const network::CorsErrorStatus& status)
    : devtools_request_id(devtools_request_id),
      initiator_origin(initiator_origin),
      url(url),
      status(status) {}
MockDevToolsObserver::OnCorsErrorParams::OnCorsErrorParams(
    OnCorsErrorParams&&) = default;
MockDevToolsObserver::OnCorsErrorParams::~OnCorsErrorParams() = default;

}  // namespace network
