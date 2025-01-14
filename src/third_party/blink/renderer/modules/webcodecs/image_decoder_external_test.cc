// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/image_decoder_external.h"

#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decode_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_track.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/test_underlying_source.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class ImageDecoderTest : public testing::Test {
 public:
  ~ImageDecoderTest() override {
    // Force GC before exiting since ImageDecoderExternal will create objects
    // on background threads that will race with the next test's startup. See
    // https://crbug.com/1196376
    ThreadState::Current()->CollectAllGarbageForTesting();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  ImageDecoderExternal* CreateDecoder(V8TestingScope* v8_scope,
                                      const char* file_name,
                                      const char* mime_type) {
    auto* init = MakeGarbageCollected<ImageDecoderInit>();
    init->setType(mime_type);

    auto data = ReadFile(file_name);
    DCHECK(!data->IsEmpty()) << "Missing file: " << file_name;
    init->setData(ArrayBufferOrArrayBufferViewOrReadableStream::FromArrayBuffer(
        DOMArrayBuffer::Create(std::move(data))));
    return ImageDecoderExternal::Create(v8_scope->GetScriptState(), init,
                                        v8_scope->GetExceptionState());
  }

  ImageDecodeResult* ToImageDecodeResult(V8TestingScope* v8_scope,
                                         ScriptValue value) {
    return NativeValueTraits<ImageDecodeResult>::NativeValue(
        v8_scope->GetIsolate(), value.V8Value(), v8_scope->GetExceptionState());
  }

  ImageDecodeOptions* MakeOptions(uint32_t frame_index = 0,
                                  bool complete_frames_only = true) {
    auto* options = MakeGarbageCollected<ImageDecodeOptions>();
    options->setFrameIndex(frame_index);
    options->setCompleteFramesOnly(complete_frames_only);
    return options;
  }

  scoped_refptr<SharedBuffer> ReadFile(StringView file_name) {
    StringBuilder file_path;
    file_path.Append(test::BlinkWebTestsDir());
    file_path.Append('/');
    file_path.Append(file_name);
    return test::ReadFromFile(file_path.ToString());
  }

  bool IsTypeSupported(V8TestingScope* v8_scope, String type) {
    auto promise =
        ImageDecoderExternal::isTypeSupported(v8_scope->GetScriptState(), type);
    ScriptPromiseTester tester(v8_scope->GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_FALSE(tester.IsRejected());

    auto v8_value = tester.Value().V8Value();
    EXPECT_TRUE(v8_value->IsBoolean());
    return v8_value.As<v8::Boolean>()->Value();
  }
};

TEST_F(ImageDecoderTest, IsTypeSupported) {
  V8TestingScope v8_scope;
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/jpeg"));
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/pjpeg"));
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/jpg"));

  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/png"));
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/x-png"));
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/apng"));

  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/gif"));

  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/webp"));

  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/x-icon"));
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/vnd.microsoft.icon"));

  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/bmp"));
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/x-xbitmap"));

  EXPECT_EQ(IsTypeSupported(&v8_scope, "image/avif"),
            BUILDFLAG(ENABLE_AV1_DECODER));

  EXPECT_FALSE(IsTypeSupported(&v8_scope, "image/svg+xml"));
  EXPECT_FALSE(IsTypeSupported(&v8_scope, "image/heif"));
  EXPECT_FALSE(IsTypeSupported(&v8_scope, "image/pcx"));
  EXPECT_FALSE(IsTypeSupported(&v8_scope, "image/bpg"));
}

TEST_F(ImageDecoderTest, DecodeEmpty) {
  V8TestingScope v8_scope;

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType("image/png");
  init->setData(ArrayBufferOrArrayBufferViewOrReadableStream::FromArrayBuffer(
      DOMArrayBuffer::Create(SharedBuffer::Create())));
  auto* decoder = ImageDecoderExternal::Create(v8_scope.GetScriptState(), init,
                                               v8_scope.GetExceptionState());
  EXPECT_FALSE(decoder);
  EXPECT_TRUE(v8_scope.GetExceptionState().HadException());
}

TEST_F(ImageDecoderTest, DecodeNeuteredAtConstruction) {
  V8TestingScope v8_scope;

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  auto* buffer = DOMArrayBuffer::Create(SharedBuffer::Create());

  init->setType("image/png");
  init->setData(
      ArrayBufferOrArrayBufferViewOrReadableStream::FromArrayBuffer(buffer));

  ArrayBufferContents contents;
  ASSERT_TRUE(buffer->Transfer(v8_scope.GetIsolate(), contents));

  auto* decoder = ImageDecoderExternal::Create(v8_scope.GetScriptState(), init,
                                               v8_scope.GetExceptionState());
  EXPECT_FALSE(decoder);
  EXPECT_TRUE(v8_scope.GetExceptionState().HadException());
}

TEST_F(ImageDecoderTest, DecodeNeuteredAtDecodeTime) {
  V8TestingScope v8_scope;

  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);

  constexpr char kTestFile[] = "images/resources/animated.gif";
  auto data = ReadFile(kTestFile);
  DCHECK(!data->IsEmpty()) << "Missing file: " << kTestFile;

  auto* buffer = DOMArrayBuffer::Create(std::move(data));

  init->setData(
      ArrayBufferOrArrayBufferViewOrReadableStream::FromArrayBuffer(buffer));

  auto* decoder = ImageDecoderExternal::Create(v8_scope.GetScriptState(), init,
                                               v8_scope.GetExceptionState());
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  ArrayBufferContents contents;
  ASSERT_TRUE(buffer->Transfer(v8_scope.GetIsolate(), contents));

  auto promise = decoder->decode(MakeOptions(0, true));
  ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_FALSE(tester.IsRejected());
}

TEST_F(ImageDecoderTest, DecodeUnsupported) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/svg+xml";
  EXPECT_FALSE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/test.svg", kImageType);
  EXPECT_TRUE(decoder);
  EXPECT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    auto promise = decoder->decodeMetadata();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }

  {
    auto promise = decoder->decode(MakeOptions(0, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }
}

TEST_F(ImageDecoderTest, DecoderCreationMixedCaseMimeType) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/GiF";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), "image/gif");
}

TEST_F(ImageDecoderTest, DecodeGif) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    auto promise = decoder->decodeMetadata();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  const auto& tracks = decoder->tracks();
  ASSERT_EQ(tracks.length(), 1u);
  EXPECT_EQ(tracks.AnonymousIndexedGetter(0)->animated(), true);
  EXPECT_EQ(tracks.selectedTrack().value()->animated(), true);

  EXPECT_EQ(decoder->type(), kImageType);
  EXPECT_EQ(tracks.selectedTrack().value()->frameCount(), 2u);
  EXPECT_EQ(tracks.selectedTrack().value()->repetitionCount(), INFINITY);
  EXPECT_EQ(decoder->complete(), true);

  {
    auto promise = decoder->decode(MakeOptions(0, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->duration(), 0u);
    EXPECT_EQ(frame->displayWidth(), 16u);
    EXPECT_EQ(frame->displayHeight(), 16u);
  }

  {
    auto promise = decoder->decode(MakeOptions(1, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->duration(), 0u);
    EXPECT_EQ(frame->displayWidth(), 16u);
    EXPECT_EQ(frame->displayHeight(), 16u);
  }

  // Decoding past the end should result in a rejected promise.
  auto promise = decoder->decode(MakeOptions(3, true));
  ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsRejected());
}

TEST_F(ImageDecoderTest, DecoderReset) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), "image/gif");
  decoder->reset();

  // Ensure decoding works properly after reset.
  {
    auto promise = decoder->decodeMetadata();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  const auto& tracks = decoder->tracks();
  ASSERT_EQ(tracks.length(), 1u);
  EXPECT_EQ(tracks.AnonymousIndexedGetter(0)->animated(), true);
  EXPECT_EQ(tracks.selectedTrack().value()->animated(), true);

  EXPECT_EQ(decoder->type(), kImageType);
  EXPECT_EQ(tracks.selectedTrack().value()->frameCount(), 2u);
  EXPECT_EQ(tracks.selectedTrack().value()->repetitionCount(), INFINITY);
  EXPECT_EQ(decoder->complete(), true);

  {
    auto promise = decoder->decode(MakeOptions(0, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->duration(), 0u);
    EXPECT_EQ(frame->displayWidth(), 16u);
    EXPECT_EQ(frame->displayHeight(), 16u);
  }
}

TEST_F(ImageDecoderTest, DecoderClose) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), "image/gif");
  decoder->close();

  {
    auto promise = decoder->decodeMetadata();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }

  {
    auto promise = decoder->decode(MakeOptions(0, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }
}

TEST_F(ImageDecoderTest, DecoderContextDestroyed) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), "image/gif");

  // Decoder creation will queue metadata decoding which should be counted as
  // pending activity.
  EXPECT_TRUE(decoder->HasPendingActivity());
  {
    auto promise = decoder->decodeMetadata();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
  }

  // After metadata resolution completes, we should return to no activity.
  EXPECT_FALSE(decoder->HasPendingActivity());

  // Queue some activity.
  decoder->decode();
  EXPECT_TRUE(decoder->HasPendingActivity());

  // Destroying the context should close() the decoder and stop all activity.
  v8_scope.GetExecutionContext()->NotifyContextDestroyed();
  EXPECT_FALSE(decoder->HasPendingActivity());

  // Promises won't resolve or reject now that the context is destroyed, but we
  // should ensure decodeMetadata() and decode() don't trigger any issues.
  decoder->decodeMetadata();
  decoder->decode(MakeOptions(0, true));

  // This will fail if a decode() or decodeMetadata() was queued.
  EXPECT_FALSE(decoder->HasPendingActivity());
}

TEST_F(ImageDecoderTest, DecoderReadableStream) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));

  auto data = ReadFile("images/resources/animated-10color.gif");

  Persistent<TestUnderlyingSource> underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(v8_scope.GetScriptState());
  Persistent<ReadableStream> stream =
      ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                      underlying_source, 0);

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);
  init->setData(
      ArrayBufferOrArrayBufferViewOrReadableStream::FromReadableStream(stream));

  Persistent<ImageDecoderExternal> decoder = ImageDecoderExternal::Create(
      v8_scope.GetScriptState(), init, IGNORE_EXCEPTION_FOR_TESTING);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), kImageType);

  constexpr size_t kNumChunks = 2;
  const size_t chunk_size = (data->size() + 1) / kNumChunks;

  const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(data->Data());
  underlying_source->Enqueue(ScriptValue(
      v8_scope.GetIsolate(), ToV8(DOMUint8Array::Create(data_ptr, chunk_size),
                                  v8_scope.GetScriptState())));

  // Ensure we have metadata.
  {
    auto promise = decoder->decodeMetadata();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  // Deselect the current track.
  ASSERT_TRUE(decoder->tracks().selectedTrack());
  decoder->tracks().selectedTrack().value()->setSelected(false);

  // Enqueue remaining data.
  underlying_source->Enqueue(
      ScriptValue(v8_scope.GetIsolate(),
                  ToV8(DOMUint8Array::Create(data_ptr + chunk_size,
                                             data->size() - chunk_size),
                       v8_scope.GetScriptState())));
  underlying_source->Close();

  // Metadata should resolve okay while no track is selected.
  {
    auto promise = decoder->decodeMetadata();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  // Decodes should be rejected while no track is selected.
  {
    auto promise = decoder->decode();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }

  // Select a track again.
  decoder->tracks().AnonymousIndexedGetter(0)->setSelected(true);

  // Verify a decode completes successfully.
  {
    auto promise = decoder->decode();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->displayWidth(), 100u);
    EXPECT_EQ(frame->displayHeight(), 100u);
  }
}

TEST_F(ImageDecoderTest, DecoderReadableStreamAvif) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/avif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));

  auto data = ReadFile("images/resources/avif/star-animated-8bpc.avif");

  Persistent<TestUnderlyingSource> underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(v8_scope.GetScriptState());
  Persistent<ReadableStream> stream =
      ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                      underlying_source, 0);

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);
  init->setData(
      ArrayBufferOrArrayBufferViewOrReadableStream::FromReadableStream(stream));

  Persistent<ImageDecoderExternal> decoder = ImageDecoderExternal::Create(
      v8_scope.GetScriptState(), init, IGNORE_EXCEPTION_FOR_TESTING);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), kImageType);

  // Enqueue a single byte and ensure nothing breaks.
  const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(data->Data());
  underlying_source->Enqueue(ScriptValue(
      v8_scope.GetIsolate(),
      ToV8(DOMUint8Array::Create(data_ptr, 1), v8_scope.GetScriptState())));

  auto metadata_promise = decoder->decodeMetadata();
  auto decode_promise = decoder->decode();
  base::RunLoop().RunUntilIdle();

  // One byte shouldn't be enough to decode size or fail, so no promises should
  // be resolved.
  ScriptPromiseTester metadata_tester(v8_scope.GetScriptState(),
                                      metadata_promise);
  EXPECT_FALSE(metadata_tester.IsFulfilled());
  EXPECT_FALSE(metadata_tester.IsRejected());

  ScriptPromiseTester decode_tester(v8_scope.GetScriptState(), decode_promise);
  EXPECT_FALSE(decode_tester.IsFulfilled());
  EXPECT_FALSE(decode_tester.IsRejected());

  // Append the rest of the data.
  underlying_source->Enqueue(
      ScriptValue(v8_scope.GetIsolate(),
                  ToV8(DOMUint8Array::Create(data_ptr + 1, data->size() - 1),
                       v8_scope.GetScriptState())));

  // Ensure we have metadata.
  metadata_tester.WaitUntilSettled();
  ASSERT_TRUE(metadata_tester.IsFulfilled());

  // Verify decode completes successfully.
  decode_tester.WaitUntilSettled();
  ASSERT_TRUE(decode_tester.IsFulfilled());
  auto* result = ToImageDecodeResult(&v8_scope, decode_tester.Value());
  EXPECT_TRUE(result->complete());

  auto* frame = result->image();
  EXPECT_EQ(frame->displayWidth(), 159u);
  EXPECT_EQ(frame->displayHeight(), 159u);
}

TEST_F(ImageDecoderTest, DecodePartialImage) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/png";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);

  // Read just enough to get the header and some of the image data.
  auto data = ReadFile("images/resources/dice.png");
  auto* array_buffer = DOMArrayBuffer::Create(128, 1);
  ASSERT_TRUE(data->GetBytes(array_buffer->Data(), array_buffer->ByteLength()));

  init->setData(ArrayBufferOrArrayBufferViewOrReadableStream::FromArrayBuffer(
      array_buffer));
  auto* decoder = ImageDecoderExternal::Create(v8_scope.GetScriptState(), init,
                                               v8_scope.GetExceptionState());
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    auto promise = decoder->decodeMetadata();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  {
    auto promise1 = decoder->decode();
    auto promise2 = decoder->decode(MakeOptions(2, true));

    ScriptPromiseTester tester1(v8_scope.GetScriptState(), promise1);
    ScriptPromiseTester tester2(v8_scope.GetScriptState(), promise2);

    // Order is inverted here to catch a specific issue where out of range
    // resolution is handled ahead of decode. https://crbug.com/1200137.
    tester2.WaitUntilSettled();
    ASSERT_TRUE(tester2.IsRejected());

    tester1.WaitUntilSettled();
    ASSERT_TRUE(tester1.IsRejected());
  }
}

// TODO(crbug.com/1073995): Add tests for each format, partial decoding,
// reduced resolution decoding, premultiply, and ignored color behavior.

}  // namespace

}  // namespace blink
