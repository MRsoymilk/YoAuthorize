#include <gtest/gtest.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "yoauthorize/crypto/crypto.h"
#include "yoauthorize/protocol/handshake.h"
#include "yoauthorize/protocol/messages.h"

namespace crypto = yoauthorize::crypto;
namespace protocol = yoauthorize::protocol;

TEST(ProtocolHandshakeTest, EstablishesAuthenticatedBidirectionalChannel) {
  const auto identity = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(identity);
  protocol::ClientHandshake client({{"service-1", identity.value.public_key}},
                                   "sdk-0.1", 42);
  protocol::ServerHandshake server("service-1", identity.value.private_key);

  const auto client_hello = client.start();
  ASSERT_TRUE(client_hello);
  const auto server_hello = server.handleClientHello(client_hello.value);
  ASSERT_TRUE(server_hello);
  const auto client_finished = client.handleServerHello(server_hello.value);
  ASSERT_TRUE(client_finished);
  ASSERT_EQ(server.handleClientFinished(client_finished.value),
            protocol::HandshakeError::None);
  EXPECT_EQ(client.state(), protocol::HandshakeState::Established);
  EXPECT_EQ(server.state(), protocol::HandshakeState::Established);

  const std::vector<std::uint8_t> request{'r', 'e', 'q'};
  const auto encrypted_request = client.seal(request);
  ASSERT_TRUE(encrypted_request);
  const auto opened_request = server.open(encrypted_request.bytes);
  ASSERT_TRUE(opened_request);
  EXPECT_EQ(opened_request.bytes, request);

  const std::vector<std::uint8_t> response{'r', 'e', 's'};
  const auto encrypted_response = server.seal(response);
  ASSERT_TRUE(encrypted_response);
  const auto opened_response = client.open(encrypted_response.bytes);
  ASSERT_TRUE(opened_response);
  EXPECT_EQ(opened_response.bytes, response);
}

TEST(ProtocolHandshakeTest, RejectsUnknownServiceIdentity) {
  const auto identity = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(identity);
  protocol::ClientHandshake client({}, "sdk-0.1", 42);
  protocol::ServerHandshake server("service-1", identity.value.private_key);

  const auto client_hello = client.start();
  ASSERT_TRUE(client_hello);
  const auto server_hello = server.handleClientHello(client_hello.value);
  ASSERT_TRUE(server_hello);

  EXPECT_EQ(client.handleServerHello(server_hello.value).error,
            protocol::HandshakeError::UnknownServiceKey);
  EXPECT_EQ(client.state(), protocol::HandshakeState::Failed);
}

TEST(ProtocolHandshakeTest, RejectsTamperedServerHello) {
  const auto identity = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(identity);
  protocol::ClientHandshake client({{"service-1", identity.value.public_key}},
                                   "sdk-0.1", 42);
  protocol::ServerHandshake server("service-1", identity.value.private_key);

  const auto client_hello = client.start();
  ASSERT_TRUE(client_hello);
  auto server_hello = server.handleClientHello(client_hello.value);
  ASSERT_TRUE(server_hello);
  auto decoded = protocol::decodeServerHello(server_hello.value);
  ASSERT_TRUE(decoded);
  decoded.value.signature[0] ^= 1;
  const auto tampered = protocol::encodeServerHello(decoded.value);
  ASSERT_TRUE(tampered);

  EXPECT_EQ(client.handleServerHello(tampered.value).error,
            protocol::HandshakeError::InvalidServiceSignature);
  EXPECT_EQ(client.state(), protocol::HandshakeState::Failed);
}

TEST(ProtocolHandshakeTest, RejectsUnsupportedVersion) {
  const auto identity = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(identity);
  protocol::ClientHandshake client({}, "sdk-0.1", 42, 2, 3);
  protocol::ServerHandshake server("service-1", identity.value.private_key, 1,
                                   1);

  const auto client_hello = client.start();
  ASSERT_TRUE(client_hello);
  EXPECT_EQ(server.handleClientHello(client_hello.value).error,
            protocol::HandshakeError::UnsupportedVersion);
}
