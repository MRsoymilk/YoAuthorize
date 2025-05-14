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
  std::string name;
};

struct DISK {
  std::string serial_number;
  std::string manufacturer;
  std::string name;
};

struct GPU {
  std::string serial_number;
  std::string manufacturer;
  std::string name;
};

struct MEMORY {
  std::string serial_number;
  std::string manufacturer;
};

struct MOTHERBOARD {
  std::string serial_number;
  std::string manufacturer;
  std::string version;
  std::string name;
};

struct NETWORK {
  std::string name;
  std::string ipv4;
  std::string mac;
};

struct OS {
  std::string id;
  std::string name;
  std::string version;
};

#endif  // !INFO_DEF_H
