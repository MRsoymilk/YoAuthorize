#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "ya_communicate/module/publisher.h"
#include "ya_communicate/module/subscriber.h"

namespace ya::module {

class PubSubTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static int port = 18000;
    address_ = "tcp://127.0.0.1:" + std::to_string(port++);
    std::cout << "Setting up test with address: " << address_ << std::endl;

    publisher_ = std::make_unique<Publisher>(address_);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(500));  // Increased delay
    subscriber1_ = std::make_unique<Subscriber>(address_);
    subscriber2_ = std::make_unique<Subscriber>(address_);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(500));  // Additional delay for subscribers
  }

  void TearDown() override {
    subscriber1_.reset();
    subscriber2_.reset();
    publisher_.reset();
    std::cout << "Tearing down test" << std::endl;
  }

  std::string address_;
  std::unique_ptr<Publisher> publisher_;
  std::unique_ptr<Subscriber> subscriber1_;
  std::unique_ptr<Subscriber> subscriber2_;
};

TEST_F(PubSubTest, PublishAndReceive) {
  std::string topic = "news";
  std::string message = "Breaking news!";
  std::string expected = topic + ":" + message;
  std::cout << "Starting PublishAndReceive test" << std::endl;

  subscriber1_->subscribe(topic);
  std::thread sub_thread([this, &expected]() {
    std::string received = subscriber1_->receive();
    EXPECT_EQ(received, expected);
  });

  std::this_thread::sleep_for(
      std::chrono::milliseconds(100));  // Ensure subscriber is ready
  std::cout << "Publishing message: " << expected << std::endl;
  publisher_->publish(topic, message);
  sub_thread.join();
}

TEST_F(PubSubTest, MultipleSubscribers) {
  std::string topic = "weather";
  std::string message = "Sunny today";
  std::string expected = topic + ":" + message;
  std::vector<std::string> received_messages(2);

  subscriber1_->subscribe(topic);
  subscriber2_->subscribe(topic);
  std::thread sub1_thread([this, &received_messages]() {
    received_messages[0] = subscriber1_->receive();
  });
  std::thread sub2_thread([this, &received_messages]() {
    received_messages[1] = subscriber2_->receive();
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  publisher_->publish(topic, message);
  sub1_thread.join();
  sub2_thread.join();

  EXPECT_EQ(received_messages[0], expected);
  EXPECT_EQ(received_messages[1], expected);
}

TEST_F(PubSubTest, TopicFiltering) {
  std::string topic1 = "sports";
  std::string topic2 = "news";
  std::string message = "Update";
  std::string expected = topic1 + ":" + message;

  subscriber1_->subscribe(topic1);
  subscriber2_->subscribe(topic2);
  std::thread sub1_thread([this, &expected]() {
    std::string received = subscriber1_->receive();
    EXPECT_EQ(received, expected);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  publisher_->publish(topic1, message);
  publisher_->publish(topic2, message);
  sub1_thread.join();

  std::thread sub2_thread([this, &topic2, &message]() {
    std::string expected2 = topic2 + ":" + message;
    std::string received = subscriber2_->receive();
    EXPECT_EQ(received, expected2);
  });
  publisher_->publish(topic2, message);
  sub2_thread.join();
}

TEST_F(PubSubTest, ReceiveTimeout) {
  subscriber1_->subscribe("nonexistent");
  EXPECT_THROW(subscriber1_->receive(), std::runtime_error);
}

TEST(PubSubInvalidAddressTest, InvalidAddressThrows) {
  EXPECT_THROW(Publisher("invalid://address"), std::runtime_error);
  EXPECT_THROW(Subscriber("tcp://invalid:port"), std::runtime_error);
}

TEST_F(PubSubTest, NoSubscribers) {
  EXPECT_NO_THROW(publisher_->publish("test", "No subscribers"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

}  // namespace ya::module
