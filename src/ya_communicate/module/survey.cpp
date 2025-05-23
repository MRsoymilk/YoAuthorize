#include "survey.h"

#include <nng/nng.h>
#include <nng/protocol/survey0/respond.h>
#include <nng/protocol/survey0/survey.h>

#include <cstring>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace ya::module {

class Survey::Impl {
 public:
  Impl(ROLE role, const std::string& address) : role_(role), socket_(0) {
    // Validate address format (e.g., tcp://host:port)
    if (!std::regex_match(address, std::regex("tcp://[\\w\\d\\.:]+:\\d+"))) {
      throw std::runtime_error("Invalid address format: " + address);
    }

    int rv;
    if (role == ROLE::INITIATOR) {
      if ((rv = nng_surveyor0_open(&socket_)) != 0) {
        throw std::runtime_error("Failed to open surveyor socket: " +
                                 std::string(nng_strerror(rv)));
      }
      // Set survey deadline (1 second)
      if ((rv = nng_socket_set_ms(socket_, NNG_OPT_SURVEYOR_SURVEYTIME,
                                  1000)) != 0) {
        nng_close(socket_);
        throw std::runtime_error("Failed to set survey deadline: " +
                                 std::string(nng_strerror(rv)));
      }
      // Bind to address
      if ((rv = nng_listen(socket_, address.c_str(), nullptr, 0)) != 0) {
        nng_close(socket_);
        throw std::runtime_error("Failed to listen on " + address + ": " +
                                 std::string(nng_strerror(rv)));
      }
    } else {  // VOTER
      if ((rv = nng_respondent0_open(&socket_)) != 0) {
        throw std::runtime_error("Failed to open respondent socket: " +
                                 std::string(nng_strerror(rv)));
      }
      // Connect to initiator
      if ((rv = nng_dial(socket_, address.c_str(), nullptr, 0)) != 0) {
        nng_close(socket_);
        throw std::runtime_error("Failed to dial " + address + ": " +
                                 std::string(nng_strerror(rv)));
      }
    }
  }

  ~Impl() {
    if (socket_.id != 0) {
      nng_close(socket_);
    }
  }

  void send_survey(const std::string& survey) {
    if (role_ != ROLE::INITIATOR) {
      throw std::runtime_error("Send survey only allowed for INITIATOR role");
    }
    nng_msg* msg;
    int rv;
    if ((rv = nng_msg_alloc(&msg, survey.size())) != 0) {
      throw std::runtime_error("Failed to allocate survey message: " +
                               std::string(nng_strerror(rv)));
    }
    memcpy(nng_msg_body(msg), survey.data(), survey.size());
    if ((rv = nng_sendmsg(socket_, msg, 0)) != 0) {
      nng_msg_free(msg);
      throw std::runtime_error("Failed to send survey: " +
                               std::string(nng_strerror(rv)));
    }
  }

  std::vector<std::string> collect_responses() {
    if (role_ != ROLE::INITIATOR) {
      throw std::runtime_error(
          "Collect responses only allowed for INITIATOR role");
    }
    std::vector<std::string> responses;
    while (true) {
      nng_msg* msg;
      int rv = nng_recvmsg(socket_, &msg, 0);
      if (rv == NNG_ETIMEDOUT) {
        break;  // Survey deadline reached
      }
      if (rv != 0) {
        throw std::runtime_error("Failed to receive response: " +
                                 std::string(nng_strerror(rv)));
      }
      responses.emplace_back(static_cast<char*>(nng_msg_body(msg)),
                             nng_msg_len(msg));
      nng_msg_free(msg);
    }
    return responses;
  }

  std::string receive_survey() {
    if (role_ != ROLE::VOTER) {
      throw std::runtime_error("Receive survey only allowed for VOTER role");
    }
    nng_msg* msg;
    int rv;
    if ((rv = nng_recvmsg(socket_, &msg, 0)) != 0) {
      throw std::runtime_error("Failed to receive survey: " +
                               std::string(nng_strerror(rv)));
    }
    std::string result(static_cast<char*>(nng_msg_body(msg)), nng_msg_len(msg));
    nng_msg_free(msg);
    return result;
  }

  void respond(const std::string& response) {
    if (role_ != ROLE::VOTER) {
      throw std::runtime_error("Respond only allowed for VOTER role");
    }
    nng_msg* msg;
    int rv;
    if ((rv = nng_msg_alloc(&msg, response.size())) != 0) {
      throw std::runtime_error("Failed to allocate response message: " +
                               std::string(nng_strerror(rv)));
    }
    memcpy(nng_msg_body(msg), response.data(), response.size());
    if ((rv = nng_sendmsg(socket_, msg, 0)) != 0) {
      nng_msg_free(msg);
      throw std::runtime_error("Failed to send response: " +
                               std::string(nng_strerror(rv)));
    }
  }

 private:
  ROLE role_;
  nng_socket socket_;
};

Survey::Survey(ROLE role, const std::string& address)
    : m_impl(std::make_unique<Impl>(role, address)) {}

Survey::~Survey() {}

void Survey::send_survey(const std::string& survey) {
  m_impl->send_survey(survey);
}

std::vector<std::string> Survey::collect_responses() {
  return m_impl->collect_responses();
}

std::string Survey::receive_survey() { return m_impl->receive_survey(); }

void Survey::respond(const std::string& response) { m_impl->respond(response); }

}  // namespace ya::module
