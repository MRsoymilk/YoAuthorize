#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>

#include "yoauthorize/core/status.h"

namespace yoauthorize::core {

enum class MachineIdentityComponentType { MachineId, ProductUuid };

enum class MachineIdentityComponentStatus {
  Available,
  Missing,
  Unreadable,
  Invalid,
};

enum class MachineIdentityReadStatus { Success, Missing, Failure };

struct MachineIdentityReadResult {
  MachineIdentityReadStatus status = MachineIdentityReadStatus::Failure;
  std::string contents;
};

using MachineIdentityReadFunction =
    std::function<MachineIdentityReadResult(std::string_view path)>;

struct LinuxMachineIdentityConfig {
  std::string machine_id_path = "/etc/machine-id";
  std::string product_uuid_path = "/sys/class/dmi/id/product_uuid";
  bool require_machine_id = true;
  bool require_product_uuid = false;
};

struct MachineIdentityComponent {
  MachineIdentityComponentType type = MachineIdentityComponentType::MachineId;
  MachineIdentityComponentStatus status =
      MachineIdentityComponentStatus::Missing;
};

struct MachineIdentity {
  std::string exact_id;
  std::array<MachineIdentityComponent, 2> components{};
};

Result<MachineIdentity> collectLinuxMachineIdentity(
    const LinuxMachineIdentityConfig& config = {},
    MachineIdentityReadFunction read_file = {});

}  // namespace yoauthorize::core
