// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_GPU_FACTORIES_RETRIEVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_GPU_FACTORIES_RETRIEVER_H_

#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace blink {

using OutputCB = CrossThreadOnceFunction<void(
    media::GpuVideoAcceleratorFactories* factories)>;

// Retrieves an instance of GpuVideoAcceleratorFactories ready
// for accelerated video encoding.
// Specifically
//   1. It gets GPU factories from the main thread when called from a worker
//   2. Makes sure that GPU factories already know about supported encoding
//      and decoding profiles.
void RetrieveGpuFactoriesWithKnownEncoderSupport(OutputCB callback);

// TODO: Add RetrieveGpuFactoriesWithKnownDecoderSupport and use it in
// VideoDecoder.

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_GPU_FACTORIES_RETRIEVER_H_
