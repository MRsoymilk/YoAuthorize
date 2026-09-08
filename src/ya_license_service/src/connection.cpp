#include "yoauthorize/service/connection.h"

#include <algorithm>
#include <cstddef>

#include "yoauthorize/protocol/frame.h"
#include "yoauthorize/protocol/messages.h"

namespace yoauthorize::service {
namespace {

std::span<const std::byte> asBytes(std::span<const std::uint8_t> bytes) {
  return {reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()};
}

std::vector<std::uint8_t> wrapPlaintext(
    const std::vector<std::uint8_t>& payload) {
  const protocol::FrameHeader header{
      .payload_size = static_cast<std::uint32_t>(payload.size())};
  const auto encoded_header = protocol::encodeHeader(header);
  std::vector<std::uint8_t> frame;
  frame.reserve(encoded_header.size() + payload.size());
  for (const auto byte : encoded_header) {
    frame.push_back(std::to_integer<std::uint8_t>(byte));
  }
  frame.insert(frame.end(), payload.begin(), payload.end());
  return frame;
}

protocol::SessionId protocolId(const core::SessionId& id) { return id; }

}  // namespace

Connection::Connection(std::string service_key_id,
                       crypto::Key identity_private_key,
                       const core::LicenseSnapshot& license,
                       core::SessionManager& sessions,
                       std::string peer_identity,
                       std::uint32_t heartbeat_interval_ms,
                       std::uint32_t heartbeat_timeout_ms)
    : handshake_(std::move(service_key_id), identity_private_key),
      license_(license),
      sessions_(sessions),
      peer_identity_(std::move(peer_identity)),
      heartbeat_interval_ms_(heartbeat_interval_ms),
      heartbeat_timeout_ms_(heartbeat_timeout_ms) {
  crypto::cleanse(identity_private_key);
}

Connection::~Connection() { closeSession(); }

ConnectionResult Connection::handle(std::span<const std::uint8_t> frame,
                                    std::uint64_t now_monotonic_ms,
                                    std::uint64_t now_unix_seconds) {
  if (state_ == State::Closed) return {.error = ConnectionError::Closed};
  if (state_ == State::AwaitingHello) return handleHello(frame);
  if (state_ == State::AwaitingFinished) {
    const std::vector<std::uint8_t> input(frame.begin(), frame.end());
    if (handshake_.handleClientFinished(input) !=
        protocol::HandshakeError::None) {
      state_ = State::Closed;
      return {.error = ConnectionError::HandshakeFailed, .closed = true};
    }
    state_ = State::AwaitingAuthorize;
    return {};
  }

  const auto opened = handshake_.open(frame);
  if (!opened) {
    state_ = State::Closed;
    closeSession();
    return {.error = ConnectionError::InvalidFrame, .closed = true};
  }
  if (state_ == State::AwaitingAuthorize) {
    return handleAuthorize(opened.bytes, now_monotonic_ms, now_unix_seconds);
  }
  return handleActive(opened.bytes, now_monotonic_ms, now_unix_seconds);
}

ConnectionResult Connection::handleHello(std::span<const std::uint8_t> frame) {
  const auto decoded = protocol::decodeFrame(asBytes(frame));
  if (!decoded || decoded.header.flags != 0 || decoded.header.sequence != 0) {
    state_ = State::Closed;
    return {.error = ConnectionError::InvalidFrame, .closed = true};
  }
  const std::vector<std::uint8_t> hello(
      reinterpret_cast<const std::uint8_t*>(decoded.payload.data()),
      reinterpret_cast<const std::uint8_t*>(decoded.payload.data()) +
          decoded.payload.size());
  const auto response = handshake_.handleClientHello(hello);
  if (!response) {
    state_ = State::Closed;
    return {.error = ConnectionError::HandshakeFailed, .closed = true};
  }
  state_ = State::AwaitingFinished;
  return {.response = wrapPlaintext(response.value)};
}

ConnectionResult Connection::handleAuthorize(
    std::span<const std::uint8_t> plaintext, std::uint64_t now_monotonic_ms,
    std::uint64_t now_unix_seconds) {
  const std::vector<std::uint8_t> bytes(plaintext.begin(), plaintext.end());
  const auto request = protocol::decodeAuthorize(bytes);
  if (!request ||
      request.value.protocol_version != handshake_.selectedVersion()) {
    state_ = State::Closed;
    return {.error = ConnectionError::InvalidMessage, .closed = true};
  }

  protocol::AuthResultMessage response{
      .request_id = request.value.request_id,
      .protocol_version = handshake_.selectedVersion(),
      .heartbeat_interval_ms = heartbeat_interval_ms_,
      .heartbeat_timeout_ms = heartbeat_timeout_ms_,
  };
  ConnectionError result_error = ConnectionError::None;
  if (request.value.product_id != license_.product_id) {
    response.error = protocol::AuthorizationError::ProductMismatch;
    result_error = ConnectionError::ProductMismatch;
  } else if (license_.expire_time != 0 &&
             now_unix_seconds >= license_.expire_time) {
    response.error = protocol::AuthorizationError::Expired;
    result_error = ConnectionError::LicenseExpired;
  } else {
    auto session = sessions_.create(license_.product_id, peer_identity_,
                                    license_.features, license_.expire_time,
                                    now_monotonic_ms, license_.max_sessions);
    if (!session) {
      response.error = protocol::AuthorizationError::SessionLimitExceeded;
      result_error = ConnectionError::SessionLimitExceeded;
    } else {
      session_ = std::move(session.value);
      response.session_id = protocolId(session_->id);
      response.features = session_->features;
      response.expire_time = session_->expire_time;
      state_ = State::Active;
    }
  }
  const auto encoded = protocol::encodeAuthResult(response);
  if (!encoded) {
    state_ = State::Closed;
    closeSession();
    return {.error = ConnectionError::InvalidMessage, .closed = true};
  }
  auto result =
      encryptedResponse(encoded.value, result_error != ConnectionError::None);
  result.error = result_error;
  return result;
}

ConnectionResult Connection::handleActive(
    std::span<const std::uint8_t> plaintext, std::uint64_t now_monotonic_ms,
    std::uint64_t now_unix_seconds) {
  const std::vector<std::uint8_t> bytes(plaintext.begin(), plaintext.end());
  const auto heartbeat = protocol::decodeHeartbeat(bytes);
  if (heartbeat) {
    if (heartbeat.value.protocol_version != handshake_.selectedVersion() ||
        !session_ || heartbeat.value.session_id != protocolId(session_->id) ||
        sessions_.heartbeat(session_->id, now_monotonic_ms) !=
            core::ErrorCode::None) {
      state_ = State::Closed;
      closeSession();
      return {.error = ConnectionError::InvalidSession, .closed = true};
    }
    const bool expired =
        license_.expire_time != 0 && now_unix_seconds >= license_.expire_time;
    const protocol::HeartbeatAckMessage ack{
        .request_id = heartbeat.value.request_id,
        .protocol_version = handshake_.selectedVersion(),
        .state = expired ? protocol::AuthorizationState::Expired
                         : protocol::AuthorizationState::Valid,
        .features = session_->features,
        .expire_time = session_->expire_time,
    };
    const auto encoded = protocol::encodeHeartbeatAck(ack);
    if (!encoded) {
      state_ = State::Closed;
      closeSession();
      return {.error = ConnectionError::InvalidMessage, .closed = true};
    }
    if (expired) {
      closeSession();
      auto result = encryptedResponse(encoded.value, true);
      result.error = ConnectionError::LicenseExpired;
      return result;
    }
    return encryptedResponse(encoded.value);
  }

  const auto close = protocol::decodeCloseSession(bytes);
  if (close && close.value.protocol_version == handshake_.selectedVersion()) {
    closeSession();
    state_ = State::Closed;
    return {.closed = true};
  }
  state_ = State::Closed;
  closeSession();
  return {.error = ConnectionError::InvalidMessage, .closed = true};
}

ConnectionResult Connection::encryptedResponse(
    std::vector<std::uint8_t> plaintext, bool close_after) {
  const auto sealed = handshake_.seal(plaintext);
  if (!sealed) {
    state_ = State::Closed;
    closeSession();
    return {.error = ConnectionError::InvalidFrame, .closed = true};
  }
  if (close_after) state_ = State::Closed;
  return {.response = sealed.bytes, .closed = close_after};
}

void Connection::closeSession() {
  if (session_) {
    sessions_.close(session_->id);
    session_.reset();
  }
}

bool Connection::established() const {
  return state_ == State::AwaitingAuthorize || state_ == State::Active;
}

bool Connection::closed() const { return state_ == State::Closed; }

}  // namespace yoauthorize::service
