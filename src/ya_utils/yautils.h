#ifndef YA_UTILS_H

#include <string>

class YaUtils {
 public:
  static std::string GetExeDir();
  static std::string GetExeName();
  static std::string GetExePath();
  static void ProcessArgs(int argc, char* argv[]);

 private:
  static std::string m_exe_name;
};

#endif  // !YA_UTILS_H
