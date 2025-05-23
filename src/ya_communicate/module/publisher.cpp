#include "publisher.h"

#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>

#include <cstring>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>

namespace ya::module {

class Publisher::Impl {
 public:
  Impl(const std::string& address) : socket_(0) {
    // Validate address: tcp://host:port or inproc://name
    if (!std::regex_match(
            address,
            std::regex("(tcp://[\\w\\d\\.:]+:\\d+|inproc://[\\w\\d\\.:]+)"))) {
      throw std::runtime_error("Invalid address format: " + address);
    }

    int rv;
    if ((rv = nng_pub0_open(&socket_)) != 0) {
      throw std::runtime_error("Failed to open publisher socket: " +
                               std::string(nng_strerror(rv)));
    }
    std::cout << "Publisher binding to " << address << std::endl;
    if ((rv = nng_listen(socket_, address.c_str(), nullptr, 0)) != 0) {
      nng_close(socket_);
      throw std::runtime_error("Failed to listen on " + address + ": " +
                               std::string(nng_strerror(rv)));
    }
  }

  ~Impl() {
    if (socket_.id != 0) {
      nng_close(socket_);
    }
  }

  void publish(const std::string& topic, const std::string& message) {
    std::string full_message = topic.empty() ? message : topic + ":" + message;
    nng_msg* msg;
    int rv;
    if ((rv = nng_msg_alloc(&msg, full_message.size())) != 0) {
      throw std::runtime_error("Failed to allocate message: " +
                               std::string(nng_strerror(rv)));
    }
    memcpy(nng_msg_body(msg), full_message.data(), full_message.size());
    std::cout << "Publishing: " << full_message << std::endl;
    if ((rv = nng_sendmsg(socket_, msg, 0)) != 0) {
      nng_msg_free(msg);
      throw std::runtime_error("Failed to publish message: " +
                               std::string(nng_strerror(rv)));
    }
  }

 private:
  nng_socket socket_;
};

Publisher::Publisher(const std::string& address)
    : m_impl(std::make_unique<Impl>(address)) {}

Publisher::~Publisher() {}

void Publisher::publish(const std::string& topic, const std::string& message) {
  m_impl->publish(topic, message);
}

}  // namespace ya::module
