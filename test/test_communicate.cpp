#include <iostream>
#include <string>

int main() {
  try {
    // Define broadcast address (used by all nodes)
    std::string broadcast_url = "tls+tcp://127.0.0.1:5557";

    // Create two nodes with TLS
    ya::YaCommunicate::P2PNode node1("node1", "tls+tcp://127.0.0.1:5555",
                                     broadcast_url, "certs/server.crt",
                                     "certs/server.key");
    ya::YaCommunicate::P2PNode node2("node2", "tls+tcp://127.0.0.1:5556",
                                     broadcast_url, "certs/server.crt",
                                     "certs/server.key");

    // Subscribe to status updates
    node1.subscribe_status([](const ya::YaCommunicate::NodeStatus& status) {
      std::cout << "Node1 received status: " << status.to_string() << std::endl;
    });
    node2.subscribe_status([](const ya::YaCommunicate::NodeStatus& status) {
      std::cout << "Node2 received status: " << status.to_string() << std::endl;
    });

    // Start nodes
    node1.start();
    node2.start();

    // Wait for nodes to discover each other
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Query status from node1
    auto statuses = node1.query_all_status();
    std::cout << "Node statuses from node1:\n";
    for (const auto& status : statuses) {
      std::cout << status.to_string() << std::endl;
    }

    // Print known nodes from node2
    auto nodes = node2.get_known_nodes();
    std::cout << "Known nodes from node2:\n";
    for (const auto& url : nodes) {
      std::cout << url << std::endl;
    }

    // Run for a while to demonstrate broadcasting
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Stop nodes
    node1.stop();
    node2.stop();

  } catch (const ya::YaCommunicate::CommException& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }

  return 0;
}
