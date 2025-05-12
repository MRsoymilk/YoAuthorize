#include "yautils.h"
#include <filesystem>

std::string YaUtils::m_exe_name = "";

std::string YaUtils::GetExeDir() {
  std::string path = std::filesystem::current_path().string();
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

std::string YaUtils::GetExeName() { return m_exe_name; }

std::string YaUtils::GetExePath() { return GetExeDir() + "/" + GetExeName(); }

std::string YaUtils::ProcessArgs(int argc, char* argv[]) {
  std::vector<std::string> args(argv, argv + argc);
  std::filesystem::path exe_name = argv[0];
  m_exe_name = exe_name.filename().string();
  return "";
}