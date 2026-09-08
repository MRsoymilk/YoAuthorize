#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "yoauthorize/core/status.h"
#include "yoauthorize/crypto/crypto.h"

namespace yoauthorize::core {

enum class LicenseType { Trial, Subscription, Permanent };
enum class MachineMode { Unbound, Exact };

struct LicenseSnapshot {
  std::string license_id;
  std::string product_id;
  std::string customer_id;
  std::uint64_t issue_time = 0;
  std::uint64_t not_before = 0;
  std::uint64_t expire_time = 0;
  LicenseType type = LicenseType::Trial;
  std::vector<std::string> features;
  std::uint32_t max_sessions = 0;
  MachineMode machine_mode = MachineMode::Unbound;
  std::string machine_id;
};

class LicenseValidator {
 public:
  explicit LicenseValidator(
      std::unordered_map<std::string, crypto::Key> trusted_keys);

  Result<LicenseSnapshot> validate(std::span<const std::uint8_t> package,
                                   std::string_view expected_product,
                                   std::string_view machine_id,
                                   std::uint64_t now) const;

 private:
  std::unordered_map<std::string, crypto::Key> trusted_keys_;
};

}  // namespace yoauthorize::core
