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
