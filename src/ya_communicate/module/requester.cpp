#include "requester.h"

#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/reqrep0/req.h>
#include <nng/supplemental/http/http.h>
#include <nng/supplemental/tls/tls.h>

#include <cstring>

namespace ya::module {
Requester::Requester(const std::string& url)
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

Requester::~Requester() {
  if (m_dialer) {
    nng_dialer_close(*(nng_dialer*)m_dialer);
    delete (nng_dialer*)m_dialer;
  }
  if (m_socket) {
    nng_close(*(nng_socket*)m_socket);
    delete (nng_socket*)m_socket;
  }
}

std::string Requester::request(const std::string& msg) {
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
}  // namespace ya::module
