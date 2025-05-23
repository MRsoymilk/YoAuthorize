#ifndef REQUESTER_H
#define REQUESTER_H

#include "node_def.h"

namespace ya::module {

class Requester {
 public:
  Requester(const std::string& url);
  ~Requester();

  // Send a request and receive a reply (blocking)
  std::string request(const std::string& msg);

  // Prevent copying
  Requester(const Requester&) = delete;
  Requester& operator=(const Requester&) = delete;

 private:
  void* m_socket;  // Opaque pointer to NNG socket
  void* m_dialer;  // Opaque pointer to NNG dialer
};

}  // namespace ya::module

#endif
