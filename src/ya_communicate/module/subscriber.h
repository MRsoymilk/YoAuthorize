#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include <memory>

namespace ya::module {

class Subscriber {
 public:
  Subscriber(const std::string& address);
  ~Subscriber();

  void subscribe(const std::string& topic);

  std::string receive();

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace ya::module

#endif
