#ifndef YA_GPU_H
#define YA_GPU_H

#include <vector>

#include "info_def.h"

class YaGPU {
 public:
  YaGPU();
  ~YaGPU();

  std::vector<GPU> getGPU();

 private:
  void init();

 private:
  std::vector<GPU> m_gpus;
};

#endif  // !YA_GPU_H
