#include "yabios.h"

#include <fstream>

#include "platform_def.h"

YaBIOS::YaBIOS() { init(); }

YaBIOS::~YaBIOS() {}

std::string YaBIOS::getSerialNumber() { return m_bios.serial_number; }

std::string YaBIOS::getManufacturer() { return m_bios.manufacturer; }

std::string YaBIOS::getVersion() { return m_bios.version; }

std::string YaBIOS::getDate() { return m_bios.date; }

void YaBIOS::init() {
#if defined(YA_LINUX)
  // /sys/class/dmi
  {
    std::ifstream file("/sys/class/dmi/id/bios_serial");
    if (file.is_open()) {
      std::getline(file, m_bios.serial_number);
      file.close();
    }
  }
  {
    std::ifstream file("/sys/class/dmi/id/bios_vendor");
    if (file.is_open()) {
      std::getline(file, m_bios.manufacturer);
      file.close();
    }
  }
  {
    std::ifstream file("/sys/class/dmi/id/bios_version");
    if (file.is_open()) {
      std::getline(file, m_bios.version);
      file.close();
    }
  }
  {
    std::ifstream file("/sys/class/dmi/id/bios_date");
    if (file.is_open()) {
      std::getline(file, m_bios.date);
      file.close();
    }
  }
#endif
}
