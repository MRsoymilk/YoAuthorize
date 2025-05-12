#include <gtest/gtest.h>

#include "yalog.h"

TEST(LogTest, LogMessage) {
  LOG_TRACE("this is trace");
  LOG_DEBUG("this is debug");
  LOG_WARN("this is warn");
  LOG_ERROR("this is error");
  LOG_CRITICAL("this is critical");

  LOG_INFO("number: {}", 1);
  LOG_INFO("string: {}", "message");
  LOG_INFO("bool: {}", true);
  LOG_INFO("double: {}", 3.1415926);

  LOG_INFO("test order: {2} {1} {0}", 0, 1, 2);
  LOG_INFO("test multi: bool: {}; string: {}; number: {}", false, "string",
           123);
  EXPECT_TRUE(true);
}
