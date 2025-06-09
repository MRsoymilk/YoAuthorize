#include "yajson.h"

#include <simdjson.h>

#include <nlohmann/json.hpp>
#include <sstream>

namespace ya {

class YaJson::Impl {
 public:
  Impl() { loadFromString("{}"); }

  void loadFromString(const std::string& str) {
    auto json_ptr = std::make_shared<simdjson::padded_string>(str);
    m_json_data = json_ptr;
    auto doc = m_parser.parse(*json_ptr);
    m_document = std::move(doc.value());
  }

  void loadFromFile(const std::string& file) {
    auto result = simdjson::padded_string::load(file);
    auto json_ptr =
        std::make_shared<simdjson::padded_string>(std::move(result.value()));
    m_json_data = json_ptr;
    auto doc = m_parser.parse(*json_ptr);
    m_document = std::move(doc.value());
  }

  std::string getString(const std::string& key) {
    auto val = m_document[key];
    return std::string(val.value().get_string().value());
  }

  int64_t getInt(const std::string& key) {
    auto val = m_document[key];
    return val.value().get_int64().value();
  }

  bool getBool(const std::string& key) {
    auto val = m_document[key];
    return val.value().get_bool().value();
  }

  double getDouble(const std::string& key) {
    auto val = m_document[key];
    return val.value().get_double().value();
  }

  std::vector<int64_t> getArray(const std::string& key) {
    std::vector<int64_t> result;
    auto val = m_document[key];
    simdjson::dom::array arr = val.value().get_array();
    for (auto item : arr) {
      result.push_back(item.get_int64().value());
    }
    return result;
  }

  static std::unique_ptr<Impl> fromElement(
      const simdjson::dom::element& element,
      std::shared_ptr<simdjson::padded_string> json_data) {
    if (!element.is_object()) {
      throw std::runtime_error("Element is not an object.");
    }
    auto impl = std::make_unique<Impl>();
    impl->m_document = element;
    impl->m_json_data = std::move(json_data);
    impl->m_parser = simdjson::dom::parser();
    return impl;
  }

  std::unique_ptr<Impl> getObject(const std::string& key) {
    auto val = m_document[key];
    simdjson::dom::element obj = val.value();
    return Impl::fromElement(obj, m_json_data);
  }

  std::vector<std::unique_ptr<Impl>> getArrayObject(const std::string& key) {
    std::vector<std::unique_ptr<Impl>> result;
    auto arr_val = m_document[key];
    simdjson::dom::array arr = arr_val.value().get_array();
    for (auto elem : arr) {
      if (!elem.is_object()) {
        throw std::runtime_error("Array element is not an object");
      }
      result.push_back(Impl::fromElement(elem, m_json_data));
    }
    return result;
  }

  void setString(const std::string& key, const std::string& value) {
    modifyJson([&](nlohmann::json& j) { j[key] = value; });
  }

  void setInt(const std::string& key, int64_t value) {
    modifyJson([&](nlohmann::json& j) { j[key] = value; });
  }

  void setBool(const std::string& key, bool value) {
    modifyJson([&](nlohmann::json& j) { j[key] = value; });
  }

  void setDouble(const std::string& key, double value) {
    modifyJson([&](nlohmann::json& j) { j[key] = value; });
  }

  void setArray(const std::string& key, const std::vector<int64_t>& value) {
    modifyJson([&](nlohmann::json& j) { j[key] = value; });
  }

  void setObject(const std::string& key, const YaJson& value) {
    modifyJson([&](nlohmann::json& j) {
      nlohmann::json obj = nlohmann::json::parse(value.toString());
      j[key] = obj;
    });
  }

  std::string toString() const {
    std::stringstream ss;
    ss << m_document;
    return ss.str();
  }

 private:
  simdjson::dom::parser m_parser;
  std::shared_ptr<simdjson::padded_string> m_json_data;
  simdjson::dom::element m_document;

  void modifyJson(const std::function<void(nlohmann::json&)>& modifier) {
    std::stringstream ss;
    ss << m_document;
    std::string json_str = ss.str();
    nlohmann::json j = nlohmann::json::parse(json_str);
    modifier(j);
    std::string new_json_str = j.dump();
    auto json_ptr = std::make_shared<simdjson::padded_string>(new_json_str);
    m_json_data = json_ptr;
    auto doc = m_parser.parse(*json_ptr);
    m_document = std::move(doc.value());
  }
};

YaJson::YaJson() : m_impl(std::make_unique<Impl>()) {}

YaJson::YaJson(const std::string& json) : m_impl(std::make_unique<Impl>()) {
  if (json.ends_with(".json")) {
    m_impl->loadFromFile(json);
  } else {
    m_impl->loadFromString(json);
  }
}

YaJson::YaJson(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}

YaJson::~YaJson() {}

void YaJson::loadFromString(const std::string& str) {
  m_impl->loadFromString(str);
}

void YaJson::loadFromFile(const std::string& file) {
  m_impl->loadFromFile(file);
}

std::string YaJson::getString(const std::string& key) {
  return m_impl->getString(key);
}

int64_t YaJson::getInt(const std::string& key) { return m_impl->getInt(key); }

bool YaJson::getBool(const std::string& key) { return m_impl->getBool(key); }

double YaJson::getDouble(const std::string& key) {
  return m_impl->getDouble(key);
}

std::vector<int64_t> YaJson::getArray(const std::string& key) {
  return m_impl->getArray(key);
}

YaJson YaJson::getObject(const std::string& key) {
  return YaJson(m_impl->getObject(key));
}

std::vector<YaJson> YaJson::getArrayObject(const std::string& key) {
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
  try {
    // Try string first
    try {
      os << proxy.m_parent.getString(proxy.m_key);
      return os;
    } catch (...) {
    }

    // Try bool
    try {
      os << (proxy.m_parent.getBool(proxy.m_key) ? "true" : "false");
      return os;
    } catch (...) {
    }

    // Try double
    try {
      os << proxy.m_parent.getDouble(proxy.m_key);
      return os;
    } catch (...) {
    }

    // Try int64
    try {
      os << proxy.m_parent.getInt(proxy.m_key);
      return os;
    } catch (...) {
    }

    // Try array
    try {
      std::vector<int64_t> arr = proxy.m_parent.getArray(proxy.m_key);
      os << "[";
      for (size_t i = 0; i < arr.size(); ++i) {
        os << arr[i];
        if (i < arr.size() - 1) os << ", ";
      }
      os << "]";
      return os;
    } catch (...) {
    }

    // Try object
    try {
      YaJson obj = proxy.m_parent.getObject(proxy.m_key);
      os << obj.toString();
      return os;
    } catch (...) {
    }

    throw std::runtime_error("Key not found or unsupported type: " +
                             proxy.m_key);
  } catch (const std::exception& e) {
    throw std::runtime_error("Error accessing key " + proxy.m_key + ": " +
                             e.what());
  }
}

}  // namespace ya
