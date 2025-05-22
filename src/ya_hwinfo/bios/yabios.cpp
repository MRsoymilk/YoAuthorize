#include "yabios.h"

#include <fstream>

#include "platform_def.h"
#if defined(YA_WINDOWS)
#include "hwinfooperator.h"
#endif

namespace ya {

YaBIOS::YaBIOS() { init(); }

YaBIOS::~YaBIOS() {}

std::string YaBIOS::getSerialNumber() { return m_bios.serial_number; }

std::string YaBIOS::getManufacturer() { return m_bios.manufacturer; }

std::string YaBIOS::getVersion() { return m_bios.version; }

std::string YaBIOS::getDate() { return m_bios.date; }

void YaBIOS::init() {
#if defined(YA_WINDOWS)
  WmiQuery wmi;
  if (wmi.initialize()) {
    auto serials = wmi.query(L"Win32_BIOS", L"SerialNumber");
    if (!serials.empty()) {
      m_bios.serial_number = std::string(serials[0].begin(), serials[0].end());
    }
    auto vendors = wmi.query(L"Win32_BIOS", L"Manufacturer");
    if (!vendors.empty()) {
      m_bios.manufacturer = std::string(vendors[0].begin(), vendors[0].end());
    }
    auto versions = wmi.query(L"Win32_BIOS", L"SMBIOSBIOSVersion");
    if (!versions.empty()) {
      m_bios.version = std::string(versions[0].begin(), versions[0].end());
    }
    auto dates = wmi.query(L"Win32_BIOS", L"ReleaseDate");
    if (!dates.empty()) {
      std::string raw = std::string(dates[0].begin(), dates[0].end());
      if (!raw.empty() && raw.size() >= 8) {
        // yyyymmddhhmmss
        m_bios.date =
            raw.substr(0, 4) + "-" + raw.substr(4, 2) + "-" + raw.substr(6, 2);
      }
    }
  }
#endif
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

}  // namespace ya
