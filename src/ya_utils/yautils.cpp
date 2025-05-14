#include "yautils.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <thread>
#include <vector>

std::string YaUtils::Exe::m_exe_name = "";
std::map<std::string, std::chrono::steady_clock::time_point>
    YaUtils::Timer::m_timers = {};

std::string YaUtils::Exe::GetExeDir() {
  std::string path = std::filesystem::current_path().string();
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

std::string YaUtils::Exe::GetExeName() { return m_exe_name; }

std::string YaUtils::Exe::GetExePath() {
  return GetExeDir() + "/" + GetExeName();
}

void YaUtils::Exe::ProcessArgs(int argc, char* argv[]) {
  std::vector<std::string> args(argv, argv + argc);
  std::filesystem::path exe_name = argv[0];
  m_exe_name = exe_name.filename().string();
}

void YaUtils::Timer::Sleep(unsigned int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void YaUtils::Timer::StartTimer(const std::string& timer_id) {
  m_timers[timer_id] = std::chrono::steady_clock::now();
}

double YaUtils::Timer::GetElapsedTime_ms(const std::string& timer_id) {
  return GetElapsedTime_μs(timer_id) / 1'000.0;
}

double YaUtils::Timer::GetElapsedTime_s(const std::string& timer_id) {
  return GetElapsedTime_μs(timer_id) / 1'000'000.0;
}

double YaUtils::Timer::GetElapsedTime_μs(const std::string& timer_id) {
  auto now = std::chrono::steady_clock::now();
  auto it = m_timers.find(timer_id);
  if (it == m_timers.end()) {
    return 0.0;
  }
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(now - it->second);
  return static_cast<double>(duration.count());
}
