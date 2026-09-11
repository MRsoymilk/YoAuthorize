#include "yoauthorize/core/machine_identity.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <span>
#include <utility>
#include <vector>

#include "yoauthorize/crypto/crypto.h"

namespace yoauthorize::core {
namespace {

constexpr std::string_view kHashDomain = "YOAUTHORIZE-MACHINE-ID-V1";
constexpr std::string_view kComponentHashDomain =
    "YOAUTHORIZE-MACHINE-COMPONENT-V1";
constexpr std::size_t kMaxComponentSize = 4096;

struct NormalizedComponent {
  std::string_view type;
  std::string value;
};

bool isAsciiSpace(char value) {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
         value == '\f' || value == '\v';
}

bool isAsciiHex(char value) {
  return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') ||
         (value >= 'A' && value <= 'F');
}

char asciiLower(char value) {
  if (value >= 'A' && value <= 'F') {
    return static_cast<char>(value + ('a' - 'A'));
  }
  return value;
}

std::string_view trimAscii(std::string_view value) {
  while (!value.empty() && isAsciiSpace(value.front())) {
    value.remove_prefix(1);
  }
  while (!value.empty() && isAsciiSpace(value.back())) {
    value.remove_suffix(1);
  }
  return value;
}

bool normalizeMachineId(std::string_view input, std::string& output) {
  input = trimAscii(input);
  if (input.size() != 32 ||
      !std::all_of(input.begin(), input.end(), isAsciiHex)) {
    return false;
  }
  output.resize(input.size());
  std::transform(input.begin(), input.end(), output.begin(), asciiLower);
  return true;
}

bool normalizeProductUuid(std::string_view input, std::string& output) {
  input = trimAscii(input);
  constexpr std::array<std::size_t, 4> hyphens{8, 13, 18, 23};
  if (input.size() != 36) {
    return false;
  }

  output.clear();
  output.reserve(32);
  for (std::size_t index = 0; index < input.size(); ++index) {
    if (std::find(hyphens.begin(), hyphens.end(), index) != hyphens.end()) {
      if (input[index] != '-') {
        return false;
      }
    } else {
      if (!isAsciiHex(input[index])) {
        return false;
      }
      output.push_back(asciiLower(input[index]));
    }
  }
  return true;
}

MachineIdentityReadResult readFile(std::string_view path) {
  errno = 0;
  std::ifstream stream(std::string(path), std::ios::binary);
  if (!stream) {
    return {.status = errno == ENOENT ? MachineIdentityReadStatus::Missing
                                      : MachineIdentityReadStatus::Failure};
  }

  std::string contents;
  contents.reserve(128);
  char value = 0;
  while (contents.size() <= kMaxComponentSize && stream.get(value)) {
    contents.push_back(value);
  }
  if (stream.bad() || contents.size() > kMaxComponentSize) {
    return {.status = MachineIdentityReadStatus::Failure};
  }
  return {.status = MachineIdentityReadStatus::Success,
          .contents = std::move(contents)};
}

void appendU32(std::vector<std::uint8_t>& output, std::size_t value) {
  const auto narrowed = static_cast<std::uint32_t>(value);
  for (std::size_t index = 0; index < 4; ++index) {
    output.push_back(static_cast<std::uint8_t>(narrowed >> (24U - index * 8U)));
  }
}

void appendField(std::vector<std::uint8_t>& output, std::string_view value) {
  appendU32(output, value.size());
  output.insert(output.end(), value.begin(), value.end());
}

std::string hexDigest(const crypto::Sha256Digest& digest) {
  constexpr char digits[] = "0123456789abcdef";
  std::string output;
  output.reserve(digest.size() * 2);
  for (const auto byte : digest) {
    output.push_back(digits[byte >> 4U]);
    output.push_back(digits[byte & 0x0fU]);
  }
  return output;
}

MachineIdentityComponentStatus readComponent(
    const MachineIdentityReadFunction& reader, std::string_view path,
    MachineIdentityComponentType type, std::vector<NormalizedComponent>& out) {
  MachineIdentityReadResult read;
  try {
    read = reader(path);
  } catch (...) {
    return MachineIdentityComponentStatus::Unreadable;
  }

  if (read.status == MachineIdentityReadStatus::Missing) {
    return MachineIdentityComponentStatus::Missing;
  }
  if (read.status != MachineIdentityReadStatus::Success) {
    return MachineIdentityComponentStatus::Unreadable;
  }

  std::string normalized;
  const bool valid = type == MachineIdentityComponentType::MachineId
                         ? normalizeMachineId(read.contents, normalized)
                         : normalizeProductUuid(read.contents, normalized);
  if (!valid) {
    return MachineIdentityComponentStatus::Invalid;
  }

  out.push_back({.type = type == MachineIdentityComponentType::MachineId
                             ? "machine-id"
                             : "product-uuid",
                 .value = std::move(normalized)});
  return MachineIdentityComponentStatus::Available;
}

}  // namespace

Result<MachineIdentity> collectLinuxMachineIdentity(
    const LinuxMachineIdentityConfig& config,
    MachineIdentityReadFunction read_file) {
  if (!read_file) {
    read_file = readFile;
  }

  MachineIdentity identity;
  std::vector<NormalizedComponent> normalized;
  identity.components = {
      MachineIdentityComponent{
          .type = MachineIdentityComponentType::MachineId,
          .status = readComponent(read_file, config.machine_id_path,
                                  MachineIdentityComponentType::MachineId,
                                  normalized)},
      MachineIdentityComponent{
          .type = MachineIdentityComponentType::ProductUuid,
          .status = readComponent(read_file, config.product_uuid_path,
                                  MachineIdentityComponentType::ProductUuid,
                                  normalized)},
  };

  if ((config.require_machine_id &&
       identity.components[0].status !=
           MachineIdentityComponentStatus::Available) ||
      (config.require_product_uuid &&
       identity.components[1].status !=
           MachineIdentityComponentStatus::Available)) {
    return {.error = ErrorCode::MachineIdentityUnavailable,
            .value = std::move(identity)};
  }
  if (normalized.empty()) {
    return {.error = ErrorCode::MachineIdentityUnavailable,
            .value = std::move(identity)};
  }

  std::sort(normalized.begin(), normalized.end(),
            [](const auto& left, const auto& right) {
              return left.type < right.type;
            });
  std::vector<std::uint8_t> transcript;
  appendField(transcript, kHashDomain);
  appendU32(transcript, normalized.size());
  for (const auto& component : normalized) {
    std::vector<std::uint8_t> component_input;
    appendField(component_input, kComponentHashDomain);
    appendField(component_input, component.type);
    appendField(component_input, component.value);
    const auto component_digest = crypto::sha256(component_input);
    if (!component_digest) {
      return {.error = ErrorCode::InternalError, .value = std::move(identity)};
    }
    appendField(transcript, component.type);
    appendU32(transcript, component_digest.value.size());
    transcript.insert(transcript.end(), component_digest.value.begin(),
                      component_digest.value.end());
  }

  const auto digest = crypto::sha256(transcript);
  if (!digest) {
    return {.error = ErrorCode::InternalError, .value = std::move(identity)};
  }
  identity.exact_id = hexDigest(digest.value);
  return {.value = std::move(identity)};
}

}  // namespace yoauthorize::core
