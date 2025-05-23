#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <memory>

namespace ya::module {

class Publisher {
 public:
  Publisher(const std::string &address);
  ~Publisher();

  void publish(const std::string &topic, const std::string &message);

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace ya::module

#endif
