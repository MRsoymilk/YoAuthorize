#ifndef YAJSON_H
#define YAJSON_H

#include <memory>
#include <string>
#include <vector>

namespace ya {

class YaJson {
 public:
  YaJson();
  explicit YaJson(const std::string& json);
  YaJson(YaJson&& other) noexcept;
  YaJson& operator=(YaJson&& other) noexcept;
  ~YaJson();

  // 加载 JSON 数据
  void loadFromString(const std::string& str);
  void loadFromFile(const std::string& file);

  // 获取 JSON 数据
  std::string getString(const std::string& key) const;
  int64_t getInt(const std::string& key) const;
  bool getBool(const std::string& key) const;
  double getDouble(const std::string& key) const;
  std::vector<int64_t> getArray(const std::string& key) const;
  YaJson getObject(const std::string& key) const;
  std::vector<YaJson> getArrayObject(const std::string& key) const;
  std::string toString() const;

  class YaJsonProxy;

  // 用于赋值的代理对象
  YaJsonProxy operator[](const std::string& key);

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;

  // 私有构造函数，供内部使用
  YaJson(std::unique_ptr<Impl> impl);
};

class YaJson::YaJsonProxy {
 public:
  YaJsonProxy(YaJson& parent, const std::string& key);

  // 赋值操作符
  YaJsonProxy& operator=(const std::string& value);
  YaJsonProxy& operator=(const char* value);
  YaJsonProxy& operator=(int value);
  YaJsonProxy& operator=(int64_t value);
  YaJsonProxy& operator=(bool value);
  YaJsonProxy& operator=(double value);
  YaJsonProxy& operator=(const std::vector<int64_t>& value);
  YaJsonProxy& operator=(const YaJson& value);

 private:
  YaJson& m_parent;
  std::string m_key;

  friend std::ostream& operator<<(std::ostream& os, const YaJsonProxy& proxy);
};

std::ostream& operator<<(std::ostream& os, const YaJson::YaJsonProxy& proxy);

}  // namespace ya

#endif  // YAJSON_H
