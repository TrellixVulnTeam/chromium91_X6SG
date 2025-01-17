// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_HELPER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_HELPER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "gin/public/isolate_holder.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace auction_worklet {

// Helper for Javascript operations. Owns a V8 isolate, and manages operations
// on it. Must be deleted after all V8 objects created using its isolate. It
// facilitates creating objects from JSON and running scripts in isolated
// contexts.
//
// Currently, multiple AuctionV8Helpers can be in use at once, each will have
// its own V8 isolate.  All AuctionV8Helpers are assumed to be created on the
// same thread (V8 startup is done only once per process, and not behind a
// lock).
class AuctionV8Helper {
 public:
  // Timeout for script execution.
  static const base::TimeDelta kScriptTimeout;

  // Helper class to set up v8 scopes to use Isolate. All methods expect a
  // FullIsolateScope to be have been created on the current thread, and a
  // context to be entered.
  class FullIsolateScope {
   public:
    explicit FullIsolateScope(AuctionV8Helper* v8_helper);
    explicit FullIsolateScope(const FullIsolateScope&) = delete;
    FullIsolateScope& operator=(const FullIsolateScope&) = delete;
    ~FullIsolateScope();

   private:
    const v8::Locker locker_;
    const v8::Isolate::Scope isolate_scope_;
    const v8::HandleScope handle_scope_;
  };

  AuctionV8Helper();
  explicit AuctionV8Helper(const AuctionV8Helper&) = delete;
  AuctionV8Helper& operator=(const AuctionV8Helper&) = delete;
  ~AuctionV8Helper();

  v8::Isolate* isolate() { return isolate_holder_->isolate(); }

  // Context that can be used for persistent items that can then be used in
  // other contexts - compiling functions, creating objects, etc.
  v8::Local<v8::Context> scratch_context() {
    return scratch_context_.Get(isolate());
  }

  // Create a v8::Context. The one thing this does that v8::Context::New() does
  // not is remove access the Date object.
  v8::Local<v8::Context> CreateContext(
      v8::Handle<v8::ObjectTemplate> global_template =
          v8::Handle<v8::ObjectTemplate>());

  // Creates a v8::String from an ASCII string literal, which should never fail.
  v8::Local<v8::String> CreateStringFromLiteral(const char* ascii_string);

  // Attempts to create a v8::String from a UTF-8 string. Returns empty string
  // if input is not UTF-8.
  v8::MaybeLocal<v8::String> CreateUtf8String(base::StringPiece utf8_string);

  // The passed in JSON must be a valid UTF-8 JSON string.
  v8::MaybeLocal<v8::Value> CreateValueFromJson(v8::Local<v8::Context> context,
                                                base::StringPiece utf8_json);

  // Convenience wrappers around the above Create* methods. Attempt to create
  // the corresponding value type and append it to the passed in argument
  // vector. Useful for assembling arguments to a Javascript function. Return
  // false on failure.
  bool AppendUtf8StringValue(base::StringPiece utf8_string,
                             std::vector<v8::Local<v8::Value>>* args)
      WARN_UNUSED_RESULT;
  bool AppendJsonValue(v8::Local<v8::Context> context,
                       base::StringPiece utf8_json,
                       std::vector<v8::Local<v8::Value>>* args)
      WARN_UNUSED_RESULT;

  // Convenience wrapper that adds the specified value into the provided Object.
  bool InsertValue(base::StringPiece key,
                   v8::Local<v8::Value> value,
                   v8::Local<v8::Object> object) WARN_UNUSED_RESULT;

  // Convenience wrapper that creates an Object by parsing `utf8_json` as JSON
  // and then inserts it into the provided Object.
  bool InsertJsonValue(v8::Local<v8::Context> context,
                       base::StringPiece key,
                       base::StringPiece utf8_json,
                       v8::Local<v8::Object> object) WARN_UNUSED_RESULT;

  // Attempts to convert |value| to JSON and write it to |out|. Returns false on
  // failure.
  bool ExtractJson(v8::Local<v8::Context> context,
                   v8::Local<v8::Value> value,
                   std::string* out);

  // Compiles the provided script. Despite not being bound to a context, there
  // still must be an active context for this method to be invoked.
  v8::MaybeLocal<v8::UnboundScript> Compile(const std::string& src,
                                            const GURL& src_url);

  // Binds a script and runs it in the passed in context, returning the result.
  // Note that the returned value could include references to objects or
  // functions contained within the context, so is likely not safe to use in
  // other contexts without sanitization.
  //
  // Assumes passed in context is the active context. Passed in context must be
  // using the Helper's isolate.
  //
  // Running this multiple times in the same context will re-load the entire
  // script file in the context, and then run the script again.
  v8::MaybeLocal<v8::Value> RunScript(v8::Local<v8::Context> context,
                                      v8::Local<v8::UnboundScript> script,
                                      base::StringPiece script_name,
                                      std::vector<v8::Local<v8::Value>> args =
                                          std::vector<v8::Local<v8::Value>>());

  void set_script_timeout_for_testing(base::TimeDelta script_timeout) {
    script_timeout_ = script_timeout;
  }

 private:
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
  v8::Global<v8::Context> scratch_context_;
  // Script timeout. Can be changed for testing.
  base::TimeDelta script_timeout_ = kScriptTimeout;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_HELPER_H_
