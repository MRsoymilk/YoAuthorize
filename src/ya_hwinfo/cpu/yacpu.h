#ifndef YACPU_H
#define YACPU_H

#include <string>

class YaCPU {
 public:
  YaCPU();
  ~YaCPU();

  std::string getSerialNumber();
  std::string getArchitecture();
  std::string getManufacturer();
  std::string getName();

 private:
  void init();

 private:
  std::string m_serial_number;
  std::string m_architecture;
  std::string m_manufacturer;
  std::string m_name;
};

#endif  // YACPU_H
