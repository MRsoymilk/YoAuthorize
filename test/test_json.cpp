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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
