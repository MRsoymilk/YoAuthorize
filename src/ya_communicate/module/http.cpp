#include "http.h"

#include <nng/nng.h>
#include <nng/supplemental/http/http.h>
#include <nng/supplemental/tls/tls.h>

#include <atomic>
#include <cstring>
#include <string>

#include "node_def.h"

namespace ya::module {

class Http::Impl {
 public:
  Impl() {}
  ~Impl() {}

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

  void start(int port) {
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

  void stop() {
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
  }

 private:
  nng_http_server* m_http_server;
  nng_http_handler* m_http_handler_status;
  nng_http_handler* m_http_handler_nodes;
  std::atomic<bool> m_http_running;
  std::string m_cert_file;
  std::string m_key_file;
  int m_rv;
};

Http::Http() { m_impl = std::make_unique<Impl>(); }

Http::~Http() {}

void Http::start(int port) { m_impl->start(port); }

void Http::stop() { m_impl->stop(); }

}  // namespace ya::module
