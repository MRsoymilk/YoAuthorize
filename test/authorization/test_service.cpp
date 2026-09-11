#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

#include "yoauthorize/core/session.h"
#include "yoauthorize/crypto/crypto.h"
#include "yoauthorize/protocol/frame.h"
#include "yoauthorize/protocol/handshake.h"
#include "yoauthorize/protocol/messages.h"
#include "yoauthorize/service/connection.h"

namespace core = yoauthorize::core;
namespace crypto = yoauthorize::crypto;
namespace protocol = yoauthorize::protocol;
namespace service = yoauthorize::service;

namespace {

std::vector<std::uint8_t> wrapPlaintext(
    const std::vector<std::uint8_t>& payload) {
  const auto header = protocol::encodeHeader(
      {.payload_size = static_cast<std::uint32_t>(payload.size())});
  std::vector<std::uint8_t> frame;
  for (const auto byte : header) {
    frame.push_back(std::to_integer<std::uint8_t>(byte));
  }
  frame.insert(frame.end(), payload.begin(), payload.end());
  return frame;
}

std::vector<std::uint8_t> unwrapPlaintext(
    const std::vector<std::uint8_t>& frame) {
  const auto decoded = protocol::decodeFrame(std::span(
      reinterpret_cast<const std::byte*>(frame.data()), frame.size()));
  EXPECT_TRUE(decoded);
  return {reinterpret_cast<const std::uint8_t*>(decoded.payload.data()),
          reinterpret_cast<const std::uint8_t*>(decoded.payload.data()) +
              decoded.payload.size()};
}

struct EstablishedConnection {
  crypto::KeyPair identity;
  core::LicenseSnapshot license{
      .product_id = "product-1",
      .type = core::LicenseType::Permanent,
      .features = {"capture", "export"},
      .max_sessions = 1,
  };
  core::SessionManager sessions;
  std::unique_ptr<service::Connection> server;
  std::unique_ptr<protocol::ClientHandshake> client;

  EstablishedConnection() {
    identity = crypto::generateEd25519KeyPair().value;
    server = std::make_unique<service::Connection>(
        "service-1", identity.private_key, license, sessions, "uid=1000", 5000,
        15000);
    client = std::make_unique<protocol::ClientHandshake>(
        std::unordered_map<std::string, crypto::Key>{
            {"service-1", identity.public_key}},
        "test-sdk", 1);
  }

  bool establish() {
    const auto hello = client->start();
    if (!hello) return false;
    const auto server_hello = server->handle(wrapPlaintext(hello.value), 1, 10);
    if (!server_hello || !server_hello.response) return false;
    const auto finished =
        client->handleServerHello(unwrapPlaintext(*server_hello.response));
    if (!finished) return false;
    const auto accepted = server->handle(finished.value, 2, 10);
    return accepted && !accepted.response;
  }
};

}  // namespace

TEST(ServiceConnectionTest, AuthorizesHeartbeatsAndClosesSession) {
  EstablishedConnection connection;
  ASSERT_TRUE(connection.establish());
  const protocol::AuthorizeMessage authorize{
      .request_id = 2,
      .protocol_version = 1,
      .product_id = "product-1",
      .process_id = 100,
  };
  const auto encoded = protocol::encodeAuthorize(authorize);
  ASSERT_TRUE(encoded);
  const auto sealed = connection.client->seal(encoded.value);
  ASSERT_TRUE(sealed);
  const auto authorized = connection.server->handle(sealed.bytes, 3, 10);
  ASSERT_TRUE(authorized);
  ASSERT_TRUE(authorized.response);
  const auto opened = connection.client->open(*authorized.response);
  ASSERT_TRUE(opened);
  const auto result = protocol::decodeAuthResult(opened.bytes);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result.value.session_id);
  EXPECT_EQ(result.value.features, connection.license.features);
  EXPECT_EQ(connection.sessions.size(), 1U);

  protocol::HeartbeatMessage heartbeat{
      .request_id = 3,
      .protocol_version = 1,
      .session_id = *result.value.session_id,
  };
  const auto heartbeat_bytes = protocol::encodeHeartbeat(heartbeat);
  ASSERT_TRUE(heartbeat_bytes);
  const auto heartbeat_frame = connection.client->seal(heartbeat_bytes.value);
  ASSERT_TRUE(heartbeat_frame);
  const auto heartbeat_result =
      connection.server->handle(heartbeat_frame.bytes, 4, 10);
  ASSERT_TRUE(heartbeat_result.response);
  const auto ack_bytes = connection.client->open(*heartbeat_result.response);
  ASSERT_TRUE(ack_bytes);
  const auto ack = protocol::decodeHeartbeatAck(ack_bytes.bytes);
  ASSERT_TRUE(ack);
  EXPECT_EQ(ack.value.state, protocol::AuthorizationState::Valid);

  const auto close_bytes =
      protocol::encodeCloseSession({.request_id = 4, .protocol_version = 1});
  ASSERT_TRUE(close_bytes);
  const auto close_frame = connection.client->seal(close_bytes.value);
  ASSERT_TRUE(close_frame);
  EXPECT_TRUE(connection.server->handle(close_frame.bytes, 5, 10).closed);
  EXPECT_EQ(connection.sessions.size(), 0U);
}

TEST(ServiceConnectionTest, ReturnsEncryptedProductMismatch) {
  EstablishedConnection connection;
  ASSERT_TRUE(connection.establish());
  const auto request = protocol::encodeAuthorize({
      .request_id = 2,
      .protocol_version = 1,
      .product_id = "other-product",
      .process_id = 100,
  });
  ASSERT_TRUE(request);
  const auto sealed = connection.client->seal(request.value);
  ASSERT_TRUE(sealed);
  const auto rejected = connection.server->handle(sealed.bytes, 3, 10);
  EXPECT_EQ(rejected.error, service::ConnectionError::ProductMismatch);
  EXPECT_TRUE(rejected.closed);
  ASSERT_TRUE(rejected.response);
  const auto opened = connection.client->open(*rejected.response);
  ASSERT_TRUE(opened);
  const auto result = protocol::decodeAuthResult(opened.bytes);
  ASSERT_TRUE(result);
  EXPECT_EQ(result.value.error, protocol::AuthorizationError::ProductMismatch);
}

TEST(ServiceConnectionTest, RejectsLicenseThatExpiredAfterStartup) {
  EstablishedConnection connection;
  connection.license.type = core::LicenseType::Subscription;
  connection.license.expire_time = 20;
  ASSERT_TRUE(connection.establish());
  const auto request = protocol::encodeAuthorize({
      .request_id = 2,
      .protocol_version = 1,
      .product_id = "product-1",
      .process_id = 100,
  });
  ASSERT_TRUE(request);
  const auto sealed = connection.client->seal(request.value);
  ASSERT_TRUE(sealed);

  const auto rejected = connection.server->handle(sealed.bytes, 3, 20);

  EXPECT_EQ(rejected.error, service::ConnectionError::LicenseExpired);
  ASSERT_TRUE(rejected.response);
  const auto opened = connection.client->open(*rejected.response);
  ASSERT_TRUE(opened);
  const auto result = protocol::decodeAuthResult(opened.bytes);
  ASSERT_TRUE(result);
  EXPECT_EQ(result.value.error, protocol::AuthorizationError::Expired);
}
