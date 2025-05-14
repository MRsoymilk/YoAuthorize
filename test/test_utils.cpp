#include <gtest/gtest.h>

#include <format>
#include <iostream>

#include "yautils.h"

class ExeUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // simulate
    const char* argv0 = "./test_app";
    YaUtils::Exe::ProcessArgs(1, const_cast<char**>(&argv0));
  }
};

TEST_F(ExeUtilsTest, ExePathValid) {
  std::string path = YaUtils::Exe::GetExePath();
  EXPECT_FALSE(path.empty());
  std::cout << "Exe Path: " << path << std::endl;
}

TEST_F(ExeUtilsTest, ExeNameValid) {
  std::string name = YaUtils::Exe::GetExeName();
  EXPECT_FALSE(name.empty());
  std::cout << std::format("App Name: {}", name) << std::endl;
}

TEST_F(ExeUtilsTest, ExeDirValid) {
  std::string dir = YaUtils::Exe::GetExeDir();
  EXPECT_FALSE(dir.empty());
  std::cout << std::format("App Dir: {}", dir) << std::endl;
}

TEST_F(ExeUtilsTest, FullPathConsistent) {
  std::string path = YaUtils::Exe::GetExePath();
  std::string dir = YaUtils::Exe::GetExeDir();
  std::string name = YaUtils::Exe::GetExeName();
  EXPECT_TRUE(path.find(dir) != std::string::npos);
  EXPECT_TRUE(path.find(name) != std::string::npos);
  std::cout << std::format("Full Path: {}", path) << std::endl;
}

TEST(TimerTest, SleepAccuracy) {
  YaUtils::Timer::StartTimer();
  YaUtils::Timer::Sleep(1000);  // sleep 1 second

  int64_t elapsed_us = YaUtils::Timer::GetElapsedTime_μs();
  int64_t elapsed_ms = YaUtils::Timer::GetElapsedTime_ms();
  double elapsed_s = YaUtils::Timer::GetElapsedTime_s();

  std::cout << std::format("Elapsed: {} μs", elapsed_us) << std::endl;
  std::cout << std::format("Elapsed: {} ms", elapsed_ms) << std::endl;
  std::cout << std::format("Elapsed: {} s", elapsed_s) << std::endl;

  EXPECT_GE(elapsed_ms, 950);
  EXPECT_LE(elapsed_ms, 1100);
}
