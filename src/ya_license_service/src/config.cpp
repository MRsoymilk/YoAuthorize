#include "yoauthorize/service/config.h"

#include <limits>
#include <optional>
#include <toml++/toml.hpp>
#include <unordered_set>

namespace yoauthorize::service {
namespace {

std::optional<std::string> requiredString(const toml::table& table,
                                          std::string_view key) {
  auto value = table[key].value<std::string>();
  if (!value || value->empty()) {
    return std::nullopt;
  }
  return value;
}

template <typename T>
bool assignInteger(const toml::table& table, std::string_view key, T& output,
                   std::uint64_t minimum, std::uint64_t maximum) {
  const auto node = table[key];
  if (!node) {
    return true;
  }
  const auto value = node.value<std::int64_t>();
  if (!value || *value < 0 || static_cast<std::uint64_t>(*value) < minimum ||
      static_cast<std::uint64_t>(*value) > maximum) {
    return false;
  }
  output = static_cast<T>(*value);
  return true;
}

bool parseIds(const toml::node_view<const toml::node>& node,
              std::vector<std::uint32_t>& output) {
  const auto* values = node.as_array();
  if (!values) {
    return false;
  }
  std::unordered_set<std::uint32_t> unique;
  for (const auto& item : *values) {
    const auto value = item.value<std::int64_t>();
    if (!value || *value < 0 ||
        static_cast<std::uint64_t>(*value) >
            std::numeric_limits<std::uint32_t>::max()) {
      return false;
    }
    const auto id = static_cast<std::uint32_t>(*value);
    if (!unique.insert(id).second) {
      return false;
    }
    output.push_back(id);
  }
  return true;
}

}  // namespace

ConfigResult<ServiceConfig> loadConfig(const std::filesystem::path& path) {
  std::error_code filesystem_error;
  const auto status = std::filesystem::symlink_status(path, filesystem_error);
  if (filesystem_error || !std::filesystem::is_regular_file(status)) {
    return {.error = ConfigError::Io};
  }
  toml::table document;
  try {
    document = toml::parse_file(path.string());
  } catch (const toml::parse_error&) {
    return {.error = ConfigError::Parse};
  } catch (const std::exception&) {
    return {.error = ConfigError::Io};
  }

  const auto* service = document["service"].as_table();
  const auto* keys = document["license_keys"].as_array();
  if (!service || !keys || keys->empty()) {
    return {.error = ConfigError::MissingField};
  }

  const auto identity_id = requiredString(*service, "identity_key_id");
  const auto identity_path = requiredString(*service, "identity_private_key");
  const auto license_path = requiredString(*service, "license");
  const auto product_id = requiredString(*service, "product_id");
  const auto allowed_uids = (*service)["allowed_uids"];
  if (!identity_id || !identity_path || !license_path || !product_id ||
      identity_id->size() > std::numeric_limits<std::uint16_t>::max() ||
      !allowed_uids) {
    return {.error = ConfigError::MissingField};
  }

  ServiceConfig config{
      .identity_key_id = *identity_id,
      .identity_private_key_path = *identity_path,
      .license_path = *license_path,
      .product_id = *product_id,
  };
  if (const auto value = requiredString(*service, "socket")) {
    config.socket_path = *value;
  } else if ((*service)["socket"]) {
    return {.error = ConfigError::InvalidValue};
  }
  if (!parseIds(allowed_uids, config.allowed_uids) ||
      config.allowed_uids.empty()) {
    return {.error = ConfigError::InvalidValue};
  }
  if (const auto allowed_gids = (*service)["allowed_gids"];
      allowed_gids && !parseIds(allowed_gids, config.allowed_gids)) {
    return {.error = ConfigError::InvalidValue};
  }
  if (!assignInteger(*service, "socket_mode", config.socket_mode, 0, 0777) ||
      !assignInteger(*service, "backlog", config.backlog, 1, 4096) ||
      !assignInteger(*service, "max_connections", config.max_connections, 1,
                     10000) ||
      !assignInteger(*service, "handshake_timeout_ms",
                     config.handshake_timeout_ms, 100, 60000) ||
      !assignInteger(*service, "io_timeout_ms", config.io_timeout_ms, 100,
                     60000) ||
      !assignInteger(*service, "idle_timeout_ms", config.idle_timeout_ms, 1000,
                     3600000) ||
      !assignInteger(*service, "heartbeat_interval_ms",
                     config.heartbeat_interval_ms, 1000, 60000) ||
      !assignInteger(*service, "heartbeat_timeout_ms",
                     config.heartbeat_timeout_ms, 2000, 300000) ||
      config.heartbeat_timeout_ms <= config.heartbeat_interval_ms) {
    return {.error = ConfigError::InvalidValue};
  }

  const auto* machine = document["machine_identity"].as_table();
  if (machine) {
    if (const auto value = requiredString(*machine, "machine_id_path")) {
      config.machine_identity.machine_id_path = *value;
    } else if ((*machine)["machine_id_path"]) {
      return {.error = ConfigError::InvalidValue};
    }
    if (const auto value = requiredString(*machine, "product_uuid_path")) {
      config.machine_identity.product_uuid_path = *value;
    } else if ((*machine)["product_uuid_path"]) {
      return {.error = ConfigError::InvalidValue};
    }
    const auto required_machine =
        (*machine)["require_machine_id"].value<bool>();
    const auto required_uuid = (*machine)["require_product_uuid"].value<bool>();
    if (((*machine)["require_machine_id"] && !required_machine) ||
        ((*machine)["require_product_uuid"] && !required_uuid)) {
      return {.error = ConfigError::InvalidValue};
    }
    if (required_machine) {
      config.machine_identity.require_machine_id = *required_machine;
    }
    if (required_uuid) {
      config.machine_identity.require_product_uuid = *required_uuid;
    }
  }

  std::unordered_set<std::string> key_ids;
  for (const auto& item : *keys) {
    const auto* key = item.as_table();
    if (!key) {
      return {.error = ConfigError::InvalidValue};
    }
    const auto id = requiredString(*key, "id");
    const auto key_path = requiredString(*key, "public_key");
    if (!id || !key_path) {
      return {.error = ConfigError::MissingField};
    }
    if (!key_ids.insert(*id).second) {
      return {.error = ConfigError::DuplicateKeyId};
    }
    config.license_keys.push_back({.id = *id, .public_key_path = *key_path});
  }
  return {.value = std::move(config)};
}

}  // namespace yoauthorize::service
