#include "yoauthorize/protocol/messages.h"

#include <flatbuffers/flatbuffer_builder.h>

#include <algorithm>
#include <span>
#include <utility>

#include "protocol_generated.h"
#include "yoauthorize/protocol/envelope.h"

namespace yoauthorize::protocol {
namespace {

constexpr std::size_t kMaxSdkVersionSize = 64;
constexpr std::size_t kMaxKeyIdSize = 255;
constexpr std::size_t kMaxProductIdSize = 255;
constexpr std::size_t kMaxFeatureSize = 128;
constexpr std::size_t kMaxFeatures = 256;
constexpr std::size_t kMaxErrorMessageSize = 256;

template <typename Array>
bool copyFixed(const flatbuffers::Vector<std::uint8_t>* source, Array& output) {
  if (!source || source->size() != output.size()) {
    return false;
  }
  std::ranges::copy(*source, output.begin());
  return true;
}

template <typename Offset>
std::vector<std::uint8_t> finish(flatbuffers::FlatBufferBuilder& builder,
                                 Offset body, Message type,
                                 std::uint16_t protocol_version,
                                 std::uint64_t request_id) {
  const auto envelope =
      CreateEnvelope(builder, protocol_version, request_id, type, body.Union());
  FinishEnvelopeBuffer(builder, envelope);
  return {builder.GetBufferPointer(),
          builder.GetBufferPointer() + builder.GetSize()};
}

const Envelope* checkedEnvelope(const std::vector<std::uint8_t>& bytes) {
  return verifyEnvelope(bytes) == EnvelopeError::None
             ? GetEnvelope(bytes.data())
             : nullptr;
}

bool validApplicationEnvelope(const Envelope& envelope, Message type) {
  return envelope.protocol_version() != 0 && envelope.request_id() != 0 &&
         envelope.body_type() == type;
}

bool validFeatures(const std::vector<std::string>& features) {
  return features.size() <= kMaxFeatures &&
         std::ranges::all_of(features, [](const auto& feature) {
           return !feature.empty() && feature.size() <= kMaxFeatureSize;
         });
}

bool validAuthorizationError(AuthorizationError error) {
  switch (error) {
    case AuthorizationError::None:
    case AuthorizationError::InvalidFrame:
    case AuthorizationError::UnsupportedVersion:
    case AuthorizationError::UnsupportedMessage:
    case AuthorizationError::LicenseNotFound:
    case AuthorizationError::InvalidLicense:
    case AuthorizationError::InvalidSignature:
    case AuthorizationError::NotYetValid:
    case AuthorizationError::Expired:
    case AuthorizationError::MachineMismatch:
    case AuthorizationError::ProductMismatch:
    case AuthorizationError::Revoked:
    case AuthorizationError::InvalidSession:
    case AuthorizationError::SessionExpired:
    case AuthorizationError::SessionLimitExceeded:
    case AuthorizationError::DuplicateRequest:
    case AuthorizationError::AuthenticationFailed:
    case AuthorizationError::ServiceIdentityInvalid:
    case AuthorizationError::InternalError:
    case AuthorizationError::StorageError:
      return true;
  }
  return false;
}

bool validAuthorizationState(AuthorizationState state) {
  switch (state) {
    case AuthorizationState::Valid:
    case AuthorizationState::Grace:
    case AuthorizationState::Expired:
    case AuthorizationState::Revoked:
      return true;
    case AuthorizationState::Unknown:
      return false;
  }
  return false;
}

std::vector<std::string> copyFeatures(
    const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*
        source) {
  std::vector<std::string> features;
  if (!source || source->size() > kMaxFeatures) return features;
  features.reserve(source->size());
  for (const auto* feature : *source) {
    if (!feature || feature->empty() || feature->size() > kMaxFeatureSize) {
      return {};
    }
    features.push_back(feature->str());
  }
  return features;
}

std::vector<flatbuffers::Offset<flatbuffers::String>> createFeatures(
    flatbuffers::FlatBufferBuilder& builder,
    const std::vector<std::string>& features) {
  std::vector<flatbuffers::Offset<flatbuffers::String>> offsets;
  offsets.reserve(features.size());
  for (const auto& feature : features) {
    offsets.push_back(builder.CreateString(feature));
  }
  return offsets;
}

}  // namespace

MessageResult<std::vector<std::uint8_t>> encodeClientHello(
    const ClientHelloMessage& message) {
  if (message.request_id == 0 || message.min_version == 0 ||
      message.min_version > message.max_version ||
      message.sdk_version.empty() ||
      message.sdk_version.size() > kMaxSdkVersionSize) {
    return {.error = MessageError::InvalidField};
  }
  flatbuffers::FlatBufferBuilder builder;
  const auto body = CreateClientHello(
      builder, message.min_version, message.max_version,
      builder.CreateVector(message.nonce.data(), message.nonce.size()),
      builder.CreateVector(message.public_key.data(),
                           message.public_key.size()),
      builder.CreateString(message.sdk_version));
  return {.value = finish(builder, body, Message::ClientHello, 0,
                          message.request_id)};
}

MessageResult<ClientHelloMessage> decodeClientHello(
    const std::vector<std::uint8_t>& bytes) {
  const auto* envelope = checkedEnvelope(bytes);
  if (!envelope) return {.error = MessageError::InvalidEnvelope};
  if (envelope->protocol_version() != 0 || envelope->request_id() == 0 ||
      envelope->body_type() != Message::ClientHello) {
    return {.error = MessageError::UnexpectedType};
  }
  const auto* body = envelope->body_as_ClientHello();
  ClientHelloMessage message{
      .request_id = envelope->request_id(),
      .min_version = body->min_protocol_version(),
      .max_version = body->max_protocol_version(),
      .sdk_version = body->sdk_version() ? body->sdk_version()->str() : ""};
  if (message.min_version == 0 || message.min_version > message.max_version ||
      message.sdk_version.empty() ||
      message.sdk_version.size() > kMaxSdkVersionSize ||
      !copyFixed(body->client_nonce(), message.nonce) ||
      !copyFixed(body->ephemeral_public_key(), message.public_key)) {
    return {.error = MessageError::InvalidField};
  }
  return {.value = std::move(message)};
}

MessageResult<std::vector<std::uint8_t>> encodeServerHello(
    const ServerHelloMessage& message) {
  if (message.request_id == 0 || message.selected_version == 0 ||
      message.service_key_id.empty() ||
      message.service_key_id.size() > kMaxKeyIdSize) {
    return {.error = MessageError::InvalidField};
  }
  flatbuffers::FlatBufferBuilder builder;
  const auto body = CreateServerHello(
      builder, message.selected_version,
      builder.CreateVector(message.nonce.data(), message.nonce.size()),
      builder.CreateVector(message.public_key.data(),
                           message.public_key.size()),
      builder.CreateString(message.service_key_id),
      builder.CreateVector(message.signature.data(), message.signature.size()));
  return {.value = finish(builder, body, Message::ServerHello, 0,
                          message.request_id)};
}

MessageResult<ServerHelloMessage> decodeServerHello(
    const std::vector<std::uint8_t>& bytes) {
  const auto* envelope = checkedEnvelope(bytes);
  if (!envelope) return {.error = MessageError::InvalidEnvelope};
  if (envelope->protocol_version() != 0 || envelope->request_id() == 0 ||
      envelope->body_type() != Message::ServerHello) {
    return {.error = MessageError::UnexpectedType};
  }
  const auto* body = envelope->body_as_ServerHello();
  ServerHelloMessage message{
      .request_id = envelope->request_id(),
      .selected_version = body->selected_protocol_version(),
      .service_key_id =
          body->service_key_id() ? body->service_key_id()->str() : ""};
  if (message.selected_version == 0 || message.service_key_id.empty() ||
      message.service_key_id.size() > kMaxKeyIdSize ||
      !copyFixed(body->server_nonce(), message.nonce) ||
      !copyFixed(body->ephemeral_public_key(), message.public_key) ||
      !copyFixed(body->signature(), message.signature)) {
    return {.error = MessageError::InvalidField};
  }
  return {.value = std::move(message)};
}

MessageResult<std::vector<std::uint8_t>> encodeClientFinished(
    const ClientFinishedMessage& message) {
  if (message.request_id == 0 || message.protocol_version == 0) {
    return {.error = MessageError::InvalidField};
  }
  flatbuffers::FlatBufferBuilder builder;
  const auto body = CreateClientFinished(
      builder, builder.CreateVector(message.transcript_hash.data(),
                                    message.transcript_hash.size()));
  return {.value = finish(builder, body, Message::ClientFinished,
                          message.protocol_version, message.request_id)};
}

MessageResult<ClientFinishedMessage> decodeClientFinished(
    const std::vector<std::uint8_t>& bytes) {
  const auto* envelope = checkedEnvelope(bytes);
  if (!envelope) return {.error = MessageError::InvalidEnvelope};
  if (envelope->protocol_version() == 0 || envelope->request_id() == 0 ||
      envelope->body_type() != Message::ClientFinished) {
    return {.error = MessageError::UnexpectedType};
  }
  const auto* body = envelope->body_as_ClientFinished();
  ClientFinishedMessage message{
      .request_id = envelope->request_id(),
      .protocol_version = envelope->protocol_version()};
  if (!copyFixed(body->transcript_hash(), message.transcript_hash)) {
    return {.error = MessageError::InvalidField};
  }
  return {.value = std::move(message)};
}

MessageResult<std::vector<std::uint8_t>> encodeAuthorize(
    const AuthorizeMessage& message) {
  if (message.request_id == 0 || message.protocol_version == 0 ||
      message.product_id.empty() ||
      message.product_id.size() > kMaxProductIdSize ||
      message.process_id == 0) {
    return {.error = MessageError::InvalidField};
  }
  flatbuffers::FlatBufferBuilder builder;
  const auto body =
      CreateAuthorize(builder, builder.CreateString(message.product_id),
                      message.process_id, message.process_start_time);
  return {.value = finish(builder, body, Message::Authorize,
                          message.protocol_version, message.request_id)};
}

MessageResult<AuthorizeMessage> decodeAuthorize(
    const std::vector<std::uint8_t>& bytes) {
  const auto* envelope = checkedEnvelope(bytes);
  if (!envelope) return {.error = MessageError::InvalidEnvelope};
  if (!validApplicationEnvelope(*envelope, Message::Authorize)) {
    return {.error = MessageError::UnexpectedType};
  }
  const auto* body = envelope->body_as_Authorize();
  AuthorizeMessage message{
      .request_id = envelope->request_id(),
      .protocol_version = envelope->protocol_version(),
      .product_id = body->product_id() ? body->product_id()->str() : "",
      .process_id = body->process_id(),
      .process_start_time = body->process_start_time(),
  };
  if (message.product_id.empty() ||
      message.product_id.size() > kMaxProductIdSize ||
      message.process_id == 0) {
    return {.error = MessageError::InvalidField};
  }
  return {.value = std::move(message)};
}

MessageResult<std::vector<std::uint8_t>> encodeAuthResult(
    const AuthResultMessage& message) {
  if (message.request_id == 0 || message.protocol_version == 0 ||
      !validAuthorizationError(message.error) ||
      !validFeatures(message.features) ||
      (message.error == AuthorizationError::None && !message.session_id) ||
      (message.error != AuthorizationError::None && message.session_id)) {
    return {.error = MessageError::InvalidField};
  }
  flatbuffers::FlatBufferBuilder builder;
  const auto features = createFeatures(builder, message.features);
  const auto session = message.session_id
                           ? builder.CreateVector(message.session_id->data(),
                                                  message.session_id->size())
                           : 0;
  const auto body = CreateAuthResult(
      builder, static_cast<ErrorCode>(message.error), session,
      builder.CreateVector(features), message.expire_time,
      message.heartbeat_interval_ms, message.heartbeat_timeout_ms);
  return {.value = finish(builder, body, Message::AuthResult,
                          message.protocol_version, message.request_id)};
}

MessageResult<AuthResultMessage> decodeAuthResult(
    const std::vector<std::uint8_t>& bytes) {
  const auto* envelope = checkedEnvelope(bytes);
  if (!envelope) return {.error = MessageError::InvalidEnvelope};
  if (!validApplicationEnvelope(*envelope, Message::AuthResult)) {
    return {.error = MessageError::UnexpectedType};
  }
  const auto* body = envelope->body_as_AuthResult();
  AuthResultMessage message{
      .request_id = envelope->request_id(),
      .protocol_version = envelope->protocol_version(),
      .error = static_cast<AuthorizationError>(body->error()),
      .features = copyFeatures(body->features()),
      .expire_time = body->expire_time(),
      .heartbeat_interval_ms = body->heartbeat_interval_ms(),
      .heartbeat_timeout_ms = body->heartbeat_timeout_ms(),
  };
  if (body->features() && body->features()->size() != message.features.size()) {
    return {.error = MessageError::InvalidField};
  }
  if (body->session_id() && !body->session_id()->empty()) {
    SessionId id{};
    if (!copyFixed(body->session_id(), id)) {
      return {.error = MessageError::InvalidField};
    }
    message.session_id = id;
  }
  if (!validAuthorizationError(message.error) ||
      (message.error == AuthorizationError::None && !message.session_id) ||
      (message.error != AuthorizationError::None && message.session_id)) {
    return {.error = MessageError::InvalidField};
  }
  return {.value = std::move(message)};
}

MessageResult<std::vector<std::uint8_t>> encodeHeartbeat(
    const HeartbeatMessage& message) {
  if (message.request_id == 0 || message.protocol_version == 0) {
    return {.error = MessageError::InvalidField};
  }
  flatbuffers::FlatBufferBuilder builder;
  const auto body =
      CreateHeartbeat(builder, builder.CreateVector(message.session_id.data(),
                                                    message.session_id.size()));
  return {.value = finish(builder, body, Message::Heartbeat,
                          message.protocol_version, message.request_id)};
}

MessageResult<HeartbeatMessage> decodeHeartbeat(
    const std::vector<std::uint8_t>& bytes) {
  const auto* envelope = checkedEnvelope(bytes);
  if (!envelope) return {.error = MessageError::InvalidEnvelope};
  if (!validApplicationEnvelope(*envelope, Message::Heartbeat)) {
    return {.error = MessageError::UnexpectedType};
  }
  HeartbeatMessage message{.request_id = envelope->request_id(),
                           .protocol_version = envelope->protocol_version()};
  if (!copyFixed(envelope->body_as_Heartbeat()->session_id(),
                 message.session_id)) {
    return {.error = MessageError::InvalidField};
  }
  return {.value = message};
}

MessageResult<std::vector<std::uint8_t>> encodeHeartbeatAck(
    const HeartbeatAckMessage& message) {
  if (message.request_id == 0 || message.protocol_version == 0 ||
      !validAuthorizationState(message.state) ||
      !validFeatures(message.features)) {
    return {.error = MessageError::InvalidField};
  }
  flatbuffers::FlatBufferBuilder builder;
  const auto features = createFeatures(builder, message.features);
  const auto body =
      CreateHeartbeatAck(builder, static_cast<LicenseState>(message.state),
                         builder.CreateVector(features), message.expire_time);
  return {.value = finish(builder, body, Message::HeartbeatAck,
                          message.protocol_version, message.request_id)};
}

MessageResult<HeartbeatAckMessage> decodeHeartbeatAck(
    const std::vector<std::uint8_t>& bytes) {
  const auto* envelope = checkedEnvelope(bytes);
  if (!envelope) return {.error = MessageError::InvalidEnvelope};
  if (!validApplicationEnvelope(*envelope, Message::HeartbeatAck)) {
    return {.error = MessageError::UnexpectedType};
  }
  const auto* body = envelope->body_as_HeartbeatAck();
  HeartbeatAckMessage message{
      .request_id = envelope->request_id(),
      .protocol_version = envelope->protocol_version(),
      .state = static_cast<AuthorizationState>(body->state()),
      .features = copyFeatures(body->features()),
      .expire_time = body->expire_time(),
  };
  if (!validAuthorizationState(message.state) ||
      (body->features() &&
       body->features()->size() != message.features.size())) {
    return {.error = MessageError::InvalidField};
  }
  return {.value = std::move(message)};
}

MessageResult<std::vector<std::uint8_t>> encodeCloseSession(
    const CloseSessionMessage& message) {
  if (message.request_id == 0 || message.protocol_version == 0) {
    return {.error = MessageError::InvalidField};
  }
  flatbuffers::FlatBufferBuilder builder;
  const auto body = CreateCloseSession(builder);
  return {.value = finish(builder, body, Message::CloseSession,
                          message.protocol_version, message.request_id)};
}

MessageResult<CloseSessionMessage> decodeCloseSession(
    const std::vector<std::uint8_t>& bytes) {
  const auto* envelope = checkedEnvelope(bytes);
  if (!envelope) return {.error = MessageError::InvalidEnvelope};
  if (!validApplicationEnvelope(*envelope, Message::CloseSession)) {
    return {.error = MessageError::UnexpectedType};
  }
  return {.value = {.request_id = envelope->request_id(),
                    .protocol_version = envelope->protocol_version()}};
}

MessageResult<std::vector<std::uint8_t>> encodeError(
    const ErrorMessage& message) {
  if (message.request_id == 0 || message.protocol_version == 0 ||
      !validAuthorizationError(message.code) ||
      message.code == AuthorizationError::None || message.message.empty() ||
      message.message.size() > kMaxErrorMessageSize) {
    return {.error = MessageError::InvalidField};
  }
  flatbuffers::FlatBufferBuilder builder;
  const auto body = CreateError(builder, static_cast<ErrorCode>(message.code),
                                builder.CreateString(message.message));
  return {.value = finish(builder, body, Message::Error,
                          message.protocol_version, message.request_id)};
}

MessageResult<ErrorMessage> decodeError(
    const std::vector<std::uint8_t>& bytes) {
  const auto* envelope = checkedEnvelope(bytes);
  if (!envelope) return {.error = MessageError::InvalidEnvelope};
  if (!validApplicationEnvelope(*envelope, Message::Error)) {
    return {.error = MessageError::UnexpectedType};
  }
  const auto* body = envelope->body_as_Error();
  ErrorMessage message{
      .request_id = envelope->request_id(),
      .protocol_version = envelope->protocol_version(),
      .code = static_cast<AuthorizationError>(body->code()),
      .message = body->message() ? body->message()->str() : "",
  };
  if (!validAuthorizationError(message.code) ||
      message.code == AuthorizationError::None || message.message.empty() ||
      message.message.size() > kMaxErrorMessageSize) {
    return {.error = MessageError::InvalidField};
  }
  return {.value = std::move(message)};
}

}  // namespace yoauthorize::protocol
