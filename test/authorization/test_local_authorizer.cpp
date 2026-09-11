#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "yoauthorize/crypto/crypto.h"
#include "yoauthorize/sdk/local_authorizer.h"
#include "yoauthorize/tools/issuer.h"

namespace crypto = yoauthorize::crypto;
namespace sdk = yoauthorize::sdk;
namespace tools = yoauthorize::tools;

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    auto pattern = (std::filesystem::temp_directory_path() /
                    "yoauthorize-local-sdk-XXXXXX")
                       .string();
    path_ = ::mkdtemp(pattern.data());
  }
  ~TemporaryDirectory() { std::filesystem::remove_all(path_); }
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

void writeFile(const std::filesystem::path& path,
               std::span<const std::uint8_t> bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

}  // namespace

TEST(LocalAuthorizerTest, ValidatesSignedLicenseFile) {
  TemporaryDirectory directory;
  const auto keys = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(keys);
  const auto now = static_cast<std::uint64_t>(std::time(nullptr));
  const auto package = tools::issueLicense({.key_id = "issuer-1",
                                            .license_id = "license-1",
                                            .product_id = "product-1",
                                            .customer_id = "customer-1",
                                            .issue_time = now,
                                            .not_before = now,
                                            .features = {"capture", "export"},
                                            .max_sessions = 1},
                                           keys.value.private_key);
  ASSERT_TRUE(package);
  const auto license_path = directory.path() / "license.yalc";
  const auto machine_path = directory.path() / "machine-id";
  writeFile(license_path, package.package);
  std::ofstream(machine_path) << "0123456789abcdef0123456789abcdef\n";
  const sdk::LocalAuthorizationConfig config{
      .product_id = "product-1",
      .trusted_license_keys = {{"issuer-1", keys.value.public_key}},
      .machine_id_path = machine_path.string(),
      .product_uuid_path = (directory.path() / "missing-uuid").string(),
  };

  const auto result = sdk::authorizeLicenseFile(license_path, config);

  ASSERT_TRUE(result);
  EXPECT_EQ(result.snapshot.state, sdk::ClientState::Valid);
  EXPECT_EQ(result.snapshot.features,
            (std::vector<std::string>{"capture", "export"}));
}

TEST(LocalAuthorizerTest, RejectsTamperingAndSymlinks) {
  TemporaryDirectory directory;
  const auto keys = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(keys);
  const auto now = static_cast<std::uint64_t>(std::time(nullptr));
  auto package = tools::issueLicense({.key_id = "issuer-1",
                                      .license_id = "license-1",
                                      .product_id = "product-1",
                                      .customer_id = "customer-1",
                                      .issue_time = now,
                                      .not_before = now,
                                      .max_sessions = 1},
                                     keys.value.private_key);
  ASSERT_TRUE(package);
  package.package.back() ^= 1;
  const auto machine_path = directory.path() / "machine-id";
  std::ofstream(machine_path) << "0123456789abcdef0123456789abcdef\n";
  const sdk::LocalAuthorizationConfig config{
      .product_id = "product-1",
      .trusted_license_keys = {{"issuer-1", keys.value.public_key}},
      .machine_id_path = machine_path.string(),
      .product_uuid_path = (directory.path() / "missing-uuid").string(),
  };
  EXPECT_EQ(sdk::authorizeLicense(package.package, config).status.error,
            sdk::ClientError::InvalidLicense);

  const auto target = directory.path() / "license.yalc";
  writeFile(target, package.package);
  const auto link = directory.path() / "license-link.yalc";
  std::filesystem::create_symlink(target, link);
  EXPECT_EQ(sdk::authorizeLicenseFile(link, config).status.error,
            sdk::ClientError::LicenseFile);
}
