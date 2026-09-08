#pragma once

#include <chrono>
#include <cstddef>
#include <span>

namespace yoauthorize::transport {

using Deadline = std::chrono::steady_clock::time_point;

inline constexpr Deadline noDeadline() noexcept { return Deadline::max(); }

enum class StatusCode {
  Ok,
  Timeout,
  EndOfFile,
  Canceled,
  InvalidArgument,
  SystemError,
};

struct Status {
  StatusCode code = StatusCode::Ok;
  int system_error = 0;

  explicit operator bool() const noexcept { return code == StatusCode::Ok; }
  bool operator==(const Status&) const = default;

  static constexpr Status success() noexcept { return {}; }
};

class ITransport {
 public:
  virtual Status connect(Deadline deadline) = 0;
  virtual Status readExact(std::span<std::byte> output, Deadline deadline) = 0;
  virtual Status writeAll(std::span<const std::byte> data,
                          Deadline deadline) = 0;
  virtual void close() noexcept = 0;
  virtual ~ITransport() = default;
};

}  // namespace yoauthorize::transport
