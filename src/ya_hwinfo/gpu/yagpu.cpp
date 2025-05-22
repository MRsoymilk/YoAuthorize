#include "yagpu.h"

#include <filesystem>
#include <fstream>

#include "platform_def.h"
#if defined(YA_WINDOWS)
#include "hwinfooperator.h"
#endif

namespace ya {

YaGPU::YaGPU() { init(); }

YaGPU::~YaGPU() {}

std::vector<GPU> YaGPU::getGPU() { return m_gpus; }

void YaGPU::init() {
#if defined(YA_WINDOWS)
  WmiQuery wmi;
  if (wmi.initialize()) {
    auto names = wmi.query(L"Win32_VideoController", L"Name");
    auto vendors = wmi.query(L"Win32_VideoController", L"AdapterCompatibility");
    auto device_ids = wmi.query(L"Win32_VideoController", L"PNPDeviceID");

    size_t count = names.size();
    for (size_t i = 0; i < count; ++i) {
      GPU gpu;
      gpu.name = std::string(names[i].begin(), names[i].end());
      gpu.manufacturer = std::string(vendors[i].begin(), vendors[i].end());
      gpu.serial_number = std::string(device_ids[i].begin(), device_ids[i].end());
      m_gpus.push_back(gpu);
    }
  }
#endif
#if defined(YA_LINUX)
  namespace fs = std::filesystem;
  std::string base_path = "/sys/class/drm";

  auto trim = [](const std::string& str) -> std::string {
    const auto begin = str.find_first_not_of(" \t\n\r");
    const auto end = str.find_last_not_of(" \t\n\r");
    if (begin == std::string::npos || end == std::string::npos) return "";
    return str.substr(begin, end - begin + 1);
  };

  auto vendor_id_to_name = [](const std::string& id) -> std::string {
    if (id == "0x10de") return "NVIDIA";
    if (id == "0x1002") return "AMD";
    if (id == "0x8086") return "Intel";
    return "";
  };

  for (const auto& entry : fs::directory_iterator(base_path)) {
    const std::string name = entry.path().filename().string();

    if (name.find("card") == 0 &&
        fs::exists(entry.path() / "device" / "vendor")) {
      std::ifstream vendor_file(entry.path() / "device" / "vendor");
      std::ifstream device_file(entry.path() / "device" / "device");

      std::string vendor_id, device_id;
      std::getline(vendor_file, vendor_id);
      std::getline(device_file, device_id);

      vendor_id = trim(vendor_id);
      device_id = trim(device_id);

      GPU gpu;
      gpu.manufacturer = vendor_id_to_name(vendor_id);
      gpu.serial_number = device_id;
      gpu.name = name;
      m_gpus.push_back(gpu);
    }
  }
#endif
}

}  // namespace ya
