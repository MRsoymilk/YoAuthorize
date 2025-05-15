#ifndef YA_MEMORY_H
#define YA_MEMORY_H
#include <vector>

#include "info_def.h"
class YaMEMORY {
 public:
  YaMEMORY();
  ~YaMEMORY();

  std::vector<MEMORY> getMEMORY();

 private:
  void init();

 private:
  std::vector<MEMORY> m_memories;
};

#endif  // !YA_MEMORY_H
