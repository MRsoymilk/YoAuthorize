#ifndef INFO_DEF_H
#define INFO_DEF_H

#include <string>

struct BIOS {
  std::string serial_number;
  std::string manufacturer;
  std::string date;
  std::string version;
};

struct CPU {
  std::string serial_number;
  std::string architecture;
  std::string manufacturer;
};

struct DISK {
  std::string serial_number;
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

struct MOTHERBOARD {
  std::string serial_number;
  std::string manufacturer;
};

struct NETWORK {
  std::string serial_number;
  std::string manufacturer;
};

struct OS {
  std::string serial_number;
  std::string name;
  std::string version;
};

#endif  // !INFO_DEF_H
