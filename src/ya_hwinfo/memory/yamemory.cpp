#include "yamemory.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "platform_def.h"
#if defined(YA_WINDOWS)
#include "hwinfooperator.h"
#endif

YaMEMORY::YaMEMORY() { init(); }

YaMEMORY::~YaMEMORY() {}

std::vector<MEMORY> YaMEMORY::getMEMORY() { return m_memories; }

void YaMEMORY::init() {
#if defined(YA_WINDOWS)
  WmiQuery wmi;
  if (wmi.initialize()) {
    auto manufacturers = wmi.query(L"Win32_PhysicalMemory", L"Manufacturer");
    auto serials = wmi.query(L"Win32_PhysicalMemory", L"SerialNumber");

    size_t count = serials.size();
    for (size_t i = 0; i < count; ++i) {
      MEMORY memory;
      if (i < manufacturers.size())
        memory.manufacturer =
            std::string(manufacturers[i].begin(), manufacturers[i].end());
      if (i < serials.size())
        memory.serial_number =
            std::string(serials[i].begin(), serials[i].end());
      m_memories.push_back(memory);
    }
  }
#endif
#if defined(YA_LINUX)
  namespace fs = std::filesystem;

  // 从结构中提取字符串
  auto extract_string = [](const std::vector<char>& data, int index,
                           size_t string_area_offset) -> std::string {
    if (index == 0) return "";
    size_t i = string_area_offset;
    int current = 1;
    while (i < data.size()) {
      std::string s(&data[i]);
      if (current == index) return s;
      i += s.length() + 1;
      current++;
    }
    return "";
  };

  for (const auto& entry :
       fs::directory_iterator("/sys/firmware/dmi/entries")) {
    if (!entry.is_directory()) continue;

    auto path = entry.path();
    if (path.filename().string().find("17-") != 0)
      continue;  // Type 17 是 Memory Device

    std::ifstream f(path / "raw", std::ios::binary);
    if (!f) continue;

    std::vector<char> data((std::istreambuf_iterator<char>(f)), {});

    if (data.size() < 0x20) continue;

    // 字符串索引在固定偏移处
    uint8_t manufacturer_index = data[0x17];
    uint8_t serial_index = data[0x18];

    // 找到字符串区域开始位置（结构结束后）
    size_t string_offset = 0x1F;
    while (string_offset + 1 < data.size() &&
           (data[string_offset] != 0 || data[string_offset + 1] != 0)) {
      ++string_offset;
    }
    string_offset += 2;  // Skip double zero
    MEMORY memory;
    memory.manufacturer = extract_string(data, manufacturer_index, 0x20);
    memory.serial_number = extract_string(data, serial_index, 0x20);

    m_memories.push_back(memory);
  }
#endif
}
