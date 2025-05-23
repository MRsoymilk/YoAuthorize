#include <gtest/gtest.h>

#include <string>

#include "yajson.h"

const std::string json_str = R"(
{
  "text": "RT @PostGradProblem: In preparation for the NFL lockout...",
  "neg": -999,
  "pi": 3.1415926,
  "user": {
    "profile_image_url": "http://a1.twimg.com/profile_images/455128973/gCsVUnofNqqyd6tdOGevROvko1_500_normal.jpg",
    "test":[
      {"name": "A"},
      {"name": "B"},
      {"name": "C"}
    ]
  }
}
)";

TEST(YaJsonTest, BasicFields) {
  ya::YaJson json(json_str);

  EXPECT_EQ(json.getString("text"),
            "RT @PostGradProblem: In preparation for the NFL lockout...");
  EXPECT_EQ(json.getInt("neg"), -999);
  EXPECT_DOUBLE_EQ(json.getDouble("pi"), 3.1415926);

  ya::YaJson user = json.getObject("user");
  EXPECT_EQ(user.getString("profile_image_url"),
            "http://a1.twimg.com/profile_images/455128973/"
            "gCsVUnofNqqyd6tdOGevROvko1_500_normal.jpg");

  auto testArray = user.getArrayObject("test");
  ASSERT_EQ(testArray.size(), 3u);
  EXPECT_EQ(testArray[0].getString("name"), "A");
  EXPECT_EQ(testArray[1].getString("name"), "B");
  EXPECT_EQ(testArray[2].getString("name"), "C");
}

TEST(YaJsonTest, SetString) {
  ya::YaJson json(json_str);

  // Update existing key
  json["text"] = "Updated text";
  EXPECT_EQ(json.getString("text"), "Updated text");

  // Add new key
  json["new_text"] = "New string value";
  EXPECT_EQ(json.getString("new_text"), "New string value");
}

TEST(YaJsonTest, SetInt) {
  ya::YaJson json(json_str);

  // Update existing key
  json["neg"] = -123;
  EXPECT_EQ(json.getInt("neg"), -123);

  // Add new key
  json["new_int"] = 456;
  EXPECT_EQ(json.getInt("new_int"), 456);
}

TEST(YaJsonTest, SetDouble) {
  ya::YaJson json(json_str);

  // Update existing key
  json["pi"] = 2.71828;
  EXPECT_DOUBLE_EQ(json.getDouble("pi"), 2.71828);

  // Add new key
  json["new_double"] = 1.414;
  EXPECT_DOUBLE_EQ(json.getDouble("new_double"), 1.414);
}

TEST(YaJsonTest, SetBool) {
  ya::YaJson json(json_str);

  // Add new key
  json["is_active"] = true;
  EXPECT_TRUE(json.getBool("is_active"));

  // Update bool
  json["is_active"] = false;
  EXPECT_FALSE(json.getBool("is_active"));
}

TEST(YaJsonTest, SetArray) {
  ya::YaJson json(json_str);

  // Add new array
  std::vector<int64_t> new_array = {10, 20, 30};
  json["numbers"] = new_array;
  auto result = json.getArray("numbers");
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0], 10);
  EXPECT_EQ(result[1], 20);
  EXPECT_EQ(result[2], 30);

  // Update array
  new_array = {40, 50};
  json["numbers"] = new_array;
  result = json.getArray("numbers");
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], 40);
  EXPECT_EQ(result[1], 50);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
