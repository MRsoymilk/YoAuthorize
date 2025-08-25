#include "single.h"

#include "module/http.h"

namespace ya::arch {
class Single::Impl {
 public:
  void start_http_server(int port) { m_http.start(port); }

  void stop_http_server() { m_http.stop(); }

 private:
  ya::module::Http m_http;
};

Single::Single() { m_impl = std::make_unique<Impl>(); }

Single::~Single() {}

void Single::start_http_server(int port) { m_impl->start_http_server(port); }

void Single::stop_http_server() { m_impl->stop_http_server(); }

}  // namespace ya::arch
