#pragma once
#include <memory>
#include <string>
#include <vector>

class YaJson {
 private:
  class Impl;

 public:
  YaJson();
  explicit YaJson(const std::string& json);
  explicit YaJson(std::unique_ptr<Impl> impl);
  YaJson(YaJson&&) noexcept;
  YaJson& operator=(YaJson&&) noexcept;
  YaJson(const YaJson&) = delete;
  YaJson& operator=(const YaJson&) = delete;
  ~YaJson();

  void loadFromString(const std::string& str);
  void loadFromFile(const std::string& file);

  std::string getString(const std::string& key);
  int64_t getInt(const std::string& key);
  bool getBool(const std::string& key);
  double getDouble(const std::string& key);
  std::vector<int64_t> getArray(const std::string& key);

  YaJson getObject(const std::string& key);
  std::vector<YaJson> getArrayObject(const std::string& key);

 private:
  std::unique_ptr<Impl> m_impl;
};
