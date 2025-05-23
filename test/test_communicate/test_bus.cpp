#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ya_communicate/module/bus.h"

namespace ya::module {

class BusTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static int id = 0;
    address_ = "inproc://bus" + std::to_string(id++);
    std::cout << "Setting up test with address: " << address_ << std::endl;

    node1_ = std::make_unique<Bus>(address_);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    node2_ = std::make_unique<Bus>(address_);
    node3_ = std::make_unique<Bus>(address_);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  void TearDown() override {
    node1_.reset();
    node2_.reset();
    node3_.reset();
    std::cout << "Tearing down test" << std::endl;
  }

  std::string address_;
  std::unique_ptr<Bus> node1_;
  std::unique_ptr<Bus> node2_;
  std::unique_ptr<Bus> node3_;
};

TEST_F(BusTest, SendAndReceive) {
  std::string message = "Hello, Bus!";
  std::cout << "Starting SendAndReceive test" << std::endl;

  std::thread receive_thread([this, &message]() {
    std::string received = node2_->receive();
    EXPECT_EQ(received, message);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "Sending message: " << message << std::endl;
  node1_->send(message);
  receive_thread.join();
}

TEST_F(BusTest, MultipleNodesReceive) {
  std::string message = "Broadcast test";
  std::vector<std::string> received_messages(2);

  std::thread node2_thread([this, &received_messages]() {
    received_messages[0] = node2_->receive();
  });
  std::thread node3_thread([this, &received_messages]() {
    received_messages[1] = node3_->receive();
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  node1_->send(message);
  node2_thread.join();
  node3_thread.join();

  EXPECT_EQ(received_messages[0], message);
  EXPECT_EQ(received_messages[1], message);
}

TEST_F(BusTest, ReceiveTimeout) {
  EXPECT_THROW(node1_->receive(), std::runtime_error);
}

TEST(BusInvalidAddressTest, InvalidAddressThrows) {
  EXPECT_THROW(Bus("invalid://address"), std::runtime_error);
}

TEST_F(BusTest, TcpSendAndReceive) {
  address_ = "tcp://127.0.0.1:18000";
  node1_ = std::make_unique<Bus>(address_);
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  node2_ = std::make_unique<Bus>(address_);
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::string message = "TCP bus test";

  std::thread receive_thread([this, &message]() {
    std::string received = node2_->receive();
    EXPECT_EQ(received, message);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  node1_->send(message);
  receive_thread.join();
}

TEST_F(BusTest, SimpleSendAndReceive) {
  std::string message = "Simple bus test";
  node1_->send(message);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::string received = node2_->receive();
  EXPECT_EQ(received, message);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

}  // namespace ya::module
