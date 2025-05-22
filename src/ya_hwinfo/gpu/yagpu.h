#ifndef YA_GPU_H
#define YA_GPU_H

#include <vector>

#include "info_def.h"

namespace ya {

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

}  // namespace ya

#endif  // !YA_GPU_H
