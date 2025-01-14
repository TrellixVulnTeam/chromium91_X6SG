// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/looping_file_cast_agent.h"

#include <string>
#include <utility>
#include <vector>

#include "cast/common/channel/message_util.h"
#include "cast/standalone_sender/looping_file_sender.h"
#include "cast/streaming/capture_recommendations.h"
#include "cast/streaming/constants.h"
#include "cast/streaming/offer_messages.h"
#include "json/value.h"
#include "platform/api/tls_connection_factory.h"
#include "util/stringprintf.h"
#include "util/trace_logging.h"

namespace openscreen {
namespace cast {
namespace {

using DeviceMediaPolicy = SenderSocketFactory::DeviceMediaPolicy;

// TODO(miu): These string constants appear in a few places and should be
// de-duped to a common location.
constexpr char kMirroringAppId[] = "0F5096E8";
constexpr char kMirroringAudioOnlyAppId[] = "85CDB22F";

// Parses the given string as a JSON object. If the parse fails, an empty object
// is returned.
//
// TODO(miu): De-dupe this code (same as in cast/receiver/application_agent.cc)!
Json::Value ParseAsObject(absl::string_view value) {
  ErrorOr<Json::Value> parsed = json::Parse(value);
  if (parsed.is_value() && parsed.value().isObject()) {
    return std::move(parsed.value());
  }
  return Json::Value(Json::objectValue);
}

// Returns true if the 'type' field in |object| has the given |type|.
//
// TODO(miu): De-dupe this code (same as in cast/receiver/application_agent.cc)!
bool HasType(const Json::Value& object, CastMessageType type) {
  OSP_DCHECK(object.isObject());
  const Json::Value& value =
      object.get(kMessageKeyType, Json::Value::nullSingleton());
  return value.isString() && value.asString() == CastMessageTypeToString(type);
}

// Returns the string found in object[field] if possible; otherwise, returns
// |fallback|. The fallback string is returned if |object| is not an object or
// the |field| key does not reference a string within the object.
std::string ExtractStringFieldValue(const Json::Value& object,
                                    const char* field,
                                    std::string fallback = {}) {
  if (object.isObject() && object[field].isString()) {
    return object[field].asString();
  }
  return fallback;
}

}  // namespace

LoopingFileCastAgent::LoopingFileCastAgent(TaskRunner* task_runner,
                                           ShutdownCallback shutdown_callback)
    : task_runner_(task_runner),
      shutdown_callback_(std::move(shutdown_callback)),
      connection_handler_(&router_, this),
      socket_factory_(this, task_runner_),
      connection_factory_(
          TlsConnectionFactory::CreateFactory(&socket_factory_, task_runner_)),
      message_port_(&router_) {
  router_.AddHandlerForLocalId(kPlatformSenderId, this);
  socket_factory_.set_factory(connection_factory_.get());
}

LoopingFileCastAgent::~LoopingFileCastAgent() {
  Shutdown();
}

void LoopingFileCastAgent::Connect(ConnectionSettings settings) {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneSender);

  OSP_DCHECK(!connection_settings_);
  connection_settings_ = std::move(settings);
  const auto policy = connection_settings_->should_include_video
                          ? DeviceMediaPolicy::kIncludesVideo
                          : DeviceMediaPolicy::kAudioOnly;

  task_runner_->PostTask([this, policy] {
    wake_lock_ = ScopedWakeLock::Create(task_runner_);
    socket_factory_.Connect(connection_settings_->receiver_endpoint, policy,
                            &router_);
  });
}

void LoopingFileCastAgent::OnConnected(SenderSocketFactory* factory,
                                       const IPEndpoint& endpoint,
                                       std::unique_ptr<CastSocket> socket) {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneSender);

  if (message_port_.GetSocketId() != ToCastSocketId(nullptr)) {
    OSP_LOG_WARN << "Already connected, dropping peer at: " << endpoint;
    return;
  }
  message_port_.SetSocket(socket->GetWeakPtr());
  router_.TakeSocket(this, std::move(socket));

  OSP_LOG_INFO << "Launching Mirroring App on the Cast Receiver...";
  static constexpr char kLaunchMessageTemplate[] =
      R"({"type":"LAUNCH", "requestId":%d, "appId":"%s"})";
  router_.Send(VirtualConnection{kPlatformSenderId, kPlatformReceiverId,
                                 message_port_.GetSocketId()},
               MakeSimpleUTF8Message(
                   kReceiverNamespace,
                   StringPrintf(kLaunchMessageTemplate, next_request_id_++,
                                GetMirroringAppId())));
}

void LoopingFileCastAgent::OnError(SenderSocketFactory* factory,
                                   const IPEndpoint& endpoint,
                                   Error error) {
  OSP_LOG_ERROR << "Cast agent received socket factory error: " << error;
  Shutdown();
}

void LoopingFileCastAgent::OnClose(CastSocket* cast_socket) {
  OSP_VLOG << "Cast agent socket closed.";
  Shutdown();
}

void LoopingFileCastAgent::OnError(CastSocket* socket, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket error: " << error;
  Shutdown();
}

bool LoopingFileCastAgent::IsConnectionAllowed(
    const VirtualConnection& virtual_conn) const {
  return true;
}

void LoopingFileCastAgent::OnMessage(VirtualConnectionRouter* router,
                                     CastSocket* socket,
                                     ::cast::channel::CastMessage message) {
  if (message_port_.GetSocketId() == ToCastSocketId(socket) &&
      !message_port_.client_sender_id().empty() &&
      message_port_.client_sender_id() == message.destination_id()) {
    OSP_DCHECK(message_port_.client_sender_id() != kPlatformSenderId);
    message_port_.OnMessage(router, socket, std::move(message));
    return;
  }

  if (message.destination_id() != kPlatformSenderId &&
      message.destination_id() != kBroadcastId) {
    return;  // Message not for us.
  }

  if (message.namespace_() == kReceiverNamespace &&
      message_port_.GetSocketId() == ToCastSocketId(socket)) {
    const Json::Value payload = ParseAsObject(message.payload_utf8());
    if (HasType(payload, CastMessageType::kReceiverStatus)) {
      HandleReceiverStatus(payload);
    } else if (HasType(payload, CastMessageType::kLaunchError)) {
      OSP_LOG_ERROR
          << "Failed to launch the Cast Mirroring App on the Receiver! Reason: "
          << ExtractStringFieldValue(payload, kMessageKeyReason, "UNKNOWN");
      Shutdown();
    } else if (HasType(payload, CastMessageType::kInvalidRequest)) {
      OSP_LOG_ERROR << "Cast Receiver thinks our request is invalid: "
                    << ExtractStringFieldValue(payload, kMessageKeyReason,
                                               "UNKNOWN");
    }
  }
}

const char* LoopingFileCastAgent::GetMirroringAppId() const {
  if (connection_settings_ && !connection_settings_->should_include_video) {
    return kMirroringAudioOnlyAppId;
  }
  return kMirroringAppId;
}

void LoopingFileCastAgent::HandleReceiverStatus(const Json::Value& status) {
  const Json::Value& details =
      (status[kMessageKeyStatus].isObject() &&
       status[kMessageKeyStatus][kMessageKeyApplications].isArray())
          ? status[kMessageKeyStatus][kMessageKeyApplications][0]
          : Json::Value();

  const std::string& running_app_id =
      ExtractStringFieldValue(details, kMessageKeyAppId);
  if (running_app_id != GetMirroringAppId()) {
    // The mirroring app is not running. If it was just stopped, Shutdown() will
    // tear everything down. If it has been stopped already, Shutdown() is a
    // no-op.
    Shutdown();
    return;
  }

  const std::string& session_id =
      ExtractStringFieldValue(details, kMessageKeySessionId);
  if (session_id.empty()) {
    OSP_LOG_ERROR
        << "Cannot continue: Cast Receiver did not provide a session ID for "
           "the Mirroring App running on it.";
    Shutdown();
    return;
  }
  if (app_session_id_ != session_id) {
    if (app_session_id_.empty()) {
      app_session_id_ = session_id;
    } else {
      OSP_LOG_ERROR << "Cannot continue: Different Mirroring App session is "
                       "now running on the Cast Receiver.";
      Shutdown();
      return;
    }
  }

  if (remote_connection_) {
    // The mirroring app is running and this LoopingFileCastAgent is already
    // streaming to it (or is awaiting message routing to be established). There
    // are no additional actions to be taken in response to this extra
    // RECEIVER_STATUS message.
    return;
  }

  const std::string& message_destination_id =
      ExtractStringFieldValue(details, kMessageKeyTransportId);
  if (message_destination_id.empty()) {
    OSP_LOG_ERROR
        << "Cannot continue: Cast Receiver did not provide a transport ID for "
           "routing messages to the Mirroring App running on it.";
    Shutdown();
    return;
  }

  remote_connection_.emplace(
      VirtualConnection{MakeUniqueSessionId("streaming_sender"),
                        message_destination_id, message_port_.GetSocketId()});
  OSP_LOG_INFO << "Starting-up message routing to the Cast Receiver's "
                  "Mirroring App (sessionId="
               << app_session_id_ << ")...";
  connection_handler_.OpenRemoteConnection(
      *remote_connection_,
      [this](bool success) { OnRemoteMessagingOpened(success); });
}

void LoopingFileCastAgent::OnRemoteMessagingOpened(bool success) {
  if (!remote_connection_) {
    return;  // Shutdown() was called in the meantime.
  }

  if (success) {
    OSP_LOG_INFO << "Starting streaming session...";
    CreateAndStartSession();
  } else {
    OSP_LOG_INFO << "Failed to establish messaging to the Cast Receiver's "
                    "Mirroring App. Perhaps another Cast Sender is using it?";
    Shutdown();
  }
}

void LoopingFileCastAgent::CreateAndStartSession() {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneSender);

  OSP_DCHECK(remote_connection_.has_value());
  environment_ =
      std::make_unique<Environment>(&Clock::now, task_runner_, IPEndpoint{});

  SenderSession::Configuration config{
      connection_settings_->receiver_endpoint.address,
      this,
      environment_.get(),
      &message_port_,
      remote_connection_->local_id,
      remote_connection_->peer_id,
      connection_settings_->use_android_rtp_hack};
  current_session_ = std::make_unique<SenderSession>(std::move(config));
  OSP_DCHECK(!message_port_.client_sender_id().empty());

  AudioCaptureConfig audio_config;
  // Opus does best at 192kbps, so we cap that here.
  audio_config.bit_rate = 192 * 1000;
  VideoCaptureConfig video_config;
  // The video config is allowed to use whatever is left over after audio.
  video_config.max_bit_rate =
      connection_settings_->max_bitrate - audio_config.bit_rate;
  // Use default display resolution of 1080P.
  video_config.resolutions.emplace_back(DisplayResolution{});

  OSP_VLOG << "Starting session negotiation.";
  const Error negotiation_error =
      current_session_->Negotiate({audio_config}, {video_config});
  if (!negotiation_error.ok()) {
    OSP_LOG_ERROR << "Failed to negotiate a session: " << negotiation_error;
  }
}

void LoopingFileCastAgent::OnNegotiated(
    const SenderSession* session,
    SenderSession::ConfiguredSenders senders,
    capture_recommendations::Recommendations capture_recommendations) {
  if (senders.audio_sender == nullptr || senders.video_sender == nullptr) {
    OSP_LOG_ERROR << "Missing both audio and video, so exiting...";
    return;
  }

  file_sender_ = std::make_unique<LoopingFileSender>(
      environment_.get(), connection_settings_->path_to_file.c_str(), session,
      std::move(senders), connection_settings_->max_bitrate);
}

void LoopingFileCastAgent::OnError(const SenderSession* session, Error error) {
  OSP_LOG_ERROR << "SenderSession fatal error: " << error;
  Shutdown();
}

void LoopingFileCastAgent::Shutdown() {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneSender);

  file_sender_.reset();
  if (current_session_) {
    OSP_LOG_INFO << "Stopping mirroring session...";
    current_session_.reset();
  }
  OSP_DCHECK(message_port_.client_sender_id().empty());
  environment_.reset();

  if (remote_connection_) {
    const VirtualConnection connection = *remote_connection_;
    // Reset |remote_connection_| because ConnectionNamespaceHandler may
    // call-back into OnRemoteMessagingOpened().
    remote_connection_.reset();
    connection_handler_.CloseRemoteConnection(connection);
  }

  if (!app_session_id_.empty()) {
    OSP_LOG_INFO << "Stopping the Cast Receiver's Mirroring App...";
    static constexpr char kStopMessageTemplate[] =
        R"({"type":"STOP", "requestId":%d, "sessionId":"%s"})";
    std::string stop_json = StringPrintf(
        kStopMessageTemplate, next_request_id_++, app_session_id_.c_str());
    router_.Send(
        VirtualConnection{kPlatformSenderId, kPlatformReceiverId,
                          message_port_.GetSocketId()},
        MakeSimpleUTF8Message(kReceiverNamespace, std::move(stop_json)));
    app_session_id_.clear();
  }

  if (message_port_.GetSocketId() != ToCastSocketId(nullptr)) {
    router_.CloseSocket(message_port_.GetSocketId());
    message_port_.SetSocket({});
  }

  wake_lock_.reset();

  if (shutdown_callback_) {
    const ShutdownCallback callback = std::move(shutdown_callback_);
    callback();
  }
}

}  // namespace cast
}  // namespace openscreen
