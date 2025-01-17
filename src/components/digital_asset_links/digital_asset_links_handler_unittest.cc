// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/digital_asset_links/digital_asset_links_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kStatementList[] = R"(
[{
  "relation": ["other_relationship"],
  "target": {
    "namespace": "android_app",
    "package_name": "com.peter.trustedpetersactivity",
    "sha256_cert_fingerprints": [
      "FA:2A:03:CB:38:9C:F3:BE:28:E3:CA:7F:DA:2E:FA:4F:4A:96:F3:BC:45:2C:08:A2:16:A1:5D:FD:AB:46:BC:9D"
    ]
  }
}, {
  "relation": ["delegate_permission/common.handle_all_urls"],
  "target": {
    "namespace": "android_app",
    "package_name": "com.example.firstapp",
    "sha256_cert_fingerprints": [
      "64:2F:D4:BE:1C:4D:F8:36:2E:D3:50:C4:69:53:96:A1:3D:14:0A:23:AD:2F:BF:EB:6E:C6:E4:64:54:3B:34:C1"
    ]
  }
}, {
  "relation": ["delegate_permission/common.query_webapk"],
  "target": {
    "namespace": "web",
    "site": "https://example2.com/manifest.json"
  }
}]
)";

const char kDomain[] = "https://www.example.com";
const char kValidPackage[] = "com.example.firstapp";
const char kValidRelation[] = "delegate_permission/common.handle_all_urls";
const char kValidFingerprint[] =
    "64:2F:D4:BE:1C:4D:F8:36:2E:D3:50:C4:69:53:96:A1:3D:14:0A:23:AD:2F:BF:EB:"
    "6E:C6:E4:64:54:3B:34:C1";

}  // namespace

namespace digital_asset_links {
namespace {

class DigitalAssetLinksHandlerTest : public ::testing::Test {
 public:
  DigitalAssetLinksHandlerTest()
      : num_invocations_(0), result_(RelationshipCheckResult::kSuccess) {}

  void OnRelationshipCheckComplete(RelationshipCheckResult result) {
    ++num_invocations_;
    result_ = result;
  }

 protected:
  void SetUp() override { num_invocations_ = 0; }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &test_url_loader_factory_);
  }

  void AddErrorResponse(net::Error error, int response_code) {
    request_url_ =
        test_url_loader_factory_.pending_requests()->at(0).request.url;

    auto response_head = network::mojom::URLResponseHead::New();
    std::string status_line =
        "HTTP/1.1 " + base::NumberToString(response_code) + " " +
        net::GetHttpReasonPhrase(
            static_cast<net::HttpStatusCode>(response_code));
    response_head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(status_line);
    test_url_loader_factory_.AddResponse(
        request_url_, std::move(response_head), "",
        network::URLLoaderCompletionStatus(error));

    base::RunLoop().RunUntilIdle();
  }

  void AddResponse(const std::string& response) {
    request_url_ =
        test_url_loader_factory_.pending_requests()->at(0).request.url;

    test_url_loader_factory_.AddResponse(request_url_.spec(), response,
                                         net::HTTP_OK);

    base::RunLoop().RunUntilIdle();
  }

  int num_invocations_;
  RelationshipCheckResult result_;
  GURL request_url_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(DigitalAssetLinksHandlerTest);
};
}  // namespace

TEST_F(DigitalAssetLinksHandlerTest, CorrectAssetLinksUrl) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, kValidRelation, kValidFingerprint, kValidPackage,
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddResponse("");

  EXPECT_EQ(request_url_,
            "https://www.example.com/.well-known/assetlinks.json");
}

TEST_F(DigitalAssetLinksHandlerTest, PositiveResponse) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, kValidRelation, kValidFingerprint, kValidPackage,
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddResponse(kStatementList);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kSuccess);
}

TEST_F(DigitalAssetLinksHandlerTest, PackageMismatch) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, kValidRelation, kValidFingerprint, "evil.package",
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddResponse(kStatementList);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kFailure);
}

TEST_F(DigitalAssetLinksHandlerTest, SignatureMismatch) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, kValidRelation, "66:66:66:66:66:66", kValidPackage,
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddResponse(kStatementList);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kFailure);
}

TEST_F(DigitalAssetLinksHandlerTest, RelationshipMismatch) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, "take_firstborn_child", kValidFingerprint, kValidPackage,
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddResponse(kStatementList);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kFailure);
}

TEST_F(DigitalAssetLinksHandlerTest, StatementIsolation) {
  // Ensure we don't merge separate statements together.
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, "other_relationship", kValidFingerprint, kValidPackage,
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddResponse(kStatementList);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kFailure);
}

TEST_F(DigitalAssetLinksHandlerTest, BadAssetLinks_Empty) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, kValidRelation, kValidFingerprint, kValidPackage,
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddResponse("");

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kFailure);
}

TEST_F(DigitalAssetLinksHandlerTest, BadAssetLinks_NotList) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, kValidRelation, kValidFingerprint, kValidPackage,
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddResponse(R"({ "key": "value"})");

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kFailure);
}

TEST_F(DigitalAssetLinksHandlerTest, BadAssetLinks_StatementNotDict) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, kValidRelation, kValidFingerprint, kValidPackage,
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddResponse(R"([ [], [] ])");

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kFailure);
}

TEST_F(DigitalAssetLinksHandlerTest, BadAssetLinks_MissingFields) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, kValidRelation, kValidFingerprint, kValidPackage,
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddResponse(R"([ { "target" : {} } ])");

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kFailure);
}

TEST_F(DigitalAssetLinksHandlerTest, BadRequest) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, kValidRelation, kValidFingerprint, kValidPackage,
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddErrorResponse(net::OK, net::HTTP_BAD_REQUEST);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kFailure);
}

TEST_F(DigitalAssetLinksHandlerTest, NetworkError) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, kValidRelation, kValidFingerprint, kValidPackage,
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddErrorResponse(net::ERR_ABORTED, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kFailure);
}

TEST_F(DigitalAssetLinksHandlerTest, NetworkDisconnected) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForAndroidApp(
      kDomain, kValidRelation, kValidFingerprint, kValidPackage,
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddErrorResponse(net::ERR_INTERNET_DISCONNECTED, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kNoConnection);
}

TEST_F(DigitalAssetLinksHandlerTest, WebApkPositiveResponse) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForWebApk(
      kDomain, "https://example2.com/manifest.json",
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddResponse(kStatementList);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kSuccess);
}

TEST_F(DigitalAssetLinksHandlerTest, WebApkNegativeResponse) {
  DigitalAssetLinksHandler handler(GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationshipForWebApk(
      kDomain, "https://notverified.com/manifest.json",
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)));
  AddResponse(kStatementList);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::kFailure);
}

}  // namespace digital_asset_links
