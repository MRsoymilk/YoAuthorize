#include "responder.h"

#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/reqrep0/req.h>
#include <nng/supplemental/http/http.h>
#include <nng/supplemental/tls/tls.h>

#include <cstring>

namespace ya::module {

Reponder::Reponder(const std::string& url)
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

Reponder::~Reponder() {
  if (m_listener) {
    nng_listener_close(*(nng_listener*)m_listener);
    delete (nng_listener*)m_listener;
  }
  if (m_socket) {
    nng_close(*(nng_socket*)m_socket);
    delete (nng_socket*)m_socket;
  }
}

std::string Reponder::receive() {
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

void Reponder::send(const std::string& msg) {
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

}  // namespace ya::module
