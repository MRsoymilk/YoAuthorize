#include "yoauthorize/sdk/client.h"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <span>
#include <thread>
#include <utility>

#include "yoauthorize/protocol/frame.h"
#include "yoauthorize/protocol/handshake.h"
#include "yoauthorize/protocol/messages.h"
#include "yoauthorize/transport/unix_socket.h"

namespace yoauthorize::sdk {
namespace {

transport::Deadline deadlineAfter(std::chrono::milliseconds timeout) {
  return std::chrono::steady_clock::now() + timeout;
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

Status writeFrame(transport::ITransport& transport,
                  const std::vector<std::uint8_t>& frame,
                  std::chrono::milliseconds timeout) {
  const std::span bytes(reinterpret_cast<const std::byte*>(frame.data()),
                        frame.size());
  return transport.writeAll(bytes, deadlineAfter(timeout))
             ? Status{}
             : Status{.error = ClientError::Transport,
                      .message = "failed to write service frame"};
}

struct ReadResult {
  Status status;
  std::vector<std::uint8_t> frame;
};

ReadResult readFrame(transport::ITransport& transport,
                     std::chrono::milliseconds timeout) {
  std::array<std::byte, protocol::kFrameHeaderSize> header_bytes{};
  if (!transport.readExact(header_bytes, deadlineAfter(timeout))) {
    return {.status = {.error = ClientError::Transport,
                       .message = "failed to read service frame"}};
  }
  const auto header = protocol::decodeHeader(header_bytes);
  if (!header) {
    return {.status = {.error = ClientError::Protocol,
                       .message = "service sent an invalid frame"}};
  }
  std::vector<std::uint8_t> frame(protocol::kFrameHeaderSize +
                                  header.header.payload_size);
  for (std::size_t i = 0; i < header_bytes.size(); ++i) {
    frame[i] = std::to_integer<std::uint8_t>(header_bytes[i]);
  }
  std::span payload(
      reinterpret_cast<std::byte*>(frame.data()) + protocol::kFrameHeaderSize,
      header.header.payload_size);
  if (!transport.readExact(payload, deadlineAfter(timeout))) {
    return {.status = {.error = ClientError::Transport,
                       .message = "failed to read service payload"}};
  }
  return {.frame = std::move(frame)};
}

std::vector<std::uint8_t> unwrapPlaintext(
    const std::vector<std::uint8_t>& frame) {
  const auto decoded = protocol::decodeFrame(std::span(
      reinterpret_cast<const std::byte*>(frame.data()), frame.size()));
  if (!decoded || decoded.header.flags != 0 || decoded.header.sequence != 0) {
    return {};
  }
  return {reinterpret_cast<const std::uint8_t*>(decoded.payload.data()),
          reinterpret_cast<const std::uint8_t*>(decoded.payload.data()) +
              decoded.payload.size()};
}

}  // namespace

class LicenseClient::Impl {
 public:
  Status initialize(const ClientConfig& config) {
    std::lock_guard operation_lock(operation_mutex_);
    {
      std::lock_guard snapshot_lock(snapshot_mutex_);
      if (snapshot_.state != ClientState::Uninitialized) {
        return {.error = ClientError::AlreadyInitialized,
                .message = "client is already initialized"};
      }
      if (config.product_id.empty() || config.endpoint.empty() ||
          config.trusted_service_keys.empty() ||
          config.connect_timeout <= std::chrono::milliseconds::zero() ||
          config.io_timeout <= std::chrono::milliseconds::zero()) {
        snapshot_.state = ClientState::Error;
        return {.error = ClientError::InvalidConfig,
                .message = "client configuration is incomplete"};
      }
      snapshot_.state = ClientState::Connecting;
    }

    transport_ =
        std::make_unique<transport::UnixSocketTransport>(config.endpoint);
    if (!transport_->connect(deadlineAfter(config.connect_timeout))) {
      return fail(ClientError::Transport, "failed to connect to service");
    }
    handshake_ = std::make_unique<protocol::ClientHandshake>(
        config.trusted_service_keys, "yoauthorize-cpp/0.1", 1);
    const auto hello = handshake_->start();
    if (!hello || !writeFrame(*transport_, wrapPlaintext(hello.value),
                              config.io_timeout)) {
      return fail(ClientError::Protocol, "failed to start service handshake");
    }
    const auto server_frame = readFrame(*transport_, config.io_timeout);
    if (!server_frame.status) return fail(server_frame.status);
    const auto server_hello = unwrapPlaintext(server_frame.frame);
    if (server_hello.empty()) {
      return fail(ClientError::Protocol, "service hello frame is invalid");
    }
    const auto finished = handshake_->handleServerHello(server_hello);
    if (!finished) {
      return fail(ClientError::ServiceIdentityInvalid,
                  "service identity authentication failed");
    }
    if (!writeFrame(*transport_, finished.value, config.io_timeout)) {
      return fail(ClientError::Transport, "failed to finish service handshake");
    }

    const auto authorize = protocol::encodeAuthorize({
        .request_id = 2,
        .protocol_version = handshake_->selectedVersion(),
        .product_id = config.product_id,
        .process_id = static_cast<std::uint64_t>(::getpid()),
    });
    if (!authorize) {
      return fail(ClientError::Protocol, "failed to encode authorization");
    }
    const auto sealed = handshake_->seal(authorize.value);
    if (!sealed || !writeFrame(*transport_, sealed.bytes, config.io_timeout)) {
      return fail(ClientError::Transport, "failed to send authorization");
    }
    const auto response_frame = readFrame(*transport_, config.io_timeout);
    if (!response_frame.status) return fail(response_frame.status);
    const auto opened = handshake_->open(response_frame.frame);
    if (!opened) {
      return fail(ClientError::Protocol, "authorization response is invalid");
    }
    const auto response = protocol::decodeAuthResult(opened.bytes);
    if (!response ||
        response.value.error != protocol::AuthorizationError::None ||
        response.value.request_id != 2 ||
        response.value.protocol_version != handshake_->selectedVersion() ||
        !response.value.session_id ||
        response.value.heartbeat_interval_ms == 0 ||
        response.value.heartbeat_timeout_ms == 0 ||
        response.value.heartbeat_interval_ms >=
            response.value.heartbeat_timeout_ms) {
      return fail(ClientError::AuthorizationDenied,
                  "service denied authorization");
    }

    config_ = config;
    session_id_ = *response.value.session_id;
    heartbeat_interval_ =
        std::chrono::milliseconds(response.value.heartbeat_interval_ms);
    {
      std::lock_guard snapshot_lock(snapshot_mutex_);
      snapshot_.state = ClientState::Valid;
      snapshot_.features = std::move(response.value.features);
      snapshot_.expire_time = response.value.expire_time;
    }
    heartbeat_thread_ = std::thread([this] { heartbeatLoop(); });
    return {};
  }

  void shutdown() noexcept {
    std::lock_guard operation_lock(operation_mutex_);
    {
      std::lock_guard lock(wait_mutex_);
      stop_requested_ = true;
    }
    wait_condition_.notify_all();
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();

    if (transport_ && handshake_ &&
        handshake_->state() == protocol::HandshakeState::Established) {
      const auto close = protocol::encodeCloseSession({
          .request_id = next_request_id_++,
          .protocol_version = handshake_->selectedVersion(),
      });
      if (close) {
        const auto sealed = handshake_->seal(close.value);
        if (sealed) writeFrame(*transport_, sealed.bytes, config_.io_timeout);
      }
    }
    if (transport_) transport_->close();
    transport_.reset();
    handshake_.reset();
    std::lock_guard snapshot_lock(snapshot_mutex_);
    snapshot_.state = ClientState::Closed;
    snapshot_.features.clear();
  }

  LicenseSnapshot snapshot() const {
    std::lock_guard lock(snapshot_mutex_);
    return snapshot_;
  }

  bool hasFeature(std::string_view feature) const {
    std::lock_guard lock(snapshot_mutex_);
    return snapshot_.state == ClientState::Valid &&
           std::ranges::find(snapshot_.features, feature) !=
               snapshot_.features.end();
  }

 private:
  Status fail(ClientError error, std::string message) {
    return fail(Status{.error = error, .message = std::move(message)});
  }

  Status fail(Status status) {
    if (transport_) transport_->close();
    transport_.reset();
    handshake_.reset();
    std::lock_guard lock(snapshot_mutex_);
    snapshot_.state = ClientState::Error;
    snapshot_.features.clear();
    return status;
  }

  void heartbeatLoop() {
    while (true) {
      std::unique_lock wait_lock(wait_mutex_);
      if (wait_condition_.wait_for(wait_lock, heartbeat_interval_,
                                   [this] { return stop_requested_; })) {
        return;
      }
      wait_lock.unlock();

      const auto request_id = next_request_id_++;
      const auto heartbeat = protocol::encodeHeartbeat({
          .request_id = request_id,
          .protocol_version = handshake_->selectedVersion(),
          .session_id = session_id_,
      });
      if (!heartbeat) return heartbeatFailed();
      const auto sealed = handshake_->seal(heartbeat.value);
      if (!sealed ||
          !writeFrame(*transport_, sealed.bytes, config_.io_timeout)) {
        return heartbeatFailed();
      }
      const auto frame = readFrame(*transport_, config_.io_timeout);
      if (!frame.status) return heartbeatFailed();
      const auto opened = handshake_->open(frame.frame);
      if (!opened) return heartbeatFailed();
      const auto ack = protocol::decodeHeartbeatAck(opened.bytes);
      if (!ack || ack.value.request_id != request_id ||
          ack.value.protocol_version != handshake_->selectedVersion()) {
        return heartbeatFailed();
      }

      std::lock_guard snapshot_lock(snapshot_mutex_);
      switch (ack.value.state) {
        case protocol::AuthorizationState::Valid:
          snapshot_.state = ClientState::Valid;
          break;
        case protocol::AuthorizationState::Grace:
          snapshot_.state = ClientState::Grace;
          break;
        case protocol::AuthorizationState::Expired:
          snapshot_.state = ClientState::Expired;
          break;
        case protocol::AuthorizationState::Revoked:
          snapshot_.state = ClientState::Revoked;
          break;
        case protocol::AuthorizationState::Unknown:
          snapshot_.state = ClientState::Error;
          break;
      }
      snapshot_.features = snapshot_.state == ClientState::Valid
                               ? std::move(ack.value.features)
                               : std::vector<std::string>{};
      snapshot_.expire_time = ack.value.expire_time;
      if (snapshot_.state != ClientState::Valid) return;
    }
  }

  void heartbeatFailed() {
    std::lock_guard lock(snapshot_mutex_);
    snapshot_.state = ClientState::Error;
    snapshot_.features.clear();
  }

  mutable std::mutex snapshot_mutex_;
  std::mutex operation_mutex_;
  std::mutex wait_mutex_;
  std::condition_variable wait_condition_;
  LicenseSnapshot snapshot_;
  ClientConfig config_;
  protocol::SessionId session_id_{};
  std::chrono::milliseconds heartbeat_interval_{0};
  std::uint64_t next_request_id_ = 3;
  bool stop_requested_ = false;
  std::unique_ptr<transport::UnixSocketTransport> transport_;
  std::unique_ptr<protocol::ClientHandshake> handshake_;
  std::thread heartbeat_thread_;
};

LicenseClient::LicenseClient() : impl_(std::make_unique<Impl>()) {}
LicenseClient::~LicenseClient() { impl_->shutdown(); }

Status LicenseClient::initialize(const ClientConfig& config) {
  return impl_->initialize(config);
}

LicenseSnapshot LicenseClient::snapshot() const { return impl_->snapshot(); }

bool LicenseClient::hasFeature(std::string_view feature) const {
  return impl_->hasFeature(feature);
}

void LicenseClient::shutdown() noexcept { impl_->shutdown(); }

}  // namespace yoauthorize::sdk
