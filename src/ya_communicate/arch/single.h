#include <memory>

namespace ya::arch {

class Single {
 public:
  Single();
  ~Single();

  void start_http_server(int port);
  void stop_http_server();

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace ya::arch
