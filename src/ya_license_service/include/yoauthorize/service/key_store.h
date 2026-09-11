#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "yoauthorize/crypto/crypto.h"
#include "yoauthorize/service/config.h"

namespace yoauthorize::service {

enum class KeyStoreError {
  None,
  OpenFailed,
  InvalidFile,
  InvalidPermissions,
  InvalidSize,
  DuplicateKeyId,
};

struct LoadedKeys {
  LoadedKeys() = default;
  ~LoadedKeys();
  LoadedKeys(const LoadedKeys&) = delete;
  LoadedKeys& operator=(const LoadedKeys&) = delete;
  LoadedKeys(LoadedKeys&& other) noexcept;
  LoadedKeys& operator=(LoadedKeys&& other) noexcept;

  std::string identity_key_id;
  crypto::Key identity_private_key{};
  std::unordered_map<std::string, crypto::Key> license_public_keys;
};

template <typename T>
struct KeyStoreResult {
  KeyStoreError error = KeyStoreError::None;
  T value{};

  explicit operator bool() const { return error == KeyStoreError::None; }
};

KeyStoreResult<crypto::Key> loadRawKey(const std::filesystem::path& path,
                                       bool private_key);
KeyStoreResult<LoadedKeys> loadKeys(const ServiceConfig& config);

}  // namespace yoauthorize::service
