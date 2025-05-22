#ifndef YA_BIOS_H
#define YA_BIOS_H

#include "info_def.h"

namespace ya {

class YaBIOS {
 public:
  YaBIOS();
  ~YaBIOS();
  std::string getSerialNumber();
  std::string getManufacturer();
  std::string getVersion();
  std::string getDate();

 private:
  void init();

 private:
  BIOS m_bios;
};

}  // namespace ya

#endif  // !YA_BIOS_H
