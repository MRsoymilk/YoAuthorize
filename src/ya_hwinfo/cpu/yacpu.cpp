#include "yacpu.h"

#include <fstream>

#include "platform_def.h"

#if defined(YA_LINUX)
#include <sys/utsname.h>
#endif

YaCPU::YaCPU() { init(); }

YaCPU::~YaCPU() {}

std::string YaCPU::getSerialNumber() { return m_cpu.serial_number; }

std::string YaCPU::getArchitecture() { return m_cpu.architecture; }

std::string YaCPU::getManufacturer() { return m_cpu.manufacturer; }

std::string YaCPU::getName() { return m_cpu.name; }

void YaCPU::init() {
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
