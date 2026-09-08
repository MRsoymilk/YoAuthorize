#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <span>
#include <thread>
#include <vector>

#include "yoauthorize/core/license.h"
#include "yoauthorize/core/session.h"
#include "yoauthorize/crypto/crypto.h"
#include "yoauthorize/protocol/frame.h"
#include "yoauthorize/sdk/client.h"
#include "yoauthorize/service/connection.h"
#include "yoauthorize/transport/unix_socket.h"

namespace core = yoauthorize::core;
namespace crypto = yoauthorize::crypto;
namespace sdk = yoauthorize::sdk;
namespace service = yoauthorize::service;
namespace transport = yoauthorize::transport;

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    auto pattern =
        (std::filesystem::temp_directory_path() / "yoauthorize-sdk-XXXXXX")
            .string();
    path_ = ::mkdtemp(pattern.data());
  }
  ~TemporaryDirectory() { std::filesystem::remove_all(path_); }
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

std::vector<std::uint8_t> readFrame(transport::ITransport& client) {
  std::array<std::byte, yoauthorize::protocol::kFrameHeaderSize> header_bytes{};
  if (!client.readExact(header_bytes, std::chrono::steady_clock::now() +
                                          std::chrono::seconds(2))) {
    return {};
  }
  const auto header = yoauthorize::protocol::decodeHeader(header_bytes);
  if (!header) return {};
  std::vector<std::uint8_t> frame(header_bytes.size() +
                                  header.header.payload_size);
  for (std::size_t i = 0; i < header_bytes.size(); ++i) {
    frame[i] = std::to_integer<std::uint8_t>(header_bytes[i]);
  }
  std::span payload(
      reinterpret_cast<std::byte*>(frame.data()) + header_bytes.size(),
      header.header.payload_size);
  if (!client.readExact(payload, std::chrono::steady_clock::now() +
                                     std::chrono::seconds(2))) {
    return {};
  }
  return frame;
}

bool writeFrame(transport::ITransport& client,
                const std::vector<std::uint8_t>& frame) {
  return static_cast<bool>(client.writeAll(
      std::span(reinterpret_cast<const std::byte*>(frame.data()), frame.size()),
      std::chrono::steady_clock::now() + std::chrono::seconds(2)));
}

}  // namespace

TEST(LicenseClientTest, RejectsIncompleteConfiguration) {
  sdk::LicenseClient client;
  EXPECT_EQ(client.initialize({}).error, sdk::ClientError::InvalidConfig);
  EXPECT_EQ(client.snapshot().state, sdk::ClientState::Error);
  client.shutdown();
  EXPECT_EQ(client.snapshot().state, sdk::ClientState::Closed);
}

TEST(LicenseClientTest, AuthenticatesAuthorizesAndMaintainsHeartbeat) {
  TemporaryDirectory directory;
  const auto socket_path = directory.path() / "license.sock";
  transport::UnixSocketListener listener(socket_path.string());
  ASSERT_TRUE(listener.listen());
  const auto identity = crypto::generateEd25519KeyPair();
  ASSERT_TRUE(identity);
  core::LicenseSnapshot license{
      .product_id = "product-1",
      .type = core::LicenseType::Permanent,
      .features = {"capture", "export"},
      .max_sessions = 1,
  };
  core::SessionManager sessions;
  std::atomic_bool server_ok = true;
  auto server = std::async(std::launch::async, [&] {
    auto accepted = listener.accept(std::chrono::steady_clock::now() +
                                    std::chrono::seconds(2));
    if (!accepted) {
      server_ok = false;
      return;
    }
    service::Connection connection("service-1", identity.value.private_key,
                                   license, sessions, "uid=1000", 20, 200);
    while (!connection.closed()) {
      const auto frame = readFrame(*accepted.transport);
      if (frame.empty()) {
        server_ok = false;
        return;
      }
      const auto result = connection.handle(frame, 10, 10);
      if (result.response &&
          !writeFrame(*accepted.transport, *result.response)) {
        server_ok = false;
        return;
      }
      if (result.error != service::ConnectionError::None) {
        server_ok = false;
        return;
      }
    }
  });

  sdk::LicenseClient client;
  const sdk::ClientConfig config{
      .product_id = "product-1",
      .endpoint = socket_path.string(),
      .trusted_service_keys = {{"service-1", identity.value.public_key}},
      .connect_timeout = std::chrono::seconds(1),
      .io_timeout = std::chrono::seconds(1),
  };
  ASSERT_TRUE(client.initialize(config));
  EXPECT_EQ(client.snapshot().state, sdk::ClientState::Valid);
  EXPECT_TRUE(client.hasFeature("capture"));
  EXPECT_FALSE(client.hasFeature("admin"));
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  EXPECT_EQ(client.snapshot().state, sdk::ClientState::Valid);

  client.shutdown();
  server.get();
  EXPECT_TRUE(server_ok);
  EXPECT_EQ(sessions.size(), 0U);
  EXPECT_FALSE(client.hasFeature("capture"));
}
