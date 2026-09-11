#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "yoauthorize/protocol/record.h"

namespace protocol = yoauthorize::protocol;

TEST(RecordCodecTest, EncryptsAndAuthenticatesOrderedRecords) {
  protocol::RecordKey key;
  key.key.fill(7);
  key.nonce_salt = {1, 2, 3, 4};
  protocol::RecordWriter writer(key);
  protocol::RecordReader reader(key);
  const std::vector<std::uint8_t> message{'h', 'e', 'l', 'l', 'o'};

  const auto sealed = writer.seal(message);
  ASSERT_TRUE(sealed);
  const auto opened = reader.open(sealed.bytes);
  ASSERT_TRUE(opened);
  EXPECT_EQ(opened.bytes, message);
  EXPECT_EQ(writer.nextSequence(), 2);
  EXPECT_EQ(reader.nextSequence(), 2);
}

TEST(RecordCodecTest, RejectsTamperingAndReplay) {
  protocol::RecordKey key;
  key.key.fill(9);
  protocol::RecordWriter writer(key);
  protocol::RecordReader reader(key);
  const std::vector<std::uint8_t> message{'o', 'n', 'e'};

  const auto sealed = writer.seal(message);
  ASSERT_TRUE(sealed);
  auto tampered = sealed.bytes;
  tampered.back() ^= 1;
  EXPECT_EQ(reader.open(tampered).error,
            protocol::RecordError::AuthenticationFailed);

  ASSERT_TRUE(reader.open(sealed.bytes));
  EXPECT_EQ(reader.open(sealed.bytes).error,
            protocol::RecordError::InvalidSequence);
}
