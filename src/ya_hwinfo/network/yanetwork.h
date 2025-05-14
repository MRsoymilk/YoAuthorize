#ifndef YA_NETWORK_H
#define YA_NETWORK_H

#include <vector>

#include "info_def.h"

class YaNETWORK {
 public:
  YaNETWORK();
  ~YaNETWORK();
  std::vector<NETWORK> getNETWORK();

 private:
  void init();

 private:
  std::vector<NETWORK> m_networks;
};

#endif  // !YA_NETWORK_H
