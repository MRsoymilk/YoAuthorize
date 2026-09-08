#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "yoauthorize/crypto/crypto.h"

namespace yoauthorize::sdk {

enum class ClientState {
  Uninitialized,
  Connecting,
  Valid,
  Grace,
  Expired,
  Revoked,
  Error,
  Closed,
};

enum class ClientError {
  None,
  InvalidConfig,
  Transport,
  Protocol,
  ServiceIdentityInvalid,
  AuthorizationDenied,
  AlreadyInitialized,
};

struct Status {
  ClientError error = ClientError::None;
  std::string message;

  explicit operator bool() const { return error == ClientError::None; }
};

struct ClientConfig {
  std::string product_id;
  std::string endpoint = "/run/yoauthorize/license-v1.sock";
  std::unordered_map<std::string, crypto::Key> trusted_service_keys;
  std::chrono::milliseconds connect_timeout{5000};
  std::chrono::milliseconds io_timeout{5000};
};

struct LicenseSnapshot {
  ClientState state = ClientState::Uninitialized;
  std::vector<std::string> features;
  std::uint64_t expire_time = 0;
};

class LicenseClient {
 public:
  LicenseClient();
  ~LicenseClient();

  LicenseClient(const LicenseClient&) = delete;
  LicenseClient& operator=(const LicenseClient&) = delete;

  Status initialize(const ClientConfig& config);
  LicenseSnapshot snapshot() const;
  bool hasFeature(std::string_view feature) const;
  void shutdown() noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace yoauthorize::sdk
