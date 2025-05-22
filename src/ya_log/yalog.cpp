#include "yalog.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <iostream>

#define DEFINE_LOG_METHOD(level)                                  \
  void level(const std::string &msg) {                            \
    if (s_logger) {                                               \
      s_logger->level(msg);                                       \
    } else {                                                      \
      std::cerr << "Logger not initialized: [" #level "] " << msg \
                << std::endl;                                     \
    }                                                             \
  }

namespace ya {

class YaLog::Impl {
 public:
  std::shared_ptr<spdlog::logger> s_logger;

  Impl() = default;
  ~Impl() {
    if (s_logger) {
      spdlog::drop(s_logger->name());
    }
  }

  void init(const std::string &file_name, size_t max_size, size_t max_files) {
    try {
      std::vector<spdlog::sink_ptr> logSinks;
      logSinks.emplace_back(
          std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
      logSinks.emplace_back(
          std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
              file_name, max_size, max_files));

      logSinks[0]->set_pattern("[%T] [%^%L%$] %n <%P/%t>: %-10v");
      logSinks[1]->set_pattern("[%T] [%L] %n <%P/%t>: %-10v");

      s_logger = std::make_shared<spdlog::logger>("YaLog", logSinks.begin(),
                                                  logSinks.end());
      spdlog::register_logger(s_logger);
      s_logger->set_level(spdlog::level::trace);
      s_logger->flush_on(spdlog::level::trace);
    } catch (const spdlog::spdlog_ex &ex) {
      std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
  }

  DEFINE_LOG_METHOD(trace)
  DEFINE_LOG_METHOD(debug)
  DEFINE_LOG_METHOD(info)
  DEFINE_LOG_METHOD(warn)
  DEFINE_LOG_METHOD(error)
  DEFINE_LOG_METHOD(critical)
};

YaLog::YaLog() : pImpl(std::make_unique<Impl>()) { init(); }

YaLog::~YaLog() = default;

YaLog &YaLog::instance() {
  static YaLog log;
  return log;
}

void YaLog::init(const std::string &file_name, size_t max_size,
                 size_t max_files) {
  pImpl->init(file_name, max_size, max_files);
}

void YaLog::trace(const std::string &msg) { pImpl->trace(msg); }

void YaLog::debug(const std::string &msg) { pImpl->debug(msg); }

void YaLog::info(const std::string &msg) { pImpl->info(msg); }

void YaLog::warn(const std::string &msg) { pImpl->warn(msg); }

void YaLog::error(const std::string &msg) { pImpl->error(msg); }

void YaLog::critical(const std::string &msg) { pImpl->critical(msg); }

}  // namespace ya
