// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_SERVICE_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_SERVICE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/web_resource/eula_accepted_notifier.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace base {
class SequencedTaskRunner;
}  // namespace base
namespace history {
class HistoryService;
class DeletionInfo;
}  // namespace history
namespace feedstore {
class Record;
}  // namespace feedstore
namespace feedkvstore {
class Entry;
}  // namespace feedkvstore
namespace network {
class SharedURLLoaderFactory;
}  // namespace network
namespace offline_pages {
class OfflinePageModel;
class PrefetchService;
}  // namespace offline_pages
namespace signin {
class IdentityManager;
}  // namespace signin

namespace feed {
class RefreshTaskScheduler;
class MetricsReporter;
class FeedNetwork;
class FeedStore;
class FeedStream;
class PersistentKeyValueStoreImpl;
class ImageFetcher;

namespace internal {
bool ShouldClearFeed(bool is_signed_in,
                     const history::DeletionInfo& deletion_info);
}  // namespace internal

class FeedService : public KeyedService {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Returns a string which represents the top locale and region of the
    // device.
    virtual std::string GetLanguageTag() = 0;
    // Returns display metrics for the device.
    virtual DisplayMetrics GetDisplayMetrics() = 0;
    // Clear all stored data.
    virtual void ClearAll() = 0;
    // Fetch the image and store it in the disk cache.
    virtual void PrefetchImage(const GURL& url) = 0;
    // Register the synthetic field experiments for UMA.
    virtual void RegisterExperiments(const Experiments& experiments) = 0;
  };

  // Construct a FeedService given an already constructed FeedStream.
  // Used for testing only.
  explicit FeedService(std::unique_ptr<FeedStream> stream);

  // Construct a new FeedApi along with FeedService.
  FeedService(
      std::unique_ptr<Delegate> delegate,
      std::unique_ptr<RefreshTaskScheduler> refresh_task_scheduler,
      PrefService* profile_prefs,
      PrefService* local_state,
      std::unique_ptr<leveldb_proto::ProtoDatabase<feedstore::Record>> database,
      std::unique_ptr<leveldb_proto::ProtoDatabase<feedkvstore::Entry>>
          key_value_store_database,
      signin::IdentityManager* identity_manager,
      history::HistoryService* history_service,
      offline_pages::PrefetchService* prefetch_service,
      offline_pages::OfflinePageModel* offline_page_model,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const std::string& api_key,
      const ChromeInfo& chrome_info);
  ~FeedService() override;
  FeedService(const FeedService&) = delete;
  FeedService& operator=(const FeedService&) = delete;

  FeedApi* GetStream();

  void ClearCachedData();

  RefreshTaskScheduler* GetRefreshTaskScheduler() const {
    return refresh_task_scheduler_.get();
  }

  // Whether Feedv2 is enabled. If false, the FeedService should not be created.
  static bool IsEnabled(const PrefService& pref_service);

 private:
  class StreamDelegateImpl;
  class NetworkDelegateImpl;
  class HistoryObserverImpl;
  class IdentityManagerObserverImpl;
#if defined(OS_ANDROID)
  void OnApplicationStateChange(base::android::ApplicationState state);
#endif

  // These components are owned for construction of |FeedApi|. These will
  // be null if |FeedApi| is created externally.
  std::unique_ptr<Delegate> delegate_;
  std::unique_ptr<StreamDelegateImpl> stream_delegate_;
  std::unique_ptr<MetricsReporter> metrics_reporter_;
  std::unique_ptr<NetworkDelegateImpl> network_delegate_;
  std::unique_ptr<FeedNetwork> feed_network_;
  std::unique_ptr<ImageFetcher> image_fetcher_;
  std::unique_ptr<FeedStore> store_;
  std::unique_ptr<PersistentKeyValueStoreImpl> persistent_key_value_store_;
  std::unique_ptr<RefreshTaskScheduler> refresh_task_scheduler_;
  std::unique_ptr<HistoryObserverImpl> history_observer_;
  std::unique_ptr<IdentityManagerObserverImpl> identity_manager_observer_;
#if defined(OS_ANDROID)
  bool foregrounded_ = true;
  std::unique_ptr<base::android::ApplicationStatusListener>
      application_status_listener_;
#endif
  std::unique_ptr<FeedStream> stream_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_FEED_SERVICE_H_
