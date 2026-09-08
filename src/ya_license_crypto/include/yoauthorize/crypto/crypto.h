#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace yoauthorize::crypto {

inline constexpr std::size_t kKeySize = 32;
inline constexpr std::size_t kNonceSize = 12;
inline constexpr std::size_t kTagSize = 16;
inline constexpr std::size_t kSignatureSize = 64;
inline constexpr std::size_t kSha256Size = 32;

using Key = std::array<std::uint8_t, kKeySize>;
using Nonce = std::array<std::uint8_t, kNonceSize>;
using Signature = std::array<std::uint8_t, kSignatureSize>;
using Sha256Digest = std::array<std::uint8_t, kSha256Size>;

enum class CryptoError {
  None,
  InvalidInput,
  RandomFailed,
  KeyGenerationFailed,
  DerivationFailed,
  SignatureFailed,
  InvalidSignature,
  EncryptionFailed,
  AuthenticationFailed,
};

template <typename T>
struct CryptoResult {
  CryptoError error = CryptoError::None;
  T value{};

  explicit operator bool() const { return error == CryptoError::None; }
};

struct KeyPair {
  Key private_key{};
  Key public_key{};
};

CryptoError randomBytes(std::span<std::uint8_t> output);
CryptoResult<Sha256Digest> sha256(std::span<const std::uint8_t> input);

CryptoResult<KeyPair> generateEd25519KeyPair();
CryptoResult<Signature> signEd25519(const Key& private_key,
                                    std::span<const std::uint8_t> message);
CryptoError verifyEd25519(const Key& public_key,
                          std::span<const std::uint8_t> message,
                          const Signature& signature);

CryptoResult<KeyPair> generateX25519KeyPair();
CryptoResult<Key> deriveX25519(const Key& private_key,
                               const Key& peer_public_key);

CryptoResult<std::vector<std::uint8_t>> hkdfSha256(
    std::span<const std::uint8_t> key, std::span<const std::uint8_t> salt,
    std::span<const std::uint8_t> info, std::size_t output_size);

CryptoResult<std::vector<std::uint8_t>> sealChaCha20Poly1305(
    const Key& key, const Nonce& nonce, std::span<const std::uint8_t> plaintext,
    std::span<const std::uint8_t> aad = {});

CryptoResult<std::vector<std::uint8_t>> openChaCha20Poly1305(
    const Key& key, const Nonce& nonce,
    std::span<const std::uint8_t> ciphertext_and_tag,
    std::span<const std::uint8_t> aad = {});

}  // namespace yoauthorize::crypto
