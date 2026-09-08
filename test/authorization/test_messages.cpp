#include <gtest/gtest.h>

#include <vector>

#include "yoauthorize/protocol/messages.h"

namespace protocol = yoauthorize::protocol;

TEST(HandshakeMessageTest, RoundTripsClientHello) {
  protocol::ClientHelloMessage input{
      .request_id = 7,
      .min_version = 1,
      .max_version = 2,
      .sdk_version = "0.1.0",
  };
  input.nonce.fill(1);
  input.public_key.fill(2);

  const auto encoded = protocol::encodeClientHello(input);
  ASSERT_TRUE(encoded);
  const auto decoded = protocol::decodeClientHello(encoded.value);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded.value.request_id, input.request_id);
  EXPECT_EQ(decoded.value.min_version, input.min_version);
  EXPECT_EQ(decoded.value.max_version, input.max_version);
  EXPECT_EQ(decoded.value.nonce, input.nonce);
  EXPECT_EQ(decoded.value.public_key, input.public_key);
  EXPECT_EQ(decoded.value.sdk_version, input.sdk_version);
}

TEST(HandshakeMessageTest, RoundTripsServerHello) {
  protocol::ServerHelloMessage input{
      .request_id = 8,
      .selected_version = 1,
      .service_key_id = "service-1",
  };
  input.nonce.fill(3);
  input.public_key.fill(4);
  input.signature.fill(5);

  const auto encoded = protocol::encodeServerHello(input);
  ASSERT_TRUE(encoded);
  const auto decoded = protocol::decodeServerHello(encoded.value);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded.value.request_id, input.request_id);
  EXPECT_EQ(decoded.value.selected_version, input.selected_version);
  EXPECT_EQ(decoded.value.nonce, input.nonce);
  EXPECT_EQ(decoded.value.public_key, input.public_key);
  EXPECT_EQ(decoded.value.service_key_id, input.service_key_id);
  EXPECT_EQ(decoded.value.signature, input.signature);
}

TEST(HandshakeMessageTest, RoundTripsClientFinished) {
  protocol::ClientFinishedMessage input{.request_id = 9, .protocol_version = 1};
  input.transcript_hash.fill(6);

  const auto encoded = protocol::encodeClientFinished(input);
  ASSERT_TRUE(encoded);
  const auto decoded = protocol::decodeClientFinished(encoded.value);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded.value.request_id, input.request_id);
  EXPECT_EQ(decoded.value.protocol_version, input.protocol_version);
  EXPECT_EQ(decoded.value.transcript_hash, input.transcript_hash);
}

TEST(HandshakeMessageTest, RejectsWrongTypeAndInvalidFields) {
  protocol::ClientHelloMessage invalid;
  EXPECT_EQ(protocol::encodeClientHello(invalid).error,
            protocol::MessageError::InvalidField);

  protocol::ServerHelloMessage server{
      .request_id = 1, .selected_version = 1, .service_key_id = "service-1"};
  const auto encoded = protocol::encodeServerHello(server);
  ASSERT_TRUE(encoded);
  EXPECT_EQ(protocol::decodeClientHello(encoded.value).error,
            protocol::MessageError::UnexpectedType);

  auto corrupted = encoded.value;
  corrupted.resize(8);
  EXPECT_EQ(protocol::decodeServerHello(corrupted).error,
            protocol::MessageError::InvalidEnvelope);
}
