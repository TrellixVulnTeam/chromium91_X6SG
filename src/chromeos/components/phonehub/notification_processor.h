// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_PROCESSOR_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_PROCESSOR_H_

#include <google/protobuf/repeated_field.h>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "ui/gfx/image/image.h"

using google::protobuf::RepeatedPtrField;

namespace chromeos {
namespace phonehub {

class Notification;
class NotificationManager;

// A helper class that processes inline reply-able notification protos and
// updates the notification manager with such notifications to add or remove.
// Since decoding image(s) included in every notification proto are asynchronous
// calls, this class ensures that additions and removals are scheduled and
// executed synchronously via a queue of requests without unexpected race
// conditions. Note that adding notifications requires using an image utility
// process asynchronously, but removals are carried out synchronously.
class NotificationProcessor {
 public:
  using DecodeImageCallback =
      data_decoder::mojom::ImageDecoder::DecodeImageCallback;

  NotificationProcessor(NotificationManager* notification_manager);
  virtual ~NotificationProcessor();

  NotificationProcessor(const NotificationProcessor&) = delete;
  NotificationProcessor& operator=(const NotificationProcessor&) = delete;

  // Removes all notifications and clears pending unfulfilled requests.
  void ClearNotificationsAndPendingUpdates();

  // Adds only inline reply-able notifications by extracting metadata from
  // their protos and asynchronously decoding their associated images.
  virtual void AddNotifications(
      const std::vector<proto::Notification>& notification_protos);

  // Removes notifications with |notifications_ids|.
  virtual void RemoveNotifications(
      const base::flat_set<int64_t>& notification_ids);

 private:
  friend class FakeNotificationProcessor;
  friend class NotificationProcessorTest;

  // Used to track which image type is being processed.
  enum class NotificationImageField {
    kIcon = 0,
    kSharedImage = 1,
    kContactImage = 2,
  };

  // Each notification proto will be associated with one of these structs.
  // |icon| will always be populated, but |shared_image| and |contact_image| may
  // be empty.
  struct NotificationImages {
    gfx::Image icon;
    gfx::Image shared_image;
    gfx::Image contact_image;
  };

  // Each image to decode will be associated with one of these structs. Each
  // request in |pending_notification_requests_| may be associated to multiple
  // DecodeImageRequestMetadata with more than one |notification_id|.
  struct DecodeImageRequestMetadata {
    DecodeImageRequestMetadata(int64_t notification_id,
                               NotificationImageField image_field,
                               const std::string& data);

    int64_t notification_id;
    NotificationImageField image_field;
    std::string data;
  };

  // A delegate class that is faked out for testing purposes.
  class ImageDecoderDelegate {
   public:
    ImageDecoderDelegate() = default;
    virtual ~ImageDecoderDelegate() = default;

    virtual void PerformImageDecode(
        const std::string& data,
        DecodeImageCallback single_image_decoded_closure);

   private:
    // The instance of the Data Decoder used by this ImageDecoderDelegate to
    // perform any image decoding operations. The underlying service instance is
    // started lazily when needed and torn down when not in use.
    data_decoder::DataDecoder data_decoder_;
  };

  NotificationProcessor(NotificationManager* notification_manager,
                        std::unique_ptr<ImageDecoderDelegate> delegate);

  void StartDecodingImages(
      const std::vector<DecodeImageRequestMetadata>& decode_image_requests,
      base::RepeatingClosure done_closure);
  void OnDecodedBitmapReady(const DecodeImageRequestMetadata& request,
                            base::OnceClosure done_closure,
                            const SkBitmap& decoded_bitmap);
  void OnAllImagesDecoded(
      std::vector<proto::Notification> inline_replyable_notifications);

  void ProcessRequestQueue();
  void CompleteRequest();
  void AddNotificationsAndProcessNextRequest(
      const base::flat_set<Notification>& notifications);
  void RemoveNotificationsAndProcessNextRequest(
      base::flat_set<int64_t> removed_notification_ids);

  NotificationManager* notification_manager_;
  base::queue<base::OnceClosure> pending_notification_requests_;
  base::flat_map<int64_t, NotificationImages> id_to_images_map_;
  std::unique_ptr<ImageDecoderDelegate> delegate_;

  base::WeakPtrFactory<NotificationProcessor> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_NOTIFICATION_PROCESSOR_H_