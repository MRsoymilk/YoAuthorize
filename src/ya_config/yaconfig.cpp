#include "yaconfig.h"

#include <fstream>
#include <iostream>
#include <toml++/toml.hpp>

namespace ya {

class YaConfig::Impl {
 public:
  toml::table config;

  bool loadConfig(const std::string& path) {
    try {
      config = toml::parse_file(path);
      return true;
    } catch (const toml::parse_error& err) {
      std::cerr << "TOML parse error: " << err.description() << " at "
                << err.source().begin << std::endl;
      return false;
    }
  }

  bool saveConfig(const std::string& path) {
    try {
      std::ofstream file(path, std::ios::out | std::ios::trunc);
      if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
        return false;
      }
      file << config;
      file.close();
      return true;
    } catch (const std::exception& e) {
      std::cerr << "Error saving config to '" << path << "': " << e.what()
                << std::endl;
      return false;
    }
  }

  std::vector<YaVariant> getArrayVariant(const std::string& key) {
    std::vector<YaVariant> result;
    try {
      auto node = config.at_path(key);
      if (auto arr = node.as_array()) {
        for (const auto& item : *arr) {
          if (item.is_integer()) {
            if (auto val = item.value<int>()) {
              result.emplace_back(*val);
            }
          } else if (item.is_floating_point()) {
            if (auto val = item.value<double>()) {
              result.emplace_back(*val);
            }
          } else if (item.is_string()) {
            if (auto val = item.value<std::string>()) {
              result.emplace_back(*val);
            }
          } else if (item.is_boolean()) {
            if (auto val = item.value<bool>()) {
              result.emplace_back(*val);
            }
          } else {
            std::cerr << "Unsupported type in array for key '" << key
                      << "': " << item.type() << std::endl;
          }
        }
      }
    } catch (const std::exception& e) {
      std::cerr << "Error getting variant array for key '" << key
                << "': " << e.what() << std::endl;
    }
    return result;
  }

  std::string toJson() const {
    std::ostringstream oss;
    oss << toml::json_formatter{config};
    return oss.str();
  }

  std::string toYaml() const {
    std::ostringstream oss;
    oss << toml::yaml_formatter{config};
    return oss.str();
  }
};

template <typename T>
T YaConfig::get(const std::string& key, const T& defaultValue) {
  try {
    auto node = m_impl->config.at_path(key);
    if (auto val = node.value<T>()) {
      return *val;
    }
    return defaultValue;
  } catch (const std::exception& e) {
    std::cerr << "Error getting key '" << key << "': " << e.what() << std::endl;
    return defaultValue;
  }
}

template <typename T>
std::vector<T> YaConfig::getArray(const std::string& key) {
  std::vector<T> result;
  try {
    auto node = m_impl->config.at_path(key);
    if (auto arr = node.as_array()) {
      for (const auto& item : *arr) {
        if (auto val = item.value<T>()) {
          result.push_back(*val);
        }
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error getting array for key '" << key << "': " << e.what()
              << std::endl;
  }
  return result;
}

template <typename T>
void YaConfig::set(const std::string& key, const T& value) {
  try {
    m_impl->config.insert_or_assign(key, value);
  } catch (const std::exception& e) {
    std::cerr << "Error setting key '" << key << "': " << e.what() << std::endl;
  }
}

template <typename T>
void YaConfig::setArray(const std::string& key, const std::vector<T>& values) {
  try {
    toml::array arr;
    for (const auto& value : values) {
      arr.push_back(value);
    }
    m_impl->config.insert_or_assign(key, arr);
  } catch (const std::exception& e) {
    std::cerr << "Error setting array for key '" << key << "': " << e.what()
              << std::endl;
  }
}

// get
template int YaConfig::get<int>(const std::string&, const int&);
template double YaConfig::get<double>(const std::string&, const double&);
template std::string YaConfig::get<std::string>(const std::string&,
                                                const std::string&);
template bool YaConfig::get<bool>(const std::string&, const bool&);
template std::vector<int> YaConfig::getArray<int>(const std::string&);
template std::vector<double> YaConfig::getArray<double>(const std::string&);
template std::vector<std::string> YaConfig::getArray<std::string>(
    const std::string&);
template std::vector<bool> YaConfig::getArray<bool>(const std::string&);
// set
template void YaConfig::set<int>(const std::string&, const int&);
template void YaConfig::set<double>(const std::string&, const double&);
template void YaConfig::set<std::string>(const std::string&,
                                         const std::string&);
template void YaConfig::set<bool>(const std::string&, const bool&);
template void YaConfig::setArray<int>(const std::string&,
                                      const std::vector<int>&);
template void YaConfig::setArray<double>(const std::string&,
                                         const std::vector<double>&);
template void YaConfig::setArray<std::string>(const std::string&,
                                              const std::vector<std::string>&);
template void YaConfig::setArray<bool>(const std::string&,
                                       const std::vector<bool>&);

YaConfig::YaConfig() : m_impl(std::make_unique<Impl>()) {}
YaConfig::~YaConfig() = default;

YaConfig& YaConfig::instance() {
  static YaConfig inst;
  return inst;
}

bool YaConfig::load(const std::string& path) {
  return m_impl->loadConfig(path);
}

bool YaConfig::save(const std::string& path) {
  return m_impl->saveConfig(path);
}

std::string YaConfig::toJson() const { return m_impl->toJson(); }

std::string YaConfig::toYaml() const { return m_impl->toYaml(); }

std::vector<YaVariant> YaConfig::getArrayVariant(const std::string& key) {
  return m_impl->getArrayVariant(key);
}

}  // namespace ya
