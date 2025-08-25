#include "yajson.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

namespace ya {

class YaJson::Impl {
 public:
  Impl() { m_document = nlohmann::json::object(); }

  void loadFromString(const std::string& str) {
    try {
      m_document = nlohmann::json::parse(str);
    } catch (const nlohmann::json::exception& e) {
      throw std::runtime_error("JSON parse error: " + std::string(e.what()));
    }
  }

  void loadFromFile(const std::string& file) {
    std::ifstream ifs(file);
    if (!ifs.is_open()) {
      throw std::runtime_error("Failed to open file: " + file);
    }
    try {
      ifs >> m_document;
    } catch (const nlohmann::json::exception& e) {
      throw std::runtime_error("JSON parse error in file " + file + ": " +
                               e.what());
    }
  }

  std::string getString(const std::string& key) const {
    try {
      return m_document.at(key).get<std::string>();
    } catch (const nlohmann::json::out_of_range& e) {
      throw std::runtime_error("Key not found: " + key);
    } catch (const nlohmann::json::type_error& e) {
      throw std::runtime_error("Type mismatch for key " + key +
                               ": expected string");
    }
  }

  int64_t getInt(const std::string& key) const {
    try {
      return m_document.at(key).get<int64_t>();
    } catch (const nlohmann::json::out_of_range& e) {
      throw std::runtime_error("Key not found: " + key);
    } catch (const nlohmann::json::type_error& e) {
      throw std::runtime_error("Type mismatch for key " + key +
                               ": expected integer");
    }
  }

  bool getBool(const std::string& key) const {
    try {
      return m_document.at(key).get<bool>();
    } catch (const nlohmann::json::out_of_range& e) {
      throw std::runtime_error("Key not found: " + key);
    } catch (const nlohmann::json::type_error& e) {
      throw std::runtime_error("Type mismatch for key " + key +
                               ": expected boolean");
    }
  }

  double getDouble(const std::string& key) const {
    try {
      return m_document.at(key).get<double>();
    } catch (const nlohmann::json::out_of_range& e) {
      throw std::runtime_error("Key not found: " + key);
    } catch (const nlohmann::json::type_error& e) {
      throw std::runtime_error("Type mismatch for key " + key +
                               ": expected double");
    }
  }

  std::vector<int64_t> getArray(const std::string& key) const {
    try {
      std::vector<int64_t> result;
      for (const auto& item : m_document.at(key)) {
        if (!item.is_number_integer()) {
          throw std::runtime_error("Array element for key " + key +
                                   " is not an integer");
        }
        result.push_back(item.get<int64_t>());
      }
      return result;
    } catch (const nlohmann::json::out_of_range& e) {
      throw std::runtime_error("Key not found: " + key);
    } catch (const nlohmann::json::type_error& e) {
      throw std::runtime_error("Type mismatch for key " + key +
                               ": expected array");
    }
  }

  std::unique_ptr<Impl> getObject(const std::string& key) const {
    try {
      if (!m_document.at(key).is_object()) {
        throw std::runtime_error("Value for key " + key + " is not an object");
      }
      auto impl = std::make_unique<Impl>();
      impl->m_document = m_document.at(key);
      return impl;
    } catch (const nlohmann::json::out_of_range& e) {
      throw std::runtime_error("Key not found: " + key);
    }
  }

  std::vector<std::unique_ptr<Impl>> getArrayObject(
      const std::string& key) const {
    try {
      std::vector<std::unique_ptr<Impl>> result;
      for (const auto& elem : m_document.at(key)) {
        if (!elem.is_object()) {
          throw std::runtime_error("Array element for key " + key +
                                   " is not an object");
        }
        auto impl = std::make_unique<Impl>();
        impl->m_document = elem;
        result.push_back(std::move(impl));
      }
      return result;
    } catch (const nlohmann::json::out_of_range& e) {
      throw std::runtime_error("Key not found: " + key);
    } catch (const nlohmann::json::type_error& e) {
      throw std::runtime_error("Type mismatch for key " + key +
                               ": expected array of objects");
    }
  }

  void setString(const std::string& key, const std::string& value) {
    m_document[key] = value;
  }

  void setInt(const std::string& key, int64_t value) {
    m_document[key] = value;
  }

  void setBool(const std::string& key, bool value) { m_document[key] = value; }

  void setDouble(const std::string& key, double value) {
    m_document[key] = value;
  }

  void setArray(const std::string& key, const std::vector<int64_t>& value) {
    m_document[key] = value;
  }

  void setObject(const std::string& key, const YaJson& value) {
    try {
      m_document[key] = nlohmann::json::parse(value.toString());
    } catch (const nlohmann::json::exception& e) {
      throw std::runtime_error("Failed to set object for key " + key + ": " +
                               e.what());
    }
  }

  std::string toString() const {
    try {
      return m_document.dump();
    } catch (const nlohmann::json::exception& e) {
      throw std::runtime_error("Failed to serialize JSON: " +
                               std::string(e.what()));
    }
  }

 private:
  nlohmann::json m_document;
};

// --------------------- YaJson 接口 ---------------------

YaJson::YaJson() : m_impl(std::make_unique<Impl>()) {}

YaJson::YaJson(const std::string& json) : m_impl(std::make_unique<Impl>()) {
  if (json.size() > 5 && json.substr(json.size() - 5) == ".json") {
    loadFromFile(json);
  } else {
    loadFromString(json);
  }
}

YaJson::YaJson(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}

YaJson::~YaJson() = default;

void YaJson::loadFromString(const std::string& str) {
  m_impl->loadFromString(str);
}

void YaJson::loadFromFile(const std::string& file) {
  m_impl->loadFromFile(file);
}

std::string YaJson::getString(const std::string& key) const {
  return m_impl->getString(key);
}

int64_t YaJson::getInt(const std::string& key) const {
  return m_impl->getInt(key);
}

bool YaJson::getBool(const std::string& key) const {
  return m_impl->getBool(key);
}

double YaJson::getDouble(const std::string& key) const {
  return m_impl->getDouble(key);
}

std::vector<int64_t> YaJson::getArray(const std::string& key) const {
  return m_impl->getArray(key);
}

YaJson YaJson::getObject(const std::string& key) const {
  return YaJson(m_impl->getObject(key));
}

std::vector<YaJson> YaJson::getArrayObject(const std::string& key) const {
  std::vector<YaJson> objs;
  for (auto& impl : m_impl->getArrayObject(key)) {
    objs.emplace_back(YaJson(std::move(impl)));
  }
  return objs;
}

YaJson::YaJson(YaJson&& other) noexcept : m_impl(std::move(other.m_impl)) {}

YaJson& YaJson::operator=(YaJson&& other) noexcept {
  if (this != &other) {
    m_impl = std::move(other.m_impl);
  }
  return *this;
}

YaJson::YaJsonProxy YaJson::operator[](const std::string& key) {
  return YaJsonProxy(*this, key);
}

std::string YaJson::toString() const { return m_impl->toString(); }

// --------------------- Proxy 实现 ---------------------

YaJson::YaJsonProxy::YaJsonProxy(YaJson& parent, const std::string& key)
    : m_parent(parent), m_key(key) {}

YaJson::YaJsonProxy& YaJson::YaJsonProxy::operator=(const std::string& value) {
  m_parent.m_impl->setString(m_key, value);
  return *this;
}

YaJson::YaJsonProxy& YaJson::YaJsonProxy::operator=(const char* value) {
  m_parent.m_impl->setString(m_key, value);
  return *this;
}

YaJson::YaJsonProxy& YaJson::YaJsonProxy::operator=(int value) {
  m_parent.m_impl->setInt(m_key, static_cast<int64_t>(value));
  return *this;
}

YaJson::YaJsonProxy& YaJson::YaJsonProxy::operator=(int64_t value) {
  m_parent.m_impl->setInt(m_key, value);
  return *this;
}

YaJson::YaJsonProxy& YaJson::YaJsonProxy::operator=(bool value) {
  m_parent.m_impl->setBool(m_key, value);
  return *this;
}

YaJson::YaJsonProxy& YaJson::YaJsonProxy::operator=(double value) {
  m_parent.m_impl->setDouble(m_key, value);
  return *this;
}

YaJson::YaJsonProxy& YaJson::YaJsonProxy::operator=(
    const std::vector<int64_t>& value) {
  m_parent.m_impl->setArray(m_key, value);
  return *this;
}

YaJson::YaJsonProxy& YaJson::YaJsonProxy::operator=(const YaJson& value) {
  m_parent.m_impl->setObject(m_key, value);
  return *this;
}

std::ostream& operator<<(std::ostream& os, const YaJson::YaJsonProxy& proxy) {
  os << proxy.m_parent.getString(proxy.m_key);
  return os;
}

}  // namespace ya
