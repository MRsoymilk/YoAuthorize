#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "yoauthorize/crypto/crypto.h"
#include "yoauthorize/crypto/handshake.h"
#include "yoauthorize/protocol/record.h"

namespace yoauthorize::protocol {

enum class HandshakeError {
  None,
  InvalidState,
  InvalidMessage,
  UnsupportedVersion,
  UnknownServiceKey,
  InvalidServiceSignature,
  KeyExchangeFailed,
  InvalidFinished,
  InternalError,
};

template <typename T>
struct HandshakeResult {
  HandshakeError error = HandshakeError::None;
  T value{};

  explicit operator bool() const { return error == HandshakeError::None; }
};

enum class HandshakeState {
  Initial,
  HelloSent,
  AwaitingFinished,
  Established,
  Failed
};

class ClientHandshake {
 public:
  ClientHandshake(std::unordered_map<std::string, crypto::Key> trusted_keys,
                  std::string sdk_version, std::uint64_t request_id,
                  std::uint16_t min_version = 1, std::uint16_t max_version = 1);

  HandshakeResult<std::vector<std::uint8_t>> start();
  HandshakeResult<std::vector<std::uint8_t>> handleServerHello(
      const std::vector<std::uint8_t>& message);
  RecordResult seal(std::span<const std::uint8_t> plaintext);
  RecordResult open(std::span<const std::uint8_t> frame);
  HandshakeState state() const { return state_; }
  std::uint16_t selectedVersion() const { return selected_version_; }

 private:
  void fail();

  std::unordered_map<std::string, crypto::Key> trusted_keys_;
  std::string sdk_version_;
  std::uint64_t request_id_;
  std::uint16_t min_version_;
  std::uint16_t max_version_;
  std::uint16_t selected_version_ = 0;
  crypto::Key nonce_{};
  crypto::KeyPair ephemeral_{};
  std::optional<RecordWriter> writer_;
  std::optional<RecordReader> reader_;
  HandshakeState state_ = HandshakeState::Initial;
};

class ServerHandshake {
 public:
  ServerHandshake(std::string service_key_id, crypto::Key identity_private_key,
                  std::uint16_t min_version = 1, std::uint16_t max_version = 1);

  HandshakeResult<std::vector<std::uint8_t>> handleClientHello(
      const std::vector<std::uint8_t>& message);
  HandshakeError handleClientFinished(
      const std::vector<std::uint8_t>& encrypted_frame);
  RecordResult seal(std::span<const std::uint8_t> plaintext);
  RecordResult open(std::span<const std::uint8_t> frame);
  HandshakeState state() const { return state_; }
  std::uint16_t selectedVersion() const { return selected_version_; }

 private:
  void fail();

  std::string service_key_id_;
  crypto::Key identity_private_key_{};
  std::uint16_t min_version_;
  std::uint16_t max_version_;
  std::uint16_t selected_version_ = 0;
  std::uint64_t request_id_ = 0;
  crypto::Sha256Digest transcript_hash_{};
  std::optional<RecordWriter> writer_;
  std::optional<RecordReader> reader_;
  HandshakeState state_ = HandshakeState::Initial;
};

}  // namespace yoauthorize::protocol
