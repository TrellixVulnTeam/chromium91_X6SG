// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {

namespace cors {

namespace {

using WithTrustedHeaderClient = PreflightController::WithTrustedHeaderClient;
using WithNonWildcardRequestHeadersSupport =
    PreflightController::WithNonWildcardRequestHeadersSupport;

TEST(PreflightControllerCreatePreflightRequestTest, LexicographicalOrder) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("Orange", "Orange");
  request.headers.SetHeader("Apple", "Red");
  request.headers.SetHeader("Kiwifruit", "Green");
  request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                            "application/octet-stream");
  request.headers.SetHeader("Strawberry", "Red");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  std::string header;
  EXPECT_TRUE(
      preflight->headers.GetHeader(net::HttpRequestHeaders::kOrigin, &header));
  EXPECT_EQ("null", header);

  EXPECT_TRUE(preflight->headers.GetHeader(
      header_names::kAccessControlRequestHeaders, &header));
  EXPECT_EQ("apple,content-type,kiwifruit,orange,strawberry", header);
}

TEST(PreflightControllerCreatePreflightRequestTest, ExcludeSimpleHeaders) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("Accept", "everything");
  request.headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                            "everything");
  request.headers.SetHeader("Content-Language", "everything");
  request.headers.SetHeader("Save-Data", "on");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  // Do not emit empty-valued headers; an empty list of non-"CORS safelisted"
  // request headers should cause "Access-Control-Request-Headers:" to be
  // left out in the preflight request.
  std::string header;
  EXPECT_FALSE(preflight->headers.GetHeader(
      header_names::kAccessControlRequestHeaders, &header));
}

TEST(PreflightControllerCreatePreflightRequestTest, Credentials) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("Orange", "Orange");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  EXPECT_EQ(mojom::CredentialsMode::kOmit, preflight->credentials_mode);
}

TEST(PreflightControllerCreatePreflightRequestTest,
     ExcludeSimpleContentTypeHeader) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                            "text/plain");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  // Empty list also; see comment in test above.
  std::string header;
  EXPECT_FALSE(preflight->headers.GetHeader(
      header_names::kAccessControlRequestHeaders, &header));
}

TEST(PreflightControllerCreatePreflightRequestTest, IncludeSecFetchModeHeader) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("X-Custom-Header", "foobar");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  std::string header;
  EXPECT_TRUE(preflight->headers.GetHeader("Sec-Fetch-Mode", &header));
  EXPECT_EQ("cors", header);
}

TEST(PreflightControllerCreatePreflightRequestTest, IncludeNonSimpleHeader) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("X-Custom-Header", "foobar");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  std::string header;
  EXPECT_TRUE(preflight->headers.GetHeader(
      header_names::kAccessControlRequestHeaders, &header));
  EXPECT_EQ("x-custom-header", header);
}

TEST(PreflightControllerCreatePreflightRequestTest,
     IncludeNonSimpleContentTypeHeader) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                            "application/octet-stream");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  std::string header;
  EXPECT_TRUE(preflight->headers.GetHeader(
      header_names::kAccessControlRequestHeaders, &header));
  EXPECT_EQ("content-type", header);
}

TEST(PreflightControllerCreatePreflightRequestTest, ExcludeForbiddenHeaders) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader("referer", "https://www.google.com/");

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  std::string header;
  EXPECT_FALSE(preflight->headers.GetHeader(
      header_names::kAccessControlRequestHeaders, &header));
}

TEST(PreflightControllerCreatePreflightRequestTest, Tainted) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request, true);

  std::string header;
  EXPECT_TRUE(
      preflight->headers.GetHeader(net::HttpRequestHeaders::kOrigin, &header));
  EXPECT_EQ(header, "null");
}

TEST(PreflightControllerCreatePreflightRequestTest, FetchWindowId) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.request_initiator = url::Origin();
  request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                            "application/octet-stream");
  request.fetch_window_id = base::UnguessableToken::Create();

  std::unique_ptr<ResourceRequest> preflight =
      PreflightController::CreatePreflightRequestForTesting(request);

  EXPECT_EQ(request.fetch_window_id, preflight->fetch_window_id);
}

TEST(PreflightControllerOptionsTest, CheckOptions) {
  base::test::TaskEnvironment task_environment_(
      base::test::TaskEnvironment::MainThreadType::IO);
  TestURLLoaderFactory url_loader_factory;
  PreflightController preflight_controller(nullptr /* network_service */);

  network::ResourceRequest request;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin();
  preflight_controller.PerformPreflightCheck(
      base::BindOnce([](int, base::Optional<CorsErrorStatus>, bool) {}),
      request, WithTrustedHeaderClient(false),
      WithNonWildcardRequestHeadersSupport(false), false /* tainted */,
      TRAFFIC_ANNOTATION_FOR_TESTS, &url_loader_factory, net::IsolationInfo(),
      /*devtools_observer=*/mojo::NullRemote());

  preflight_controller.PerformPreflightCheck(
      base::BindOnce([](int, base::Optional<CorsErrorStatus>, bool) {}),
      request, WithTrustedHeaderClient(true),
      WithNonWildcardRequestHeadersSupport(false), false /* tainted */,
      TRAFFIC_ANNOTATION_FOR_TESTS, &url_loader_factory, net::IsolationInfo(),
      /*devtools_observer=*/mojo::NullRemote());

  ASSERT_EQ(2, url_loader_factory.NumPending());
  EXPECT_EQ(mojom::kURLLoadOptionAsCorsPreflight,
            url_loader_factory.GetPendingRequest(0)->options);
  EXPECT_EQ(mojom::kURLLoadOptionAsCorsPreflight |
                mojom::kURLLoadOptionUseHeaderClient,
            url_loader_factory.GetPendingRequest(1)->options);
}

class MockDevToolsObserver : public mojom::DevToolsObserver {
 public:
  explicit MockDevToolsObserver(
      mojo::PendingReceiver<mojom::DevToolsObserver> receiver) {
    receivers_.Add(this, std::move(receiver));
  }
  ~MockDevToolsObserver() override = default;

  MockDevToolsObserver(const MockDevToolsObserver&) = delete;
  MockDevToolsObserver& operator=(const MockDevToolsObserver&) = delete;

  mojo::PendingRemote<mojom::DevToolsObserver> Bind() {
    mojo::PendingRemote<mojom::DevToolsObserver> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void WaitUntilRequestCompleted() {
    if (completed_)
      return;
    base::RunLoop run_loop;
    wait_for_completed_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool on_raw_request_called() const { return on_raw_request_called_; }
  bool on_raw_response_called() const { return on_raw_response_called_; }
  const base::Optional<network::ResourceRequest>& preflight_request() const {
    return preflight_request_;
  }
  const network::mojom::URLResponseHeadPtr& preflight_response() const {
    return preflight_response_;
  }
  const base::Optional<network::URLLoaderCompletionStatus>& preflight_status()
      const {
    return preflight_status_;
  }
  const std::string& initiator_devtools_request_id() const {
    return initiator_devtools_request_id_;
  }

 private:
  // mojom::DevToolsObserver:
  void OnRawRequest(
      const std::string& devtools_request_id,
      const net::CookieAccessResultList& cookies_with_access_result,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
      network::mojom::ClientSecurityStatePtr client_security_state) override {
    on_raw_request_called_ = true;
  }
  void OnRawResponse(
      const std::string& devtools_request_id,
      const net::CookieAndLineAccessResultList& cookies_with_access_result,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
      const base::Optional<std::string>& raw_response_headers,
      network::mojom::IPAddressSpace resource_address_space) override {
    on_raw_response_called_ = true;
  }
  void OnCorsPreflightRequest(
      const base::UnguessableToken& devtool_request_id,
      const network::ResourceRequest& request,
      const GURL& initiator_url,
      const std::string& initiator_devtools_request_id) override {
    preflight_request_ = request;
    initiator_devtools_request_id_ = initiator_devtools_request_id;
  }
  void OnCorsPreflightResponse(
      const base::UnguessableToken& devtool_request_id,
      const GURL& url,
      network::mojom::URLResponseHeadPtr head) override {
    preflight_response_ = std::move(head);
  }
  void OnCorsPreflightRequestCompleted(
      const base::UnguessableToken& devtool_request_id,
      const network::URLLoaderCompletionStatus& status) override {
    completed_ = true;
    preflight_status_ = status;
    if (wait_for_completed_)
      std::move(wait_for_completed_).Run();
  }
  void OnCorsError(const base::Optional<std::string>& devtool_request_id,
                   const base::Optional<::url::Origin>& initiator_origin,
                   const GURL& url,
                   const network::CorsErrorStatus& status) override {}
  void Clone(mojo::PendingReceiver<DevToolsObserver> observer) override {
    receivers_.Add(this, std::move(observer));
  }
  void OnPrivateNetworkRequest(
      const base::Optional<std::string>& devtool_request_id,
      const GURL& url,
      bool is_warning,
      network::mojom::IPAddressSpace resource_address_space,
      network::mojom::ClientSecurityStatePtr client_security_state) override {}
  void OnTrustTokenOperationDone(
      const std::string& devtool_request_id,
      network::mojom::TrustTokenOperationResultPtr result) override {}

  bool completed_ = false;
  base::OnceClosure wait_for_completed_;
  bool on_raw_request_called_ = false;
  bool on_raw_response_called_ = false;
  base::Optional<network::ResourceRequest> preflight_request_;
  network::mojom::URLResponseHeadPtr preflight_response_;
  base::Optional<network::URLLoaderCompletionStatus> preflight_status_;
  std::string initiator_devtools_request_id_;

  mojo::ReceiverSet<mojom::DevToolsObserver> receivers_;
};

class PreflightControllerTest : public testing::Test {
 public:
  PreflightControllerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        test_initiator_origin_(
            url::Origin::Create(GURL("http://example.com/"))),
        access_control_allow_origin_(test_initiator_origin_) {
    CorsURLLoaderFactory::SetAllowExternalPreflightsForTesting(true);
    mojo::Remote<mojom::NetworkService> network_service_remote;
    network_service_ = NetworkService::Create(
        network_service_remote.BindNewPipeAndPassReceiver());

    auto context_params = mojom::NetworkContextParams::New();
    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    network_service_remote->CreateNetworkContext(
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = mojom::kBrowserProcessId;
    // We use network::CorsURLLoaderFactory for "internal" URLLoaderFactory
    // used by the PreflightController. Hence here we disable CORS as otherwise
    // the URLLoader would create a CORS-preflight for the preflight request.
    params->disable_web_security = true;
    params->is_corb_enabled = false;
    devtools_observer_ = std::make_unique<MockDevToolsObserver>(
        params->devtools_observer.InitWithNewPipeAndPassReceiver());
    network_context_remote_->CreateURLLoaderFactory(
        url_loader_factory_remote_.BindNewPipeAndPassReceiver(),
        std::move(params));
  }
  ~PreflightControllerTest() override {
    CorsURLLoaderFactory::SetAllowExternalPreflightsForTesting(false);
  }

 protected:
  void HandleRequestCompletion(int net_error,
                               base::Optional<CorsErrorStatus> status,
                               bool has_authorization_covered_by_wildcard) {
    net_error_ = net_error;
    status_ = status;
    has_authorization_covered_by_wildcard_ =
        has_authorization_covered_by_wildcard;

    run_loop_->Quit();
  }

  GURL GetURL(const std::string& path) { return test_server_.GetURL(path); }

  void PerformPreflightCheck(
      const ResourceRequest& request,
      bool tainted = false,
      net::IsolationInfo isolation_info = net::IsolationInfo()) {
    DCHECK(preflight_controller_);
    run_loop_ = std::make_unique<base::RunLoop>();
    preflight_controller_->PerformPreflightCheck(
        base::BindOnce(&PreflightControllerTest::HandleRequestCompletion,
                       base::Unretained(this)),
        request, WithTrustedHeaderClient(false),
        with_non_wildcard_request_headers_support_, tainted,
        TRAFFIC_ANNOTATION_FOR_TESTS, url_loader_factory_remote_.get(),
        isolation_info, devtools_observer_->Bind());
    run_loop_->Run();
  }

  void SetAccessControlAllowOrigin(const url::Origin origin) {
    access_control_allow_origin_ = origin;
  }
  void SetWithNonWildcardRequestHeadersSupport(bool value) {
    with_non_wildcard_request_headers_support_ =
        WithNonWildcardRequestHeadersSupport(value);
  }

  const url::Origin& test_initiator_origin() const {
    return test_initiator_origin_;
  }
  const url::Origin& access_control_allow_origin() const {
    return access_control_allow_origin_;
  }
  int net_error() const { return net_error_; }
  base::Optional<CorsErrorStatus> status() { return status_; }
  bool has_authorization_covered_by_wildcard() const {
    return has_authorization_covered_by_wildcard_;
  }
  base::Optional<CorsErrorStatus> success() { return base::nullopt; }
  size_t access_count() { return access_count_; }
  MockDevToolsObserver* devtools_observer() { return devtools_observer_.get(); }

 private:
  void SetUp() override {
    SetAccessControlAllowOrigin(test_initiator_origin_);

    preflight_controller_ =
        std::make_unique<PreflightController>(network_service_.get());

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &PreflightControllerTest::ServePreflight, base::Unretained(this)));

    EXPECT_TRUE(test_server_.Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> ServePreflight(
      const net::test_server::HttpRequest& request) {
    access_count_++;
    std::unique_ptr<net::test_server::BasicHttpResponse> response;
    if (request.method != net::test_server::METHOD_OPTIONS)
      return response;

    response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (net::test_server::ShouldHandle(request, "/404") ||
        net::test_server::ShouldHandle(request, "/allow") ||
        net::test_server::ShouldHandle(request, "/tainted") ||
        net::test_server::ShouldHandle(request, "/wildcard_headers")) {
      response->set_code(net::test_server::ShouldHandle(request, "/404")
                             ? net::HTTP_NOT_FOUND
                             : net::HTTP_OK);
      const url::Origin origin =
          net::test_server::ShouldHandle(request, "/tainted")
              ? url::Origin()
              : access_control_allow_origin();
      response->AddCustomHeader(header_names::kAccessControlAllowOrigin,
                                origin.Serialize());
      response->AddCustomHeader(header_names::kAccessControlAllowMethods,
                                "GET, OPTIONS");
      response->AddCustomHeader(header_names::kAccessControlMaxAge, "1000");
      response->AddCustomHeader(net::HttpRequestHeaders::kCacheControl,
                                "no-store");

      if (net::test_server::ShouldHandle(request, "/wildcard_headers")) {
        response->AddCustomHeader(header_names::kAccessControlAllowHeaders,
                                  "*");
      }
    }

    return response;
  }

  base::test::TaskEnvironment task_environment_;
  const url::Origin test_initiator_origin_;
  url::Origin access_control_allow_origin_;
  std::unique_ptr<base::RunLoop> run_loop_;

  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<MockDevToolsObserver> devtools_observer_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;
  mojo::Remote<mojom::URLLoaderFactory> url_loader_factory_remote_;

  net::test_server::EmbeddedTestServer test_server_;
  size_t access_count_ = 0;
  WithNonWildcardRequestHeadersSupport
      with_non_wildcard_request_headers_support_;

  std::unique_ptr<PreflightController> preflight_controller_;
  int net_error_ = net::OK;
  base::Optional<CorsErrorStatus> status_;
  bool has_authorization_covered_by_wildcard_ = false;
};

TEST_F(PreflightControllerTest, CheckInvalidRequest) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/404");
  request.request_initiator = test_initiator_origin();

  PerformPreflightCheck(request);
  EXPECT_EQ(net::ERR_FAILED, net_error());
  ASSERT_TRUE(status());
  EXPECT_EQ(mojom::CorsError::kPreflightInvalidStatus, status()->cors_error);
  EXPECT_EQ(1u, access_count());
}

TEST_F(PreflightControllerTest, CheckValidRequest) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/allow");
  request.request_initiator = test_initiator_origin();

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());  // Should be from the preflight cache.

  // Verify if cache related flags work to skip the preflight cache.
  request.load_flags = net::LOAD_VALIDATE_CACHE;
  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(2u, access_count());

  request.load_flags = net::LOAD_BYPASS_CACHE;
  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(3u, access_count());

  request.load_flags = net::LOAD_DISABLE_CACHE;
  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(4u, access_count());
}

TEST_F(PreflightControllerTest, CheckRequestNetworkIsolationKey) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/allow");
  const url::Origin& origin = test_initiator_origin();
  request.request_initiator = origin;
  ResourceRequest::TrustedParams trusted_params;
  trusted_params.isolation_info =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 origin, origin, net::SiteForCookies());
  request.trusted_params = {trusted_params};

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());  // Should be from the preflight cache.

  url::Origin second_origin = url::Origin::Create(GURL("https://example.com/"));
  request.request_initiator = second_origin;
  SetAccessControlAllowOrigin(second_origin);
  request.trusted_params->isolation_info =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 origin, second_origin, net::SiteForCookies());
  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(2u, access_count());
}

TEST_F(PreflightControllerTest, CheckFactoryNetworkIsolationKey) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/allow");
  const url::Origin& origin = test_initiator_origin();
  request.request_initiator = origin;

  const net::IsolationInfo isolation_info =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 origin, origin, net::SiteForCookies());

  PerformPreflightCheck(request, false, isolation_info);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());

  PerformPreflightCheck(request, false, isolation_info);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());  // Should be from the preflight cache.

  PerformPreflightCheck(request, false, net::IsolationInfo());
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(2u, access_count());  // Should not be from the preflight cache.
}

TEST_F(PreflightControllerTest, CheckTaintedRequest) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/tainted");
  request.request_initiator = test_initiator_origin();

  PerformPreflightCheck(request, true /* tainted */);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());
}

TEST_F(PreflightControllerTest, CheckResponseWithNullHeaders) {
  GURL url = GURL("https://google.com/finullurl");
  const mojom::URLResponseHead response_head;
  ResourceRequest request;
  request.url = url;
  request.request_initiator = test_initiator_origin();
  const bool tainted = false;
  base::Optional<CorsErrorStatus> detected_error_status;

  EXPECT_FALSE(response_head.headers);

  std::unique_ptr<PreflightResult> result =
      PreflightController::CreatePreflightResultForTesting(
          url, response_head, request, tainted, &detected_error_status);

  EXPECT_FALSE(result);
}

TEST_F(PreflightControllerTest, DevToolsEvents) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/allow");
  request.request_initiator = test_initiator_origin();
  // Set the devtools id to trigger the DevTools event call on
  // NetworkServiceClient.
  request.devtools_request_id = "TEST";

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  ASSERT_FALSE(status());
  EXPECT_EQ(1u, access_count());

  // Check the DevTools event results.
  devtools_observer()->WaitUntilRequestCompleted();
  EXPECT_TRUE(devtools_observer()->on_raw_request_called());
  EXPECT_TRUE(devtools_observer()->on_raw_response_called());
  ASSERT_TRUE(devtools_observer()->preflight_request().has_value());
  EXPECT_EQ(request.url, devtools_observer()->preflight_request()->url);
  EXPECT_EQ("OPTIONS", devtools_observer()->preflight_request()->method);
  ASSERT_TRUE(devtools_observer()->preflight_response());
  ASSERT_TRUE(devtools_observer()->preflight_response()->headers);
  EXPECT_EQ(
      200, devtools_observer()->preflight_response()->headers->response_code());
  ASSERT_TRUE(devtools_observer()->preflight_status().has_value());
  EXPECT_EQ(net::OK, devtools_observer()->preflight_status()->error_code);
  EXPECT_EQ("TEST", devtools_observer()->initiator_devtools_request_id());
}

TEST_F(PreflightControllerTest, AuthorizationIsCoveredByWildcard) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/wildcard_headers");
  request.request_initiator = test_initiator_origin();
  request.headers.SetHeader("authorization", "foobar");

  SetWithNonWildcardRequestHeadersSupport(false);

  PerformPreflightCheck(request);
  EXPECT_EQ(net::OK, net_error());
  EXPECT_EQ(status(), success());
  EXPECT_EQ(1u, access_count());
  EXPECT_TRUE(has_authorization_covered_by_wildcard());
}

TEST_F(PreflightControllerTest, AuthorizationIsNotCoveredByWildcard) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GetURL("/wildcard_headers");
  request.request_initiator = test_initiator_origin();
  request.headers.SetHeader("authorization", "foobar");

  SetWithNonWildcardRequestHeadersSupport(true);

  PerformPreflightCheck(request);
  EXPECT_EQ(net::ERR_FAILED, net_error());
  ASSERT_NE(status(), success());
  EXPECT_EQ(mojom::CorsError::kHeaderDisallowedByPreflightResponse,
            status()->cors_error);
  EXPECT_EQ(1u, access_count());
  EXPECT_TRUE(has_authorization_covered_by_wildcard());
}

}  // namespace

}  // namespace cors

}  // namespace network
