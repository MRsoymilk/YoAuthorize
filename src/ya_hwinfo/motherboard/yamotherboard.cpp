#include "yamotherboard.h"

#include <fstream>

#include "platform_def.h"

YaMOTHERBOARD::YaMOTHERBOARD() { init(); }

YaMOTHERBOARD::~YaMOTHERBOARD() {}

std::string YaMOTHERBOARD::getSerialNumber() {
  return m_motherboard.serial_number;
}

std::string YaMOTHERBOARD::getManufacturer() {
  return m_motherboard.manufacturer;
}

std::string YaMOTHERBOARD::getVersion() { return m_motherboard.version; }

std::string YaMOTHERBOARD::getName() { return m_motherboard.name; }

void YaMOTHERBOARD::init() {
#if defined(YA_LINUX)
  // /sys/class/dmi
  {
    std::ifstream file("/sys/class/dmi/id/board_serial");
    if (file.is_open()) {
      std::getline(file, m_motherboard.serial_number);
      file.close();
    }
  }
  {
    std::ifstream file("/sys/class/dmi/id/board_vendor");
    if (file.is_open()) {
      std::getline(file, m_motherboard.manufacturer);
      file.close();
    }
  }
  {
    std::ifstream file("/sys/class/dmi/id/board_version");
    if (file.is_open()) {
      std::getline(file, m_motherboard.version);
      file.close();
    }
  }
  {
    std::ifstream file("/sys/class/dmi/id/board_name");
    if (file.is_open()) {
      std::getline(file, m_motherboard.name);
      file.close();
    }
  }
#endif
}
