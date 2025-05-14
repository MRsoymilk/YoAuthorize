#include <gtest/gtest.h>

#include <iostream>
#include <string>
#include <thread>

#include "yacommunicate.h"

TEST(YaCommunicateTest, TLSCommunicationTest) {
  try {
    std::string broadcast_url = "tls+tcp://127.0.0.1:5557";

    ya::YaCommunicate::P2PNode node1("node1", "tls+tcp://127.0.0.1:5555",
                                     broadcast_url, "certs/server.crt",
                                     "certs/server.key");
    ya::YaCommunicate::P2PNode node2("node2", "tls+tcp://127.0.0.1:5556",
                                     broadcast_url, "certs/server.crt",
                                     "certs/server.key");

    node1.subscribe_status([](const ya::YaCommunicate::NodeStatus& status) {
      std::cout << "[node1] received: " << status.to_string() << std::endl;
    });
    node2.subscribe_status([](const ya::YaCommunicate::NodeStatus& status) {
      std::cout << "[node2] received: " << status.to_string() << std::endl;
    });

    node1.start();
    node2.start();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto statuses = node1.query_all_status();
    EXPECT_FALSE(statuses.empty())
        << "Node1 should see at least one other node";

    for (const auto& s : statuses)
      std::cout << "[node1] status: " << s.to_string() << std::endl;

    auto nodes = node2.get_known_nodes();
    EXPECT_FALSE(nodes.empty()) << "Node2 should know at least one node";

    for (const auto& n : nodes)
      std::cout << "[node2] knows: " << n << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(5));

    node1.stop();
    node2.stop();

  } catch (const ya::YaCommunicate::CommException& e) {
    FAIL() << "Exception caught: " << e.what();
  }
}
