#include <gtest/gtest.h>

#include "ya_communicate/module/http.h"
#include "ya_communicate/module/pipeline.h"
#include "ya_communicate/module/requester.h"
#include "ya_communicate/module/responder.h"

std::string exec(const char* cmd) {
  std::array<char, 128> buffer;
  std::string result;

  std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    return "error";
  }

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

TEST(TestModuleHttp, Api) {
  ya::module::Http http;
  http.start(18000);
  // /status
  std::string res_status = exec("curl -s http://127.0.0.1:18000/status");
  std::cout << "/status: " << res_status << std::endl;
  // /nodes
  std::string res_nodes = exec("curl -s http://127.0.0.1:18000/nodes");
  std::cout << "/nodes: " << res_nodes << std::endl;
}

TEST(TestModuleReqRep, Communicate) {
  const std::string url = "tcp://127.0.0.1:19000";
  std::string server_received;
  std::string client_reply;

  std::thread server_thread([&]() {
    try {
      ya::module::Reponder server(url);
      server_received = server.receive();
      server.send("pong");
    } catch (const std::exception& ex) {
      FAIL() << "Server exception: " << ex.what();
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  try {
    ya::module::Requester client(url);
    client_reply = client.request("ping");
  } catch (const std::exception& ex) {
    FAIL() << "Client exception: " << ex.what();
  }

  server_thread.join();

  ASSERT_EQ(server_received, "ping");
  ASSERT_EQ(client_reply, "pong");
}

class PipelineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Use different ports for each test instance to avoid conflicts
    static int port = 5555;
    address = "tcp://127.0.0.1:" + std::to_string(port++);

    puller = std::make_unique<ya::module::Pipeline>(
        ya::module::Pipeline::ROLE::PULLER, address);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Ensure puller is listening
    pusher = std::make_unique<ya::module::Pipeline>(
        ya::module::Pipeline::ROLE::PUSHER, address);
  }

  void TearDown() override {
    pusher.reset();
    puller.reset();
  }

  std::string address;
  std::unique_ptr<ya::module::Pipeline> pusher;
  std::unique_ptr<ya::module::Pipeline> puller;
};

TEST_F(PipelineTest, SendReceiveMessage) {
  std::string test_message = "Hello, Pipeline!";
  std::thread receiver_thread([this, &test_message]() {
    std::string received = puller->receive();
    EXPECT_EQ(received, test_message);
  });

  pusher->send(test_message);
  receiver_thread.join();
}

TEST_F(PipelineTest, PullerCannotSend) {
  EXPECT_THROW(puller->send("Should fail"), std::runtime_error);
}

TEST_F(PipelineTest, PusherCannotReceive) {
  EXPECT_THROW(pusher->receive(), std::runtime_error);
}

TEST_F(PipelineTest, MultipleMessages) {
  std::vector<std::string> messages = {"Msg1", "Msg2", "Msg3"};
  std::vector<std::string> received_messages;

  std::thread receiver_thread([this, &received_messages]() {
    for (int i = 0; i < 3; ++i) {
      received_messages.push_back(puller->receive());
    }
  });

  for (const auto& msg : messages) {
    pusher->send(msg);
  }
  receiver_thread.join();

  ASSERT_EQ(received_messages.size(), messages.size());
  for (size_t i = 0; i < messages.size(); ++i) {
    EXPECT_EQ(received_messages[i], messages[i]);
  }
}

TEST_F(PipelineTest, DifferentAddresses) {
  std::string alt_address = "tcp://127.0.0.1:5566";
  auto alt_puller = std::make_unique<ya::module::Pipeline>(
      ya::module::Pipeline::ROLE::PULLER, alt_address);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  auto alt_pusher = std::make_unique<ya::module::Pipeline>(
      ya::module::Pipeline::ROLE::PUSHER, alt_address);

  std::string test_message = "Test different address";
  std::thread receiver_thread([&alt_puller, &test_message]() {
    std::string received = alt_puller->receive();
    EXPECT_EQ(received, test_message);
  });

  alt_pusher->send(test_message);
  receiver_thread.join();
}

TEST(PipelineNoConnectionTest, PusherFailsWithoutPuller) {
  EXPECT_THROW(ya::module::Pipeline(ya::module::Pipeline::ROLE::PUSHER,
                                    "tcp://127.0.0.1:5567"),
               std::runtime_error);
}

TEST(PipelineInvalidAddressTest, InvalidAddressThrows) {
  EXPECT_THROW(ya::module::Pipeline(ya::module::Pipeline::ROLE::PULLER,
                                    "invalid://address"),
               std::runtime_error);
  EXPECT_THROW(ya::module::Pipeline(ya::module::Pipeline::ROLE::PUSHER,
                                    "tcp://invalid:port"),
               std::runtime_error);
}
