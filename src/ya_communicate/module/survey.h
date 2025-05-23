#ifndef SURVEY_H
#define SURVEY_H

#include <memory>
#include <vector>

namespace ya::module {

class Survey {
 public:
  enum class ROLE { INITIATOR, VOTER };

 public:
  Survey(ROLE role, const std::string& address);
  ~Survey();

  void send_survey(const std::string& survey);
  std::vector<std::string> collect_responses();

  std::string receive_survey();
  void respond(const std::string& response);

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace ya::module

#endif
