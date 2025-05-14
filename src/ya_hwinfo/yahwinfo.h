#ifndef YA_HWINFO_H
#define YA_HWINFO_H

#include <memory>
#include <vector>

#include "info_def.h"

class YaHwinfo {
 public:
  YaHwinfo();
  ~YaHwinfo();
  BIOS getBIOS();
  std::vector<CPU> getCPU();
  std::vector<GPU> getGPU();
  std::vector<MEMORY> getMEMORY();
  std::vector<OS> getOS();
  std::vector<DISK> getDISK();
  MOTHERBOARD getMOTHERBOARD();
  std::vector<NETWORK> getNETWORK();

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

#endif  // !YA_HWINFO_H
