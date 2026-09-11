#include "yoauthorize/tools/issuer.h"

#include <flatbuffers/flatbuffer_builder.h>

#include <limits>
#include <span>
#include <string_view>

#include "license_generated.h"

namespace yoauthorize::tools {
namespace {

constexpr std::string_view kSignatureDomain = "YOAUTHORIZE-LICENSE-V1";
constexpr std::size_t kMaxIdentifierSize = 255;
constexpr std::size_t kMaxFeatures = 256;
constexpr std::size_t kMaxFeatureSize = 128;

void appendU16(std::vector<std::uint8_t>& output, std::uint16_t value) {
  output.push_back(static_cast<std::uint8_t>(value >> 8U));
  output.push_back(static_cast<std::uint8_t>(value));
}

void appendU32(std::vector<std::uint8_t>& output, std::uint32_t value) {
  for (std::size_t i = 0; i < 4; ++i) {
    output.push_back(static_cast<std::uint8_t>(value >> (24U - 8U * i)));
  }
}

bool valid(const LicenseDefinition& definition) {
  if (definition.key_id.empty() ||
      definition.key_id.size() > std::numeric_limits<std::uint16_t>::max() ||
      definition.license_id.empty() ||
      definition.license_id.size() > kMaxIdentifierSize ||
      definition.product_id.empty() ||
      definition.product_id.size() > kMaxIdentifierSize ||
      definition.customer_id.empty() ||
      definition.customer_id.size() > kMaxIdentifierSize ||
      definition.issue_time == 0 || definition.not_before == 0 ||
      definition.not_before < definition.issue_time ||
      definition.max_sessions == 0 ||
      definition.features.size() > kMaxFeatures) {
    return false;
  }
  if (definition.expire_time != 0 &&
      definition.expire_time <= definition.not_before) {
    return false;
  }
  for (const auto& feature : definition.features) {
    if (feature.empty() || feature.size() > kMaxFeatureSize) return false;
  }
  return true;
}

}  // namespace

IssueResult issueLicense(const LicenseDefinition& definition,
                         const crypto::Key& signing_private_key) {
  if (!valid(definition)) return {.error = IssueError::InvalidInput};

  flatbuffers::FlatBufferBuilder payload_builder;
  const auto policy = license::CreateMachinePolicy(
      payload_builder,
      definition.machine_id.empty() ? license::MachineMode::Unbound
                                    : license::MachineMode::Exact,
      payload_builder.CreateString(definition.machine_id));
  std::vector<flatbuffers::Offset<flatbuffers::String>> features;
  features.reserve(definition.features.size());
  for (const auto& feature : definition.features) {
    features.push_back(payload_builder.CreateString(feature));
  }
  const auto payload = license::CreateLicensePayload(
      payload_builder, payload_builder.CreateString(definition.license_id),
      payload_builder.CreateString(definition.product_id),
      payload_builder.CreateString(definition.customer_id),
      definition.issue_time, definition.not_before, definition.expire_time,
      definition.expire_time == 0 ? license::LicenseType::Permanent
                                  : license::LicenseType::Subscription,
      payload_builder.CreateVector(features), definition.max_sessions, policy);
  payload_builder.Finish(payload);
  const std::span payload_bytes(payload_builder.GetBufferPointer(),
                                payload_builder.GetSize());

  std::vector<std::uint8_t> signed_bytes;
  signed_bytes.reserve(kSignatureDomain.size() + 2 + 2 +
                       definition.key_id.size() + 4 + payload_bytes.size());
  signed_bytes.insert(signed_bytes.end(), kSignatureDomain.begin(),
                      kSignatureDomain.end());
  appendU16(signed_bytes, 1);
  appendU16(signed_bytes, static_cast<std::uint16_t>(definition.key_id.size()));
  signed_bytes.insert(signed_bytes.end(), definition.key_id.begin(),
                      definition.key_id.end());
  appendU32(signed_bytes, static_cast<std::uint32_t>(payload_bytes.size()));
  signed_bytes.insert(signed_bytes.end(), payload_bytes.begin(),
                      payload_bytes.end());
  const auto signature = crypto::signEd25519(signing_private_key, signed_bytes);
  if (!signature) return {.error = IssueError::SigningFailed};

  flatbuffers::FlatBufferBuilder package_builder;
  const auto package = license::CreateLicensePackage(
      package_builder, 1, package_builder.CreateString(definition.key_id),
      package_builder.CreateVector(payload_bytes.data(), payload_bytes.size()),
      package_builder.CreateVector(signature.value.data(),
                                   signature.value.size()));
  license::FinishLicensePackageBuffer(package_builder, package);
  return {.package = {
              package_builder.GetBufferPointer(),
              package_builder.GetBufferPointer() + package_builder.GetSize()}};
}

}  // namespace yoauthorize::tools
