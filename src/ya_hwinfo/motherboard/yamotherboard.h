#ifndef YA_MOTHERBOARD_H
#define YA_MOTHERBOARD_H

#include "info_def.h"

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

#endif  // !YA_MOTHERBOARD_H
