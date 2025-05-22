#ifndef YA_COMMUNICATE_H
#define YA_COMMUNICATE_H

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ya {

class YaCommunicate {
public:
    // Exception class for communication errors
    class CommException : public std::runtime_error {
    public:
        CommException(const std::string& msg) : std::runtime_error(msg) {}
    };

    // Node status structure
    struct NodeStatus {
      std::string id;       // Unique node identifier
      std::string address;  // URL (e.g., tls+tcp://127.0.0.1:5555)
      long long uptime;          // Seconds since node started
      std::string info;     // Additional info (e.g., load, version)

      std::string to_string() const;
    };

    // P2P node class
    class P2PNode {
     public:
      // Constructor with TLS support
      P2PNode(const std::string& id, const std::string& listen_url,
              const std::string& broadcast_url,
              const std::string& cert_file = "",
              const std::string& key_file = "");
      ~P2PNode();

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
      P2PNode(const P2PNode&) = delete;
      P2PNode& operator=(const P2PNode&) = delete;

     private:
      class Impl;
      std::unique_ptr<Impl> m_impl;
    };

    // Server class for req/rep protocol
    class Server {
    public:
        Server(const std::string& url);
        ~Server();

        // Receive a message (blocking)
        std::string receive();

        // Send a reply to the last received message
        void send(const std::string& msg);

        // Prevent copying
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

    private:
     void* m_socket;    // Opaque pointer to NNG socket
     void* m_listener;  // Opaque pointer to NNG listener
    };

    // Client class for req/rep protocol
    class Client {
    public:
        Client(const std::string& url);
        ~Client();

        // Send a request and receive a reply (blocking)
        std::string request(const std::string& msg);

        // Prevent copying
        Client(const Server&) = delete;
        Client& operator=(const Server&) = delete;

    private:
     void* m_socket;  // Opaque pointer to NNG socket
     void* m_dialer;  // Opaque pointer to NNG dialer
    };
};

} // namespace ya

#endif  // !YA_COMMUNICATE_H
