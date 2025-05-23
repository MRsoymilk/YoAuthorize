#ifndef RESPONDER_H
#define RESPONDER_H

#include <string>

#include "node_def.h"

namespace ya::module {

class Reponder {
 public:
  Reponder(const std::string& url);
  ~Reponder();

  // Receive a message (blocking)
  std::string receive();

  // Send a reply to the last received message
  void send(const std::string& msg);

  // Prevent copying
  Reponder(const Reponder&) = delete;
  Reponder& operator=(const Reponder&) = delete;

 private:
  void* m_socket;    // Opaque pointer to NNG socket
  void* m_listener;  // Opaque pointer to NNG listener
};

}  // namespace ya::module

#endif
