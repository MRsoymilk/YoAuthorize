#include "yoauthorize/transport/unix_socket.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <utility>

namespace yoauthorize::transport {
namespace {

Status systemError(int error = errno) {
  return {.code = StatusCode::SystemError, .system_error = error};
}

Status invalidArgument() { return {.code = StatusCode::InvalidArgument}; }

int createCancelFd() { return ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK); }

void signalCancel(int fd) noexcept {
  if (fd < 0) {
    return;
  }
  const std::uint64_t value = 1;
  while (::write(fd, &value, sizeof(value)) < 0 && errno == EINTR) {
  }
}

Status makeAddress(const std::string& path, sockaddr_un& address,
                   socklen_t& length) {
  if (path.empty() || path.size() >= sizeof(address.sun_path)) {
    return invalidArgument();
  }
  address = {};
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
  length =
      static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size() + 1);
  return Status::success();
}

int timeoutMilliseconds(Deadline deadline) {
  if (deadline == noDeadline()) {
    return -1;
  }
  const auto remaining = deadline - std::chrono::steady_clock::now();
  if (remaining <= Deadline::duration::zero()) {
    return 0;
  }
  const auto milliseconds =
      std::chrono::ceil<std::chrono::milliseconds>(remaining).count();
  return static_cast<int>(milliseconds > INT_MAX ? INT_MAX : milliseconds);
}

Status waitFor(int fd, int cancel_fd, short events, Deadline deadline) {
  pollfd descriptors[2]{{.fd = fd, .events = events, .revents = 0},
                        {.fd = cancel_fd, .events = POLLIN, .revents = 0}};
  for (;;) {
    const int result = ::poll(descriptors, 2, timeoutMilliseconds(deadline));
    if (result < 0) {
      if (errno == EINTR) {
        continue;
      }
      return systemError();
    }
    if (result == 0) {
      return {.code = StatusCode::Timeout};
    }
    if ((descriptors[1].revents & POLLIN) != 0) {
      return {.code = StatusCode::Canceled};
    }
    if ((descriptors[0].revents & (events | POLLERR | POLLHUP | POLLNVAL)) !=
        0) {
      return Status::success();
    }
  }
}

int duplicateFd(int fd) {
  int duplicate;
  do {
    duplicate = ::fcntl(fd, F_DUPFD_CLOEXEC, 0);
  } while (duplicate < 0 && errno == EINTR);
  return duplicate;
}

void closeFd(int fd) noexcept {
  if (fd >= 0) {
    ::close(fd);
  }
}

struct OperationFd {
  int value = -1;
  ~OperationFd() { closeFd(value); }
};

Status removeStaleSocket(const std::string& path) {
  struct stat metadata{};
  if (::lstat(path.c_str(), &metadata) < 0) {
    return errno == ENOENT ? Status::success() : systemError();
  }
  if (!S_ISSOCK(metadata.st_mode)) {
    return {.code = StatusCode::SystemError, .system_error = EEXIST};
  }

  sockaddr_un address{};
  socklen_t address_length = 0;
  if (const Status status = makeAddress(path, address, address_length);
      !status) {
    return status;
  }
  const int probe =
      ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (probe < 0) {
    return systemError();
  }
  int result;
  do {
    result = ::connect(probe, reinterpret_cast<const sockaddr*>(&address),
                       address_length);
  } while (result < 0 && errno == EINTR);
  const int connect_error = result == 0 ? 0 : errno;
  closeFd(probe);

  if (result == 0 || connect_error == EINPROGRESS || connect_error == EAGAIN ||
      connect_error == EALREADY) {
    return systemError(EADDRINUSE);
  }
  if (connect_error == ENOENT) {
    return Status::success();
  }
  if (connect_error != ECONNREFUSED) {
    return systemError(connect_error);
  }
  struct stat current{};
  if (::lstat(path.c_str(), &current) < 0) {
    return errno == ENOENT ? Status::success() : systemError();
  }
  if (!S_ISSOCK(current.st_mode) || current.st_dev != metadata.st_dev ||
      current.st_ino != metadata.st_ino) {
    return systemError(EAGAIN);
  }
  return ::unlink(path.c_str()) == 0 ? Status::success() : systemError();
}

}  // namespace

struct UnixSocketTransport::State {
  explicit State(std::string socket_path)
      : path(std::move(socket_path)), cancel_fd(createCancelFd()) {}
  explicit State(int accepted_fd)
      : fd(accepted_fd), cancel_fd(createCancelFd()) {}

  ~State() {
    closeFd(fd);
    closeFd(cancel_fd);
  }

  OperationFd operationFd() const {
    std::lock_guard lock(mutex);
    return {.value = fd < 0 ? -1 : duplicateFd(fd)};
  }

  void resetSocket() noexcept {
    int socket_fd = -1;
    {
      std::lock_guard lock(mutex);
      std::swap(socket_fd, fd);
    }
    if (socket_fd >= 0) {
      ::shutdown(socket_fd, SHUT_RDWR);
      closeFd(socket_fd);
    }
  }

  bool isCanceled() const noexcept {
    std::lock_guard lock(mutex);
    return canceled;
  }

  mutable std::mutex mutex;
  std::string path;
  int fd = -1;
  int cancel_fd = -1;
  bool canceled = false;
};

UnixSocketTransport::UnixSocketTransport(std::string path)
    : state_(std::make_unique<State>(std::move(path))) {}

UnixSocketTransport::UnixSocketTransport(int socket_fd)
    : state_(std::make_unique<State>(socket_fd)) {}

UnixSocketTransport::~UnixSocketTransport() { close(); }

UnixSocketTransport::UnixSocketTransport(UnixSocketTransport&&) noexcept =
    default;
UnixSocketTransport& UnixSocketTransport::operator=(
    UnixSocketTransport&&) noexcept = default;

Status UnixSocketTransport::connect(Deadline deadline) {
  if (!state_ || state_->cancel_fd < 0) {
    return systemError(EMFILE);
  }
  sockaddr_un address{};
  socklen_t address_length = 0;
  if (const Status status = makeAddress(state_->path, address, address_length);
      !status) {
    return status;
  }

  const int fd =
      ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return systemError();
  }
  {
    std::lock_guard lock(state_->mutex);
    if (state_->canceled) {
      closeFd(fd);
      return {.code = StatusCode::Canceled};
    }
    if (state_->fd >= 0) {
      closeFd(fd);
      return systemError(EISCONN);
    }
    state_->fd = fd;
  }
  OperationFd operation = state_->operationFd();
  if (operation.value < 0) {
    return systemError();
  }

  int result;
  do {
    result =
        ::connect(operation.value, reinterpret_cast<const sockaddr*>(&address),
                  address_length);
  } while (result < 0 && errno == EINTR);
  if (result == 0) {
    return state_->isCanceled() ? Status{.code = StatusCode::Canceled}
                                : Status::success();
  }
  if (errno != EINPROGRESS) {
    const Status status = systemError();
    state_->resetSocket();
    return status;
  }
  if (Status status =
          waitFor(operation.value, state_->cancel_fd, POLLOUT, deadline);
      !status) {
    if (status.code != StatusCode::Canceled) {
      state_->resetSocket();
    }
    return status;
  }
  int socket_error = 0;
  socklen_t error_size = sizeof(socket_error);
  if (::getsockopt(operation.value, SOL_SOCKET, SO_ERROR, &socket_error,
                   &error_size) < 0) {
    const Status status = systemError();
    state_->resetSocket();
    return status;
  }
  if (socket_error != 0) {
    state_->resetSocket();
    return systemError(socket_error);
  }
  return state_->isCanceled() ? Status{.code = StatusCode::Canceled}
                              : Status::success();
}

Status UnixSocketTransport::readExact(std::span<std::byte> output,
                                      Deadline deadline) {
  if (!state_) {
    return {.code = StatusCode::Canceled};
  }
  OperationFd operation = state_->operationFd();
  if (operation.value < 0) {
    return {.code = StatusCode::Canceled};
  }
  std::size_t offset = 0;
  while (offset < output.size()) {
    if (Status status =
            waitFor(operation.value, state_->cancel_fd, POLLIN, deadline);
        !status) {
      return status;
    }
    const ssize_t count = ::recv(operation.value, output.data() + offset,
                                 output.size() - offset, 0);
    if (count > 0) {
      offset += static_cast<std::size_t>(count);
    } else if (count == 0) {
      return {.code = StatusCode::EndOfFile};
    } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
      return systemError();
    }
  }
  return Status::success();
}

Status UnixSocketTransport::writeAll(std::span<const std::byte> data,
                                     Deadline deadline) {
  if (!state_) {
    return {.code = StatusCode::Canceled};
  }
  OperationFd operation = state_->operationFd();
  if (operation.value < 0) {
    return {.code = StatusCode::Canceled};
  }
  std::size_t offset = 0;
  while (offset < data.size()) {
    if (Status status =
            waitFor(operation.value, state_->cancel_fd, POLLOUT, deadline);
        !status) {
      return status;
    }
    const ssize_t count = ::send(operation.value, data.data() + offset,
                                 data.size() - offset, MSG_NOSIGNAL);
    if (count > 0) {
      offset += static_cast<std::size_t>(count);
    } else if (count == 0) {
      return {.code = StatusCode::EndOfFile};
    } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
      return systemError();
    }
  }
  return Status::success();
}

void UnixSocketTransport::close() noexcept {
  if (!state_) {
    return;
  }
  int fd = -1;
  {
    std::lock_guard lock(state_->mutex);
    state_->canceled = true;
    std::swap(fd, state_->fd);
  }
  signalCancel(state_->cancel_fd);
  if (fd >= 0) {
    ::shutdown(fd, SHUT_RDWR);
    closeFd(fd);
  }
}

PeerCredentialsResult UnixSocketTransport::peerCredentials() const noexcept {
  if (!state_) {
    return {{.code = StatusCode::Canceled}, {}};
  }
  OperationFd operation = state_->operationFd();
  if (operation.value < 0) {
    return {{.code = StatusCode::Canceled}, {}};
  }
  struct ucred credentials{};
  socklen_t size = sizeof(credentials);
  if (::getsockopt(operation.value, SOL_SOCKET, SO_PEERCRED, &credentials,
                   &size) < 0) {
    return {systemError(), {}};
  }
  return {
      Status::success(),
      {.pid = credentials.pid, .uid = credentials.uid, .gid = credentials.gid}};
}

bool UnixSocketTransport::isOpen() const noexcept {
  if (!state_) {
    return false;
  }
  std::lock_guard lock(state_->mutex);
  return state_->fd >= 0;
}

struct UnixSocketListener::State {
  State(std::string socket_path, mode_t socket_mode, int socket_backlog)
      : path(std::move(socket_path)),
        mode(socket_mode),
        backlog(socket_backlog),
        cancel_fd(createCancelFd()) {}

  ~State() {
    closeFd(fd);
    closeFd(cancel_fd);
  }

  OperationFd operationFd() const {
    std::lock_guard lock(mutex);
    return {.value = fd < 0 ? -1 : duplicateFd(fd)};
  }

  mutable std::mutex mutex;
  std::string path;
  mode_t mode;
  int backlog;
  int fd = -1;
  int cancel_fd = -1;
  dev_t device = 0;
  ino_t inode = 0;
  bool owns_path = false;
  bool canceled = false;
};

UnixSocketListener::UnixSocketListener(std::string path, mode_t mode,
                                       int backlog)
    : state_(std::make_unique<State>(std::move(path), mode, backlog)) {}

UnixSocketListener::~UnixSocketListener() { close(); }

Status UnixSocketListener::listen() {
  if (!state_ || state_->backlog <= 0 || (state_->mode & ~0777) != 0) {
    return invalidArgument();
  }
  if (state_->cancel_fd < 0) {
    return systemError(EMFILE);
  }
  {
    std::lock_guard lock(state_->mutex);
    if (state_->canceled) {
      return {.code = StatusCode::Canceled};
    }
    if (state_->fd >= 0) {
      return systemError(EADDRINUSE);
    }
  }
  sockaddr_un address{};
  socklen_t address_length = 0;
  if (const Status status = makeAddress(state_->path, address, address_length);
      !status) {
    return status;
  }
  if (const Status status = removeStaleSocket(state_->path); !status) {
    return status;
  }
  const int fd =
      ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    return systemError();
  }

  static std::mutex umask_mutex;
  int bind_result;
  {
    std::lock_guard lock(umask_mutex);
    const mode_t previous_umask = ::umask(0077);
    bind_result =
        ::bind(fd, reinterpret_cast<const sockaddr*>(&address), address_length);
    ::umask(previous_umask);
  }
  if (bind_result < 0) {
    const Status status = systemError();
    closeFd(fd);
    return status;
  }
  if (::chmod(state_->path.c_str(), state_->mode) < 0 ||
      ::listen(fd, state_->backlog) < 0) {
    const Status status = systemError();
    closeFd(fd);
    ::unlink(state_->path.c_str());
    return status;
  }
  struct stat metadata{};
  if (::lstat(state_->path.c_str(), &metadata) < 0) {
    const Status status = systemError();
    closeFd(fd);
    ::unlink(state_->path.c_str());
    return status;
  }
  {
    std::lock_guard lock(state_->mutex);
    if (state_->canceled) {
      closeFd(fd);
      ::unlink(state_->path.c_str());
      return {.code = StatusCode::Canceled};
    }
    if (state_->fd >= 0) {
      closeFd(fd);
      ::unlink(state_->path.c_str());
      return systemError(EADDRINUSE);
    }
    state_->fd = fd;
    state_->device = metadata.st_dev;
    state_->inode = metadata.st_ino;
    state_->owns_path = true;
  }
  return Status::success();
}

AcceptResult UnixSocketListener::accept(Deadline deadline) {
  if (!state_) {
    return {{.code = StatusCode::Canceled}, nullptr};
  }
  OperationFd operation = state_->operationFd();
  if (operation.value < 0) {
    return {{.code = StatusCode::Canceled}, nullptr};
  }
  for (;;) {
    if (Status status =
            waitFor(operation.value, state_->cancel_fd, POLLIN, deadline);
        !status) {
      return {status, nullptr};
    }
    const int fd = ::accept4(operation.value, nullptr, nullptr,
                             SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd >= 0) {
      auto transport =
          std::unique_ptr<UnixSocketTransport>(new UnixSocketTransport(fd));
      if (transport->state_->cancel_fd < 0) {
        return {systemError(EMFILE), nullptr};
      }
      return {Status::success(), std::move(transport)};
    }
    if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
      return {systemError(), nullptr};
    }
  }
}

void UnixSocketListener::close() noexcept {
  if (!state_) {
    return;
  }
  int fd = -1;
  dev_t device = 0;
  ino_t inode = 0;
  bool owns_path = false;
  {
    std::lock_guard lock(state_->mutex);
    state_->canceled = true;
    std::swap(fd, state_->fd);
    device = state_->device;
    inode = state_->inode;
    owns_path = state_->owns_path;
    state_->owns_path = false;
  }
  signalCancel(state_->cancel_fd);
  closeFd(fd);

  struct stat metadata{};
  if (owns_path && ::lstat(state_->path.c_str(), &metadata) == 0 &&
      S_ISSOCK(metadata.st_mode) && metadata.st_dev == device &&
      metadata.st_ino == inode) {
    ::unlink(state_->path.c_str());
  }
}

bool UnixSocketListener::isOpen() const noexcept {
  if (!state_) {
    return false;
  }
  std::lock_guard lock(state_->mutex);
  return state_->fd >= 0;
}

}  // namespace yoauthorize::transport
