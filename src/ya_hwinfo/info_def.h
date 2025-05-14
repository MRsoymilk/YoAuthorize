#ifndef INFO_DEF_H
#define INFO_DEF_H

#include <string>

struct CPU {
  std::string serial_number;
  std::string architecture;
  std::string manufacturer;
};

struct GPU {
  std::string serial_number;
  std::string manufacturer;
};

struct MEMORY {
  std::string serial_number;
  std::string manufacturer;
};

struct OS {
  std::string serial_number;
  std::string name;
  std::string version;
};

struct DISK {
  std::string serial_number;
  std::string manufacturer;
};

struct MOTHERBOARD {
  std::string serial_number;
  std::string manufacturer;
};

struct NETWORK {
  std::string serial_number;
  std::string manufacturer;
};

#endif  // !INFO_DEF_H
