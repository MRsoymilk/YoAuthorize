#pragma once

#ifndef __linux__
#error "YoAuthorize Unix domain socket transport currently requires Linux"
#endif

#include <sys/types.h>

#include <memory>
#include <span>
#include <string>

#include "yoauthorize/transport/transport.h"

namespace yoauthorize::transport {

struct PeerCredentials {
  pid_t pid = 0;
  uid_t uid = 0;
  gid_t gid = 0;

  bool operator==(const PeerCredentials&) const = default;
};

struct PeerCredentialsResult {
  Status status{};
  PeerCredentials credentials{};

  explicit operator bool() const noexcept {
    return status.code == StatusCode::Ok;
  }
};

class UnixSocketTransport final : public ITransport {
 public:
  explicit UnixSocketTransport(std::string path);
  ~UnixSocketTransport() override;

  UnixSocketTransport(const UnixSocketTransport&) = delete;
  UnixSocketTransport& operator=(const UnixSocketTransport&) = delete;
  UnixSocketTransport(UnixSocketTransport&&) noexcept;
  UnixSocketTransport& operator=(UnixSocketTransport&&) noexcept;

  Status connect(Deadline deadline) override;
  Status readExact(std::span<std::byte> output, Deadline deadline) override;
  Status writeAll(std::span<const std::byte> data, Deadline deadline) override;
  void close() noexcept override;

  PeerCredentialsResult peerCredentials() const noexcept;
  bool isOpen() const noexcept;

 private:
  struct State;
  explicit UnixSocketTransport(int socket_fd);

  std::unique_ptr<State> state_;

  friend class UnixSocketListener;
};

struct AcceptResult {
  Status status{};
  std::unique_ptr<UnixSocketTransport> transport;

  explicit operator bool() const noexcept {
    return status.code == StatusCode::Ok;
  }
};

class UnixSocketListener final {
 public:
  explicit UnixSocketListener(std::string path, mode_t mode = 0660,
                              int backlog = 128);
  ~UnixSocketListener();

  UnixSocketListener(const UnixSocketListener&) = delete;
  UnixSocketListener& operator=(const UnixSocketListener&) = delete;
  UnixSocketListener(UnixSocketListener&&) = delete;
  UnixSocketListener& operator=(UnixSocketListener&&) = delete;

  Status listen();
  AcceptResult accept(Deadline deadline);
  void close() noexcept;
  bool isOpen() const noexcept;

 private:
  struct State;
  std::unique_ptr<State> state_;
};

}  // namespace yoauthorize::transport
