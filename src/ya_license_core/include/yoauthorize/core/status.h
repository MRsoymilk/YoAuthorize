#pragma once

namespace yoauthorize::core {

enum class ErrorCode {
  None,
  InvalidFormat,
  UnsupportedVersion,
  UnknownKey,
  InvalidSignature,
  InvalidLicense,
  NotYetValid,
  Expired,
  ProductMismatch,
  MachineMismatch,
  SessionLimitExceeded,
  InvalidSession,
  InternalError,
};

template <typename T>
struct Result {
  ErrorCode error = ErrorCode::None;
  T value{};

  explicit operator bool() const { return error == ErrorCode::None; }
};

}  // namespace yoauthorize::core
