#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

#include "yoauthorize/crypto/crypto.h"

namespace crypto = yoauthorize::crypto;

namespace {

std::vector<std::uint8_t> fromHex(std::string_view hex) {
  auto value = [](char digit) -> std::uint8_t {
    if (digit >= '0' && digit <= '9') return digit - '0';
    if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
    return digit - 'A' + 10;
  };
  std::vector<std::uint8_t> bytes;
  bytes.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    bytes.push_back(
        static_cast<std::uint8_t>((value(hex[i]) << 4U) | value(hex[i + 1])));
  }
  return bytes;
}

template <std::size_t Size>
std::array<std::uint8_t, Size> arrayFromHex(std::string_view hex) {
  const auto bytes = fromHex(hex);
  EXPECT_EQ(bytes.size(), Size);
  std::array<std::uint8_t, Size> result{};
  std::ranges::copy(bytes, result.begin());
  return result;
}

}  // namespace

TEST(CryptoTest, MatchesSha256Vector) {
  const std::vector<std::uint8_t> input{'a', 'b', 'c'};
  const auto result = crypto::sha256(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(result.value,
            arrayFromHex<crypto::kSha256Size>(
                "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61"
                "f20015ad"));
}

TEST(CryptoTest, MatchesEd25519Rfc8032Vector) {
  const auto private_key = arrayFromHex<crypto::kKeySize>(
      "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
  const auto public_key = arrayFromHex<crypto::kKeySize>(
      "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
  const auto expected = arrayFromHex<crypto::kSignatureSize>(
      "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
      "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");

  const auto signature = crypto::signEd25519(private_key, {});
  ASSERT_TRUE(signature);
  EXPECT_EQ(signature.value, expected);
  EXPECT_EQ(crypto::verifyEd25519(public_key, {}, signature.value),
            crypto::CryptoError::None);
}

TEST(CryptoTest, MatchesX25519Rfc7748Vector) {
  const auto alice_private = arrayFromHex<crypto::kKeySize>(
      "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
  const auto bob_public = arrayFromHex<crypto::kKeySize>(
      "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
  const auto expected = arrayFromHex<crypto::kKeySize>(
      "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");

  const auto shared = crypto::deriveX25519(alice_private, bob_public);
  ASSERT_TRUE(shared);
  EXPECT_EQ(shared.value, expected);
}

TEST(CryptoTest, MatchesHkdfRfc5869Vector) {
  const std::vector<std::uint8_t> key(22, 0x0b);
  const auto salt = fromHex("000102030405060708090a0b0c");
  const auto info = fromHex("f0f1f2f3f4f5f6f7f8f9");
  const auto expected = fromHex(
      "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
      "34007208d5b887185865");

  const auto output = crypto::hkdfSha256(key, salt, info, expected.size());
  ASSERT_TRUE(output);
  EXPECT_EQ(output.value, expected);
}

TEST(CryptoTest, RejectsModifiedCiphertext) {
  crypto::Key key{};
  crypto::Nonce nonce{};
  ASSERT_EQ(crypto::randomBytes(key), crypto::CryptoError::None);
  ASSERT_EQ(crypto::randomBytes(nonce), crypto::CryptoError::None);
  const std::vector<std::uint8_t> plaintext{'s', 'e', 'c', 'r', 'e', 't'};
  const std::vector<std::uint8_t> aad{'h', 'e', 'a', 'd', 'e', 'r'};

  auto sealed = crypto::sealChaCha20Poly1305(key, nonce, plaintext, aad);
  ASSERT_TRUE(sealed);
  auto opened = crypto::openChaCha20Poly1305(key, nonce, sealed.value, aad);
  ASSERT_TRUE(opened);
  EXPECT_EQ(opened.value, plaintext);

  sealed.value[0] ^= 1;
  opened = crypto::openChaCha20Poly1305(key, nonce, sealed.value, aad);
  EXPECT_EQ(opened.error, crypto::CryptoError::AuthenticationFailed);
}
