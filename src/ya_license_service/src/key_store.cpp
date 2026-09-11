#include "yoauthorize/service/key_store.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <utility>

namespace yoauthorize::service {

LoadedKeys::~LoadedKeys() { crypto::cleanse(identity_private_key); }

LoadedKeys::LoadedKeys(LoadedKeys&& other) noexcept
    : identity_key_id(std::move(other.identity_key_id)),
      identity_private_key(other.identity_private_key),
      license_public_keys(std::move(other.license_public_keys)) {
  crypto::cleanse(other.identity_private_key);
}

LoadedKeys& LoadedKeys::operator=(LoadedKeys&& other) noexcept {
  if (this != &other) {
    crypto::cleanse(identity_private_key);
    identity_key_id = std::move(other.identity_key_id);
    identity_private_key = other.identity_private_key;
    license_public_keys = std::move(other.license_public_keys);
    crypto::cleanse(other.identity_private_key);
  }
  return *this;
}

KeyStoreResult<crypto::Key> loadRawKey(const std::filesystem::path& path,
                                       bool private_key) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0) {
    return {.error = errno == ELOOP ? KeyStoreError::InvalidFile
                                    : KeyStoreError::OpenFailed};
  }

  struct stat info{};
  if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
    ::close(fd);
    return {.error = KeyStoreError::InvalidFile};
  }
  if (private_key && (info.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
    ::close(fd);
    return {.error = KeyStoreError::InvalidPermissions};
  }
  if (info.st_size != static_cast<off_t>(crypto::kKeySize)) {
    ::close(fd);
    return {.error = KeyStoreError::InvalidSize};
  }

  crypto::Key key{};
  std::size_t offset = 0;
  while (offset < key.size()) {
    const auto count = ::read(fd, key.data() + offset, key.size() - offset);
    if (count <= 0) {
      crypto::cleanse(key);
      ::close(fd);
      return {.error = KeyStoreError::InvalidFile};
    }
    offset += static_cast<std::size_t>(count);
  }
  ::close(fd);
  return {.value = key};
}

KeyStoreResult<LoadedKeys> loadKeys(const ServiceConfig& config) {
  auto identity = loadRawKey(config.identity_private_key_path, true);
  if (!identity) {
    return {.error = identity.error};
  }

  LoadedKeys loaded;
  loaded.identity_key_id = config.identity_key_id;
  loaded.identity_private_key = identity.value;
  crypto::cleanse(identity.value);
  for (const auto& key : config.license_keys) {
    auto public_key = loadRawKey(key.public_key_path, false);
    if (!public_key) {
      crypto::cleanse(loaded.identity_private_key);
      return {.error = public_key.error};
    }
    if (!loaded.license_public_keys.emplace(key.id, public_key.value).second) {
      crypto::cleanse(public_key.value);
      crypto::cleanse(loaded.identity_private_key);
      return {.error = KeyStoreError::DuplicateKeyId};
    }
    crypto::cleanse(public_key.value);
  }
  return {.value = std::move(loaded)};
}

}  // namespace yoauthorize::service
