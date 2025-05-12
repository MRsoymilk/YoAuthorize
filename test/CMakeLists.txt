# 最低 CMake 版本
cmake_minimum_required(VERSION 3.14)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 启用测试
enable_testing()

# 包含项目头文件目录
include_directories(${PROJECT_SOURCE_DIR}/src)

# 测试可执行文件：test_config
add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE 
  GTest::gtest 
  GTest::gtest_main 
  ya_config
  tomlplusplus::tomlplusplus
)
add_test(NAME TestConfig COMMAND test_config)

# 测试可执行文件：test_log
add_executable(test_log test_log.cpp)
target_link_libraries(test_log PRIVATE 
  GTest::gtest 
  GTest::gtest_main 
  ya_log 
  spdlog::spdlog
)
add_test(NAME TestLog COMMAND test_log)

# 测试可执行文件：test_utils
# add_executable(test_utils test_utils.cpp)
# target_link_libraries(test_utils PRIVATE
#   GTest::gtest
#   GTest::gtest_main
#   nng
#   # ya_utils  # 如果 ya_utils 存在，取消注释
# )
# add_test(NAME TestUtils COMMAND test_utils)

# 启用 GoogleTest 测试发现
include(GoogleTest)
gtest_discover_tests(test_config)
gtest_discover_tests(test_log)
# gtest_discover_tests(test_utils)
