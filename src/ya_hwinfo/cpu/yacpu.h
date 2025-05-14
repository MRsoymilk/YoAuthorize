#ifndef YACPU_H
#define YACPU_H

#include "info_def.h"

class YaCPU {
 public:
  YaCPU();
  ~YaCPU();

  std::string getSerialNumber();
  std::string getArchitecture();
  std::string getManufacturer();
  std::string getName();

 private:
  void init();

 private:
  CPU m_cpu;
};

#endif  // YACPU_H
