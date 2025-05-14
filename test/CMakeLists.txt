cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

include_directories(${PROJECT_SOURCE_DIR}/src)

# ========================= test config =========================
add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE 
  GTest::gtest 
  GTest::gtest_main 
  ya_config
  tomlplusplus::tomlplusplus
)
add_test(NAME TestConfig COMMAND test_config)

# ========================= test log =========================
add_executable(test_log test_log.cpp)
target_link_libraries(test_log PRIVATE 
  GTest::gtest 
  GTest::gtest_main 
  ya_log 
  spdlog::spdlog
)
add_test(NAME TestLog COMMAND test_log)

# ========================= test communicate =========================
add_executable(test_communicate test_communicate.cpp)
target_link_libraries(test_communicate PRIVATE
  GTest::gtest
  GTest::gtest_main
  ya_communicate
  nng
)
add_test(NAME TestCommunicate COMMAND test_communicate)

# ========================= test utils =========================
add_executable(test_utils test_utils.cpp)
target_link_libraries(test_utils PRIVATE
  GTest::gtest
  GTest::gtest_main
  ya_utils
)
add_test(NAME TestUtils COMMAND test_utils)

# ========================= test json =========================
add_executable(test_json test_json.cpp)
target_link_libraries(test_json PRIVATE
  GTest::gtest
  GTest::gtest_main
  ya_json
  simdjson
)
add_test(NAME TestJson COMMAND test_json)

include(GoogleTest)
gtest_discover_tests(test_config)
gtest_discover_tests(test_log)
gtest_discover_tests(test_communicate)
gtest_discover_tests(test_utils)
gtest_discover_tests(test_json)

