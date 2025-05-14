#include <gtest/gtest.h>

#include <format>
#include <string>

#include "yahwinfo.h"

TEST(HwinfoTest, BIOS) {
  YaHwinfo info;
  auto bios = info.getBIOS();

  std::cout << std::format(
                   "BIOS:\n"
                   "manufacturer:  {}\n"
                   "serial_number: {}\n"
                   "date:          {}\n"
                   "version:       {}\n",
                   bios.manufacturer, bios.serial_number, bios.date,
                   bios.version)
            << std::endl;
}

TEST(HwinfoTest, CPU) {
  YaHwinfo info;
  auto cpu = info.getCPU();
  std::cout << std::format(
                   "CPU:\n"
                   "name:          {}\n"
                   "manufacturer:  {}\n"
                   "serial_number: {}\n"
                   "architecture:  {}\n",
                   cpu.name, cpu.manufacturer, cpu.serial_number,
                   cpu.architecture)
            << std::endl;
}

TEST(HwinfoTest, DISK) {
  YaHwinfo info;
  auto disk = info.getDISK();
  for (int i = 0; i < disk.size(); ++i) {
    std::cout << std::format(
                     "DISK_{}:\n"
                     "name:          {}\n"
                     "manufacturer:  {}\n"
                     "serial_number: {}\n",
                     i, disk[i].name, disk[i].manufacturer,
                     disk[i].serial_number)
              << std::endl;
  }
}

TEST(HwinfoTest, GPU) {
  YaHwinfo info;
  auto gpu = info.getGPU();
  for (int i = 0; i < gpu.size(); ++i) {
    std::cout << std::format(
                     "GPU_{}:\n"
                     "name:          {}\n"
                     "manufacturer:  {}\n"
                     "serial_number: {}\n",
                     i, gpu[i].name, gpu[i].manufacturer, gpu[i].serial_number)
              << std::endl;
  }
}

TEST(HwinfoTest, MEMORY) {
  YaHwinfo info;
  auto memory = info.getMEMORY();
}

TEST(HwinfoTest, MOTHERBOARD) {
  YaHwinfo info;
  auto motherboard = info.getMOTHERBOARD();

  std::cout << std::format(
                   "MOTHERBOARD:\n"
                   "name:          {}\n"
                   "manufacturer:  {}\n"
                   "serial_number: {}\n"
                   "version:       {}\n",
                   motherboard.name, motherboard.manufacturer,
                   motherboard.serial_number, motherboard.version)
            << std::endl;
}

TEST(HwinfoTest, NETWORK) {
  YaHwinfo info;
  auto network = info.getNETWORK();
}

TEST(HwinfoTest, OS) {
  YaHwinfo info;
  auto os = info.getOS();

  std::cout << std::format(
                   "OS:\n"
                   "id:          {}\n"
                   "name:        {}\n"
                   "version:     {}\n",
                   os.id, os.name, os.version)
            << std::endl;
}
