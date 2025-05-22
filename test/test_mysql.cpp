#include <gtest/gtest.h>

#include "yasqldriver.h"

class YaSqlMysqlTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(driver.loadDriver("mysql"));

    std::string uri = "mysqlx://user:password@127.0.0.1:33060/test_db";
    ASSERT_TRUE(driver.connect(uri));

    driver.query("DROP TABLE IF EXISTS users");
    driver.query("CREATE TABLE users (name VARCHAR(100), age INT)");
  }

  ya::YaSqlDriver driver;
};

TEST_F(YaSqlMysqlTest, InsertAndQuery) {
  std::map<std::string, std::string> data = {{"name", "Alice"}, {"age", "25"}};
  EXPECT_TRUE(driver.insert("users", data));

  auto results = driver.query("SELECT name, age FROM users");
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0]["name"], "Alice");
  EXPECT_EQ(results[0]["age"], "25");
}

TEST_F(YaSqlMysqlTest, UpdateData) {
  driver.insert("users", {{"name", "Bob"}, {"age", "30"}});
  EXPECT_TRUE(driver.update("users", {{"age", "31"}}, "name = 'Bob'"));

  auto results = driver.query("SELECT * FROM users WHERE name = 'Bob'");
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0]["age"], "31");
}

TEST_F(YaSqlMysqlTest, RemoveData) {
  driver.insert("users", {{"name", "Charlie"}, {"age", "40"}});
  EXPECT_TRUE(driver.remove("users", "name = 'Charlie'"));

  auto results = driver.query("SELECT * FROM users");
  EXPECT_TRUE(results.empty());
}

TEST_F(YaSqlMysqlTest, QueryMultipleRows) {
  driver.insert("users", {{"name", "Eve"}, {"age", "22"}});
  driver.insert("users", {{"name", "Frank"}, {"age", "33"}});

  auto results = driver.query("SELECT * FROM users ORDER BY name");
  ASSERT_EQ(results.size(), 2);
  EXPECT_EQ(results[0]["name"], "Eve");
  EXPECT_EQ(results[1]["name"], "Frank");
}
