#ifndef YA_LOG_H
#define YA_LOG_H

#include <format>
#include <memory>
#include <string>

namespace ya {

class YaLog {
 public:
  static YaLog &instance();
  ~YaLog();

  void init(const std::string &file_name = "./log/ya-net.log",
            size_t max_size = 10 * 1024 * 1024, size_t max_files = 10);

  void trace(const std::string &msg);
  void debug(const std::string &msg);
  void info(const std::string &msg);
  void warn(const std::string &msg);
  void error(const std::string &msg);
  void critical(const std::string &msg);

 private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
  YaLog();
  YaLog(const YaLog &) = delete;
  YaLog &operator=(const YaLog &) = delete;
};

}  // namespace ya

#define YA_LOG ya::YaLog::instance()

#define FORMAT_LOG(fmt, ...)                           \
  [&]() -> std::string {                               \
    try {                                              \
      return std::format(fmt, ##__VA_ARGS__);          \
    } catch (const std::format_error &e) {             \
      return std::string("Format error: ") + e.what(); \
    }                                                  \
  }()

#define LOG_TRACE(fmt, ...) YA_LOG.trace(FORMAT_LOG(fmt, ##__VA_ARGS__))
#define LOG_DEBUG(fmt, ...) YA_LOG.debug(FORMAT_LOG(fmt, ##__VA_ARGS__))
#define LOG_INFO(fmt, ...) YA_LOG.info(FORMAT_LOG(fmt, ##__VA_ARGS__))
#define LOG_WARN(fmt, ...) YA_LOG.warn(FORMAT_LOG(fmt, ##__VA_ARGS__))
#define LOG_ERROR(fmt, ...) YA_LOG.error(FORMAT_LOG(fmt, ##__VA_ARGS__))
#define LOG_CRITICAL(fmt, ...) YA_LOG.critical(FORMAT_LOG(fmt, ##__VA_ARGS__))

#endif  // !YA_LOG_H
