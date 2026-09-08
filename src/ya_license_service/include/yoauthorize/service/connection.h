#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "yoauthorize/core/license.h"
#include "yoauthorize/core/session.h"
#include "yoauthorize/crypto/crypto.h"
#include "yoauthorize/protocol/handshake.h"

namespace yoauthorize::service {

enum class ConnectionError {
  None,
  InvalidFrame,
  HandshakeFailed,
  InvalidMessage,
  ProductMismatch,
  SessionLimitExceeded,
  InvalidSession,
  LicenseExpired,
  Closed,
};

struct ConnectionResult {
  ConnectionError error = ConnectionError::None;
  std::optional<std::vector<std::uint8_t>> response;
  bool closed = false;

  explicit operator bool() const { return error == ConnectionError::None; }
};

class Connection {
 public:
  Connection(std::string service_key_id, crypto::Key identity_private_key,
             const core::LicenseSnapshot& license,
             core::SessionManager& sessions, std::string peer_identity,
             std::uint32_t heartbeat_interval_ms,
             std::uint32_t heartbeat_timeout_ms);
  ~Connection();

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  ConnectionResult handle(std::span<const std::uint8_t> frame,
                          std::uint64_t now_monotonic_ms,
                          std::uint64_t now_unix_seconds);
  bool established() const;
  bool closed() const;

 private:
  enum class State {
    AwaitingHello,
    AwaitingFinished,
    AwaitingAuthorize,
    Active,
    Closed
  };

  ConnectionResult handleHello(std::span<const std::uint8_t> frame);
  ConnectionResult handleAuthorize(std::span<const std::uint8_t> plaintext,
                                   std::uint64_t now_monotonic_ms,
                                   std::uint64_t now_unix_seconds);
  ConnectionResult handleActive(std::span<const std::uint8_t> plaintext,
                                std::uint64_t now_monotonic_ms,
                                std::uint64_t now_unix_seconds);
  ConnectionResult encryptedResponse(std::vector<std::uint8_t> plaintext,
                                     bool close_after = false);
  void closeSession();

  protocol::ServerHandshake handshake_;
  const core::LicenseSnapshot& license_;
  core::SessionManager& sessions_;
  std::string peer_identity_;
  std::uint32_t heartbeat_interval_ms_;
  std::uint32_t heartbeat_timeout_ms_;
  std::optional<core::Session> session_;
  State state_ = State::AwaitingHello;
};

}  // namespace yoauthorize::service
