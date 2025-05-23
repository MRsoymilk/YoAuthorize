#ifndef HTTP_H
#define HTTP_H

#include <memory>

namespace ya::module {

class Http {
 public:
  Http();
  ~Http();
  void start(int port);
  void stop();

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace ya::module

#endif
