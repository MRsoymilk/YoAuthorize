#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "yoauthorize/core/machine_identity.h"

namespace yoauthorize::service {

enum class ConfigError {
  None,
  Io,
  Parse,
  MissingField,
  InvalidValue,
  DuplicateKeyId,
};

template <typename T>
struct ConfigResult {
  ConfigError error = ConfigError::None;
  T value{};

  explicit operator bool() const { return error == ConfigError::None; }
};

struct TrustKeyConfig {
  std::string id;
  std::filesystem::path public_key_path;
};

struct ServiceConfig {
  std::string identity_key_id;
  std::filesystem::path identity_private_key_path;
  std::filesystem::path license_path;
  std::string product_id;
  std::vector<TrustKeyConfig> license_keys;
  std::filesystem::path socket_path = "/run/yoauthorize/license-v1.sock";
  std::uint32_t socket_mode = 0660;
  int backlog = 64;
  std::vector<std::uint32_t> allowed_uids;
  std::vector<std::uint32_t> allowed_gids;
  std::uint32_t max_connections = 64;
  std::uint32_t handshake_timeout_ms = 5000;
  std::uint32_t io_timeout_ms = 5000;
  std::uint32_t idle_timeout_ms = 30000;
  core::LinuxMachineIdentityConfig machine_identity;
};

ConfigResult<ServiceConfig> loadConfig(const std::filesystem::path& path);

}  // namespace yoauthorize::service
