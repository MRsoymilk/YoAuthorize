#ifndef P2P_H
#define P2P_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "node_def.h"

namespace ya::arch {

class P2P {
 public:
  // Constructor with TLS support
  P2P(const std::string& id, const std::string& listen_url,
      const std::string& broadcast_url, const std::string& cert_file = "",
      const std::string& key_file = "");
  ~P2P();

  // Start the node (server, broadcaster, and subscriber)
  void start();

  // Stop the node
  void stop();

  // Query status of all known nodes (via req/rep)
  std::vector<NodeStatus> query_all_status();

  // Get local node status
  NodeStatus get_local_status() const;

  // Subscribe to status updates (callback receives new status)
  void subscribe_status(std::function<void(const NodeStatus&)> callback);

  // Get current known nodes
  std::vector<std::string> get_known_nodes() const;

  // Start HTTP server on specified port
  void start_http_server(int port);

  // Stop HTTP server
  void stop_http_server();
  // Prevent copying
  P2P(const P2P&) = delete;
  P2P& operator=(const P2P&) = delete;

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace ya::arch

#endif
