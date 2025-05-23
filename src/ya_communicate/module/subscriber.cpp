#include "subscriber.h"

#include <nng/nng.h>
#include <nng/protocol/pubsub0/sub.h>

#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>

namespace ya::module {

class Subscriber::Impl {
 public:
  Impl(const std::string& address) : socket_(0) {
    // Validate address: tcp://host:port or inproc://name
    if (!std::regex_match(
            address,
            std::regex("(tcp://[\\w\\d\\.:]+:\\d+|inproc://[\\w\\d\\.:]+)"))) {
      throw std::runtime_error("Invalid address format: " + address);
    }

    int rv;
    if ((rv = nng_sub0_open(&socket_)) != 0) {
      throw std::runtime_error("Failed to open subscriber socket: " +
                               std::string(nng_strerror(rv)));
    }
    if ((rv = nng_socket_set_ms(socket_, NNG_OPT_RECVTIMEO, 3000)) != 0) {
      nng_close(socket_);
      throw std::runtime_error("Failed to set receive timeout: " +
                               std::string(nng_strerror(rv)));
    }
    std::cout << "Subscriber connecting to " << address << std::endl;
    if ((rv = nng_dial(socket_, address.c_str(), nullptr, 0)) != 0) {
      nng_close(socket_);
      throw std::runtime_error("Failed to dial " + address + ": " +
                               std::string(nng_strerror(rv)));
    }
  }

  ~Impl() {
    if (socket_.id != 0) {
      nng_close(socket_);
    }
  }

  void subscribe(const std::string& topic) {
    int rv;
    std::cout << "Subscribing to topic: " << topic << std::endl;
    if ((rv = nng_socket_set_string(socket_, NNG_OPT_SUB_SUBSCRIBE,
                                    topic.c_str())) != 0) {
      throw std::runtime_error("Failed to subscribe to topic '" + topic +
                               "': " + std::string(nng_strerror(rv)));
    }
  }

  std::string receive() {
    const int max_retries = 5;
    for (int retry = 0; retry < max_retries; ++retry) {
      nng_msg* msg;
      int rv;
      std::cout << "Attempting to receive message (retry " << retry + 1 << ")"
                << std::endl;
      if ((rv = nng_recvmsg(socket_, &msg, 0)) == 0) {
        std::string result(static_cast<char*>(nng_msg_body(msg)),
                           nng_msg_len(msg));
        nng_msg_free(msg);
        std::cout << "Received message: " << result << std::endl;
        return result;
      }
      if (rv == NNG_ETIMEDOUT) {
        std::cout << "Receive timed out, retrying..." << std::endl;
        continue;
      }
      throw std::runtime_error("Failed to receive message: " +
                               std::string(nng_strerror(rv)));
    }
    throw std::runtime_error("Receive timed out after " +
                             std::to_string(max_retries) + " retries");
  }

 private:
  nng_socket socket_;
};

Subscriber::Subscriber(const std::string& address)
    : m_impl(std::make_unique<Impl>(address)) {}

Subscriber::~Subscriber() {}

void Subscriber::subscribe(const std::string& topic) {
  m_impl->subscribe(topic);
}

std::string Subscriber::receive() { return m_impl->receive(); }

}  // namespace ya::module
