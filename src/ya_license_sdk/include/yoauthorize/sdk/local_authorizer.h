#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>

#include "yoauthorize/crypto/crypto.h"
#include "yoauthorize/sdk/client.h"

namespace yoauthorize::sdk {

struct LocalAuthorizationConfig {
  std::string product_id;
  std::unordered_map<std::string, crypto::Key> trusted_license_keys;
  std::string machine_id_path = "/etc/machine-id";
  std::string product_uuid_path = "/sys/class/dmi/id/product_uuid";
  bool require_machine_id = true;
  bool require_product_uuid = false;
};

struct MachineIdentityResult {
  Status status;
  std::string machine_id;

  explicit operator bool() const { return static_cast<bool>(status); }
};

struct LocalAuthorizationResult {
  Status status;
  LicenseSnapshot snapshot;

  explicit operator bool() const { return static_cast<bool>(status); }
};

MachineIdentityResult machineIdentity(
    const LocalAuthorizationConfig& config = {});
LocalAuthorizationResult authorizeLicense(
    std::span<const std::uint8_t> package,
    const LocalAuthorizationConfig& config);
LocalAuthorizationResult authorizeLicenseFile(
    const std::filesystem::path& path, const LocalAuthorizationConfig& config);

}  // namespace yoauthorize::sdk
