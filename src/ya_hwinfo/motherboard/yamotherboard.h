#ifndef YA_MOTHERBOARD_H
#define YA_MOTHERBOARD_H

#include "info_def.h"

namespace ya {

class YaMOTHERBOARD {
 public:
  YaMOTHERBOARD();
  ~YaMOTHERBOARD();
  std::string getSerialNumber();
  std::string getManufacturer();
  std::string getVersion();
  std::string getName();

 private:
  void init();

 private:
  MOTHERBOARD m_motherboard;
};

}  // namespace ya

#endif  // !YA_MOTHERBOARD_H
