// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_result.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/cors/cors.h"

namespace network {

namespace cors {

namespace {

// Timeout values below are at the discretion of the user agent.

// Default cache expiry time for an entry that does not have
// Access-Control-Max-Age header in its CORS-preflight response.
constexpr base::TimeDelta kDefaultTimeout = base::TimeDelta::FromSeconds(5);

// Maximum cache expiry time. Even if a CORS-preflight response contains
// Access-Control-Max-Age header that specifies a longer expiry time, this
// maximum time is applied.
constexpr base::TimeDelta kMaxTimeout = base::TimeDelta::FromHours(2);

// Holds TickClock instance to overwrite TimeTicks::Now() for testing.
const base::TickClock* tick_clock_for_testing = nullptr;

// We define the value here because we want a lower-cased header name.
constexpr char kAuthorization[] = "authorization";

base::TimeTicks Now() {
  if (tick_clock_for_testing)
    return tick_clock_for_testing->NowTicks();
  return base::TimeTicks::Now();
}

base::TimeDelta ParseAccessControlMaxAge(
    const base::Optional<std::string>& max_age) {
  if (!max_age) {
    return kDefaultTimeout;
  }

  int64_t seconds;
  if (!base::StringToInt64(*max_age, &seconds)) {
    return kDefaultTimeout;
  }

  // Negative value doesn't make sense - use 0 instead, to represent that the
  // entry cannot be cached.
  if (seconds < 0) {
    return base::TimeDelta();
  }
  // To avoid integer overflow, we compare seconds instead of comparing
  // TimeDeltas.
  static_assert(
      kMaxTimeout == base::TimeDelta::FromSeconds(kMaxTimeout.InSeconds()),
      "`kMaxTimeout` must be a multiple of one second.");
  if (seconds >= kMaxTimeout.InSeconds()) {
    return kMaxTimeout;
  }

  return base::TimeDelta::FromSeconds(seconds);
}

// Parses |string| as a Access-Control-Allow-* header value, storing the result
// in |set|. This function returns false when |string| does not satisfy the
// syntax here: https://fetch.spec.whatwg.org/#http-new-header-syntax.
bool ParseAccessControlAllowList(const base::Optional<std::string>& string,
                                 base::flat_set<std::string>* set,
                                 bool insert_in_lower_case) {
  DCHECK(set);

  if (!string)
    return true;

  net::HttpUtil::ValuesIterator it(string->begin(), string->end(), ',', true);
  while (it.GetNext()) {
    base::StringPiece value = it.value_piece();
    if (!net::HttpUtil::IsToken(value)) {
      set->clear();
      return false;
    }
    set->insert(insert_in_lower_case ? base::ToLowerASCII(value)
                                     : value.as_string());
  }
  return true;
}

}  // namespace

// static
void PreflightResult::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_for_testing = tick_clock;
}

// static
std::unique_ptr<PreflightResult> PreflightResult::Create(
    const mojom::CredentialsMode credentials_mode,
    const base::Optional<std::string>& allow_methods_header,
    const base::Optional<std::string>& allow_headers_header,
    const base::Optional<std::string>& max_age_header,
    base::Optional<mojom::CorsError>* detected_error) {
  std::unique_ptr<PreflightResult> result =
      base::WrapUnique(new PreflightResult(credentials_mode));
  base::Optional<mojom::CorsError> error =
      result->Parse(allow_methods_header, allow_headers_header, max_age_header);
  if (error) {
    if (detected_error)
      *detected_error = error;
    return nullptr;
  }
  return result;
}

PreflightResult::PreflightResult(const mojom::CredentialsMode credentials_mode)
    : credentials_(credentials_mode == mojom::CredentialsMode::kInclude) {}

PreflightResult::~PreflightResult() = default;

base::Optional<CorsErrorStatus> PreflightResult::EnsureAllowedCrossOriginMethod(
    const std::string& method) const {
  // Request method is normalized to upper case, and comparison is performed in
  // case-sensitive way, that means access control header should provide an
  // upper case method list.
  const std::string normalized_method = base::ToUpperASCII(method);
  if (methods_.find(normalized_method) != methods_.end() ||
      IsCorsSafelistedMethod(normalized_method)) {
    return base::nullopt;
  }

  if (!credentials_ && methods_.find("*") != methods_.end())
    return base::nullopt;

  return CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                         method);
}

base::Optional<CorsErrorStatus>
PreflightResult::EnsureAllowedCrossOriginHeaders(
    const net::HttpRequestHeaders& headers,
    bool is_revalidating,
    WithNonWildcardRequestHeadersSupport
        with_non_wildcard_request_headers_support) const {
  const bool has_wildcard = !credentials_ && headers_.contains("*");
  if (has_wildcard) {
    if (with_non_wildcard_request_headers_support) {
      // "authorization" is the only member of
      // https://fetch.spec.whatwg.org/#cors-non-wildcard-request-header-name.
      if (headers.HasHeader(kAuthorization) &&
          !headers_.contains(kAuthorization)) {
        CorsErrorStatus error_status(
            mojom::CorsError::kHeaderDisallowedByPreflightResponse,
            kAuthorization);
        error_status.has_authorization_covered_by_wildcard_on_preflight = true;
        return error_status;
      }
    }
    return base::nullopt;
  }

  // Forbidden headers are forbidden to be used by JavaScript, and checked
  // beforehand. But user-agents may add these headers internally, and it's
  // fine.
  for (const auto& name : CorsUnsafeNotForbiddenRequestHeaderNames(
           headers.GetHeaderVector(), is_revalidating)) {
    // Header list check is performed in case-insensitive way. Here, we have a
    // parsed header list set in lower case, and search each header in lower
    // case.
    if (!headers_.contains(name)) {
      return CorsErrorStatus(
          mojom::CorsError::kHeaderDisallowedByPreflightResponse, name);
    }
  }
  return base::nullopt;
}

bool PreflightResult::IsExpired() const {
  return absolute_expiry_time_ <= Now();
}

bool PreflightResult::EnsureAllowedRequest(
    mojom::CredentialsMode credentials_mode,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool is_revalidating,
    WithNonWildcardRequestHeadersSupport
        with_non_wildcard_request_headers_support) const {
  if (!credentials_ && credentials_mode == mojom::CredentialsMode::kInclude) {
    return false;
  }

  if (EnsureAllowedCrossOriginMethod(method)) {
    return false;
  }

  if (EnsureAllowedCrossOriginHeaders(
          headers, is_revalidating,
          with_non_wildcard_request_headers_support)) {
    return false;
  }

  return true;
}

base::Optional<mojom::CorsError> PreflightResult::Parse(
    const base::Optional<std::string>& allow_methods_header,
    const base::Optional<std::string>& allow_headers_header,
    const base::Optional<std::string>& max_age_header) {
  DCHECK(methods_.empty());
  DCHECK(headers_.empty());

  // Keeps parsed method case for case-sensitive search.
  if (!ParseAccessControlAllowList(allow_methods_header, &methods_, false))
    return mojom::CorsError::kInvalidAllowMethodsPreflightResponse;

  // Holds parsed headers in lower case for case-insensitive search.
  if (!ParseAccessControlAllowList(allow_headers_header, &headers_, true))
    return mojom::CorsError::kInvalidAllowHeadersPreflightResponse;

  const base::TimeDelta expiry_delta = ParseAccessControlMaxAge(max_age_header);
  absolute_expiry_time_ = Now() + expiry_delta;

  return base::nullopt;
}

bool PreflightResult::HasAuthorizationCoveredByWildcard(
    const net::HttpRequestHeaders& headers) const {
  // "*" acts as a wildcard symbol only when `credentials_` is false.
  const bool has_wildcard =
      !credentials_ && headers_.find("*") != headers_.end();

  return has_wildcard && headers.HasHeader(kAuthorization) &&
         !headers_.contains(kAuthorization);
}

}  // namespace cors

}  // namespace network
