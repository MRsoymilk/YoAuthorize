#ifndef YA_CONFIG_H
#define YA_CONFIG_H

#include <memory>
#include <string>
#include <variant>
#include <vector>

using YaVariant = std::variant<int, double, std::string, bool>;

class YaConfig {
 public:
  ~YaConfig();
  static YaConfig& instance();

  bool load(const std::string& path);
  bool save(const std::string& path);

  template <typename T>
  T get(const std::string& key, const T& defaultValue = T());

  template <typename T>
  std::vector<T> getArray(const std::string& key);

  template <typename T>
  void set(const std::string& key, const T& value);

  template <typename T>
  void setArray(const std::string& key, const std::vector<T>& values);

  std::vector<YaVariant> getArrayVariant(const std::string& key);

  std::string toJson() const;
  std::string toYaml() const;

 private:
  YaConfig();
  class Impl;
  std::unique_ptr<Impl> m_impl;

  YaConfig(const YaConfig&) = delete;
  YaConfig& operator=(const YaConfig&) = delete;
};

#define YA_CONFIG YaConfig::instance()

#endif  // !YA_CONFIG_H
