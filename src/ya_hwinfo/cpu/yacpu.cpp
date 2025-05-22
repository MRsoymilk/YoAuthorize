#include "yacpu.h"

#include <fstream>

#include "platform_def.h"
#if defined(YA_WINDOWS)
#include "hwinfooperator.h"
#endif
#if defined(YA_LINUX)
#include <sys/utsname.h>
#endif

namespace ya {

YaCPU::YaCPU() { init(); }

YaCPU::~YaCPU() {}

std::string YaCPU::getSerialNumber() { return m_cpu.serial_number; }

std::string YaCPU::getArchitecture() { return m_cpu.architecture; }

std::string YaCPU::getManufacturer() { return m_cpu.manufacturer; }

std::string YaCPU::getName() { return m_cpu.name; }

void YaCPU::init() {
#if defined(YA_WINDOWS)
  WmiQuery wmi;
  if (wmi.initialize()) {
    auto serials = wmi.query(L"Win32_Processor", L"ProcessorId");
    if (!serials.empty()) {
      m_cpu.serial_number = std::string(serials[0].begin(), serials[0].end());
    }

    auto names = wmi.query(L"Win32_Processor", L"Name");
    if (!names.empty()) {
      m_cpu.name = std::string(names[0].begin(), names[0].end());
    }

    auto vendors = wmi.query(L"Win32_Processor", L"Manufacturer");
    if (!vendors.empty()) {
      m_cpu.manufacturer = std::string(vendors[0].begin(), vendors[0].end());
    }

    auto archs = wmi.query(L"Win32_Processor", L"Architecture");
    if (!archs.empty()) {
      int archCode = std::stoi(std::wstring(archs[0]));
      switch (archCode) {
        case 0:
          m_cpu.architecture = "x86";
          break;
        case 9:
          m_cpu.architecture = "x64";
          break;
        case 5:
          m_cpu.architecture = "ARM";
          break;
        case 12:
          m_cpu.architecture = "ARM64";
          break;
        default:
          m_cpu.architecture = "Unknown";
          break;
      }
    }
  }
#endif
#if defined(YA_LINUX)
  // /proc/cpuinfo

  auto get_cpu_field = [](const std::string& field) -> std::string {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    if (!cpuinfo.is_open()) {
      return "";
    }

    while (std::getline(cpuinfo, line)) {
      if (line.find(field) != std::string::npos) {
        size_t pos = line.find(":");
        if (pos != std::string::npos) {
          std::string value = line.substr(pos + 1);
          value.erase(0, value.find_first_not_of(" \t"));
          value.erase(value.find_last_not_of(" \t") + 1);
          return value.empty() ? "" : value;
        }
      }
    }
    return "";
  };

  m_cpu.manufacturer = get_cpu_field("vendor_id");
  m_cpu.name = get_cpu_field("model name");
  m_cpu.serial_number = get_cpu_field("serial");

  struct utsname buf;
  if (uname(&buf) == 0) {
    m_cpu.architecture = buf.machine;
  } else {
    m_cpu.architecture = "";
  }

  if (m_cpu.serial_number.empty() || m_cpu.serial_number == "Not Specified") {
    m_cpu.serial_number = "";
  }
#endif
}

}  // namespace ya
