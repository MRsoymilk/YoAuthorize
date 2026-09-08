#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>

#include "yoauthorize/crypto/crypto.h"
#include "yoauthorize/tools/issuer.h"

namespace {

bool writeNewFile(const std::filesystem::path& path,
                  std::span<const std::uint8_t> bytes, mode_t mode) {
  const int fd =
      ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
  if (fd < 0) return false;
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const auto count =
        ::write(fd, bytes.data() + offset, bytes.size() - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      ::close(fd);
      std::filesystem::remove(path);
      return false;
    }
    offset += static_cast<std::size_t>(count);
  }
  const bool flushed = ::fsync(fd) == 0;
  const bool closed = ::close(fd) == 0;
  const bool success = flushed && closed;
  if (!success) std::filesystem::remove(path);
  return success;
}

yoauthorize::crypto::CryptoResult<yoauthorize::crypto::Key> readPrivateKey(
    const std::filesystem::path& path) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0) {
    return {.error = yoauthorize::crypto::CryptoError::InvalidInput};
  }
  struct stat info{};
  yoauthorize::crypto::Key key{};
  if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) ||
      (info.st_mode & (S_IRWXG | S_IRWXO)) != 0 ||
      info.st_size != static_cast<off_t>(key.size())) {
    ::close(fd);
    return {.error = yoauthorize::crypto::CryptoError::InvalidInput};
  }
  std::size_t offset = 0;
  while (offset < key.size()) {
    const auto count = ::read(fd, key.data() + offset, key.size() - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) {
      yoauthorize::crypto::cleanse(key);
      ::close(fd);
      return {.error = yoauthorize::crypto::CryptoError::InvalidInput};
    }
    offset += static_cast<std::size_t>(count);
  }
  ::close(fd);
  return {.value = key};
}

template <typename T>
bool parseNumber(std::string_view text, T& value) {
  const auto* begin = text.data();
  const auto* end = begin + text.size();
  const auto result = std::from_chars(begin, end, value);
  return result.ec == std::errc{} && result.ptr == end;
}

int keygen(int argc, char** argv) {
  if (argc != 4) return 2;
  auto keys = yoauthorize::crypto::generateEd25519KeyPair();
  if (!keys) return 1;
  if (!writeNewFile(argv[2], keys.value.private_key, 0600)) {
    yoauthorize::crypto::cleanse(keys.value.private_key);
    std::cerr << "failed to create private key\n";
    return 1;
  }
  if (!writeNewFile(argv[3], keys.value.public_key, 0644)) {
    std::filesystem::remove(argv[2]);
    yoauthorize::crypto::cleanse(keys.value.private_key);
    std::cerr << "failed to create public key\n";
    return 1;
  }
  yoauthorize::crypto::cleanse(keys.value.private_key);
  return 0;
}

int issue(int argc, char** argv) {
  if (argc < 10) return 2;
  std::uint64_t validity_seconds = 0;
  std::uint32_t max_sessions = 0;
  if (!parseNumber(argv[8], validity_seconds) ||
      !parseNumber(argv[9], max_sessions) || max_sessions == 0) {
    return 2;
  }
  auto private_key = readPrivateKey(argv[2]);
  if (!private_key) {
    std::cerr << "failed to load signing private key\n";
    return 1;
  }
  const auto now = static_cast<std::uint64_t>(std::time(nullptr));
  yoauthorize::tools::LicenseDefinition definition{
      .key_id = argv[3],
      .license_id = argv[5],
      .product_id = argv[6],
      .customer_id = argv[7],
      .issue_time = now,
      .not_before = now,
      .expire_time = validity_seconds == 0 ? 0 : now + validity_seconds,
      .max_sessions = max_sessions,
  };
  for (int i = 10; i < argc; ++i) definition.features.emplace_back(argv[i]);
  const auto package =
      yoauthorize::tools::issueLicense(definition, private_key.value);
  yoauthorize::crypto::cleanse(private_key.value);
  if (!package || !writeNewFile(argv[4], package.package, 0644)) {
    std::cerr << "failed to issue license\n";
    return 1;
  }
  return 0;
}

void usage() {
  std::cerr
      << "usage:\n"
      << "  yoauthorize-license-generator keygen <private-key> <public-key>\n"
      << "  yoauthorize-license-generator issue <private-key> <key-id> "
         "<output> <license-id> <product-id> <customer-id> "
         "<valid-seconds|0> <max-sessions> [feature...]\n";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && std::string_view(argv[1]) == "keygen") {
    const int result = keygen(argc, argv);
    if (result == 2) usage();
    return result;
  }
  if (argc >= 2 && std::string_view(argv[1]) == "issue") {
    const int result = issue(argc, argv);
    if (result == 2) usage();
    return result;
  }
  usage();
  return 2;
}
