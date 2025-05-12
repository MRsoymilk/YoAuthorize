#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <variant>
#include <vector>

#include "yaconfig.h"

class YaConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::ofstream file("test_config.toml");
    file << "[test]\n"
         << "integer = 42\n"
         << "floating = 3.14\n"
         << "text = \"hello\"\n"
         << "flag = true\n"
         << "numbers = [1, 2.5, \"three\", true]\n"
         << "integers = [10, 20, 30]\n";
    file.close();
  }

  void TearDown() override {
    std::remove("test_config.toml");
    std::remove("output_config.toml");
  }

  const std::string config_path = "test_config.toml";
  const std::string output_path = "output_config.toml";
};

TEST_F(YaConfigTest, LoadConfig) {
  EXPECT_TRUE(YA_CONFIG.load(config_path)) << "Failed to load config";
}

TEST_F(YaConfigTest, GetScalarValues) {
  ASSERT_TRUE(YA_CONFIG.load(config_path));

  EXPECT_EQ(YA_CONFIG.get<int>("test.integer", 0), 42);
  EXPECT_DOUBLE_EQ(YA_CONFIG.get<double>("test.floating", 0.0), 3.14);
  EXPECT_EQ(YA_CONFIG.get<std::string>("test.text", ""), "hello");
  EXPECT_TRUE(YA_CONFIG.get<bool>("test.flag", false));

  EXPECT_EQ(YA_CONFIG.get<int>("test.missing", 100), 100);
}

TEST_F(YaConfigTest, GetArrayValues) {
  ASSERT_TRUE(YA_CONFIG.load(config_path));

  std::vector<int> expected_integers = {10, 20, 30};
  EXPECT_EQ(YA_CONFIG.getArray<int>("test.integers"), expected_integers);

  std::vector<std::string> empty_array =
      YA_CONFIG.getArray<std::string>("test.missing_array");
  EXPECT_TRUE(empty_array.empty());
}

TEST_F(YaConfigTest, GetArrayVariant) {
  ASSERT_TRUE(YA_CONFIG.load(config_path));

  auto arr = YA_CONFIG.getArrayVariant("test.numbers");
  ASSERT_EQ(arr.size(), 4) << "Array size should be 4";

  std::cout << std::fixed;
  std::visit([](auto&& arg) { std::cout << arg << std::endl; }, arr[0]);

  EXPECT_TRUE(std::holds_alternative<int>(arr[0]));
  EXPECT_EQ(std::get<int>(arr[0]), 1);

  EXPECT_TRUE(std::holds_alternative<double>(arr[1]));
  EXPECT_DOUBLE_EQ(std::get<double>(arr[1]), 2.5);

  EXPECT_TRUE(std::holds_alternative<std::string>(arr[2]));
  EXPECT_EQ(std::get<std::string>(arr[2]), "three");

  EXPECT_TRUE(std::holds_alternative<bool>(arr[3]));
  EXPECT_TRUE(std::get<bool>(arr[3]));

  for (const auto& v : arr) {
    std::visit([](auto&& arg) { std::cout << arg << " "; }, v);
  }
  std::cout << std::endl;
}

TEST_F(YaConfigTest, SetAndSave) {
  ASSERT_TRUE(YA_CONFIG.load(config_path));

  YA_CONFIG.set<int>("test.new_integer", 100);
  YA_CONFIG.set<std::string>("test.new_text", std::string("world"));
  YA_CONFIG.setArray("test.new_array", std::vector<int>{40, 50, 60});

  EXPECT_TRUE(YA_CONFIG.save(output_path)) << "Failed to save config";

  EXPECT_TRUE(YA_CONFIG.load(output_path));
  EXPECT_EQ(YA_CONFIG.get<int>("test.new_integer"), 100);
  EXPECT_EQ(YA_CONFIG.get<std::string>("test.new_text"), "world");
  EXPECT_EQ(YA_CONFIG.getArray<int>("test.new_array"),
            (std::vector<int>{40, 50, 60}));
}

TEST_F(YaConfigTest, ToJsonAndYaml) {
  ASSERT_TRUE(YA_CONFIG.load(config_path));

  std::string json = YA_CONFIG.toJson();
  EXPECT_TRUE(json.find("\"integer\": 42") != std::string::npos);
  EXPECT_TRUE(json.find("\"text\": \"hello\"") != std::string::npos);

  std::string yaml = YA_CONFIG.toYaml();
  EXPECT_TRUE(yaml.find("integer: 42") != std::string::npos);
  EXPECT_TRUE(yaml.find("text: hello") != std::string::npos);
}

TEST_F(YaConfigTest, InvalidPath) {
  EXPECT_FALSE(YA_CONFIG.load("nonexistent.toml"));

  EXPECT_FALSE(YA_CONFIG.save("/invalid/path/config.toml"));
}
