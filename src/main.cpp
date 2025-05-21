#include <iostream>

#include "version.h"

int main(int argc, char* argv[]) {
  std::cout << "hello, ya-net!" << std::endl;
  std::cout << "Version: " << PROJECT_VERSION << std::endl;
  return 0;
}
