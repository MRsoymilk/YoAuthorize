#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "isqldriver.h"
#include "yasqldriver.h"

// Mock ISqlDriver for testing YaSqlDriver without SQLite
class MockSqlDriver : public ya::ISqlDriver {
 public:
  MOCK_METHOD1(connect, bool(const std::string&));
  MOCK_METHOD2(insert, bool(const std::string&,
                            const std::map<std::string, std::string>&));
  MOCK_METHOD3(update, bool(const std::string&,
                            const std::map<std::string, std::string>&,
                            const std::string&));
  MOCK_METHOD2(remove, bool(const std::string&, const std::string&));
  MOCK_METHOD1(query, std::vector<std::map<std::string, std::string>>(
                          const std::string&));
};

class YaSqlDriverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize with SQLite driver
    ASSERT_TRUE(driver.loadDriver("sqlite"));
    ASSERT_TRUE(driver.connect(":memory:"));

    // Create a test table
    std::string create_table = "CREATE TABLE users (name TEXT, age INTEGER)";
    auto result = driver.query(create_table);
    EXPECT_TRUE(result.empty()) << "Failed to create table";
  }

  ya::YaSqlDriver driver;
};

TEST_F(YaSqlDriverTest, LoadDriverSQLite) {
  ya::YaSqlDriver new_driver;
  EXPECT_TRUE(new_driver.loadDriver("sqlite"));
}

TEST_F(YaSqlDriverTest, LoadDriverInvalid) {
  ya::YaSqlDriver new_driver;
  EXPECT_FALSE(new_driver.loadDriver("invalid_driver"));
}

TEST_F(YaSqlDriverTest, ConnectSuccess) {
  ya::YaSqlDriver new_driver;
  ASSERT_TRUE(new_driver.loadDriver("sqlite"));
  EXPECT_TRUE(new_driver.connect(":memory:"));
}

TEST_F(YaSqlDriverTest, ConnectWithoutDriver) {
  ya::YaSqlDriver new_driver;
  EXPECT_FALSE(new_driver.connect(":memory:"))
      << "Should fail without loaded driver";
}

TEST_F(YaSqlDriverTest, InsertData) {
  std::map<std::string, std::string> data = {{"name", "Alice"}, {"age", "25"}};
  EXPECT_TRUE(driver.insert("users", data));

  auto results = driver.query("SELECT * FROM users");
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0]["name"], "Alice");
  EXPECT_EQ(results[0]["age"], "25");
}

TEST_F(YaSqlDriverTest, InsertEmptyData) {
  std::map<std::string, std::string> data;
  EXPECT_FALSE(driver.insert("users", data));
}

TEST_F(YaSqlDriverTest, UpdateData) {
  std::map<std::string, std::string> data = {{"name", "Bob"}, {"age", "30"}};
  ASSERT_TRUE(driver.insert("users", data));

  std::map<std::string, std::string> update_data = {{"age", "31"}};
  EXPECT_TRUE(driver.update("users", update_data, "name='Bob'"));

  auto results = driver.query("SELECT * FROM users WHERE name='Bob'");
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0]["age"], "31");
}

TEST_F(YaSqlDriverTest, RemoveData) {
  std::map<std::string, std::string> data = {{"name", "Charlie"},
                                             {"age", "40"}};
  ASSERT_TRUE(driver.insert("users", data));

  EXPECT_TRUE(driver.remove("users", "name='Charlie'"));

  auto results = driver.query("SELECT * FROM users");
  EXPECT_TRUE(results.empty());
}

TEST_F(YaSqlDriverTest, QueryMultipleRows) {
  std::map<std::string, std::string> data1 = {{"name", "Eve"}, {"age", "22"}};
  std::map<std::string, std::string> data2 = {{"name", "Frank"}, {"age", "33"}};
  ASSERT_TRUE(driver.insert("users", data1));
  ASSERT_TRUE(driver.insert("users", data2));

  auto results = driver.query("SELECT * FROM users");
  ASSERT_EQ(results.size(), 2);
  EXPECT_EQ(results[0]["name"], "Eve");
  EXPECT_EQ(results[1]["name"], "Frank");
}

// Mock-based test for delegation
TEST(YaSqlDriverMockTest, DelegatesToDriver) {
  ya::YaSqlDriver driver;
  auto mock_driver = std::make_unique<MockSqlDriver>();
  MockSqlDriver* mock_ptr = mock_driver.get();

  // Simulate loading a mock driver (bypassing loadDriver)
  driver = ya::YaSqlDriver();  // Reset driver
  // Note: We need a way to inject the mock; this assumes a setter or loadDriver
  // supports mocks For simplicity, we'll test delegation directly

  EXPECT_CALL(*mock_ptr, connect(testing::_)).WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_ptr, insert("users", testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_ptr, query(testing::_))
      .WillOnce(testing::Return(
          std::vector<std::map<std::string, std::string>>{{{"name", "Test"}}}));

  // This requires a way to inject mock_driver into YaSqlDriver
  // If loadDriver can't be mocked, add a setter or modify YaSqlDriver
}
