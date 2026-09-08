#include "yoauthorize/core/license.h"

#include <flatbuffers/verifier.h>

#include <algorithm>
#include <array>
#include <limits>

#include "license_generated.h"

namespace yoauthorize::core {
namespace {

constexpr std::size_t kMaxLicenseSize = 1024 * 1024;
constexpr std::string_view kSignatureDomain = "YOAUTHORIZE-LICENSE-V1";

void appendU16(std::vector<std::uint8_t>& output, std::uint16_t value) {
  output.push_back(static_cast<std::uint8_t>(value >> 8U));
  output.push_back(static_cast<std::uint8_t>(value));
}

void appendU32(std::vector<std::uint8_t>& output, std::uint32_t value) {
  for (std::size_t i = 0; i < 4; ++i) {
    output.push_back(static_cast<std::uint8_t>(value >> (24U - 8U * i)));
  }
}

std::vector<std::uint8_t> signatureInput(
    std::uint16_t version, std::string_view key_id,
    std::span<const std::uint8_t> payload) {
  std::vector<std::uint8_t> input;
  input.reserve(kSignatureDomain.size() + 2 + 2 + key_id.size() + 4 +
                payload.size());
  input.insert(input.end(), kSignatureDomain.begin(), kSignatureDomain.end());
  appendU16(input, version);
  appendU16(input, static_cast<std::uint16_t>(key_id.size()));
  input.insert(input.end(), key_id.begin(), key_id.end());
  appendU32(input, static_cast<std::uint32_t>(payload.size()));
  input.insert(input.end(), payload.begin(), payload.end());
  return input;
}

LicenseType convertType(license::LicenseType type) {
  switch (type) {
    case license::LicenseType::Subscription:
      return LicenseType::Subscription;
    case license::LicenseType::Permanent:
      return LicenseType::Permanent;
    case license::LicenseType::Trial:
      return LicenseType::Trial;
  }
  return LicenseType::Trial;
}

}  // namespace

LicenseValidator::LicenseValidator(
    std::unordered_map<std::string, crypto::Key> trusted_keys)
    : trusted_keys_(std::move(trusted_keys)) {}

Result<LicenseSnapshot> LicenseValidator::validate(
    std::span<const std::uint8_t> package, std::string_view expected_product,
    std::string_view machine_id, std::uint64_t now) const {
  if (package.empty() || package.size() > kMaxLicenseSize ||
      package.size() < 8 ||
      !license::LicensePackageBufferHasIdentifier(package.data())) {
    return {.error = ErrorCode::InvalidFormat};
  }
  flatbuffers::Verifier package_verifier(package.data(), package.size());
  if (!license::VerifyLicensePackageBuffer(package_verifier)) {
    return {.error = ErrorCode::InvalidFormat};
  }

  const auto* outer = license::GetLicensePackage(package.data());
  if (outer->format_version() != 1) {
    return {.error = ErrorCode::UnsupportedVersion};
  }
  if (!outer->key_id() || outer->key_id()->empty() ||
      outer->key_id()->size() > std::numeric_limits<std::uint16_t>::max() ||
      !outer->payload() || outer->payload()->empty() || !outer->signature() ||
      outer->signature()->size() != crypto::kSignatureSize) {
    return {.error = ErrorCode::InvalidLicense};
  }

  const std::string key_id = outer->key_id()->str();
  const auto key = trusted_keys_.find(key_id);
  if (key == trusted_keys_.end()) {
    return {.error = ErrorCode::UnknownKey};
  }
  const std::span payload(outer->payload()->data(), outer->payload()->size());
  const auto signed_bytes =
      signatureInput(outer->format_version(), key_id, payload);
  crypto::Signature signature{};
  std::ranges::copy(*outer->signature(), signature.begin());
  if (crypto::verifyEd25519(key->second, signed_bytes, signature) !=
      crypto::CryptoError::None) {
    return {.error = ErrorCode::InvalidSignature};
  }

  flatbuffers::Verifier payload_verifier(payload.data(), payload.size());
  if (!payload_verifier.VerifyBuffer<license::LicensePayload>(nullptr)) {
    return {.error = ErrorCode::InvalidLicense};
  }
  const auto* data =
      flatbuffers::GetRoot<license::LicensePayload>(payload.data());
  if (!data->license_id() || data->license_id()->empty() ||
      !data->product_id() || data->product_id()->empty() ||
      data->max_sessions() == 0 || !data->machine_policy()) {
    return {.error = ErrorCode::InvalidLicense};
  }
  if (data->license_type() != license::LicenseType::Trial &&
      data->license_type() != license::LicenseType::Subscription &&
      data->license_type() != license::LicenseType::Permanent) {
    return {.error = ErrorCode::InvalidLicense};
  }
  if (data->product_id()->string_view() != expected_product) {
    return {.error = ErrorCode::ProductMismatch};
  }
  if (now < data->not_before()) {
    return {.error = ErrorCode::NotYetValid};
  }
  if (data->license_type() == license::LicenseType::Permanent &&
      data->expire_time() != 0) {
    return {.error = ErrorCode::InvalidLicense};
  }
  if (data->license_type() != license::LicenseType::Permanent &&
      (data->expire_time() <= data->not_before() ||
       now >= data->expire_time())) {
    return {.error = ErrorCode::Expired};
  }

  const auto* policy = data->machine_policy();
  if (policy->mode() != license::MachineMode::Unbound &&
      policy->mode() != license::MachineMode::Exact) {
    return {.error = ErrorCode::InvalidLicense};
  }
  if (policy->mode() == license::MachineMode::Exact &&
      (!policy->machine_id() || policy->machine_id()->empty() ||
       policy->machine_id()->string_view() != machine_id)) {
    return {.error = ErrorCode::MachineMismatch};
  }

  LicenseSnapshot snapshot{
      .license_id = data->license_id()->str(),
      .product_id = data->product_id()->str(),
      .customer_id = data->customer_id() ? data->customer_id()->str() : "",
      .issue_time = data->issue_time(),
      .not_before = data->not_before(),
      .expire_time = data->expire_time(),
      .type = convertType(data->license_type()),
      .max_sessions = data->max_sessions(),
      .machine_mode = policy->mode() == license::MachineMode::Exact
                          ? MachineMode::Exact
                          : MachineMode::Unbound,
      .machine_id = policy->machine_id() ? policy->machine_id()->str() : "",
  };
  if (data->features()) {
    snapshot.features.reserve(data->features()->size());
    for (const auto* feature : *data->features()) {
      if (!feature || feature->empty()) {
        return {.error = ErrorCode::InvalidLicense};
      }
      snapshot.features.push_back(feature->str());
    }
  }
  return {.value = std::move(snapshot)};
}

}  // namespace yoauthorize::core
