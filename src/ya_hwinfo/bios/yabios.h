#ifndef YA_BIOS_H
#define YA_BIOS_H

#include "info_def.h"

class YaBIOS {
 public:
  YaBIOS();
  ~YaBIOS();
  std::string getSerialNumber();
  std::string getManufacturer();
  std::string getVersion();
  std::string getDate();
  void init();

 private:
  std::string m_serial_number;
  std::string m_manufacturer;
  std::string m_version;
  std::string m_date;
};

#endif  // !YA_BIOS_H
