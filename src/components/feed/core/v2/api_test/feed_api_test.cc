// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/api_test/feed_api_test.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/keyvalue_store.pb.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/proto/v2/wire/chrome_client_info.pb.h"
#include "components/feed/core/proto/v2/wire/request.pb.h"
#include "components/feed/core/proto/v2/wire/there_and_back_again_data.pb.h"
#include "components/feed/core/proto/v2/xsurface.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/feed/core/v2/test/test_util.h"
#include "components/feed/feed_feature_list.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/offline_pages/core/client_namespace_constants.h"

namespace feed {
namespace test {

std::unique_ptr<StreamModel> LoadModelFromStore(const StreamType& stream_type,
                                                FeedStore* store) {
  std::unique_ptr<StreamModelUpdateRequest> data =
      StoredModelData(stream_type, store);
  if (data) {
    auto model = std::make_unique<StreamModel>();
    model->Update(std::move(data));
    return model;
  }
  return nullptr;
}

std::unique_ptr<StreamModelUpdateRequest> StoredModelData(
    const StreamType& stream_type,
    FeedStore* store) {
  LoadStreamFromStoreTask::Result result;
  auto complete = [&](LoadStreamFromStoreTask::Result task_result) {
    result = std::move(task_result);
  };
  LoadStreamFromStoreTask load_task(
      LoadStreamFromStoreTask::LoadType::kFullLoad, stream_type, store,
      /*missed_last_refresh=*/false, base::BindLambdaForTesting(complete));
  // We want to load the data no matter how stale.
  load_task.IgnoreStalenessForTesting();

  base::RunLoop run_loop;
  load_task.Execute(run_loop.QuitClosure());
  run_loop.Run();

  if (result.status == LoadStreamStatus::kLoadedFromStore) {
    return std::move(result.update_request);
  }
  LOG(WARNING) << "LoadModelFromStore failed with " << result.status;
  return nullptr;
}

std::string ModelStateFor(
    std::unique_ptr<StreamModelUpdateRequest> update_request,
    std::vector<feedstore::DataOperation> operations,
    std::vector<feedstore::DataOperation> more_operations) {
  StreamModel model;
  model.Update(std::move(update_request));
  model.ExecuteOperations(operations);
  model.ExecuteOperations(more_operations);
  return model.DumpStateForTesting();
}

std::string ModelStateFor(const StreamType& stream_type, FeedStore* store) {
  auto model = LoadModelFromStore(stream_type, store);
  if (model) {
    return model->DumpStateForTesting();
  }
  return "{Failed to load model from store}";
}

feedwire::FeedAction MakeFeedAction(int64_t id, size_t pad_size) {
  feedwire::FeedAction action;

  std::string pad;
  if (pad_size > 0) {
    pad = " " + std::string(pad_size - 1, 'a');
  }

  action.mutable_action_payload()->set_action_payload_data(
      base::StrCat({base::NumberToString(id), pad}));
  return action;
}

std::vector<feedstore::StoredAction> ReadStoredActions(FeedStore* store) {
  base::RunLoop run_loop;
  CallbackReceiver<std::vector<feedstore::StoredAction>> cr(&run_loop);
  store->ReadActions(cr.Bind());
  run_loop.Run();
  CHECK(cr.GetResult());
  return std::move(*cr.GetResult());
}

std::string SerializedOfflineBadgeContent() {
  feedxsurface::OfflineBadgeContent testbadge;
  std::string badge_serialized;
  testbadge.set_available_offline(true);
  testbadge.SerializeToString(&badge_serialized);
  return badge_serialized;
}

feedwire::ThereAndBackAgainData MakeThereAndBackAgainData(int64_t id) {
  feedwire::ThereAndBackAgainData msg;
  *msg.mutable_action_payload() = MakeFeedAction(id).action_payload();
  return msg;
}

TestUnreadContentObserver::TestUnreadContentObserver() = default;
TestUnreadContentObserver::~TestUnreadContentObserver() = default;
void TestUnreadContentObserver::HasUnreadContentChanged(
    bool has_unread_content) {
  calls.push_back(has_unread_content);
}

TestSurfaceBase::TestSurfaceBase(const StreamType& stream_type,
                                 FeedStream* stream)
    : FeedStreamSurface(stream_type) {
  if (stream)
    Attach(stream);
}

TestSurfaceBase::~TestSurfaceBase() {
  if (stream_)
    Detach();
}

void TestSurfaceBase::Attach(FeedStream* stream) {
  EXPECT_FALSE(stream_);
  stream_ = stream->GetWeakPtr();
  stream_->AttachSurface(this);
}

void TestSurfaceBase::Detach() {
  EXPECT_TRUE(stream_);
  stream_->DetachSurface(this);
  stream_ = nullptr;
}

void TestSurfaceBase::StreamUpdate(const feedui::StreamUpdate& stream_update) {
  DVLOG(1) << "StreamUpdate: " << stream_update;
  // Some special-case treatment for the loading spinner. We don't count it
  // toward |initial_state|.
  bool is_initial_loading_spinner = IsInitialLoadSpinnerUpdate(stream_update);
  if (!initial_state && !is_initial_loading_spinner) {
    initial_state = stream_update;
  }
  update = stream_update;

  described_updates_.push_back(CurrentState());
}
void TestSurfaceBase::ReplaceDataStoreEntry(base::StringPiece key,
                                            base::StringPiece data) {
  data_store_entries_[key.as_string()] = data.as_string();
}
void TestSurfaceBase::RemoveDataStoreEntry(base::StringPiece key) {
  data_store_entries_.erase(key.as_string());
}

void TestSurfaceBase::Clear() {
  initial_state = base::nullopt;
  update = base::nullopt;
  described_updates_.clear();
}

std::string TestSurfaceBase::DescribeUpdates() {
  std::string result = base::JoinString(described_updates_, " -> ");
  described_updates_.clear();
  return result;
}

std::map<std::string, std::string> TestSurfaceBase::GetDataStoreEntries()
    const {
  return data_store_entries_;
}
std::string TestSurfaceBase::CurrentState() {
  if (update && IsInitialLoadSpinnerUpdate(*update))
    return "loading";

  if (!initial_state)
    return "empty";

  bool has_loading_spinner = false;
  for (int i = 0; i < update->updated_slices().size(); ++i) {
    const feedui::StreamUpdate_SliceUpdate& slice_update =
        update->updated_slices(i);
    if (slice_update.has_slice() &&
        slice_update.slice().has_zero_state_slice()) {
      CHECK(update->updated_slices().size() == 1)
          << "Zero state with other slices" << *update;
      // Returns either "no-cards" or "cant-refresh".
      return update->updated_slices()[0].slice().slice_id();
    }
    if (slice_update.has_slice() &&
        slice_update.slice().has_loading_spinner_slice()) {
      CHECK_EQ(i, update->updated_slices().size() - 1)
          << "Loading spinner in an unexpected place" << *update;
      has_loading_spinner = true;
    }
  }
  std::stringstream ss;
  if (has_loading_spinner) {
    ss << update->updated_slices().size() - 1 << " slices +spinner";
  } else {
    ss << update->updated_slices().size() << " slices";
  }
  return ss.str();
}

bool TestSurfaceBase::IsInitialLoadSpinnerUpdate(
    const feedui::StreamUpdate& update) {
  return update.updated_slices().size() == 1 &&
         update.updated_slices()[0].has_slice() &&
         update.updated_slices()[0].slice().has_loading_spinner_slice();
}

TestForYouSurface::TestForYouSurface(FeedStream* stream)
    : TestSurfaceBase(kForYouStream, stream) {}
TestWebFeedSurface::TestWebFeedSurface(FeedStream* stream)
    : TestSurfaceBase(kWebFeedStream, stream) {}

TestImageFetcher::TestImageFetcher(
    scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory)
    : ImageFetcher(url_loader_factory) {}

ImageFetchId TestImageFetcher::Fetch(
    const GURL& url,
    base::OnceCallback<void(NetworkResponse)> callback) {
  // Emulate a response.
  NetworkResponse response = {"dummyresponse", 200};
  std::move(callback).Run(std::move(response));
  return id_generator_.GenerateNextId();
}

TestFeedNetwork::TestFeedNetwork() = default;
TestFeedNetwork::~TestFeedNetwork() = default;

void TestFeedNetwork::SendQueryRequest(
    NetworkRequestType request_type,
    const feedwire::Request& request,
    bool force_signed_out_request,
    const std::string& gaia,
    base::OnceCallback<void(QueryRequestResult)> callback) {
  forced_signed_out_request = force_signed_out_request;
  ++send_query_call_count;
  // Emulate a successful response.
  // The response body is currently an empty message, because most of the
  // time we want to inject a translated response for ease of test-writing.
  query_request_sent = request;
  QueryRequestResult result;
  result.response_info.status_code = 200;
  result.response_info.response_body_bytes = 100;
  result.response_info.fetch_duration = base::TimeDelta::FromMilliseconds(42);
  result.response_info.was_signed_in = true;
  if (injected_response_) {
    result.response_body = std::make_unique<feedwire::Response>(
        std::move(injected_response_.value()));
  } else {
    result.response_body = std::make_unique<feedwire::Response>();
  }
  Reply(base::BindOnce(std::move(callback), std::move(result)));
}

template <typename API>
void DebugLogApiResponse(std::string request_bytes,
                         const FeedNetwork::RawResponse& raw_response) {
  typename API::Request request;
  if (request.ParseFromString(request_bytes)) {
    VLOG(1) << "Request: " << ToTextProto(request);
  }
  typename API::Response response;
  if (response.ParseFromString(raw_response.response_bytes)) {
    VLOG(1) << "Response: " << ToTextProto(response);
  }
}

void DebugLogResponse(base::StringPiece api_path,
                      base::StringPiece method,
                      std::string request_bytes,
                      const FeedNetwork::RawResponse& raw_response) {
  VLOG(1) << "TestFeedNetwork responding to request " << method << " "
          << api_path;
  if (api_path == UploadActionsDiscoverApi::RequestPath()) {
    DebugLogApiResponse<UploadActionsDiscoverApi>(request_bytes, raw_response);
  } else if (api_path == ListRecommendedWebFeedDiscoverApi::RequestPath()) {
    DebugLogApiResponse<ListRecommendedWebFeedDiscoverApi>(request_bytes,
                                                           raw_response);
  } else if (api_path == ListWebFeedsDiscoverApi::RequestPath()) {
    DebugLogApiResponse<ListWebFeedsDiscoverApi>(request_bytes, raw_response);
  }
}

void TestFeedNetwork::SendDiscoverApiRequest(
    base::StringPiece api_path,
    base::StringPiece method,
    std::string request_bytes,
    const std::string& gaia,
    base::OnceCallback<void(RawResponse)> callback) {
  api_requests_sent_[api_path.as_string()] = request_bytes;
  ++api_request_count_[api_path.as_string()];
  std::vector<RawResponse>& injected_responses =
      injected_api_responses_[api_path.as_string()];

  // If there is no injected response, create a default response.
  if (injected_responses.empty()) {
    if (api_path == UploadActionsDiscoverApi::RequestPath()) {
      feedwire::UploadActionsRequest request;
      ASSERT_TRUE(request.ParseFromString(request_bytes));
      feedwire::UploadActionsResponse response_message;
      response_message.mutable_consistency_token()->set_token(
          consistency_token);
      InjectApiResponse<UploadActionsDiscoverApi>(response_message);
    }
    if (api_path == ListRecommendedWebFeedDiscoverApi::RequestPath()) {
      feedwire::webfeed::ListRecommendedWebFeedsRequest request;
      ASSERT_TRUE(request.ParseFromString(request_bytes));
      feedwire::webfeed::ListRecommendedWebFeedsResponse response_message;
      InjectResponse(response_message);
    }
    if (api_path == ListWebFeedsDiscoverApi::RequestPath()) {
      feedwire::webfeed::ListWebFeedsRequest request;
      ASSERT_TRUE(request.ParseFromString(request_bytes));
      feedwire::webfeed::ListWebFeedsResponse response_message;
      InjectResponse(response_message);
    }
  }

  if (!injected_responses.empty()) {
    RawResponse response = injected_responses[0];
    injected_responses.erase(injected_responses.begin());
    DebugLogResponse(api_path, method, request_bytes, response);
    Reply(base::BindOnce(std::move(callback), std::move(response)));
    return;
  }
  ASSERT_TRUE(false) << "No API response injected, and no default is available:"
                     << api_path;
}

void TestFeedNetwork::CancelRequests() {
  NOTIMPLEMENTED();
}

void TestFeedNetwork::InjectRealFeedQueryResponse() {
  base::FilePath response_file_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &response_file_path));
  response_file_path = response_file_path.AppendASCII(
      "components/test/data/feed/response.binarypb");
  std::string response_data;
  CHECK(base::ReadFileToString(response_file_path, &response_data));

  feedwire::Response response;
  CHECK(response.ParseFromString(response_data));

  injected_response_ = response;
}

void TestFeedNetwork::InjectEmptyActionRequestResult() {
  InjectApiRawResponse<UploadActionsDiscoverApi>({});
}

base::Optional<feedwire::UploadActionsRequest>
TestFeedNetwork::GetActionRequestSent() {
  return GetApiRequestSent<UploadActionsDiscoverApi>();
}

int TestFeedNetwork::GetActionRequestCount() const {
  return GetApiRequestCount<UploadActionsDiscoverApi>();
}

void TestFeedNetwork::ClearTestData() {
  injected_api_responses_.clear();
  api_requests_sent_.clear();
  api_request_count_.clear();
  injected_response_.reset();
}

void TestFeedNetwork::SendResponse() {
  ASSERT_TRUE(send_responses_on_command_)
      << "For use only send_responses_on_command_";
  if (reply_closures_.empty()) {
    // No replies queued yet, wait for the next one.
    base::RunLoop run_loop;
    on_reply_added_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  ASSERT_FALSE(reply_closures_.empty()) << "No replies ready to send";
  auto callback = std::move(reply_closures_[0]);
  reply_closures_.erase(reply_closures_.begin());
  std::move(callback).Run();
}

void TestFeedNetwork::SendResponsesOnCommand(bool on) {
  if (send_responses_on_command_ == on)
    return;
  if (!on) {
    while (!reply_closures_.empty()) {
      SendResponse();
    }
  }
  send_responses_on_command_ = on;
}

void TestFeedNetwork::Reply(base::OnceClosure reply_closure) {
  if (send_responses_on_command_) {
    reply_closures_.push_back(std::move(reply_closure));
    if (on_reply_added_)
      on_reply_added_.Run();
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(reply_closure));
  }
}

TestWireResponseTranslator::TestWireResponseTranslator() = default;
TestWireResponseTranslator::~TestWireResponseTranslator() = default;
RefreshResponseData TestWireResponseTranslator::TranslateWireResponse(
    feedwire::Response response,
    StreamModelUpdateRequest::Source source,
    bool was_signed_in_request,
    base::Time current_time) const {
  if (!injected_responses_.empty()) {
    if (injected_responses_[0].model_update_request)
      injected_responses_[0].model_update_request->source = source;
    RefreshResponseData result = std::move(injected_responses_[0]);
    injected_responses_.erase(injected_responses_.begin());
    return result;
  }
  return WireResponseTranslator::TranslateWireResponse(
      std::move(response), source, was_signed_in_request, current_time);
}
void TestWireResponseTranslator::InjectResponse(
    std::unique_ptr<StreamModelUpdateRequest> response,
    base::Optional<std::string> session_id) {
  DCHECK(!response->stream_data.signed_in() || !session_id);
  RefreshResponseData data;
  data.model_update_request = std::move(response);
  data.session_id = std::move(session_id);
  InjectResponse(std::move(data));
}
void TestWireResponseTranslator::InjectResponse(
    RefreshResponseData response_data) {
  injected_responses_.push_back(std::move(response_data));
}
bool TestWireResponseTranslator::InjectedResponseConsumed() const {
  return injected_responses_.empty();
}

FakeRefreshTaskScheduler::FakeRefreshTaskScheduler() = default;
FakeRefreshTaskScheduler::~FakeRefreshTaskScheduler() = default;
void FakeRefreshTaskScheduler::EnsureScheduled(RefreshTaskId id,
                                               base::TimeDelta run_time) {
  scheduled_run_times[id] = run_time;
}
void FakeRefreshTaskScheduler::Cancel(RefreshTaskId id) {
  canceled_tasks.insert(id);
}
void FakeRefreshTaskScheduler::RefreshTaskComplete(RefreshTaskId id) {
  completed_tasks.insert(id);
}

void FakeRefreshTaskScheduler::Clear() {
  scheduled_run_times.clear();
  canceled_tasks.clear();
  completed_tasks.clear();
}

TestMetricsReporter::TestMetricsReporter(PrefService* prefs)
    : MetricsReporter(prefs) {}
TestMetricsReporter::~TestMetricsReporter() = default;
void TestMetricsReporter::ContentSliceViewed(const StreamType& stream_type,
                                             int index_in_stream) {
  slice_viewed_index = index_in_stream;
  MetricsReporter::ContentSliceViewed(stream_type, index_in_stream);
}
void TestMetricsReporter::OnLoadStream(
    LoadStreamStatus load_from_store_status,
    LoadStreamStatus final_status,
    bool loaded_new_content_from_network,
    base::TimeDelta stored_content_age,
    std::unique_ptr<LoadLatencyTimes> latencies) {
  load_stream_from_store_status = load_from_store_status;
  load_stream_status = final_status;
  LOG(INFO) << "OnLoadStream: " << final_status
            << " (store status: " << load_from_store_status << ")";
  MetricsReporter::OnLoadStream(load_from_store_status, final_status,
                                loaded_new_content_from_network,
                                stored_content_age, std::move(latencies));
}
void TestMetricsReporter::OnLoadMoreBegin(SurfaceId surface_id) {
  load_more_surface_id = surface_id;
  MetricsReporter::OnLoadMoreBegin(surface_id);
}
void TestMetricsReporter::OnLoadMore(LoadStreamStatus final_status) {
  load_more_status = final_status;
  MetricsReporter::OnLoadMore(final_status);
}
void TestMetricsReporter::OnBackgroundRefresh(LoadStreamStatus final_status) {
  background_refresh_status = final_status;
  MetricsReporter::OnBackgroundRefresh(final_status);
}
void TestMetricsReporter::OnClearAll(base::TimeDelta time_since_last_clear) {
  this->time_since_last_clear = time_since_last_clear;
  MetricsReporter::OnClearAll(time_since_last_clear);
}
void TestMetricsReporter::OnUploadActions(UploadActionsStatus status) {
  upload_action_status = status;
  MetricsReporter::OnUploadActions(status);
}

TestPrefetchService::TestPrefetchService() = default;
TestPrefetchService::~TestPrefetchService() = default;
void TestPrefetchService::SetSuggestionProvider(
    offline_pages::SuggestionsProvider* suggestions_provider) {
  suggestions_provider_ = suggestions_provider;
}
void TestPrefetchService::NewSuggestionsAvailable() {
  ++new_suggestions_available_call_count_;
}

offline_pages::SuggestionsProvider*
TestPrefetchService::suggestions_provider() {
  return suggestions_provider_;
}
int TestPrefetchService::NewSuggestionsAvailableCallCount() const {
  return new_suggestions_available_call_count_;
}
TestOfflinePageModel::TestOfflinePageModel() = default;
TestOfflinePageModel::~TestOfflinePageModel() = default;
void TestOfflinePageModel::AddObserver(Observer* observer) {
  CHECK(observers_.insert(observer).second);
}
void TestOfflinePageModel::RemoveObserver(Observer* observer) {
  CHECK_EQ(1UL, observers_.erase(observer));
}
void TestOfflinePageModel::GetPagesWithCriteria(
    const offline_pages::PageCriteria& criteria,
    offline_pages::MultipleOfflinePageItemCallback callback) {
  std::vector<offline_pages::OfflinePageItem> result;
  for (const offline_pages::OfflinePageItem& item : items_) {
    if (MeetsCriteria(criteria, item)) {
      result.push_back(item);
    }
  }
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void TestOfflinePageModel::AddTestPage(const GURL& url) {
  offline_pages::OfflinePageItem item;
  item.url = url;
  item.client_id =
      offline_pages::ClientId(offline_pages::kSuggestedArticlesNamespace, "");
  items_.push_back(item);
}

void TestOfflinePageModel::CallObserverOfflinePageAdded(
    const offline_pages::OfflinePageItem& item) {
  for (Observer* observer : observers_) {
    observer->OfflinePageAdded(this, item);
  }
}

void TestOfflinePageModel::CallObserverOfflinePageDeleted(
    const offline_pages::OfflinePageItem& item) {
  for (Observer* observer : observers_) {
    observer->OfflinePageDeleted(item);
  }
}

FeedApiTest::FeedApiTest() = default;
FeedApiTest::~FeedApiTest() = default;
void FeedApiTest::SetUp() {
  SetupFeatures();
  kTestTimeEpoch = base::Time::Now();

  // Reset to default config, since tests can change it.
  Config config;
  // Disable fetching of recommended web feeds at startup to
  // avoid a delayed task in tests that don't need it.
  config.fetch_web_feed_info_delay = base::TimeDelta();
  SetFeedConfigForTesting(config);

  feed::prefs::RegisterFeedSharedProfilePrefs(profile_prefs_.registry());
  feed::RegisterProfilePrefs(profile_prefs_.registry());
  metrics_reporter_ = std::make_unique<TestMetricsReporter>(&profile_prefs_);

  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_factory_);
  image_fetcher_ =
      std::make_unique<TestImageFetcher>(shared_url_loader_factory_);

  CreateStream();
}

void FeedApiTest::TearDown() {
  // Unblock network responses to allow clean teardown.
  network_.SendResponsesOnCommand(false);
  // Ensure the task queue can return to idle. Failure to do so may be
  // due to a stuck task that never called |TaskComplete()|.
  WaitForIdleTaskQueue();
  // ProtoDatabase requires PostTask to clean up.
  store_.reset();
  persistent_key_value_store_.reset();
  task_environment_.RunUntilIdle();
  // FeedStoreTest.OvewriteStream and OverwriteStreamWebFeed depends on
  // kTestTimeEpoch == UnixEpoch(). i.e. using MakeTypicalInitialModelState
  // with default arguments. Need to reset kTestTimeEpoch to avoid the tests'
  // flaky failure.
  kTestTimeEpoch = base::Time::UnixEpoch();
}
bool FeedApiTest::IsEulaAccepted() {
  return is_eula_accepted_;
}
bool FeedApiTest::IsOffline() {
  return is_offline_;
}
std::string FeedApiTest::GetSyncSignedInGaia() {
  return signed_in_gaia_;
}
DisplayMetrics FeedApiTest::GetDisplayMetrics() {
  DisplayMetrics result;
  result.density = 200;
  result.height_pixels = 800;
  result.width_pixels = 350;
  return result;
}
std::string FeedApiTest::GetLanguageTag() {
  return "en-US";
}
void FeedApiTest::PrefetchImage(const GURL& url) {
  prefetched_images_.push_back(url);
  prefetch_image_call_count_++;
}

void FeedApiTest::CreateStream() {
  ChromeInfo chrome_info;
  chrome_info.channel = version_info::Channel::STABLE;
  chrome_info.version = base::Version({99, 1, 9911, 2});
  stream_ = std::make_unique<FeedStream>(
      &refresh_scheduler_, metrics_reporter_.get(), this, &profile_prefs_,
      &network_, image_fetcher_.get(), store_.get(),
      persistent_key_value_store_.get(), &prefetch_service_,
      &offline_page_model_, chrome_info);

  WaitForIdleTaskQueue();  // Wait for any initialization.
  stream_->SetWireResponseTranslatorForTesting(&response_translator_);
}

bool FeedApiTest::IsTaskQueueIdle() const {
  return !stream_->GetTaskQueueForTesting()->HasPendingTasks() &&
         !stream_->GetTaskQueueForTesting()->HasRunningTask();
}

void FeedApiTest::WaitForIdleTaskQueue() {
  RunLoopUntil(base::BindLambdaForTesting([&]() {
    return IsTaskQueueIdle() &&
           !stream_->subscriptions().is_loading_model_for_testing();
  }));
}

void FeedApiTest::UnloadModel(const StreamType& stream_type) {
  WaitForIdleTaskQueue();
  stream_->UnloadModel(stream_type);
}

std::string FeedApiTest::DumpStoreState(bool print_keys) {
  base::RunLoop run_loop;
  std::unique_ptr<std::map<std::string, feedstore::Record>> records;
  auto callback =
      [&](bool,
          std::unique_ptr<std::map<std::string, feedstore::Record>> result) {
        records = std::move(result);
        run_loop.Quit();
      };
  store_->GetDatabaseForTesting()->LoadKeysAndEntries(
      base::BindLambdaForTesting(callback));

  run_loop.Run();
  std::stringstream ss;
  for (const auto& item : *records) {
    if (print_keys)
      ss << '"' << item.first << "\": ";

    ss << item.second << '\n';
  }
  return ss.str();
}

void FeedApiTest::UploadActions(std::vector<feedwire::FeedAction> actions) {
  size_t actions_remaining = actions.size();
  for (feedwire::FeedAction& action : actions) {
    stream_->UploadAction(action, (--actions_remaining) == 0ul,
                          base::DoNothing());
  }
}

RefreshTaskId FeedStreamTestForAllStreamTypes::GetRefreshTaskId() const {
  RefreshTaskId id;
  CHECK(GetStreamType().GetRefreshTaskId(id));
  return id;
}

}  // namespace test
}  // namespace feed
