#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "yoauthorize/service/config.h"

namespace service = yoauthorize::service;

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    auto pattern =
        (std::filesystem::temp_directory_path() / "yoauthorize-config-XXXXXX")
            .string();
    path_ = ::mkdtemp(pattern.data());
  }
  ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

void write(const std::filesystem::path& path, std::string_view contents) {
  std::ofstream output(path);
  output << contents;
}

constexpr std::string_view kValidConfig = R"(
[service]
identity_key_id = "service-1"
identity_private_key = "/etc/yoauthorize/service.key"
license = "/var/lib/yoauthorize/license.bin"
product_id = "product-1"
socket = "/tmp/yoauthorize.sock"
socket_mode = 432
backlog = 32
allowed_uids = [1000]
allowed_gids = [100]
max_connections = 20
handshake_timeout_ms = 1000
io_timeout_ms = 2000
idle_timeout_ms = 10000

[machine_identity]
require_machine_id = true
require_product_uuid = true

[[license_keys]]
id = "issuer-1"
public_key = "/etc/yoauthorize/issuer-1.pub"
)";

}  // namespace

TEST(ServiceConfigTest, LoadsTypedConfiguration) {
  TemporaryDirectory directory;
  const auto path = directory.path() / "service.toml";
  write(path, kValidConfig);

  const auto result = service::loadConfig(path);

  ASSERT_TRUE(result);
  EXPECT_EQ(result.value.identity_key_id, "service-1");
  EXPECT_EQ(result.value.socket_mode, 0660U);
  EXPECT_EQ(result.value.backlog, 32);
  ASSERT_EQ(result.value.license_keys.size(), 1U);
  EXPECT_EQ(result.value.license_keys.front().id, "issuer-1");
  EXPECT_TRUE(result.value.machine_identity.require_product_uuid);
}

TEST(ServiceConfigTest, RejectsMissingAndDuplicateSecuritySettings) {
  TemporaryDirectory directory;
  const auto missing = directory.path() / "missing.toml";
  write(missing, "[service]\nproduct_id = \"product-1\"\n");
  EXPECT_EQ(service::loadConfig(missing).error,
            service::ConfigError::MissingField);

  const auto duplicate = directory.path() / "duplicate.toml";
  write(duplicate, std::string(kValidConfig) + R"(
[[license_keys]]
id = "issuer-1"
public_key = "/etc/yoauthorize/issuer-2.pub"
)");
  EXPECT_EQ(service::loadConfig(duplicate).error,
            service::ConfigError::DuplicateKeyId);
}

TEST(ServiceConfigTest, RejectsUnsafeRangesAndPeerPolicy) {
  TemporaryDirectory directory;
  const auto path = directory.path() / "invalid.toml";
  auto config = std::string(kValidConfig);
  config.replace(config.find("socket_mode = 432"), 17, "socket_mode = 1024");
  write(path, config);
  EXPECT_EQ(service::loadConfig(path).error,
            service::ConfigError::InvalidValue);

  config = std::string(kValidConfig);
  config.replace(config.find("allowed_uids = [1000]"), 21, "allowed_uids = []");
  write(path, config);
  EXPECT_EQ(service::loadConfig(path).error,
            service::ConfigError::InvalidValue);
}
