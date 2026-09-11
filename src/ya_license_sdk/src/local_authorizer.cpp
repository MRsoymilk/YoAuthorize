#include "yoauthorize/sdk/local_authorizer.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <ctime>
#include <vector>

#include "yoauthorize/core/license.h"
#include "yoauthorize/core/machine_identity.h"

namespace yoauthorize::sdk {
namespace {

constexpr std::size_t kMaxLicenseSize = 1024 * 1024;

Status mapLicenseError(core::ErrorCode error) {
  switch (error) {
    case core::ErrorCode::None:
      return {};
    case core::ErrorCode::Expired:
      return {.error = ClientError::AuthorizationDenied,
              .message = "license has expired"};
    case core::ErrorCode::NotYetValid:
      return {.error = ClientError::AuthorizationDenied,
              .message = "license is not yet valid"};
    case core::ErrorCode::ProductMismatch:
      return {.error = ClientError::AuthorizationDenied,
              .message = "license product does not match"};
    case core::ErrorCode::MachineMismatch:
      return {.error = ClientError::AuthorizationDenied,
              .message = "license is bound to another machine"};
    case core::ErrorCode::UnknownKey:
      return {.error = ClientError::InvalidLicense,
              .message = "license signing key is not trusted"};
    case core::ErrorCode::InvalidSignature:
      return {.error = ClientError::InvalidLicense,
              .message = "license signature is invalid"};
    default:
      return {.error = ClientError::InvalidLicense,
              .message = "license format or policy is invalid"};
  }
}

struct FileResult {
  Status status;
  std::vector<std::uint8_t> bytes;
};

FileResult readLicense(const std::filesystem::path& path) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0) {
    return {.status = {.error = ClientError::LicenseFile,
                       .message = "license file could not be opened"}};
  }
  struct stat info{};
  if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0 ||
      info.st_size > static_cast<off_t>(kMaxLicenseSize)) {
    ::close(fd);
    return {.status = {.error = ClientError::LicenseFile,
                       .message = "license file is invalid or too large"}};
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(info.st_size));
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto count = ::read(fd, bytes.data() + offset, bytes.size() - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      ::close(fd);
      return {.status = {.error = ClientError::LicenseFile,
                         .message = "license file could not be read"}};
    }
    offset += static_cast<std::size_t>(count);
  }
  ::close(fd);
  return {.bytes = std::move(bytes)};
}

}  // namespace

MachineIdentityResult machineIdentity(const LocalAuthorizationConfig& config) {
  const core::LinuxMachineIdentityConfig machine_config{
      .machine_id_path = config.machine_id_path,
      .product_uuid_path = config.product_uuid_path,
      .require_machine_id = config.require_machine_id,
      .require_product_uuid = config.require_product_uuid,
  };
  const auto identity = core::collectLinuxMachineIdentity(machine_config);
  if (!identity) {
    return {.status = {.error = ClientError::MachineIdentity,
                       .message = "machine identity is unavailable"}};
  }
  return {.machine_id = identity.value.exact_id};
}

LocalAuthorizationResult authorizeLicense(
    std::span<const std::uint8_t> package,
    const LocalAuthorizationConfig& config) {
  if (config.product_id.empty() || config.trusted_license_keys.empty()) {
    return {.status = {.error = ClientError::InvalidConfig,
                       .message = "local authorization is not configured"}};
  }
  const auto identity = machineIdentity(config);
  if (!identity) return {.status = identity.status};
  const core::LicenseValidator validator(config.trusted_license_keys);
  const auto license =
      validator.validate(package, config.product_id, identity.machine_id,
                         static_cast<std::uint64_t>(std::time(nullptr)));
  if (!license) return {.status = mapLicenseError(license.error)};
  return {.snapshot = {.state = ClientState::Valid,
                       .features = license.value.features,
                       .expire_time = license.value.expire_time}};
}

LocalAuthorizationResult authorizeLicenseFile(
    const std::filesystem::path& path, const LocalAuthorizationConfig& config) {
  const auto file = readLicense(path);
  if (!file.status) return {.status = file.status};
  return authorizeLicense(file.bytes, config);
}

}  // namespace yoauthorize::sdk
