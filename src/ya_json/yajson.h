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

  void loadFromString(const std::string& str);
  void loadFromFile(const std::string& file);

  std::string getString(const std::string& key);
  int64_t getInt(const std::string& key);
  bool getBool(const std::string& key);
  double getDouble(const std::string& key);
  std::vector<int64_t> getArray(const std::string& key);
  YaJson getObject(const std::string& key);
  std::vector<YaJson> getArrayObject(const std::string& key);

  class YaJsonProxy;

  // Overload operator[] to return a proxy for assignment
  YaJsonProxy operator[](const std::string& key);

 private:
  class Impl;
  std::unique_ptr<Impl> m_impl;

  // Private constructor for internal use
  YaJson(std::unique_ptr<Impl> impl);
};

class YaJson::YaJsonProxy {
 public:
  YaJsonProxy(YaJson& parent, const std::string& key);

  // Assignment operators for different types
  YaJsonProxy& operator=(const std::string& value);
  YaJsonProxy& operator=(const char* value);
  YaJsonProxy& operator=(int value);  // Added for int literals
  YaJsonProxy& operator=(int64_t value);
  YaJsonProxy& operator=(bool value);
  YaJsonProxy& operator=(double value);
  YaJsonProxy& operator=(const std::vector<int64_t>& value);

 private:
  YaJson& m_parent;
  std::string m_key;
};

}  // namespace ya

#endif  // YAJSON_H
