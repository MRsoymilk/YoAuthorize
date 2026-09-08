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

}  // namespace yoauthorize::protocol
