#include <flatbuffers/flatbuffer_builder.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "protocol_generated.h"
#include "yoauthorize/protocol/envelope.h"

namespace protocol = yoauthorize::protocol;

TEST(EnvelopeTest, VerifiesValidMessage) {
  flatbuffers::FlatBufferBuilder builder;
  const auto ping = protocol::CreatePing(builder);
  const auto envelope = protocol::CreateEnvelope(
      builder, 1, 42, protocol::Message::Ping, ping.Union());
  protocol::FinishEnvelopeBuffer(builder, envelope);

  EXPECT_EQ(
      protocol::verifyEnvelope({builder.GetBufferPointer(), builder.GetSize()}),
      protocol::EnvelopeError::None);
}

TEST(EnvelopeTest, RejectsMissingIdentifier) {
  flatbuffers::FlatBufferBuilder builder;
  const auto ping = protocol::CreatePing(builder);
  const auto envelope = protocol::CreateEnvelope(
      builder, 1, 42, protocol::Message::Ping, ping.Union());
  builder.Finish(envelope);

  EXPECT_EQ(
      protocol::verifyEnvelope({builder.GetBufferPointer(), builder.GetSize()}),
      protocol::EnvelopeError::InvalidIdentifier);
}

TEST(EnvelopeTest, RejectsTruncatedAndOversizedBuffers) {
  const std::vector<std::uint8_t> truncated{'Y', 'A', 'M', 'S'};
  EXPECT_EQ(protocol::verifyEnvelope(truncated),
            protocol::EnvelopeError::InvalidIdentifier);

  const std::vector<std::uint8_t> oversized(9);
  EXPECT_EQ(protocol::verifyEnvelope(oversized, 8),
            protocol::EnvelopeError::TooLarge);
}
