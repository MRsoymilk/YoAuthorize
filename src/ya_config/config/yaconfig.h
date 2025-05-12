#ifndef YA_CONFIG_H
#define YA_CONFIG_H

#include <memory>
#include <string>
#include <variant>
#include <vector>

using YaVariant = std::variant<int64_t, float, std::string, bool>;

class YaConfig {
 public:
  ~YaConfig();
  static YaConfig& instance();

  bool loadConfig(const std::string& path);

  std::string getString(const std::string& key,
                        const std::string& defaultValue = "");
  std::vector<std::string> getArrayString(const std::string& key);
  std::vector<int> getArrayInt(const std::string& key);
  int getInt(const std::string& key, int defaultValue = 0);
  std::vector<YaVariant> getArrayVariant(const std::string& key);

  std::string toJson(const std::string& str);
  std::string toYaml(const std::string& str);

 private:
  YaConfig();
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

#define YA_CONFIG YaConfig::instance()

#endif
