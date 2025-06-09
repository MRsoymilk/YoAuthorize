cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

include_directories(${PROJECT_SOURCE_DIR}/src)

include(GoogleTest)

option(ENABLE_TEST_YA_CONFIG "Test ya_config module" OFF)
option(ENABLE_TEST_YA_LOG "Test ya_log module" OFF)
option(ENABLE_TEST_YA_COMMUNICATE "Test ya_communicate module" OFF)
option(ENABLE_TEST_YA_UTILS "Test ya_utils module" OFF)
option(ENABLE_TEST_YA_JSON "Test ya_json module" OFF)
option(ENABLE_TEST_YA_HWINFO "Test ya_hwinfo module" OFF)
option(ENABLE_TEST_YA_SQL "Test ya_sql module" OFF)
option(ENABLE_TEST_YA_SHELL "Test ya_shell module" OFF)

# ========================= test ya config =========================
if(ENABLE_TEST_YA_CONFIG)
  add_executable(test_config test_config.cpp)
  target_link_libraries(test_config PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_config
    tomlplusplus::tomlplusplus
  )
  add_test(NAME TestConfig COMMAND test_config)
  gtest_discover_tests(test_config)
endif()

# ========================= test ya log =========================
if(ENABLE_TEST_YA_LOG)
  add_executable(test_log test_log.cpp)
  target_link_libraries(test_log PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_log
    spdlog::spdlog
  )
  add_test(NAME TestLog COMMAND test_log)
  gtest_discover_tests(test_log)
endif()

# ========================= test ya communicate =========================
if(ENABLE_TEST_YA_COMMUNICATE)
  add_subdirectory(test_communicate)
endif()

# ========================= test ya utils =========================
if(ENABLE_TEST_YA_UTILS)
  add_executable(test_utils test_utils.cpp)
  target_link_libraries(test_utils PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_utils
    OpenSSL::SSL
    OpenSSL::Crypto
  )
  add_test(NAME TestUtils COMMAND test_utils)
  gtest_discover_tests(test_utils)
endif()

# ========================= test ya json =========================
if(ENABLE_TEST_YA_JSON)
  add_executable(test_json test_json.cpp)
  target_link_libraries(test_json PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_json
    simdjson
  )
  add_test(NAME TestJson COMMAND test_json)
  gtest_discover_tests(test_json)
endif()

# ========================= test ya hwinfo =========================
if(ENABLE_TEST_YA_HWINFO)
  add_executable(test_hwinfo test_hwinfo.cpp)
  target_link_libraries(test_hwinfo PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_hwinfo
  )
  add_test(NAME TestHwinfo COMMAND test_hwinfo)
  gtest_discover_tests(test_hwinfo)
endif()

# ========================= test ya sql =========================
if(ENABLE_TEST_YA_SQL)
  add_subdirectory(test_sql)
endif()

# ========================= test ya shell =========================
if(ENABLE_TEST_YA_SHELL)
  add_executable(test_shell test_shell.cpp)
  target_link_libraries(test_shell PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_shell
  )
  add_test(NAME TestShell COMMAND test_shell)
  gtest_discover_tests(test_shell)
endif()
