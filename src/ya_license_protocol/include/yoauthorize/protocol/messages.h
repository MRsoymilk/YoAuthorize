#pragma once

#include <array>
#include <cstdint>
#include <optional>
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

using SessionId = std::array<std::uint8_t, 16>;

enum class AuthorizationError : std::uint16_t {
  None = 0,
  InvalidFrame = 1001,
  UnsupportedVersion = 1002,
  UnsupportedMessage = 1003,
  LicenseNotFound = 2001,
  InvalidLicense = 2002,
  InvalidSignature = 2003,
  NotYetValid = 2004,
  Expired = 2005,
  MachineMismatch = 2006,
  ProductMismatch = 2007,
  Revoked = 2008,
  InvalidSession = 3001,
  SessionExpired = 3002,
  SessionLimitExceeded = 3003,
  DuplicateRequest = 3004,
  AuthenticationFailed = 4001,
  ServiceIdentityInvalid = 4002,
  InternalError = 5001,
  StorageError = 5002,
};

enum class AuthorizationState : std::uint8_t {
  Unknown = 0,
  Valid = 1,
  Grace = 2,
  Expired = 3,
  Revoked = 4,
};

struct AuthorizeMessage {
  std::uint64_t request_id = 0;
  std::uint16_t protocol_version = 0;
  std::string product_id;
  std::uint64_t process_id = 0;
  std::uint64_t process_start_time = 0;
};

struct AuthResultMessage {
  std::uint64_t request_id = 0;
  std::uint16_t protocol_version = 0;
  AuthorizationError error = AuthorizationError::None;
  std::optional<SessionId> session_id;
  std::vector<std::string> features;
  std::uint64_t expire_time = 0;
  std::uint32_t heartbeat_interval_ms = 0;
  std::uint32_t heartbeat_timeout_ms = 0;
};

struct HeartbeatMessage {
  std::uint64_t request_id = 0;
  std::uint16_t protocol_version = 0;
  SessionId session_id{};
};

struct HeartbeatAckMessage {
  std::uint64_t request_id = 0;
  std::uint16_t protocol_version = 0;
  AuthorizationState state = AuthorizationState::Unknown;
  std::vector<std::string> features;
  std::uint64_t expire_time = 0;
};

struct CloseSessionMessage {
  std::uint64_t request_id = 0;
  std::uint16_t protocol_version = 0;
};

struct ErrorMessage {
  std::uint64_t request_id = 0;
  std::uint16_t protocol_version = 0;
  AuthorizationError code = AuthorizationError::InternalError;
  std::string message;
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

MessageResult<std::vector<std::uint8_t>> encodeAuthorize(
    const AuthorizeMessage& message);
MessageResult<AuthorizeMessage> decodeAuthorize(
    const std::vector<std::uint8_t>& bytes);
MessageResult<std::vector<std::uint8_t>> encodeAuthResult(
    const AuthResultMessage& message);
MessageResult<AuthResultMessage> decodeAuthResult(
    const std::vector<std::uint8_t>& bytes);
MessageResult<std::vector<std::uint8_t>> encodeHeartbeat(
    const HeartbeatMessage& message);
MessageResult<HeartbeatMessage> decodeHeartbeat(
    const std::vector<std::uint8_t>& bytes);
MessageResult<std::vector<std::uint8_t>> encodeHeartbeatAck(
    const HeartbeatAckMessage& message);
MessageResult<HeartbeatAckMessage> decodeHeartbeatAck(
    const std::vector<std::uint8_t>& bytes);
MessageResult<std::vector<std::uint8_t>> encodeCloseSession(
    const CloseSessionMessage& message);
MessageResult<CloseSessionMessage> decodeCloseSession(
    const std::vector<std::uint8_t>& bytes);
MessageResult<std::vector<std::uint8_t>> encodeError(
    const ErrorMessage& message);
MessageResult<ErrorMessage> decodeError(const std::vector<std::uint8_t>& bytes);

}  // namespace yoauthorize::protocol
