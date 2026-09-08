#include "yoauthorize/service/service.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

#include "yoauthorize/protocol/frame.h"
#include "yoauthorize/service/connection.h"

namespace yoauthorize::service {
namespace {

constexpr std::size_t kMaxLicenseSize = 1024 * 1024;

ServiceResult<std::vector<std::uint8_t>> readLicense(
    const std::filesystem::path& path) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0) return {.error = ServiceError::LicenseFile};
  struct stat info{};
  if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0 ||
      info.st_size > static_cast<off_t>(kMaxLicenseSize)) {
    ::close(fd);
    return {.error = ServiceError::LicenseFile};
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(info.st_size));
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto count = ::read(fd, bytes.data() + offset, bytes.size() - offset);
    if (count <= 0) {
      ::close(fd);
      return {.error = ServiceError::LicenseFile};
    }
    offset += static_cast<std::size_t>(count);
  }
  ::close(fd);
  return {.value = std::move(bytes)};
}

std::uint64_t monotonicMilliseconds() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

transport::Deadline deadlineAfter(std::uint32_t milliseconds) {
  return std::chrono::steady_clock::now() +
         std::chrono::milliseconds(milliseconds);
}

ServiceResult<std::vector<std::uint8_t>> readFrame(
    transport::ITransport& transport, transport::Deadline deadline) {
  std::array<std::byte, protocol::kFrameHeaderSize> header_bytes{};
  if (!transport.readExact(header_bytes, deadline)) {
    return {.error = ServiceError::Transport};
  }
  const auto header = protocol::decodeHeader(header_bytes);
  if (!header) return {.error = ServiceError::Transport};

  std::vector<std::uint8_t> frame(protocol::kFrameHeaderSize +
                                  header.header.payload_size);
  for (std::size_t i = 0; i < header_bytes.size(); ++i) {
    frame[i] = std::to_integer<std::uint8_t>(header_bytes[i]);
  }
  std::span payload(
      reinterpret_cast<std::byte*>(frame.data()) + protocol::kFrameHeaderSize,
      header.header.payload_size);
  if (!transport.readExact(payload, deadline)) {
    return {.error = ServiceError::Transport};
  }
  return {.value = std::move(frame)};
}

bool peerAllowed(const ServiceConfig& config,
                 const transport::PeerCredentials& peer) {
  return std::ranges::find(config.allowed_uids,
                           static_cast<std::uint32_t>(peer.uid)) !=
             config.allowed_uids.end() ||
         std::ranges::find(config.allowed_gids,
                           static_cast<std::uint32_t>(peer.gid)) !=
             config.allowed_gids.end();
}

}  // namespace

AuthorizationService::AuthorizationService(ServiceConfig config,
                                           LoadedKeys keys,
                                           core::LicenseSnapshot license)
    : config_(std::move(config)),
      keys_(std::move(keys)),
      license_(std::move(license)),
      listener_(config_.socket_path.string(),
                static_cast<mode_t>(config_.socket_mode), config_.backlog) {}

ServiceResult<std::unique_ptr<AuthorizationService>>
AuthorizationService::create(const std::filesystem::path& config_path) {
  auto config = loadConfig(config_path);
  if (!config) return {.error = ServiceError::Config};
  auto keys = loadKeys(config.value);
  if (!keys) return {.error = ServiceError::Keys};
  const auto machine =
      core::collectLinuxMachineIdentity(config.value.machine_identity);
  if (!machine) return {.error = ServiceError::MachineIdentity};
  const auto license_bytes = readLicense(config.value.license_path);
  if (!license_bytes) return {.error = license_bytes.error};
  const core::LicenseValidator validator(keys.value.license_public_keys);
  auto license = validator.validate(
      license_bytes.value, config.value.product_id, machine.value.exact_id,
      static_cast<std::uint64_t>(std::time(nullptr)));
  if (!license) return {.error = ServiceError::InvalidLicense};
  return {.value =
              std::unique_ptr<AuthorizationService>(new AuthorizationService(
                  std::move(config.value), std::move(keys.value),
                  std::move(license.value)))};
}

ServiceError AuthorizationService::run(const std::atomic_bool& stop_requested) {
  if (!listener_.listen()) return ServiceError::Listen;
  while (!stop_requested.load(std::memory_order_relaxed)) {
    auto accepted = listener_.accept(deadlineAfter(250));
    if (!accepted) {
      if (accepted.status.code == transport::StatusCode::Timeout) continue;
      if (stop_requested.load(std::memory_order_relaxed)) break;
      return ServiceError::Transport;
    }
    const auto peer = accepted.transport->peerCredentials();
    if (!peer || !peerAllowed(config_, peer.credentials)) {
      accepted.transport->close();
      continue;
    }
    const auto identity = "uid=" + std::to_string(peer.credentials.uid) +
                          ";gid=" + std::to_string(peer.credentials.gid) +
                          ";pid=" + std::to_string(peer.credentials.pid);
    serve(*accepted.transport, identity);
  }
  listener_.close();
  return ServiceError::None;
}

ServiceError AuthorizationService::serve(transport::UnixSocketTransport& client,
                                         std::string peer_identity) {
  Connection connection(keys_.identity_key_id, keys_.identity_private_key,
                        license_, sessions_, std::move(peer_identity),
                        config_.heartbeat_interval_ms,
                        config_.heartbeat_timeout_ms);
  while (!connection.closed()) {
    const auto timeout = connection.established()
                             ? config_.idle_timeout_ms
                             : config_.handshake_timeout_ms;
    auto frame = readFrame(client, deadlineAfter(timeout));
    if (!frame) return frame.error;
    auto result =
        connection.handle(frame.value, monotonicMilliseconds(),
                          static_cast<std::uint64_t>(std::time(nullptr)));
    if (result.response) {
      const std::span bytes(
          reinterpret_cast<const std::byte*>(result.response->data()),
          result.response->size());
      if (!client.writeAll(bytes, deadlineAfter(config_.io_timeout_ms))) {
        return ServiceError::Transport;
      }
    }
    if (result.closed) break;
  }
  client.close();
  return ServiceError::None;
}

void AuthorizationService::stop() noexcept { listener_.close(); }

}  // namespace yoauthorize::service
