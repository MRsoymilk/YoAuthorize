#ifndef PIPELINE_H
#define PIPELINE_H

#include <memory>

namespace ya::module {

class Pipeline {
 public:
  enum class ROLE { PUSHER, PULLER };

 public:
  Pipeline(ROLE role, const std::string& address);
  ~Pipeline();

  void send(const std::string& message);
  std::string receive();

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace ya::module

#endif
