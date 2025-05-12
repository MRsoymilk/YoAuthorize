#include "yaconfig.h"

#include <iostream>
#include <toml++/toml.hpp>

class YaConfig::Impl {
 public:
  bool loadConfig(const std::string& path) {
    try {
      config = toml::parse_file(path);
      return true;
    } catch (const toml::parse_error& err) {
      std::cerr << "TOML parse error: " << err.description() << " at "
                << err.source().begin << "\n";
      return false;
    }
  }

  std::string getString(const std::string& key, const std::string& def) {
    auto val = config.at_path(key).value<std::string>();
    return val.value_or(def);
  }

  int getInt(const std::string& key, int def) {
    auto val = config.at_path(key).value<int>();
    return val.value_or(def);
  }

  std::vector<std::string> getArrayString(const std::string& key) {
    std::vector<std::string> result;
    auto node = config.at_path(key);
    if (auto arr = node.as_array()) {
      for (const auto& item : *arr) {
        if (auto val = item.value<std::string>(); val.has_value()) {
          result.push_back(*val);
        }
      }
    }
    return result;
  }

  std::vector<int> getArrayInt(const std::string& key) {
    std::vector<int> result;
    auto node = config.at_path(key);
    if (auto arr = node.as_array()) {
      for (const auto& item : *arr) {
        if (auto val = item.value<int>(); val.has_value()) {
          result.push_back(*val);
        }
      }
    }
    return result;
  }

  std::vector<YaVariant> getArrayVariant(const std::string& key) {
    std::vector<YaVariant> result;
    auto node = config.at_path(key);
    if (auto arr = node.as_array()) {
      for (const auto& item : *arr) {
        if (item.is_integer()) {
          result.emplace_back(*item.value<int64_t>());
        } else if (item.is_floating_point()) {
          result.emplace_back(*item.value<float>());
        } else if (item.is_string()) {
          result.emplace_back(*item.value<std::string>());
        } else if (item.is_boolean()) {
          result.emplace_back(*item.value<bool>());
        } else if (item.is_date() || item.is_time() || item.is_date_time()) {
          result.emplace_back(*item.value<std::string>());
        } else {
          std::cerr << "Unsupported type in array: " << item.type() << "\n";
        }
      }
    }
    return result;
  }

  toml::table toToml(const std::string& str) { return toml::parse(str); }

  std::string toJson(const std::string& str) {
    auto tbl = toToml(str);
    std::ostringstream oss;
    oss << toml::json_formatter{tbl};
    return oss.str();
  }

  std::string toYaml(const std::string& str) {
    auto tbl = toToml(str);
    std::ostringstream oss;
    oss << toml::yaml_formatter{tbl};
    return oss.str();
  }

 private:
  toml::table config;
};

YaConfig::YaConfig() : m_impl(std::make_unique<Impl>()) {}
YaConfig::~YaConfig() = default;

YaConfig& YaConfig::instance() {
  static YaConfig inst;
  return inst;
}

bool YaConfig::loadConfig(const std::string& path) {
  return m_impl->loadConfig(path);
}

std::string YaConfig::getString(const std::string& key,
                                const std::string& def) {
  return m_impl->getString(key, def);
}

int YaConfig::getInt(const std::string& key, int def) {
  return m_impl->getInt(key, def);
}

std::string YaConfig::toJson(const std::string& str) {
  return m_impl->toJson(str);
}

std::string YaConfig::toYaml(const std::string& str) {
  return m_impl->toYaml(str);
}

std::vector<YaVariant> YaConfig::getArrayVariant(const std::string& key) {
  return m_impl->getArrayVariant(key);
}

std::vector<std::string> YaConfig::getArrayString(const std::string& key) {
  return m_impl->getArrayString(key);
}

std::vector<int> YaConfig::getArrayInt(const std::string& key) {
  return m_impl->getArrayInt(key);
}
