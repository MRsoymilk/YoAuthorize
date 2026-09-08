#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "yoauthorize/crypto/crypto.h"

namespace yoauthorize::protocol {

enum class MessageError {
  None,
  InvalidEnvelope,
  UnexpectedType,
  InvalidField,
};

template <typename T>
struct MessageResult {
  MessageError error = MessageError::None;
  T value{};

  explicit operator bool() const { return error == MessageError::None; }
};

struct ClientHelloMessage {
  std::uint64_t request_id = 0;
  std::uint16_t min_version = 0;
  std::uint16_t max_version = 0;
  crypto::Key nonce{};
  crypto::Key public_key{};
  std::string sdk_version;
};

struct ServerHelloMessage {
  std::uint64_t request_id = 0;
  std::uint16_t selected_version = 0;
  crypto::Key nonce{};
  crypto::Key public_key{};
  std::string service_key_id;
  crypto::Signature signature{};
};

struct ClientFinishedMessage {
  std::uint64_t request_id = 0;
  std::uint16_t protocol_version = 0;
  crypto::Sha256Digest transcript_hash{};
};

MessageResult<std::vector<std::uint8_t>> encodeClientHello(
    const ClientHelloMessage& message);
MessageResult<ClientHelloMessage> decodeClientHello(
    const std::vector<std::uint8_t>& bytes);

MessageResult<std::vector<std::uint8_t>> encodeServerHello(
    const ServerHelloMessage& message);
MessageResult<ServerHelloMessage> decodeServerHello(
    const std::vector<std::uint8_t>& bytes);

MessageResult<std::vector<std::uint8_t>> encodeClientFinished(
    const ClientFinishedMessage& message);
MessageResult<ClientFinishedMessage> decodeClientFinished(
    const std::vector<std::uint8_t>& bytes);

}  // namespace yoauthorize::protocol
