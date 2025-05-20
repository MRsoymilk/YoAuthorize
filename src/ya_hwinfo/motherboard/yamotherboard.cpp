#include "yamotherboard.h"

#include <fstream>

#include "platform_def.h"
#if defined(YA_WINDOWS)
#include "hwinfooperator.h"
#endif

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
#if defined(YA_WINDOWS)
  WmiQuery wmi;
  if (wmi.initialize()) {
    auto serials = wmi.query(L"Win32_BaseBoard", L"SerialNumber");
    if (!serials.empty()) {
      m_motherboard.serial_number =
          std::string(serials[0].begin(), serials[0].end());
    }

    auto vendors = wmi.query(L"Win32_BaseBoard", L"Manufacturer");
    if (!vendors.empty()) {
      m_motherboard.manufacturer =
          std::string(vendors[0].begin(), vendors[0].end());
    }

    auto versions = wmi.query(L"Win32_BaseBoard", L"Version");
    if (!versions.empty()) {
      m_motherboard.version =
          std::string(versions[0].begin(), versions[0].end());
    }

    auto names = wmi.query(L"Win32_BaseBoard", L"Product");
    if (!names.empty()) {
      m_motherboard.name = std::string(names[0].begin(), names[0].end());
    }
  }
#endif
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
