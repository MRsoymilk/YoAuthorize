#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "yoauthorize/crypto/crypto.h"

namespace yoauthorize::tools {

enum class IssueError {
  None,
  InvalidInput,
  SigningFailed,
};

struct LicenseDefinition {
  std::string key_id;
  std::string license_id;
  std::string product_id;
  std::string customer_id;
  std::uint64_t issue_time = 0;
  std::uint64_t not_before = 0;
  std::uint64_t expire_time = 0;
  std::vector<std::string> features;
  std::uint32_t max_sessions = 1;
  std::string machine_id;
};

struct IssueResult {
  IssueError error = IssueError::None;
  std::vector<std::uint8_t> package;

  explicit operator bool() const { return error == IssueError::None; }
};

IssueResult issueLicense(const LicenseDefinition& definition,
                         const crypto::Key& signing_private_key);

}  // namespace yoauthorize::tools
