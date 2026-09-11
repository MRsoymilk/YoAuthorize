#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "yoauthorize/protocol/frame.h"

namespace protocol = yoauthorize::protocol;

TEST(FrameCodecTest, EncodesBigEndianHeader) {
  const protocol::FrameHeader header{
      .version = 1,
      .flags = protocol::kEncryptedFlag,
      .sequence = 0x0102030405060708ULL,
      .payload_size = 0x00010203,
  };
  const auto encoded = protocol::encodeHeader(header);
  const std::array<std::uint8_t, protocol::kFrameHeaderSize> expected{
      'Y',  'A',  'L',  '1',  0x00, 0x01, 0x00, 0x01, 0x01, 0x02,
      0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x00, 0x01, 0x02, 0x03,
  };
  for (std::size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(std::to_integer<std::uint8_t>(encoded[i]), expected[i]);
  }
}

TEST(FrameCodecTest, DecodesCompleteFrame) {
  const protocol::FrameHeader header{
      .flags = protocol::kEncryptedFlag,
      .sequence = 7,
      .payload_size = 3,
  };
  const auto encoded = protocol::encodeHeader(header);
  std::vector<std::byte> frame(encoded.begin(), encoded.end());
  frame.insert(frame.end(), {std::byte{1}, std::byte{2}, std::byte{3}});

  const auto result = protocol::decodeFrame(frame);

  ASSERT_TRUE(result);
  EXPECT_EQ(result.header, header);
  ASSERT_EQ(result.payload.size(), 3);
  EXPECT_EQ(result.payload[2], std::byte{3});
}

TEST(FrameCodecTest, RejectsInvalidInput) {
  auto bytes = protocol::encodeHeader(
      {.version = 1, .flags = 0, .sequence = 0, .payload_size = 1});
  bytes[0] = std::byte{'N'};
  EXPECT_EQ(protocol::decodeHeader(bytes).error,
            protocol::FrameError::InvalidMagic);

  bytes = protocol::encodeHeader(
      {.version = 2, .flags = 0, .sequence = 0, .payload_size = 1});
  EXPECT_EQ(protocol::decodeHeader(bytes).error,
            protocol::FrameError::UnsupportedVersion);

  bytes = protocol::encodeHeader(
      {.version = 1, .flags = 2, .sequence = 0, .payload_size = 1});
  EXPECT_EQ(protocol::decodeHeader(bytes).error,
            protocol::FrameError::UnknownFlags);
}

TEST(FrameCodecTest, EnforcesPayloadBounds) {
  auto bytes = protocol::encodeHeader(
      {.version = 1, .flags = 0, .sequence = 0, .payload_size = 0});
  EXPECT_EQ(protocol::decodeHeader(bytes).error,
            protocol::FrameError::EmptyPayload);

  bytes = protocol::encodeHeader(
      {.version = 1, .flags = 0, .sequence = 0, .payload_size = 9});
  EXPECT_EQ(protocol::decodeHeader(bytes, 8).error,
            protocol::FrameError::PayloadTooLarge);
  EXPECT_EQ(protocol::decodeFrame(bytes).error,
            protocol::FrameError::LengthMismatch);
}
