#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "yoauthorize/core/machine_identity.h"

namespace core = yoauthorize::core;

namespace {

using ReadResult = core::MachineIdentityReadResult;

core::MachineIdentityReadFunction readerFor(
    std::unordered_map<std::string, ReadResult> files) {
  return [files = std::move(files)](std::string_view path) {
    const auto found = files.find(std::string(path));
    if (found == files.end()) {
      return ReadResult{.status = core::MachineIdentityReadStatus::Missing};
    }
    return found->second;
  };
}

core::LinuxMachineIdentityConfig requiredConfig() {
  return {.machine_id_path = "machine-id",
          .product_uuid_path = "product-uuid",
          .require_machine_id = true,
          .require_product_uuid = true};
}

}  // namespace

TEST(MachineIdentityTest, MatchesStableNormalizedVector) {
  const auto config = requiredConfig();
  const auto mixed_case = readerFor({
      {"machine-id",
       {.status = core::MachineIdentityReadStatus::Success,
        .contents = " 0123456789ABCDEF0123456789abcdef\n"}},
      {"product-uuid",
       {.status = core::MachineIdentityReadStatus::Success,
        .contents = "AABBCCDD-EEFF-0011-2233-445566778899\r\n"}},
  });
  const auto canonical = readerFor({
      {"machine-id",
       {.status = core::MachineIdentityReadStatus::Success,
        .contents = "0123456789abcdef0123456789abcdef"}},
      {"product-uuid",
       {.status = core::MachineIdentityReadStatus::Success,
        .contents = "aabbccdd-eeff-0011-2233-445566778899"}},
  });

  const auto first = core::collectLinuxMachineIdentity(config, mixed_case);
  const auto second = core::collectLinuxMachineIdentity(config, canonical);

  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_EQ(first.value.exact_id, second.value.exact_id);
  EXPECT_EQ(first.value.exact_id,
            "0f6ff091996d653cfa6146c52218bcddc59d50f6597accb3bac3315d7d1b5f0a");
  EXPECT_EQ(first.value.components[0].status,
            core::MachineIdentityComponentStatus::Available);
  EXPECT_EQ(first.value.components[1].status,
            core::MachineIdentityComponentStatus::Available);
}

TEST(MachineIdentityTest, ReportsOptionalMissingSourceWithoutRawValues) {
  const auto reader = readerFor({
      {"machine-id",
       {.status = core::MachineIdentityReadStatus::Success,
        .contents = "0123456789abcdef0123456789abcdef"}},
  });
  auto config = requiredConfig();
  config.require_product_uuid = false;

  const auto result = core::collectLinuxMachineIdentity(config, reader);

  ASSERT_TRUE(result);
  EXPECT_FALSE(result.value.exact_id.empty());
  EXPECT_EQ(result.value.components[1].type,
            core::MachineIdentityComponentType::ProductUuid);
  EXPECT_EQ(result.value.components[1].status,
            core::MachineIdentityComponentStatus::Missing);
}

TEST(MachineIdentityTest, FailsExplicitlyWhenRequiredSourceIsMissing) {
  const auto reader = readerFor({
      {"machine-id",
       {.status = core::MachineIdentityReadStatus::Success,
        .contents = "0123456789abcdef0123456789abcdef"}},
  });

  const auto result =
      core::collectLinuxMachineIdentity(requiredConfig(), reader);

  EXPECT_EQ(result.error, core::ErrorCode::MachineIdentityUnavailable);
  EXPECT_TRUE(result.value.exact_id.empty());
  EXPECT_EQ(result.value.components[1].status,
            core::MachineIdentityComponentStatus::Missing);
}

TEST(MachineIdentityTest, DistinguishesUnreadableAndTypeSpecificInvalidData) {
  const auto unreadable = readerFor({
      {"machine-id",
       {.status = core::MachineIdentityReadStatus::Failure,
        .contents = "not exposed"}},
      {"product-uuid",
       {.status = core::MachineIdentityReadStatus::Success,
        .contents = "0123456789abcdef0123456789abcdef"}},
  });

  const auto result =
      core::collectLinuxMachineIdentity(requiredConfig(), unreadable);

  EXPECT_EQ(result.error, core::ErrorCode::MachineIdentityUnavailable);
  EXPECT_EQ(result.value.components[0].status,
            core::MachineIdentityComponentStatus::Unreadable);
  EXPECT_EQ(result.value.components[1].status,
            core::MachineIdentityComponentStatus::Invalid);
}

TEST(MachineIdentityTest, RejectsIdentityWithoutAnyAvailableComponent) {
  auto config = requiredConfig();
  config.require_machine_id = false;
  config.require_product_uuid = false;

  const auto result = core::collectLinuxMachineIdentity(config, readerFor({}));

  EXPECT_EQ(result.error, core::ErrorCode::MachineIdentityUnavailable);
  EXPECT_TRUE(result.value.exact_id.empty());
}
