// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/key_data.h"

#include <memory>
#include <string>

#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/storage.pb.h"
#include "components/metrics/structured/structured_events.h"
#include "components/prefs/persistent_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace structured {

namespace {

// 32 byte long test key, matching the size of a real key.
constexpr char kKey[] = "abcdefghijklmnopqrstuvwxyzabcdef";

// These project, event, and metric names are used for testing.
// - project: TestProjectOne
//   - event: TestEventOne
//     - metric: TestMetricOne
//     - metric: TestMetricTwo
// - project: TestProjectTwo

// The name hash of "TestProjectOne".
constexpr uint64_t kProjectOneHash = UINT64_C(16881314472396226433);
// The name hash of "TestProjectTwo".
constexpr uint64_t kProjectTwoHash = UINT64_C(5876808001962504629);

// The name hash of "TestMetricOne".
constexpr uint64_t kMetricOneHash = UINT64_C(637929385654885975);
// The name hash of "TestMetricTwo".
constexpr uint64_t kMetricTwoHash = UINT64_C(14083999144141567134);

// The hex-encoded frst 8 bytes of SHA256(kKey), ie. the user ID for key kKey.
constexpr char kUserId[] = "2070DF23E0D95759";

// Test values and their hashes. Hashes are the first 8 bytes of:
// HMAC_SHA256(concat(hex(kMetricNHash), kValueN), kKey)
constexpr char kValueOne[] = "value one";
constexpr char kValueTwo[] = "value two";
constexpr char kValueOneHash[] = "805B8790DC69B773";
constexpr char kValueTwoHash[] = "87CEF12FB15E0B3A";

std::string HashToHex(const uint64_t hash) {
  return base::HexEncode(&hash, sizeof(uint64_t));
}

}  // namespace

class KeyDataTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void ResetState() {
    key_data_.reset();
    base::DeleteFile(GetPath());
    ASSERT_FALSE(base::PathExists(GetPath()));
  }

  base::FilePath GetPath() { return temp_dir_.GetPath().Append("keys"); }

  void MakeKeyData() {
    key_data_ = std::make_unique<KeyData>(
        GetPath(), base::TimeDelta::FromSeconds(0), base::DoNothing());
    Wait();
  }

  void SaveKeyData() {
    key_data_->WriteNowForTest();
    Wait();
    ASSERT_TRUE(base::PathExists(GetPath()));
  }

  int Today() { return (base::Time::Now() - base::Time::UnixEpoch()).InDays(); }

  // Read the on-disk file and return the information about the key for
  // |project_name_hash|. Fails if a key does not exist.
  KeyProto GetKey(const uint64_t project_name_hash) {
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    KeyDataProto proto;
    CHECK(proto.ParseFromString(proto_str));

    const auto it = proto.keys().find(project_name_hash);
    CHECK(it != proto.keys().end());
    return it->second;
  }

  // Write a KeyDataProto to disk with a single key described by the arguments.
  void SetupKey(const uint64_t project_name_hash,
                const std::string& key,
                const int last_rotation,
                const int rotation_period) {
    // It's a test logic error for the key data to exist when calling SetupKey,
    // because it will desync the in-memory proto from the underlying storage.
    ASSERT_FALSE(key_data_);

    KeyDataProto proto;
    KeyProto& key_proto = (*proto.mutable_keys())[project_name_hash];
    key_proto.set_key(key);
    key_proto.set_last_rotation(last_rotation);
    key_proto.set_rotation_period(rotation_period);

    ASSERT_TRUE(base::WriteFile(GetPath(), proto.SerializeAsString()));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  void ExpectNoErrors() {
    histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError",
                                       0);
  }

  void ExpectKeyValidation(const int valid,
                           const int created,
                           const int rotated) {
    static const std::string histogram =
        "UMA.StructuredMetrics.KeyValidationState";
    histogram_tester_.ExpectBucketCount(histogram, KeyValidationState::kValid,
                                        valid);
    histogram_tester_.ExpectBucketCount(histogram, KeyValidationState::kCreated,
                                        created);
    histogram_tester_.ExpectBucketCount(histogram, KeyValidationState::kRotated,
                                        rotated);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
  base::ScopedMockClockOverride time_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<KeyData> key_data_;
};

// If there is no key store file present, check that new keys are generated for
// each project, and those keys are of the right length and different from each
// other.
TEST_F(KeyDataTest, GeneratesKeysForProjects) {
  // Make key data and use two keys, in order to generate them.
  MakeKeyData();
  key_data_->Id(kProjectOneHash);
  key_data_->Id(kProjectTwoHash);
  SaveKeyData();

  const std::string key_one = GetKey(kProjectOneHash).key();
  const std::string key_two = GetKey(kProjectTwoHash).key();

  EXPECT_EQ(key_one.size(), 32ul);
  EXPECT_EQ(key_two.size(), 32ul);
  EXPECT_NE(key_one, key_two);

  ExpectNoErrors();
  ExpectKeyValidation(/*valid=*/0, /*created=*/2, /*rotated=*/0);
}

// When repeatedly initialized with no key store file present, ensure the keys
// generated each time are distinct.
TEST_F(KeyDataTest, GeneratesDistinctKeys) {
  base::flat_set<std::string> keys;

  for (int i = 1; i <= 10; ++i) {
    // Reset on-disk and in-memory state, regenerate the key, and save it to
    // disk.
    ResetState();
    MakeKeyData();
    key_data_->Id(kProjectOneHash);
    SaveKeyData();

    keys.insert(GetKey(kProjectOneHash).key());
    ExpectKeyValidation(/*valid=*/0, /*created=*/i, /*rotated=*/0);
  }

  ExpectNoErrors();
  EXPECT_EQ(keys.size(), 10ul);
}

// If there is an existing key store file, check that its keys are not replaced.
TEST_F(KeyDataTest, ReuseExistingKeys) {
  // Create a file with one key.
  MakeKeyData();
  const uint64_t id_one = key_data_->Id(kProjectOneHash);
  SaveKeyData();
  ExpectKeyValidation(/*valid=*/0, /*created=*/1, /*rotated=*/0);
  const std::string key_one = GetKey(kProjectOneHash).key();

  // Reset the in-memory state, leave the on-disk state intact.
  key_data_.reset();

  // Open the file again and check we use the same key.
  MakeKeyData();
  const uint64_t id_two = key_data_->Id(kProjectOneHash);
  ExpectKeyValidation(/*valid=*/1, /*created=*/1, /*rotated=*/0);
  SaveKeyData();
  const std::string key_two = GetKey(kProjectOneHash).key();

  EXPECT_EQ(id_one, id_two);
  EXPECT_EQ(key_one, key_two);
}

// Check that different events have different hashes for the same metric and
// value.
TEST_F(KeyDataTest, DifferentEventsDifferentHashes) {
  MakeKeyData();
  EXPECT_NE(key_data_->HmacMetric(kProjectOneHash, kMetricOneHash, "value"),
            key_data_->HmacMetric(kProjectTwoHash, kMetricOneHash, "value"));
  ExpectNoErrors();
}

// Check that an event has different hashes for different metrics with the same
// value.
TEST_F(KeyDataTest, DifferentMetricsDifferentHashes) {
  MakeKeyData();
  EXPECT_NE(key_data_->HmacMetric(kProjectOneHash, kMetricOneHash, "value"),
            key_data_->HmacMetric(kProjectOneHash, kMetricTwoHash, "value"));
  ExpectNoErrors();
}

// Check that an event has different hashes for different values of the same
// metric.
TEST_F(KeyDataTest, DifferentValuesDifferentHashes) {
  MakeKeyData();
  EXPECT_NE(key_data_->HmacMetric(kProjectOneHash, kMetricOneHash, "first"),
            key_data_->HmacMetric(kProjectOneHash, kMetricOneHash, "second"));
  ExpectNoErrors();
}

// Ensure that KeyData::UserId is the expected value of SHA256(key).
TEST_F(KeyDataTest, CheckUserIDs) {
  SetupKey(kProjectOneHash, kKey, Today(), 90);

  MakeKeyData();
  EXPECT_EQ(HashToHex(key_data_->Id(kProjectOneHash)), kUserId);
  EXPECT_NE(HashToHex(key_data_->Id(kProjectTwoHash)), kUserId);
  ExpectKeyValidation(/*valid=*/1, /*created=*/1, /*rotated=*/0);
  ExpectNoErrors();
}

// Ensure that KeyData::Hash returns expected values for a known key and value.
TEST_F(KeyDataTest, CheckHashes) {
  SetupKey(kProjectOneHash, kKey, Today(), 90);

  MakeKeyData();
  EXPECT_EQ(HashToHex(key_data_->HmacMetric(kProjectOneHash, kMetricOneHash,
                                            kValueOne)),
            kValueOneHash);
  EXPECT_EQ(HashToHex(key_data_->HmacMetric(kProjectOneHash, kMetricTwoHash,
                                            kValueTwo)),
            kValueTwoHash);
  ExpectKeyValidation(/*valid=*/2, /*created=*/0, /*rotated=*/0);
  ExpectNoErrors();
}

// Check that keys for a event are correctly rotated after the default 90 day
// rotation period.
TEST_F(KeyDataTest, KeysRotated) {
  const int start_day = Today();
  SetupKey(kProjectOneHash, kKey, start_day, 90);

  MakeKeyData();
  const uint64_t first_id = key_data_->Id(kProjectOneHash);
  ExpectKeyValidation(/*valid=*/1, /*created=*/0, /*rotated=*/0);

  {
    // Advancing by 50 days, the key should not be rotated.
    time_.Advance(base::TimeDelta::FromDays(50));
    EXPECT_EQ(key_data_->Id(kProjectOneHash), first_id);
    SaveKeyData();

    ASSERT_EQ(GetKey(kProjectOneHash).last_rotation(), start_day);
    ExpectKeyValidation(/*valid=*/2, /*created=*/0, /*rotated=*/0);
  }

  {
    // Advancing by another 50 days, the key should be rotated and the last
    // rotation day should be incremented by 90.
    time_.Advance(base::TimeDelta::FromDays(50));
    EXPECT_NE(key_data_->Id(kProjectOneHash), first_id);
    SaveKeyData();

    EXPECT_EQ(GetKey(kProjectOneHash).last_rotation(), start_day + 90);
    ExpectKeyValidation(/*valid=*/2, /*created=*/0, /*rotated=*/1);

    // The rotation period could change here if it were ever updated in the xml.
    // This test relies on it being 90 days.
    ASSERT_EQ(GetKey(kProjectOneHash).rotation_period(), 90);
  }

  {
    // Advancing by 453 days, the last rotation day should now 6 periods of 90
    // days ahead.
    time_.Advance(base::TimeDelta::FromDays(453));
    key_data_->Id(kProjectOneHash);
    SaveKeyData();

    EXPECT_EQ(GetKey(kProjectOneHash).last_rotation(), start_day + 6 * 90);
    ExpectKeyValidation(/*valid=*/2, /*created=*/0, /*rotated=*/2);
  }
}

}  // namespace structured
}  // namespace metrics
