#ifndef YA_HWINFO_H
#define YA_HWINFO_H

#include <memory>
#include <vector>

#include "info_def.h"

namespace ya {

class YaHwinfo {
 public:
  YaHwinfo();
  ~YaHwinfo();
  BIOS getBIOS();
  CPU getCPU();
  std::vector<GPU> getGPU();
  std::vector<MEMORY> getMEMORY();
  OS getOS();
  std::vector<DISK> getDISK();
  MOTHERBOARD getMOTHERBOARD();
  std::vector<NETWORK> getNETWORK();

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace ya

#endif  // !YA_HWINFO_H
