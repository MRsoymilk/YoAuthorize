#ifndef YA_GPU_H
#define YA_GPU_H

#include "info_def.h"

class YaGPU {
 public:
  YaGPU();
  ~YaGPU();
  std::string getName();
  std::string getSerialNumber();
  std::string getManufacturer();

 private:
  void init();

 private:
  GPU m_gpu;
};

#endif  // !YA_GPU_H
