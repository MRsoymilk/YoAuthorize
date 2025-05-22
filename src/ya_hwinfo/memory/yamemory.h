#ifndef YA_MEMORY_H
#define YA_MEMORY_H
#include <vector>

#include "info_def.h"

namespace ya {

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

}  // namespace ya

#endif  // !YA_MEMORY_H
