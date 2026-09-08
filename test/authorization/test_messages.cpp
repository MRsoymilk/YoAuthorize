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

TEST(AuthorizationMessageTest, RoundTripsAuthorizeAndResult) {
  protocol::AuthorizeMessage authorize{
      .request_id = 10,
      .protocol_version = 1,
      .product_id = "product-1",
      .process_id = 123,
      .process_start_time = 456,
  };
  const auto encoded_authorize = protocol::encodeAuthorize(authorize);
  ASSERT_TRUE(encoded_authorize);
  const auto decoded_authorize =
      protocol::decodeAuthorize(encoded_authorize.value);
  ASSERT_TRUE(decoded_authorize);
  EXPECT_EQ(decoded_authorize.value.product_id, authorize.product_id);
  EXPECT_EQ(decoded_authorize.value.process_id, authorize.process_id);

  protocol::SessionId session_id{};
  session_id.fill(7);
  protocol::AuthResultMessage result{
      .request_id = authorize.request_id,
      .protocol_version = 1,
      .session_id = session_id,
      .features = {"capture", "export"},
      .expire_time = 1000,
      .heartbeat_interval_ms = 5000,
      .heartbeat_timeout_ms = 15000,
  };
  const auto encoded_result = protocol::encodeAuthResult(result);
  ASSERT_TRUE(encoded_result);
  const auto decoded_result = protocol::decodeAuthResult(encoded_result.value);
  ASSERT_TRUE(decoded_result);
  EXPECT_EQ(decoded_result.value.session_id, result.session_id);
  EXPECT_EQ(decoded_result.value.features, result.features);
  EXPECT_EQ(decoded_result.value.heartbeat_timeout_ms, 15000U);
}

TEST(AuthorizationMessageTest, RoundTripsHeartbeatCloseAndError) {
  protocol::HeartbeatMessage heartbeat{.request_id = 11, .protocol_version = 1};
  heartbeat.session_id.fill(8);
  const auto encoded_heartbeat = protocol::encodeHeartbeat(heartbeat);
  ASSERT_TRUE(encoded_heartbeat);
  const auto decoded_heartbeat =
      protocol::decodeHeartbeat(encoded_heartbeat.value);
  ASSERT_TRUE(decoded_heartbeat);
  EXPECT_EQ(decoded_heartbeat.value.session_id, heartbeat.session_id);

  const protocol::HeartbeatAckMessage ack{
      .request_id = 11,
      .protocol_version = 1,
      .state = protocol::AuthorizationState::Valid,
      .features = {"capture"},
      .expire_time = 1000,
  };
  const auto encoded_ack = protocol::encodeHeartbeatAck(ack);
  ASSERT_TRUE(encoded_ack);
  EXPECT_TRUE(protocol::decodeHeartbeatAck(encoded_ack.value));

  const protocol::CloseSessionMessage close{.request_id = 12,
                                            .protocol_version = 1};
  const auto encoded_close = protocol::encodeCloseSession(close);
  ASSERT_TRUE(encoded_close);
  EXPECT_TRUE(protocol::decodeCloseSession(encoded_close.value));

  const protocol::ErrorMessage error{
      .request_id = 13,
      .protocol_version = 1,
      .code = protocol::AuthorizationError::InvalidSession,
      .message = "invalid session",
  };
  const auto encoded_error = protocol::encodeError(error);
  ASSERT_TRUE(encoded_error);
  const auto decoded_error = protocol::decodeError(encoded_error.value);
  ASSERT_TRUE(decoded_error);
  EXPECT_EQ(decoded_error.value.code, error.code);
}

TEST(AuthorizationMessageTest, RejectsInvalidApplicationFields) {
  EXPECT_EQ(protocol::encodeAuthorize({}).error,
            protocol::MessageError::InvalidField);

  protocol::AuthResultMessage missing_session{.request_id = 1,
                                              .protocol_version = 1};
  EXPECT_EQ(protocol::encodeAuthResult(missing_session).error,
            protocol::MessageError::InvalidField);

  protocol::AuthResultMessage error_with_session{
      .request_id = 1,
      .protocol_version = 1,
      .error = protocol::AuthorizationError::Expired,
      .session_id = protocol::SessionId{},
  };
  EXPECT_EQ(protocol::encodeAuthResult(error_with_session).error,
            protocol::MessageError::InvalidField);
}
