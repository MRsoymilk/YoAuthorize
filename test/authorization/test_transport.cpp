#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <string>

#include "yoauthorize/transport/unix_socket.h"

namespace transport = yoauthorize::transport;
using namespace std::chrono_literals;

namespace {

class TemporarySocketPath {
 public:
  TemporarySocketPath() {
    std::string pattern =
        (std::filesystem::temp_directory_path() / "yoauthorize-XXXXXX")
            .string();
    path_.assign(pattern.begin(), pattern.end());
    path_.push_back('\0');
    EXPECT_NE(::mkdtemp(path_.data()), nullptr);
  }

  ~TemporarySocketPath() {
    std::error_code error;
    std::filesystem::remove_all(directory(), error);
  }

  std::filesystem::path directory() const { return path_.data(); }
  std::string socket() const { return (directory() / "service.sock").string(); }

 private:
  std::string path_;
};

transport::Deadline soon() { return std::chrono::steady_clock::now() + 2s; }

}  // namespace

TEST(UnixSocketTransportTest, RoundTripsAndReportsPeerCredentials) {
  TemporarySocketPath path;
  transport::UnixSocketListener listener(path.socket());
  ASSERT_TRUE(listener.listen());
  struct stat metadata{};
  ASSERT_EQ(::lstat(path.socket().c_str(), &metadata), 0);
  EXPECT_EQ(metadata.st_mode & 0777, static_cast<mode_t>(0660));

  auto accepted = std::async(std::launch::async,
                             [&listener] { return listener.accept(soon()); });
  transport::UnixSocketTransport client(path.socket());
  ASSERT_TRUE(client.connect(soon()));
  auto server_result = accepted.get();
  ASSERT_TRUE(server_result);
  auto& server = *server_result.transport;

  const auto client_peer = client.peerCredentials();
  const auto server_peer = server.peerCredentials();
  ASSERT_TRUE(client_peer);
  ASSERT_TRUE(server_peer);
  EXPECT_EQ(client_peer.credentials.pid, ::getpid());
  EXPECT_EQ(client_peer.credentials.uid, ::getuid());
  EXPECT_EQ(client_peer.credentials.gid, ::getgid());
  EXPECT_EQ(server_peer.credentials.pid, ::getpid());
  EXPECT_EQ(server_peer.credentials.uid, ::getuid());
  EXPECT_EQ(server_peer.credentials.gid, ::getgid());

  const std::array request{std::byte{'p'}, std::byte{'i'}, std::byte{'n'},
                           std::byte{'g'}};
  std::array<std::byte, request.size()> received{};
  ASSERT_TRUE(client.writeAll(request, soon()));
  ASSERT_TRUE(server.readExact(received, soon()));
  EXPECT_EQ(received, request);

  const std::array response{std::byte{'o'}, std::byte{'k'}};
  std::array<std::byte, response.size()> reply{};
  ASSERT_TRUE(server.writeAll(response, soon()));
  ASSERT_TRUE(client.readExact(reply, soon()));
  EXPECT_EQ(reply, response);
}

TEST(UnixSocketTransportTest, DistinguishesTimeoutAndEof) {
  TemporarySocketPath path;
  transport::UnixSocketListener listener(path.socket());
  ASSERT_TRUE(listener.listen());

  auto accepted = std::async(std::launch::async,
                             [&listener] { return listener.accept(soon()); });
  transport::UnixSocketTransport client(path.socket());
  ASSERT_TRUE(client.connect(soon()));
  auto server_result = accepted.get();
  ASSERT_TRUE(server_result);

  std::array<std::byte, 1> byte{};
  const auto timeout = server_result.transport->readExact(
      byte, std::chrono::steady_clock::now() + 20ms);
  EXPECT_EQ(timeout.code, transport::StatusCode::Timeout);

  client.close();
  const auto eof = server_result.transport->readExact(byte, soon());
  EXPECT_EQ(eof.code, transport::StatusCode::EndOfFile);
}

TEST(UnixSocketTransportTest, CloseCancelsPendingRead) {
  TemporarySocketPath path;
  transport::UnixSocketListener listener(path.socket());
  ASSERT_TRUE(listener.listen());

  auto accepted = std::async(std::launch::async,
                             [&listener] { return listener.accept(soon()); });
  transport::UnixSocketTransport client(path.socket());
  ASSERT_TRUE(client.connect(soon()));
  auto server_result = accepted.get();
  ASSERT_TRUE(server_result);

  std::promise<void> started;
  auto read = std::async(std::launch::async, [&] {
    std::array<std::byte, 1> byte{};
    started.set_value();
    return server_result.transport->readExact(byte, transport::noDeadline());
  });
  started.get_future().wait();
  server_result.transport->close();

  EXPECT_EQ(read.get().code, transport::StatusCode::Canceled);
}
