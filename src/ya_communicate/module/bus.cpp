#include "bus.h"

#include <nng/nng.h>
#include <nng/protocol/bus0/bus.h>

#include <cstring>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>

namespace ya::module {

class Bus::Impl {
 public:
  Impl(const std::string& address) : socket_(0) {
    if (!std::regex_match(
            address,
            std::regex("(tcp://[\\w\\d\\.:]+:\\d+|inproc://[\\w\\d]+)"))) {
      throw std::runtime_error("Invalid address format: " + address);
    }

    int rv;
    if ((rv = nng_bus0_open(&socket_)) != 0) {
      throw std::runtime_error("Failed to open bus socket: " +
                               std::string(nng_strerror(rv)));
    }
    if ((rv = nng_socket_set_ms(socket_, NNG_OPT_RECVTIMEO, 3000)) != 0) {
      nng_close(socket_);
      throw std::runtime_error("Failed to set receive timeout: " +
                               std::string(nng_strerror(rv)));
    }
    std::cout << "Bus node binding to " << address << std::endl;
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

  void send(const std::string& message) {
    nng_msg* msg;
    int rv;
    if ((rv = nng_msg_alloc(&msg, message.size())) != 0) {
      throw std::runtime_error("Failed to allocate message: " +
                               std::string(nng_strerror(rv)));
    }
    memcpy(nng_msg_body(msg), message.data(), message.size());
    std::cout << "Sending: " << message << std::endl;
    if ((rv = nng_sendmsg(socket_, msg, 0)) != 0) {
      nng_msg_free(msg);
      throw std::runtime_error("Failed to send message: " +
                               std::string(nng_strerror(rv)));
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

Bus::Bus(const std::string& address)
    : m_impl(std::make_unique<Impl>(address)) {}

Bus::~Bus() {}

void Bus::send(const std::string& message) { m_impl->send(message); }

std::string Bus::receive() { return m_impl->receive(); }

}  // namespace ya::module
