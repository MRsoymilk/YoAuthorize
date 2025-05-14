#include "yahwinfo.h"

#include "cpu/yacpu.h"
#include "disk/yadisk.h"
#include "gpu/yagpu.h"
#include "memory/yamemory.h"
#include "motherboard/yamotherboard.h"
#include "network/yanetwork.h"
#include "os/yaos.h"

class YaHwinfo::Impl {
 public:
  std::vector<CPU> getCPU() { return {}; }
  std::vector<GPU> getGPU() { return {}; }
  std::vector<MEMORY> getMEMORY() { return {}; }
  std::vector<OS> getOS() { return {}; }
  std::vector<DISK> getDISK() { return {}; }
  std::vector<MOTHERBOARD> getMOTHERBOARD() { return {}; }
  std::vector<NETWORK> getNETWORK() { return {}; }

 private:
};

std::vector<CPU> YaHwinfo::getCPU() { return m_impl->getCPU(); }

std::vector<GPU> YaHwinfo::getGPU() { return m_impl->getGPU(); }

std::vector<MEMORY> YaHwinfo::getMEMORY() { return m_impl->getMEMORY(); }

std::vector<OS> YaHwinfo::getOS() { return m_impl->getOS(); }

std::vector<DISK> YaHwinfo::getDISK() { return m_impl->getDISK(); }

std::vector<MOTHERBOARD> YaHwinfo::getMOTHERBOARD() {
  return m_impl->getMOTHERBOARD();
}

std::vector<NETWORK> YaHwinfo::getNETWORK() { return m_impl->getNETWORK(); }
