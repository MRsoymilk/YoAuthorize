#include <gtest/gtest.h>

#include "ya_communicate/module/pipeline.h"

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
