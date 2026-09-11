#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "yoauthorize/service/key_store.h"

namespace crypto = yoauthorize::crypto;
namespace service = yoauthorize::service;

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    auto pattern =
        (std::filesystem::temp_directory_path() / "yoauthorize-keys-XXXXXX")
            .string();
    path_ = ::mkdtemp(pattern.data());
  }
  ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

void writeKey(const std::filesystem::path& path, const crypto::Key& key,
              mode_t mode) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char*>(key.data()), key.size());
  output.close();
  ASSERT_EQ(::chmod(path.c_str(), mode), 0);
}

void writeText(const std::filesystem::path& path, std::string_view contents) {
  std::ofstream output(path);
  output << contents;
}

}  // namespace

TEST(KeyStoreTest, LoadsPrivateAndTrustKeys) {
  TemporaryDirectory directory;
  const auto identity = crypto::generateEd25519KeyPair();
  const auto issuer = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(identity);
  ASSERT_TRUE(issuer);
  const auto identity_path = directory.path() / "identity.key";
  const auto issuer_path = directory.path() / "issuer.pub";
  writeKey(identity_path, identity.value.private_key, 0600);
  writeKey(issuer_path, issuer.value.public_key, 0644);
  service::ServiceConfig config{
      .identity_key_id = "service-1",
      .identity_private_key_path = identity_path,
      .license_keys = {{.id = "issuer-1", .public_key_path = issuer_path}},
  };

  const auto result = service::loadKeys(config);

  ASSERT_TRUE(result);
  EXPECT_EQ(result.value.identity_private_key, identity.value.private_key);
  EXPECT_EQ(result.value.license_public_keys.at("issuer-1"),
            issuer.value.public_key);
}

TEST(KeyStoreTest, RejectsSymlinksWrongSizesAndLoosePrivatePermissions) {
  TemporaryDirectory directory;
  crypto::Key key{};
  const auto target = directory.path() / "target.key";
  writeKey(target, key, 0600);
  const auto link = directory.path() / "link.key";
  std::filesystem::create_symlink(target, link);
  EXPECT_EQ(service::loadRawKey(link, true).error,
            service::KeyStoreError::InvalidFile);

  const auto short_key = directory.path() / "short.key";
  writeText(short_key, "short");
  EXPECT_EQ(service::loadRawKey(short_key, false).error,
            service::KeyStoreError::InvalidSize);

  ASSERT_EQ(::chmod(target.c_str(), 0644), 0);
  EXPECT_EQ(service::loadRawKey(target, true).error,
            service::KeyStoreError::InvalidPermissions);
}
