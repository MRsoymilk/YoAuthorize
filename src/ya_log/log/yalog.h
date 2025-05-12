#ifndef YALOG_H
#define YALOG_H

#include <memory>

#include "spdlog/spdlog.h"

class YaLog {
 public:
  static YaLog &getInstance();
  ~YaLog();
  void init(const std::string &file_name = "./log/ya-net.log",
            size_t max_size = 10 * 1024 * 1024, size_t max_files = 10);
  static std::shared_ptr<spdlog::logger> getLogger();

 private:
  YaLog();
  static std::shared_ptr<spdlog::logger> s_logger;
};

#define YA_LOG YaLog::getInstance()
#define LOG_TRACE(...) YA_LOG.getLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...) YA_LOG.getLogger()->info(__VA_ARGS__)
#define LOG_WARN(...) YA_LOG.getLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) YA_LOG.getLogger()->error(__VA_ARGS__)

#endif  // YALOG_H
