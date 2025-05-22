#ifndef YA_OS_H
#define YA_OS_H

#include "info_def.h"

namespace ya {

class YaOS {
 public:
  YaOS();
  ~YaOS();
  std::string getID();
  std::string getName();
  std::string getVersion();

 private:
  void init();

 private:
  OS m_os;
};

}  // namespace ya

#endif  // !YA_OS_H
