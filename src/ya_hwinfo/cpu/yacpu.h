#ifndef YA_CPU_H
#define YA_CPU_H
#include "info_def.h"

class YaCPU {
 public:
  std::string getSerialNumber();
  std::string getArchitecture();
  std::string getManufacturer();
};

#endif  // !YA_CPU_H
