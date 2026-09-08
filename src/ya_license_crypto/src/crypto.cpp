#include "yoauthorize/crypto/crypto.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/rand.h>

#include <climits>
#include <memory>

namespace yoauthorize::crypto {
namespace {

template <typename T, void (*Free)(T*)>
using OpenSslPtr = std::unique_ptr<T, decltype(Free)>;

bool validSize(std::size_t size) {
  return size <= static_cast<std::size_t>(INT_MAX);
}

CryptoResult<KeyPair> generateKeyPair(int type) {
  OpenSslPtr<EVP_PKEY_CTX, EVP_PKEY_CTX_free> context(
      EVP_PKEY_CTX_new_id(type, nullptr), EVP_PKEY_CTX_free);
  EVP_PKEY* raw_key = nullptr;
  if (!context || EVP_PKEY_keygen_init(context.get()) != 1 ||
      EVP_PKEY_keygen(context.get(), &raw_key) != 1) {
    return {.error = CryptoError::KeyGenerationFailed};
  }
  OpenSslPtr<EVP_PKEY, EVP_PKEY_free> key(raw_key, EVP_PKEY_free);

  KeyPair result;
  std::size_t private_size = result.private_key.size();
  std::size_t public_size = result.public_key.size();
  if (EVP_PKEY_get_raw_private_key(key.get(), result.private_key.data(),
                                   &private_size) != 1 ||
      EVP_PKEY_get_raw_public_key(key.get(), result.public_key.data(),
                                  &public_size) != 1 ||
      private_size != kKeySize || public_size != kKeySize) {
    return {.error = CryptoError::KeyGenerationFailed};
  }
  return {.value = result};
}

OpenSslPtr<EVP_PKEY, EVP_PKEY_free> rawPrivateKey(int type, const Key& key) {
  return {EVP_PKEY_new_raw_private_key(type, nullptr, key.data(), key.size()),
          EVP_PKEY_free};
}

OpenSslPtr<EVP_PKEY, EVP_PKEY_free> rawPublicKey(int type, const Key& key) {
  return {EVP_PKEY_new_raw_public_key(type, nullptr, key.data(), key.size()),
          EVP_PKEY_free};
}

}  // namespace

CryptoError randomBytes(std::span<std::uint8_t> output) {
  if (output.empty() || !validSize(output.size())) {
    return CryptoError::InvalidInput;
  }
  return RAND_priv_bytes(output.data(), static_cast<int>(output.size())) == 1
             ? CryptoError::None
             : CryptoError::RandomFailed;
}

CryptoResult<Sha256Digest> sha256(std::span<const std::uint8_t> input) {
  OpenSslPtr<EVP_MD_CTX, EVP_MD_CTX_free> context(EVP_MD_CTX_new(),
                                                  EVP_MD_CTX_free);
  Sha256Digest digest{};
  unsigned int size = 0;
  if (!context ||
      EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
      EVP_DigestUpdate(context.get(), input.data(), input.size()) != 1 ||
      EVP_DigestFinal_ex(context.get(), digest.data(), &size) != 1 ||
      size != digest.size()) {
    return {.error = CryptoError::DerivationFailed};
  }
  return {.value = digest};
}

CryptoResult<KeyPair> generateEd25519KeyPair() {
  return generateKeyPair(EVP_PKEY_ED25519);
}

CryptoResult<Signature> signEd25519(const Key& private_key,
                                    std::span<const std::uint8_t> message) {
  auto key = rawPrivateKey(EVP_PKEY_ED25519, private_key);
  OpenSslPtr<EVP_MD_CTX, EVP_MD_CTX_free> context(EVP_MD_CTX_new(),
                                                  EVP_MD_CTX_free);
  Signature signature{};
  std::size_t signature_size = signature.size();
  if (!key || !context ||
      EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key.get()) !=
          1 ||
      EVP_DigestSign(context.get(), signature.data(), &signature_size,
                     message.data(), message.size()) != 1 ||
      signature_size != signature.size()) {
    return {.error = CryptoError::SignatureFailed};
  }
  return {.value = signature};
}

CryptoError verifyEd25519(const Key& public_key,
                          std::span<const std::uint8_t> message,
                          const Signature& signature) {
  auto key = rawPublicKey(EVP_PKEY_ED25519, public_key);
  OpenSslPtr<EVP_MD_CTX, EVP_MD_CTX_free> context(EVP_MD_CTX_new(),
                                                  EVP_MD_CTX_free);
  if (!key || !context ||
      EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr,
                           key.get()) != 1) {
    return CryptoError::SignatureFailed;
  }
  const int result =
      EVP_DigestVerify(context.get(), signature.data(), signature.size(),
                       message.data(), message.size());
  return result == 1 ? CryptoError::None : CryptoError::InvalidSignature;
}

CryptoResult<KeyPair> generateX25519KeyPair() {
  return generateKeyPair(EVP_PKEY_X25519);
}

CryptoResult<Key> deriveX25519(const Key& private_key,
                               const Key& peer_public_key) {
  auto private_value = rawPrivateKey(EVP_PKEY_X25519, private_key);
  auto public_value = rawPublicKey(EVP_PKEY_X25519, peer_public_key);
  if (!private_value || !public_value) {
    return {.error = CryptoError::InvalidInput};
  }

  OpenSslPtr<EVP_PKEY_CTX, EVP_PKEY_CTX_free> context(
      EVP_PKEY_CTX_new(private_value.get(), nullptr), EVP_PKEY_CTX_free);
  Key shared_secret{};
  std::size_t size = shared_secret.size();
  if (!context || EVP_PKEY_derive_init(context.get()) != 1 ||
      EVP_PKEY_derive_set_peer(context.get(), public_value.get()) != 1 ||
      EVP_PKEY_derive(context.get(), shared_secret.data(), &size) != 1 ||
      size != shared_secret.size()) {
    return {.error = CryptoError::DerivationFailed};
  }
  return {.value = shared_secret};
}

CryptoResult<std::vector<std::uint8_t>> hkdfSha256(
    std::span<const std::uint8_t> key, std::span<const std::uint8_t> salt,
    std::span<const std::uint8_t> info, std::size_t output_size) {
  if (key.empty() || output_size == 0) {
    return {.error = CryptoError::InvalidInput};
  }

  OpenSslPtr<EVP_KDF, EVP_KDF_free> algorithm(
      EVP_KDF_fetch(nullptr, "HKDF", nullptr), EVP_KDF_free);
  OpenSslPtr<EVP_KDF_CTX, EVP_KDF_CTX_free> context(
      algorithm ? EVP_KDF_CTX_new(algorithm.get()) : nullptr, EVP_KDF_CTX_free);
  if (!context) {
    return {.error = CryptoError::DerivationFailed};
  }

  char digest_name[] = "SHA256";
  auto* key_data = const_cast<std::uint8_t*>(key.data());
  auto* salt_data = const_cast<std::uint8_t*>(salt.data());
  auto* info_data = const_cast<std::uint8_t*>(info.data());
  OSSL_PARAM params[] = {
      OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, digest_name, 0),
      OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, key_data,
                                        key.size()),
      OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, salt_data,
                                        salt.size()),
      OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, info_data,
                                        info.size()),
      OSSL_PARAM_construct_end(),
  };

  std::vector<std::uint8_t> output(output_size);
  if (EVP_KDF_derive(context.get(), output.data(), output.size(), params) !=
      1) {
    return {.error = CryptoError::DerivationFailed};
  }
  return {.value = std::move(output)};
}

CryptoResult<std::vector<std::uint8_t>> sealChaCha20Poly1305(
    const Key& key, const Nonce& nonce, std::span<const std::uint8_t> plaintext,
    std::span<const std::uint8_t> aad) {
  if (!validSize(plaintext.size()) || !validSize(aad.size())) {
    return {.error = CryptoError::InvalidInput};
  }
  OpenSslPtr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> context(EVP_CIPHER_CTX_new(),
                                                          EVP_CIPHER_CTX_free);
  std::vector<std::uint8_t> output(plaintext.size() + kTagSize);
  int output_size = 0;
  int total_size = 0;
  if (!context ||
      EVP_EncryptInit_ex2(context.get(), EVP_chacha20_poly1305(), key.data(),
                          nonce.data(), nullptr) != 1 ||
      (!aad.empty() &&
       EVP_EncryptUpdate(context.get(), nullptr, &output_size, aad.data(),
                         static_cast<int>(aad.size())) != 1) ||
      EVP_EncryptUpdate(context.get(), output.data(), &output_size,
                        plaintext.data(),
                        static_cast<int>(plaintext.size())) != 1) {
    return {.error = CryptoError::EncryptionFailed};
  }
  total_size = output_size;
  if (EVP_EncryptFinal_ex(context.get(), output.data() + total_size,
                          &output_size) != 1) {
    return {.error = CryptoError::EncryptionFailed};
  }
  total_size += output_size;
  if (EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_AEAD_GET_TAG, kTagSize,
                          output.data() + total_size) != 1) {
    return {.error = CryptoError::EncryptionFailed};
  }
  output.resize(static_cast<std::size_t>(total_size) + kTagSize);
  return {.value = std::move(output)};
}

CryptoResult<std::vector<std::uint8_t>> openChaCha20Poly1305(
    const Key& key, const Nonce& nonce,
    std::span<const std::uint8_t> ciphertext_and_tag,
    std::span<const std::uint8_t> aad) {
  if (ciphertext_and_tag.size() < kTagSize ||
      !validSize(ciphertext_and_tag.size() - kTagSize) ||
      !validSize(aad.size())) {
    return {.error = CryptoError::InvalidInput};
  }
  const auto ciphertext =
      ciphertext_and_tag.first(ciphertext_and_tag.size() - kTagSize);
  const auto tag = ciphertext_and_tag.last(kTagSize);
  OpenSslPtr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> context(EVP_CIPHER_CTX_new(),
                                                          EVP_CIPHER_CTX_free);
  std::vector<std::uint8_t> output(ciphertext.size());
  int output_size = 0;
  int total_size = 0;
  if (!context ||
      EVP_DecryptInit_ex2(context.get(), EVP_chacha20_poly1305(), key.data(),
                          nonce.data(), nullptr) != 1 ||
      (!aad.empty() &&
       EVP_DecryptUpdate(context.get(), nullptr, &output_size, aad.data(),
                         static_cast<int>(aad.size())) != 1) ||
      EVP_DecryptUpdate(context.get(), output.data(), &output_size,
                        ciphertext.data(),
                        static_cast<int>(ciphertext.size())) != 1) {
    return {.error = CryptoError::AuthenticationFailed};
  }
  total_size = output_size;
  if (EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_AEAD_SET_TAG, kTagSize,
                          const_cast<std::uint8_t*>(tag.data())) != 1 ||
      EVP_DecryptFinal_ex(context.get(), output.data() + total_size,
                          &output_size) != 1) {
    return {.error = CryptoError::AuthenticationFailed};
  }
  output.resize(static_cast<std::size_t>(total_size + output_size));
  return {.value = std::move(output)};
}

}  // namespace yoauthorize::crypto
