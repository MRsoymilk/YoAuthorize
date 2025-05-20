#include "yaos.h"

#include <fstream>

#include "platform_def.h"
#if defined(YA_WINDOWS)
#include "hwinfooperator.h"
#endif
#if defined(YA_LINUX)
#include <sys/utsname.h>
#endif

YaOS::YaOS() { init(); }

YaOS::~YaOS() {}

std::string YaOS::getID() { return m_os.id; }

std::string YaOS::getName() { return m_os.name; }

std::string YaOS::getVersion() { return m_os.version; }

void YaOS::init() {
#if defined(YA_WINDOWS)
  WmiQuery wmi;
  if (wmi.initialize()) {
    auto names = wmi.query(L"Win32_OperatingSystem", L"Caption");
    if (!names.empty()) {
      m_os.name = std::string(names[0].begin(), names[0].end());
    }

    auto uuids = wmi.query(L"Win32_ComputerSystemProduct", L"UUID");
    if (!uuids.empty()) {
      m_os.id = std::string(uuids[0].begin(), uuids[0].end());
    }

    auto versions = wmi.query(L"Win32_OperatingSystem", L"Version");
    if (!versions.empty()) {
      m_os.version = std::string(versions[0].begin(), versions[0].end());
    }
  }
#endif
#if defined(YA_LINUX)
  {
    std::ifstream file("/etc/os-release");
    std::string line;
    while (std::getline(file, line)) {
      std::string pattern_name = "PRETTY_NAME=";
      std::string pattern_version = "VERSION_ID=";
      if (line.find(pattern_name) == 0) {
        std::string name = line.substr(pattern_name.size());
        if (!name.empty() && name.front() == '"' && name.back() == '"') {
          name = name.substr(1, name.size() - 2);
        }
        m_os.name = name;
      } else if (line.find(pattern_version) == 0) {
        std::string ver = line.substr(pattern_version.size());
        if (!ver.empty() && ver.front() == '"' && ver.back() == '"') {
          ver = ver.substr(1, ver.size() - 2);
        }
        m_os.version = ver;
      }
    }
  }
  {
    std::ifstream file("/etc/machine-id");
    if (file.is_open()) {
      std::getline(file, m_os.id);
      file.close();
    }
  }

#endif
}
