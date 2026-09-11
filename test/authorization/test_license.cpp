#include <flatbuffers/flatbuffer_builder.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "license_generated.h"
#include "yoauthorize/core/license.h"
#include "yoauthorize/crypto/crypto.h"

namespace core = yoauthorize::core;
namespace crypto = yoauthorize::crypto;
namespace license = yoauthorize::license;

namespace {

void appendU16(std::vector<std::uint8_t>& output, std::uint16_t value) {
  output.push_back(static_cast<std::uint8_t>(value >> 8U));
  output.push_back(static_cast<std::uint8_t>(value));
}

void appendU32(std::vector<std::uint8_t>& output, std::uint32_t value) {
  for (std::size_t i = 0; i < 4; ++i) {
    output.push_back(static_cast<std::uint8_t>(value >> (24U - i * 8U)));
  }
}

std::vector<std::uint8_t> makeLicense(const crypto::Key& private_key,
                                      std::string_view product,
                                      std::string_view machine) {
  flatbuffers::FlatBufferBuilder payload_builder;
  const auto policy =
      license::CreateMachinePolicy(payload_builder, license::MachineMode::Exact,
                                   payload_builder.CreateString(machine));
  std::vector<flatbuffers::Offset<flatbuffers::String>> features{
      payload_builder.CreateString("basic"),
      payload_builder.CreateString("export")};
  const auto payload = license::CreateLicensePayload(
      payload_builder, payload_builder.CreateString("license-1"),
      payload_builder.CreateString(product),
      payload_builder.CreateString("customer-1"), 900, 1000, 2000,
      license::LicenseType::Subscription,
      payload_builder.CreateVector(features), 1, policy);
  payload_builder.Finish(payload);

  const std::span payload_bytes(payload_builder.GetBufferPointer(),
                                payload_builder.GetSize());
  const std::string key_id = "issuer-1";
  std::vector<std::uint8_t> signed_bytes;
  constexpr std::string_view domain = "YOAUTHORIZE-LICENSE-V1";
  signed_bytes.insert(signed_bytes.end(), domain.begin(), domain.end());
  appendU16(signed_bytes, 1);
  appendU16(signed_bytes, static_cast<std::uint16_t>(key_id.size()));
  signed_bytes.insert(signed_bytes.end(), key_id.begin(), key_id.end());
  appendU32(signed_bytes, static_cast<std::uint32_t>(payload_bytes.size()));
  signed_bytes.insert(signed_bytes.end(), payload_bytes.begin(),
                      payload_bytes.end());
  const auto signature = crypto::signEd25519(private_key, signed_bytes);
  EXPECT_TRUE(signature);

  flatbuffers::FlatBufferBuilder package_builder;
  const auto package = license::CreateLicensePackage(
      package_builder, 1, package_builder.CreateString(key_id),
      package_builder.CreateVector(payload_bytes.data(), payload_bytes.size()),
      package_builder.CreateVector(signature.value.data(),
                                   signature.value.size()));
  license::FinishLicensePackageBuffer(package_builder, package);
  return {package_builder.GetBufferPointer(),
          package_builder.GetBufferPointer() + package_builder.GetSize()};
}

}  // namespace

TEST(LicenseValidatorTest, ValidatesSignedLicense) {
  const auto keys = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(keys);
  const auto package =
      makeLicense(keys.value.private_key, "product-a", "host-a");
  core::LicenseValidator validator({{"issuer-1", keys.value.public_key}});

  const auto result = validator.validate(package, "product-a", "host-a", 1500);

  ASSERT_TRUE(result);
  EXPECT_EQ(result.value.features,
            (std::vector<std::string>{"basic", "export"}));
  EXPECT_EQ(result.value.max_sessions, 1);
}

TEST(LicenseValidatorTest, RejectsPolicyMismatchAndTampering) {
  const auto keys = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(keys);
  auto package = makeLicense(keys.value.private_key, "product-a", "host-a");
  core::LicenseValidator validator({{"issuer-1", keys.value.public_key}});

  EXPECT_EQ(validator.validate(package, "product-b", "host-a", 1500).error,
            core::ErrorCode::ProductMismatch);
  EXPECT_EQ(validator.validate(package, "product-a", "host-b", 1500).error,
            core::ErrorCode::MachineMismatch);

  const auto* outer = license::GetLicensePackage(package.data());
  const auto payload_offset = outer->payload()->data() - package.data();
  package[static_cast<std::size_t>(payload_offset)] ^= 1;
  EXPECT_EQ(validator.validate(package, "product-a", "host-a", 1500).error,
            core::ErrorCode::InvalidSignature);
}
