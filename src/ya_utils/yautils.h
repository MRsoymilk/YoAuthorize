#ifndef YA_UTILS_H
#define YA_UTILS_H

#include <chrono>
#include <map>
#include <string>

class YaUtils {
 public:
  class Exe {
   public:
    static std::string GetExeDir();
    static std::string GetExeName();
    static std::string GetExePath();
    static void ProcessArgs(int argc, char* argv[]);

   private:
    static std::string m_exe_name;
  };

  class Timer {
   public:
    static void StartTimer(const std::string& timer_id = "0");
    static double GetElapsedTime_ms(const std::string& timer_id = "0");
    static double GetElapsedTime_s(const std::string& timer_id = "0");
    static double GetElapsedTime_μs(const std::string& timer_id = "0");
    static void Sleep(unsigned int ms);

   private:
    static std::map<std::string, std::chrono::steady_clock::time_point>
        m_timers;
  };

  class Platform {
   public:
    enum class PLATFORM { WINDOWS, LINUX, MACOS };

   public:
    static PLATFORM GetPlatform();
  };
};

#endif  // !YA_UTILS_H
