#ifndef YA_DISK_H
#define YA_DISK_H

#include <vector>

#include "info_def.h"

class YaDISK {
 public:
  YaDISK();
  ~YaDISK();
  std::vector<DISK> getDISK();

 private:
  void init();

 private:
  std::vector<DISK> m_disks;
};

#endif  // !YA_DISK_H
