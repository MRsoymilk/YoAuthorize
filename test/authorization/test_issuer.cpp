#include <gtest/gtest.h>

#include <unordered_map>

#include "yoauthorize/core/license.h"
#include "yoauthorize/crypto/crypto.h"
#include "yoauthorize/tools/issuer.h"

namespace core = yoauthorize::core;
namespace crypto = yoauthorize::crypto;
namespace tools = yoauthorize::tools;

TEST(LicenseIssuerTest, ProducesVerifiablePermanentLicense) {
  const auto keys = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(keys);
  const tools::LicenseDefinition definition{
      .key_id = "issuer-1",
      .license_id = "license-1",
      .product_id = "product-1",
      .customer_id = "customer-1",
      .issue_time = 1000,
      .not_before = 1000,
      .features = {"capture", "export"},
      .max_sessions = 2,
  };

  const auto package = tools::issueLicense(definition, keys.value.private_key);

  ASSERT_TRUE(package);
  const core::LicenseValidator validator({{"issuer-1", keys.value.public_key}});
  const auto validated =
      validator.validate(package.package, "product-1", "", 2000);
  ASSERT_TRUE(validated);
  EXPECT_EQ(validated.value.type, core::LicenseType::Permanent);
  EXPECT_EQ(validated.value.features, definition.features);
  EXPECT_EQ(validated.value.max_sessions, 2U);
}

TEST(LicenseIssuerTest, ProducesExactMachineSubscription) {
  const auto keys = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(keys);
  const tools::LicenseDefinition definition{
      .key_id = "issuer-1",
      .license_id = "license-1",
      .product_id = "product-1",
      .customer_id = "customer-1",
      .issue_time = 1000,
      .not_before = 1000,
      .expire_time = 3000,
      .max_sessions = 1,
      .machine_id = "machine-1",
  };
  const auto package = tools::issueLicense(definition, keys.value.private_key);
  ASSERT_TRUE(package);
  const core::LicenseValidator validator({{"issuer-1", keys.value.public_key}});
  EXPECT_TRUE(
      validator.validate(package.package, "product-1", "machine-1", 2000));
  EXPECT_EQ(
      validator.validate(package.package, "product-1", "machine-2", 2000).error,
      core::ErrorCode::MachineMismatch);
}

TEST(LicenseIssuerTest, RejectsInvalidDefinitions) {
  const auto keys = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(keys);
  EXPECT_EQ(tools::issueLicense({}, keys.value.private_key).error,
            tools::IssueError::InvalidInput);
}
