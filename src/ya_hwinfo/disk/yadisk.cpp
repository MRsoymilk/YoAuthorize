#include "yadisk.h"

#include <filesystem>
#include <fstream>
#include <set>

#include "platform_def.h"

YaDISK::YaDISK() { init(); }

YaDISK::~YaDISK() {}

std::vector<DISK> YaDISK::getDISK() { return m_disks; }

void YaDISK::init() {
#if defined(YA_LINUX)
  namespace fs = std::filesystem;
  std::string base_path = "/sys/block/";

  auto readSysFile = [](const std::string& path) -> std::string {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string content;
    std::getline(file, content);
    return content;
  };

  const std::set<std::string> allowed_prefixes = {"sd", "nvme", "hd"};

  for (const auto& entry : fs::directory_iterator(base_path)) {
    std::string dev = entry.path().filename().string();

    bool valid = false;
    for (const auto& prefix : allowed_prefixes) {
      if (dev.rfind(prefix, 0) == 0) {
        valid = true;
        break;
      }
    }
    if (!valid) {
      continue;
    }

    std::string device_path = base_path + dev + "/device/";
    DISK d;
    d.name = dev;
    d.serial_number = readSysFile(device_path + "serial");
    d.manufacturer = readSysFile(device_path + "model");
    m_disks.push_back(d);
  }
#endif
}
