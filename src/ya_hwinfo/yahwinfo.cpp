#include "yahwinfo.h"

#include "bios/yabios.h"
#include "cpu/yacpu.h"
#include "disk/yadisk.h"
#include "gpu/yagpu.h"
#include "memory/yamemory.h"
#include "motherboard/yamotherboard.h"
#include "network/yanetwork.h"
#include "os/yaos.h"

class YaHwinfo::Impl {
 public:
  BIOS getBIOS() {
    YaBIOS ya_bios;
    BIOS bios{
        .serial_number = ya_bios.getSerialNumber(),
        .manufacturer = ya_bios.getManufacturer(),
        .date = ya_bios.getDate(),
        .version = ya_bios.getVersion(),
    };
    return bios;
  }
  CPU getCPU() {
    YaCPU ya_cpu;
    CPU cpu{
        .serial_number = ya_cpu.getSerialNumber(),
        .architecture = ya_cpu.getArchitecture(),
        .manufacturer = ya_cpu.getManufacturer(),
        .name = ya_cpu.getName(),
    };
    return cpu;
  }
  std::vector<GPU> getGPU() { return {}; }
  std::vector<MEMORY> getMEMORY() { return {}; }
  OS getOS() {
    YaOS ya_os;
    OS os{
        .id = ya_os.getID(),
        .name = ya_os.getName(),
        .version = ya_os.getVersion(),
    };
    return os;
  }
  std::vector<DISK> getDISK() { return {}; }
  MOTHERBOARD getMOTHERBOARD() {
    YaMOTHERBOARD ya_motherboard;
    MOTHERBOARD motherboard{
        .serial_number = ya_motherboard.getSerialNumber(),
        .manufacturer = ya_motherboard.getManufacturer(),
        .version = ya_motherboard.getVersion(),
        .name = ya_motherboard.getName(),
    };
    return motherboard;
  }
  std::vector<NETWORK> getNETWORK() { return {}; }

 private:
};

YaHwinfo::YaHwinfo() { m_impl = std::make_unique<Impl>(); }

YaHwinfo::~YaHwinfo() {}

BIOS YaHwinfo::getBIOS() { return m_impl->getBIOS(); }

CPU YaHwinfo::getCPU() { return m_impl->getCPU(); }

std::vector<GPU> YaHwinfo::getGPU() { return m_impl->getGPU(); }

std::vector<MEMORY> YaHwinfo::getMEMORY() { return m_impl->getMEMORY(); }

OS YaHwinfo::getOS() { return m_impl->getOS(); }

std::vector<DISK> YaHwinfo::getDISK() { return m_impl->getDISK(); }

MOTHERBOARD YaHwinfo::getMOTHERBOARD() { return m_impl->getMOTHERBOARD(); }

std::vector<NETWORK> YaHwinfo::getNETWORK() { return m_impl->getNETWORK(); }
