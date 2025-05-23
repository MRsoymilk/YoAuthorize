#ifndef BUS_H
#define BUS_H

#include <memory>

namespace ya::module {

class Bus {
 public:
  Bus(const std::string& address);
  ~Bus();

  void send(const std::string& message);
  std::string receive();

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace ya::module

#endif
