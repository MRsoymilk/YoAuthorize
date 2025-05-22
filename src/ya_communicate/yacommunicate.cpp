#include "yacommunicate.h"

#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/reqrep0/req.h>
#include <nng/supplemental/http/http.h>
#include <nng/supplemental/tls/tls.h>

#include <cstring>
#include <map>
#include <mutex>
#include <sstream>
#include <stop_token>
#include <thread>

namespace ya {

YaCommunicate::Server::Server(const std::string& url)
    : m_socket(nullptr), m_listener(nullptr) {
  nng_socket s;
  nng_listener l;
  int rv;

  if ((rv = nng_rep0_open(&s)) != 0) {
    throw CommException("Failed to open server socket: " +
                        std::string(nng_strerror(rv)));
  }

  if ((rv = nng_listen(s, url.c_str(), &l, 0)) != 0) {
    nng_close(s);
    throw CommException("Failed to listen on " + url + ": " +
                        std::string(nng_strerror(rv)));
  }

  m_socket = new nng_socket(s);      // Store socket
  m_listener = new nng_listener(l);  // Store listener
}

YaCommunicate::Server::~Server() {
  if (m_listener) {
    nng_listener_close(*(nng_listener*)m_listener);
    delete (nng_listener*)m_listener;
  }
  if (m_socket) {
    nng_close(*(nng_socket*)m_socket);
    delete (nng_socket*)m_socket;
  }
}

std::string YaCommunicate::Server::receive() {
  char* buf = nullptr;
  size_t sz;
  int rv;

  if ((rv = nng_recv(*(nng_socket*)m_socket, &buf, &sz, NNG_FLAG_ALLOC)) != 0) {
    throw CommException("Failed to receive: " + std::string(nng_strerror(rv)));
  }

  std::string msg(buf, sz - 1);  // Exclude null terminator
  nng_free(buf, sz);             // Free buffer
  return msg;
}

void YaCommunicate::Server::send(const std::string& msg) {
  int rv;
  // Copy string to include null terminator
  char* buf = new char[msg.size() + 1];
  std::strcpy(buf, msg.c_str());

  if ((rv = nng_send(*(nng_socket*)m_socket, buf, msg.size() + 1,
                     NNG_FLAG_ALLOC)) != 0) {
    delete[] buf;  // Free if send fails
    throw CommException("Failed to send: " + std::string(nng_strerror(rv)));
  }
  // NNG_FLAG_ALLOC means NNG frees buf
}

YaCommunicate::Client::Client(const std::string& url)
    : m_socket(nullptr), m_dialer(nullptr) {
  nng_socket s;
  nng_dialer d;
  int rv;

  if ((rv = nng_req0_open(&s)) != 0) {
    throw CommException("Failed to open client socket: " +
                        std::string(nng_strerror(rv)));
  }

  if ((rv = nng_dial(s, url.c_str(), &d, 0)) != 0) {
    nng_close(s);
    throw CommException("Failed to dial " + url + ": " +
                        std::string(nng_strerror(rv)));
  }

  m_socket = new nng_socket(s);
  m_dialer = new nng_dialer(d);
}

YaCommunicate::Client::~Client() {
  if (m_dialer) {
    nng_dialer_close(*(nng_dialer*)m_dialer);
    delete (nng_dialer*)m_dialer;
  }
  if (m_socket) {
    nng_close(*(nng_socket*)m_socket);
    delete (nng_socket*)m_socket;
  }
}

std::string YaCommunicate::Client::request(const std::string& msg) {
  int rv;
  // Send request
  char* send_buf = new char[msg.size() + 1];
  std::strcpy(send_buf, msg.c_str());

  if ((rv = nng_send(*(nng_socket*)m_socket, send_buf, msg.size() + 1,
                     NNG_FLAG_ALLOC)) != 0) {
    delete[] send_buf;
    throw CommException("Failed to send request: " +
                        std::string(nng_strerror(rv)));
  }

  // Receive reply
  char* recv_buf = nullptr;
  size_t sz;
  if ((rv = nng_recv(*(nng_socket*)m_socket, &recv_buf, &sz, NNG_FLAG_ALLOC)) !=
      0) {
    throw CommException("Failed to receive reply: " +
                        std::string(nng_strerror(rv)));
  }

  std::string reply(recv_buf, sz - 1);
  nng_free(recv_buf, sz);
  return reply;
}

std::string YaCommunicate::NodeStatus::to_string() const {
  std::stringstream ss;
  ss << "Node(id=" << id << ", address=" << address << ", uptime=" << uptime
     << "s, info=" << info << ")";
  return ss.str();
}

class YaCommunicate::P2PNode::Impl {
 public:
  Impl(const std::string& id, const std::string& listen_url,
       const std::string& broadcast_url, const std::string& cert_file,
       const std::string& key_file)
      : m_id(id),
        m_listen_url(listen_url),
        m_broadcast_url(broadcast_url),
        m_running(false),
        m_start_time(std::chrono::steady_clock::now()) {
    // Initialize server socket (req/rep)
    if ((m_rv = nng_rep0_open(&m_server_socket)) != 0) {
      throw CommException("Failed to open server socket: " +
                          std::string(nng_strerror(m_rv)));
    }

    // Initialize publisher socket (pub/sub)
    if ((m_rv = nng_pub0_open(&m_pub_socket)) != 0) {
      throw CommException("Failed to open publisher socket: " +
                          std::string(nng_strerror(m_rv)));
    }

    // Initialize subscriber socket (pub/sub)
    if ((m_rv = nng_sub0_open(&m_sub_socket)) != 0) {
      throw CommException("Failed to open subscriber socket: " +
                          std::string(nng_strerror(m_rv)));
    }

    // Subscribe to all topics
    if ((m_rv = nng_socket_set(m_sub_socket, NNG_OPT_SUB_SUBSCRIBE, "", 0)) !=
        0) {
      throw CommException("Failed to set subscribe option: " +
                          std::string(nng_strerror(m_rv)));
    }

    // Configure TLS if cert_file and key_file are provided
    if (!cert_file.empty() && !key_file.empty()) {
      nng_tls_config* tls_config;
      if ((m_rv = nng_tls_config_alloc(&tls_config, NNG_TLS_MODE_SERVER)) !=
          0) {
        throw CommException("Failed to allocate TLS config: " +
                            std::string(nng_strerror(m_rv)));
      }

      if ((m_rv = nng_tls_config_own_cert(tls_config, cert_file.c_str(),
                                          key_file.c_str(), nullptr)) != 0) {
        nng_tls_config_free(tls_config);
        throw CommException("Failed to load TLS cert: " +
                            std::string(nng_strerror(m_rv)));
      }

      if ((m_rv = nng_socket_set_ptr(m_server_socket, NNG_OPT_TLS_CONFIG,
                                     tls_config)) != 0 ||
          (m_rv = nng_socket_set_ptr(m_pub_socket, NNG_OPT_TLS_CONFIG,
                                     tls_config)) != 0 ||
          (m_rv = nng_socket_set_ptr(m_sub_socket, NNG_OPT_TLS_CONFIG,
                                     tls_config)) != 0) {
        nng_tls_config_free(tls_config);
        throw CommException("Failed to set TLS config: " +
                            std::string(nng_strerror(m_rv)));
      }
    }
  }

  ~Impl() {
    stop();
    nng_close(m_server_socket);
    nng_close(m_pub_socket);
    nng_close(m_sub_socket);
  }

  void start() {
    if (m_running) {
      return;
    }

    m_running = true;

    // Start server (req/rep)
    if ((m_rv = nng_listen(m_server_socket, m_listen_url.c_str(),
                           &m_server_listener, 0)) != 0) {
      m_running = false;
      throw CommException("Failed to listen on " + m_listen_url + ": " +
                          std::string(nng_strerror(m_rv)));
    }

    // Start publisher (pub)
    if ((m_rv = nng_listen(m_pub_socket, m_broadcast_url.c_str(),
                           &m_pub_listener, 0)) != 0) {
      m_running = false;
      nng_listener_close(m_server_listener);
      throw CommException("Failed to listen on broadcast " + m_broadcast_url +
                          ": " + std::string(nng_strerror(m_rv)));
    }

    // Start subscriber (sub)
    if ((m_rv = nng_dial(m_sub_socket, m_broadcast_url.c_str(), &m_sub_dialer,
                         0)) != 0) {
      m_running = false;
      nng_listener_close(m_server_listener);
      nng_listener_close(m_pub_listener);
      throw CommException("Failed to dial broadcast " + m_broadcast_url + ": " +
                          std::string(nng_strerror(m_rv)));
    }

    m_server_thread = std::thread([this] { run_server(); });
    m_broadcast_thread = std::thread([this] { run_broadcaster(); });
    m_subscriber_thread = std::thread([this] { run_subscriber(); });
    m_node_manager_thread = std::thread([this] { run_node_manager(); });
  }

  void stop() {
    if (!m_running) {
      return;
    }

    m_running = false;
    nng_listener_close(m_server_listener);
    nng_listener_close(m_pub_listener);
    nng_dialer_close(m_sub_dialer);

    if (m_server_thread.joinable()) {
      m_server_thread.join();
    }
    if (m_broadcast_thread.joinable()) {
      m_broadcast_thread.join();
    }
    if (m_subscriber_thread.joinable()) {
      m_subscriber_thread.join();
    }
    if (m_node_manager_thread.joinable()) {
      m_node_manager_thread.join();
    }
    stop_http_server();
  }

  NodeStatus get_local_status() const {
    auto now = std::chrono::steady_clock::now();
    auto uptime =
        std::chrono::duration_cast<std::chrono::seconds>(now - m_start_time)
            .count();
    return NodeStatus{m_id, m_listen_url, uptime, "healthy"};
  }

  std::vector<NodeStatus> query_all_status() {
    std::vector<NodeStatus> statuses;
    std::lock_guard<std::mutex> lock(m_nodes_mutex);
    statuses.push_back(get_local_status());

    for (const auto& [url, status] : m_nodes) {
      try {
        nng_socket client_socket;
        nng_dialer dialer;
        if ((m_rv = nng_req0_open(&client_socket)) != 0) continue;

        if ((m_rv = nng_dial(client_socket, url.c_str(), &dialer, 0)) != 0) {
          nng_close(client_socket);
          continue;
        }

        const char* req = "STATUS";
        if ((m_rv = nng_send(client_socket, (void*)req, strlen(req) + 1, 0)) !=
            0) {
          nng_close(client_socket);
          continue;
        }

        char* buf = nullptr;
        size_t sz;
        if ((m_rv = nng_recv(client_socket, &buf, &sz, NNG_FLAG_ALLOC)) != 0) {
          nng_close(client_socket);
          continue;
        }

        std::string reply(buf, sz - 1);
        nng_free(buf, sz);
        nng_close(client_socket);

        std::stringstream ss(reply);
        std::string id, address, info;
        long uptime;
        std::getline(ss, id, '|');
        std::getline(ss, address, '|');
        ss >> uptime;
        ss.ignore(1);
        std::getline(ss, info);

        statuses.push_back(NodeStatus{id, address, uptime, info});
      } catch (...) {
        // Skip failed nodes
      }
    }

    return statuses;
  }

  void subscribe_status(std::function<void(const NodeStatus&)> callback) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_status_callback = callback;
  }

  std::vector<std::string> get_known_nodes() const {
    std::lock_guard<std::mutex> lock(m_nodes_mutex);
    std::vector<std::string> urls;
    for (const auto& [url, _] : m_nodes) {
      urls.push_back(url);
    }
    return urls;
  }

  void start_http_server(int port) {
    if (m_http_running) {
      throw CommException("HTTP server is already running");
    }

    std::string addr = "http://0.0.0.0:" + std::to_string(port);
    nng_url* url = nullptr;

    // Parse URL
    if (nng_url_parse(&url, addr.c_str()) != 0) {
      throw CommException("Failed to parse HTTP URL: " + addr);
    }

    // Allocate HTTP server
    if (nng_http_server_hold(&m_http_server, url) != 0) {
      nng_url_free(url);
      throw CommException("Failed to create HTTP server");
    }

    // Configure TLS if cert_file and key_file were provided
    if (!m_cert_file.empty() && !m_key_file.empty()) {
      nng_tls_config* tls_config;
      if ((m_rv = nng_tls_config_alloc(&tls_config, NNG_TLS_MODE_SERVER)) !=
          0) {
        nng_http_server_release(m_http_server);
        nng_url_free(url);
        throw CommException("Failed to allocate TLS config for HTTP");
      }
      if ((m_rv = nng_tls_config_own_cert(tls_config, m_cert_file.c_str(),
                                          m_key_file.c_str(), nullptr)) != 0) {
        nng_tls_config_free(tls_config);
        nng_http_server_release(m_http_server);
        nng_url_free(url);
        throw CommException("Failed to load TLS cert for HTTP");
      }
      if ((m_rv = nng_http_server_set_tls(m_http_server, tls_config)) != 0) {
        nng_tls_config_free(tls_config);
        nng_http_server_release(m_http_server);
        nng_url_free(url);
        throw CommException("Failed to set TLS config for HTTP");
      }
    }

    // Allocate handler for /status
    if (nng_http_handler_alloc(&m_http_handler_status, "/status",
                               handle_status_request) != 0) {
      nng_http_server_release(m_http_server);
      nng_url_free(url);
      throw CommException("Failed to create HTTP status handler");
    }
    nng_http_handler_set_method(m_http_handler_status, "GET");
    nng_http_handler_set_data(m_http_handler_status, this, nullptr);
    if (nng_http_server_add_handler(m_http_server, m_http_handler_status) !=
        0) {
      nng_http_handler_free(m_http_handler_status);
      nng_http_server_release(m_http_server);
      nng_url_free(url);
      throw CommException("Failed to add HTTP status handler");
    }

    // Allocate handler for /nodes
    if (nng_http_handler_alloc(&m_http_handler_nodes, "/nodes",
                               handle_nodes_request) != 0) {
      nng_http_handler_free(m_http_handler_status);
      nng_http_server_release(m_http_server);
      nng_url_free(url);
      throw CommException("Failed to create HTTP nodes handler");
    }
    nng_http_handler_set_method(m_http_handler_nodes, "GET");
    nng_http_handler_set_data(m_http_handler_nodes, this, nullptr);
    if (nng_http_server_add_handler(m_http_server, m_http_handler_nodes) != 0) {
      nng_http_handler_free(m_http_handler_status);
      nng_http_handler_free(m_http_handler_nodes);
      nng_http_server_release(m_http_server);
      nng_url_free(url);
      throw CommException("Failed to add HTTP nodes handler");
    }

    // Start the server
    if (nng_http_server_start(m_http_server) != 0) {
      nng_http_handler_free(m_http_handler_status);
      nng_http_handler_free(m_http_handler_nodes);
      nng_http_server_release(m_http_server);
      nng_url_free(url);
      throw CommException("Failed to start HTTP server");
    }

    nng_url_free(url);
  }

  void stop_http_server() {
    if (!m_http_running) return;

    m_http_running = false;
    if (m_http_server) {
      nng_http_server_stop(m_http_server);
      nng_http_server_release(m_http_server);
    }
    if (m_http_handler_status) {
      nng_http_handler_free(m_http_handler_status);
    }
    if (m_http_handler_nodes) {
      nng_http_handler_free(m_http_handler_nodes);
    }
    m_http_server = nullptr;
    m_http_handler_status = nullptr;
    m_http_handler_nodes = nullptr;

    if (m_http_thread.joinable()) {
      m_http_thread.join();
    }
  }

 private:
  static void handle_status_request(nng_aio* aio) {
    auto* req = static_cast<nng_http_req*>(nng_aio_get_input(aio, 0));
    nng_http_res* res = nullptr;
    if (nng_http_res_alloc(&res) != 0) {
      nng_aio_finish(aio, NNG_ENOMEM);
      return;
    }

    const char* body = "{\"status\": \"ok\"}";
    nng_http_res_set_status(res, NNG_HTTP_STATUS_OK);
    nng_http_res_set_header(res, "Content-Type", "application/json");
    nng_http_res_copy_data(res, body, strlen(body));
    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);
  }

  static void handle_nodes_request(nng_aio* aio) {
    auto* req = static_cast<nng_http_req*>(nng_aio_get_input(aio, 0));
    nng_http_res* res = nullptr;
    if (nng_http_res_alloc(&res) != 0) {
      nng_aio_finish(aio, NNG_ENOMEM);
      return;
    }
    std::string response = "{}";
    nng_http_res_set_status(res, NNG_HTTP_STATUS_OK);
    nng_http_res_set_header(res, "Content-Type", "application/json");
    nng_http_res_copy_data(res, response.c_str(), response.size());
    nng_aio_set_output(aio, 0, res);
    nng_aio_finish(aio, 0);
  }

  void run_server() {
    while (m_running) {
      char* buf = nullptr;
      size_t sz;
      if ((m_rv = nng_recv(m_server_socket, &buf, &sz, NNG_FLAG_ALLOC)) != 0) {
        if (m_running) {
          fprintf(stderr, "Server receive failed: %s\n", nng_strerror(m_rv));
        }
        continue;
      }

      std::string req(buf, sz - 1);
      nng_free(buf, sz);

      if (req == "STATUS") {
        auto status = get_local_status();
        std::stringstream ss;
        ss << status.id << "|" << status.address << "|" << status.uptime << "|"
           << status.info;
        std::string reply = ss.str();

        if ((m_rv = nng_send(m_server_socket, (void*)reply.c_str(),
                             reply.size() + 1, 0)) != 0) {
          fprintf(stderr, "Server send failed: %s\n", nng_strerror(m_rv));
        }
      }
    }
  }

  void run_broadcaster() {
    while (m_running) {
      auto status = get_local_status();
      std::stringstream ss;
      ss << "NODE|" << status.id << "|" << status.address;
      std::string msg = ss.str();

      if ((m_rv = nng_send(m_pub_socket, (void*)msg.c_str(), msg.size() + 1,
                           0)) != 0) {
        fprintf(stderr, "Broadcast failed: %s\n", nng_strerror(m_rv));
      }

      // Broadcast status every 5 seconds
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  }

  void run_subscriber() {
    while (m_running) {
      char* buf = nullptr;
      size_t sz;
      if ((m_rv = nng_recv(m_sub_socket, &buf, &sz, NNG_FLAG_ALLOC)) != 0) {
        if (m_running) {
          fprintf(stderr, "Subscriber receive failed: %s\n",
                  nng_strerror(m_rv));
        }
        continue;
      }

      std::string msg(buf, sz - 1);
      nng_free(buf, sz);

      // Parse message (format: type|id|address or
      // STATUS|id|address|uptime|info)
      std::stringstream ss(msg);
      std::string type, id, address;
      std::getline(ss, type, '|');
      std::getline(ss, id, '|');
      std::getline(ss, address, '|');

      if (type == "NODE") {
        // Node discovery: add to nodes list
        std::lock_guard<std::mutex> lock(m_nodes_mutex);
        if (id != m_id && address != m_listen_url) {
          m_nodes[address] = NodeStatus{id, address, 0, "discovered"};
        }
      } else if (type == "STATUS") {
        long uptime;
        std::string info;
        ss >> uptime;
        ss.ignore(1);
        std::getline(ss, info);

        std::lock_guard<std::mutex> lock(m_nodes_mutex);
        m_nodes[address] = NodeStatus{id, address, uptime, info};

        std::lock_guard<std::mutex> cb_lock(m_callback_mutex);
        if (m_status_callback) {
          m_status_callback(NodeStatus{id, address, uptime, info});
        }
      }
    }
  }

  void run_node_manager() {
    while (m_running) {
      std::lock_guard<std::mutex> lock(m_nodes_mutex);
      auto nodes_copy = m_nodes;
      for (auto it = nodes_copy.begin(); it != nodes_copy.end(); ++it) {
        try {
          nng_socket client_socket;
          nng_dialer dialer;
          if ((m_rv = nng_req0_open(&client_socket)) != 0) continue;

          if ((m_rv = nng_dial(client_socket, it->first.c_str(), &dialer, 0)) !=
              0) {
            m_nodes.erase(it->first);  // Remove unreachable node
            nng_close(client_socket);
            continue;
          }

          const char* req = "STATUS";
          if ((m_rv = nng_send(client_socket, (void*)req, strlen(req) + 1,
                               0)) != 0) {
            m_nodes.erase(it->first);
            nng_close(client_socket);
            continue;
          }

          char* buf = nullptr;
          size_t sz;
          if ((m_rv = nng_recv(client_socket, &buf, &sz, NNG_FLAG_ALLOC)) !=
              0) {
            m_nodes.erase(it->first);
            nng_close(client_socket);
            continue;
          }

          nng_free(buf, sz);
          nng_close(client_socket);
        } catch (...) {
          m_nodes.erase(it->first);
        }
      }

      // Check nodes every 10 seconds
      std::this_thread::sleep_for(std::chrono::seconds(10));
    }
  }

  std::string m_id;
  std::string m_listen_url;
  std::string m_broadcast_url;
  nng_socket m_server_socket;
  nng_listener m_server_listener;
  nng_socket m_pub_socket;
  nng_listener m_pub_listener;
  nng_socket m_sub_socket;
  nng_dialer m_sub_dialer;
  std::thread m_server_thread;
  std::thread m_broadcast_thread;
  std::thread m_subscriber_thread;
  std::thread m_node_manager_thread;
  std::map<std::string, NodeStatus> m_nodes;
  mutable std::mutex m_nodes_mutex;
  std::function<void(const NodeStatus&)> m_status_callback;
  mutable std::mutex m_callback_mutex;
  std::atomic<bool> m_running;
  std::chrono::steady_clock::time_point m_start_time;
  int m_rv;

  std::string m_cert_file;
  std::string m_key_file;
  nng_http_server* m_http_server;
  nng_http_handler* m_http_handler_status;
  nng_http_handler* m_http_handler_nodes;
  std::thread m_http_thread;
  std::atomic<bool> m_http_running;
};

YaCommunicate::P2PNode::P2PNode(const std::string& id,
                                const std::string& listen_url,
                                const std::string& broadcast_url,
                                const std::string& cert_file,
                                const std::string& key_file)
    : m_impl(std::make_unique<Impl>(id, listen_url, broadcast_url, cert_file,
                                    key_file)) {}

YaCommunicate::P2PNode::~P2PNode() = default;

void YaCommunicate::P2PNode::start() { m_impl->start(); }
void YaCommunicate::P2PNode::stop() { m_impl->stop(); }
std::vector<YaCommunicate::NodeStatus>
YaCommunicate::P2PNode::query_all_status() {
  return m_impl->query_all_status();
}
YaCommunicate::NodeStatus YaCommunicate::P2PNode::get_local_status() const {
  return m_impl->get_local_status();
}
void YaCommunicate::P2PNode::subscribe_status(
    std::function<void(const NodeStatus&)> callback) {
  m_impl->subscribe_status(callback);
}
std::vector<std::string> YaCommunicate::P2PNode::get_known_nodes() const {
  return m_impl->get_known_nodes();
}

void YaCommunicate::P2PNode::start_http_server(int port) {
  m_impl->start_http_server(port);
}

void YaCommunicate::P2PNode::stop_http_server() { m_impl->stop_http_server(); }

}  // namespace ya
