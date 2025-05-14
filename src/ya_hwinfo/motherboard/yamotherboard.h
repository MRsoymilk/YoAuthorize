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
  std::string m_serial_number;
  std::string m_manufacturer;
  std::string m_version;
  std::string m_name;
};

#endif  // !YA_MOTHERBOARD_H
