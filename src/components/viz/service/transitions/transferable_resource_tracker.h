// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_TRANSITIONS_TRANSFERABLE_RESOURCE_TRACKER_H_
#define COMPONENTS_VIZ_SERVICE_TRANSITIONS_TRANSFERABLE_RESOURCE_TRACKER_H_

#include <map>
#include <memory>
#include <vector>

#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/surfaces/surface_saved_frame.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

// This class is a simple transferable resource generator and lifetime tracker.
// Note that TransferableResourceTracker uses reserved range ResourceIds.
class VIZ_SERVICE_EXPORT TransferableResourceTracker {
 public:
  // This represents a resource that is positioned somewhere on screen.
  struct VIZ_SERVICE_EXPORT PositionedResource {
    TransferableResource resource;
    // This is the resource's initial rect.
    gfx::Rect rect;
    // This is the transform that takes `rect` into root render pass space.
    gfx::Transform target_transform;
  };

  // A resource frame consists of a root PositionedResource and a set of
  // optional shared PositionedResources. A SurfaceSavedFrame can be converted
  // to a ResourceFrame via ImportResources.
  struct VIZ_SERVICE_EXPORT ResourceFrame {
    ResourceFrame();
    ResourceFrame(ResourceFrame&& other);
    ~ResourceFrame();

    ResourceFrame& operator=(ResourceFrame&& other);

    PositionedResource root;
    std::vector<base::Optional<PositionedResource>> shared;
  };

  TransferableResourceTracker();
  TransferableResourceTracker(const TransferableResourceTracker&) = delete;
  ~TransferableResourceTracker();

  TransferableResourceTracker& operator=(const TransferableResourceTracker&) =
      delete;

  // This call converts a SurfaceSavedFrame into a ResourceFrame by converting
  // each of the resources into a TransferableResource. Note that `this` keeps
  // a ref on each of the TransferableResources returned in the ResourceFrame.
  // The ref count can be managed by calls to RefResource and UnrefResource
  // below. Note that a convenience function, `ReturnFrame`, is also provided
  // below which will unref every resource in a given ResourceFrame. Using the
  // convenience function is not a guarantee that the resources will be
  // released: it only removes one ref from each resource. The resources will
  // be released when the ref count reaches 0.
  // TODO(vmpstr): Instead of providing a convenience function, we should
  // convert ResourceFrame to be RAII so that it can be automatically
  // "returned".
  ResourceFrame ImportResources(std::unique_ptr<SurfaceSavedFrame> saved_frame);

  // Return a frame back to the tracker. This unrefs all of the resources.
  void ReturnFrame(const ResourceFrame& frame);

  // Ref count management for the resources returned by `ImportResources`.
  void RefResource(ResourceId id);
  void UnrefResource(ResourceId id);

  bool is_empty() const { return managed_resources_.empty(); }

 private:
  friend class TransferableResourceTrackerTest;

  ResourceId GetNextAvailableResourceId();

  PositionedResource ImportResource(
      SurfaceSavedFrame::OutputCopyResult output_copy);

  static_assert(std::is_same<decltype(kInvalidResourceId.GetUnsafeValue()),
                             uint32_t>::value,
                "ResourceId underlying type should be uint32_t");

  const uint32_t starting_id_;
  uint32_t next_id_;

  struct TransferableResourceHolder {
    TransferableResourceHolder();
    TransferableResourceHolder(TransferableResourceHolder&& other);
    TransferableResourceHolder(
        const TransferableResource& resource,
        std::unique_ptr<SingleReleaseCallback> release_callback);
    ~TransferableResourceHolder();

    TransferableResourceHolder& operator=(TransferableResourceHolder&& other);

    TransferableResource resource;
    std::unique_ptr<SingleReleaseCallback> release_callback;
    uint8_t ref_count = 0u;
  };

  std::map<ResourceId, TransferableResourceHolder> managed_resources_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_TRANSITIONS_TRANSFERABLE_RESOURCE_TRACKER_H_
