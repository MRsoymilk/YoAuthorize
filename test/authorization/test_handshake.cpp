#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

#include "yoauthorize/crypto/handshake.h"

namespace crypto = yoauthorize::crypto;

TEST(HandshakeTest, EncodesVersionsAndKeyIdDeterministically) {
  crypto::HandshakeTranscript transcript{
      .min_version = 1,
      .max_version = 3,
      .selected_version = 2,
      .service_key_id = "service-2026",
  };
  transcript.client_nonce.fill(1);
  transcript.server_nonce.fill(2);
  transcript.client_public_key.fill(3);
  transcript.server_public_key.fill(4);

  const auto encoded = crypto::encodeTranscript(transcript);

  constexpr std::string_view domain = "YOAUTHORIZE-SERVICE-HANDSHAKE-V1";
  ASSERT_GT(encoded.size(), domain.size() + 6);
  EXPECT_TRUE(std::equal(domain.begin(), domain.end(), encoded.begin()));
  EXPECT_EQ(encoded[domain.size()], 0);
  EXPECT_EQ(encoded[domain.size() + 1], 1);
  EXPECT_EQ(encoded[domain.size() + 2], 0);
  EXPECT_EQ(encoded[domain.size() + 3], 3);
  EXPECT_EQ(encoded[domain.size() + 4], 0);
  EXPECT_EQ(encoded[domain.size() + 5], 2);

  const auto first_hash = crypto::hashTranscript(transcript);
  ASSERT_TRUE(first_hash);
  transcript.selected_version = 1;
  const auto second_hash = crypto::hashTranscript(transcript);
  ASSERT_TRUE(second_hash);
  EXPECT_NE(first_hash.value, second_hash.value);
}

TEST(HandshakeTest, BuildsDirectionalRecordNonce) {
  const std::array<std::uint8_t, 4> salt{1, 2, 3, 4};
  const auto nonce = crypto::makeRecordNonce(salt, 0x0102030405060708ULL);
  const crypto::Nonce expected{1, 2, 3, 4, 1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_EQ(nonce, expected);
}
