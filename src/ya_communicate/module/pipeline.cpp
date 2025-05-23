#include "pipeline.h"

#include <nng/nng.h>
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

#include <cstring>
#include <regex>
#include <stdexcept>
#include <string>

namespace ya::module {

class Pipeline::Impl {
 public:
  Impl(ROLE role, const std::string& address) : role_(role), socket_(0) {
    // Basic address validation (e.g., tcp://host:port)
    if (!std::regex_match(address, std::regex("tcp://[\\w\\d\\.:]+:\\d+"))) {
      throw std::runtime_error("Invalid address format: " + address);
    }

    int rv;
    if (role == ROLE::PUSHER) {
      if ((rv = nng_push0_open(&socket_)) != 0) {
        throw std::runtime_error("Failed to open push socket: " +
                                 std::string(nng_strerror(rv)));
      }
      if ((rv = nng_dial(socket_, address.c_str(), nullptr, 0)) != 0) {
        nng_close(socket_);
        throw std::runtime_error("Failed to dial " + address + ": " +
                                 std::string(nng_strerror(rv)));
      }
    } else {  // PULLER
      if ((rv = nng_pull0_open(&socket_)) != 0) {
        throw std::runtime_error("Failed to open pull socket: " +
                                 std::string(nng_strerror(rv)));
      }
      if ((rv = nng_listen(socket_, address.c_str(), nullptr, 0)) != 0) {
        nng_close(socket_);
        throw std::runtime_error("Failed to listen on " + address + ": " +
                                 std::string(nng_strerror(rv)));
      }
    }
  }

  ~Impl() {
    if (socket_.id != 0) {
      nng_close(socket_);
    }
  }

  void send(const std::string& message) {
    if (role_ != ROLE::PUSHER) {
      throw std::runtime_error("Send operation only allowed for PUSHER role");
    }
    nng_msg* msg;
    int rv;
    if ((rv = nng_msg_alloc(&msg, message.size())) != 0) {
      throw std::runtime_error("Failed to allocate message: " +
                               std::string(nng_strerror(rv)));
    }
    memcpy(nng_msg_body(msg), message.data(), message.size());
    if ((rv = nng_sendmsg(socket_, msg, 0)) != 0) {
      nng_msg_free(msg);
      throw std::runtime_error("Failed to send message: " +
                               std::string(nng_strerror(rv)));
    }
  }

  std::string receive() {
    if (role_ != ROLE::PULLER) {
      throw std::runtime_error(
          "Receive operation only allowed for PULLER role");
    }
    nng_msg* msg;
    int rv;
    if ((rv = nng_recvmsg(socket_, &msg, 0)) != 0) {
      throw std::runtime_error("Failed to receive message: " +
                               std::string(nng_strerror(rv)));
    }
    std::string result(static_cast<char*>(nng_msg_body(msg)), nng_msg_len(msg));
    nng_msg_free(msg);
    return result;
  }

 private:
  ROLE role_;
  nng_socket socket_;
};

Pipeline::Pipeline(ROLE role, const std::string& address)
    : m_impl(std::make_unique<Impl>(role, address)) {}

Pipeline::~Pipeline() {}

void Pipeline::send(const std::string& message) { m_impl->send(message); }

std::string Pipeline::receive() { return m_impl->receive(); }

}  // namespace ya::module
