cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

include_directories(${PROJECT_SOURCE_DIR}/src)

include(GoogleTest)

option(ENABLE_TEST_CONFIG "Test ya_config module" OFF)
option(ENABLE_TEST_LOG "Test ya_log module" OFF)
option(ENABLE_TEST_COMMUNICATE "Test ya_communicate module" OFF)
option(ENABLE_TEST_UTILS "Test ya_utils module" OFF)
option(ENABLE_TEST_JSON "Test ya_json module" OFF)
option(ENABLE_TEST_HWINFO "Test ya_hwinfo module" OFF)
option(ENABLE_TEST_SQL "Test ya_sql module" OFF)

# ========================= test config =========================
if(ENABLE_TEST_CONFIG)
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

# ========================= test log =========================
if(ENABLE_TEST_LOG)
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

# ========================= test communicate =========================
if(ENABLE_TEST_COMMUNICATE)
  add_executable(test_communicate test_communicate.cpp)
  target_link_libraries(test_communicate PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_communicate
    nng
  )
  add_test(NAME TestCommunicate COMMAND test_communicate)
  gtest_discover_tests(test_communicate)
endif()

# ========================= test utils =========================
if(ENABLE_TEST_UTILS)
  add_executable(test_utils test_utils.cpp)
  target_link_libraries(test_utils PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_utils
  )
  add_test(NAME TestUtils COMMAND test_utils)
  gtest_discover_tests(test_utils)
endif()

# ========================= test json =========================
if(ENABLE_TEST_JSON)
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

# ========================= test hwinfo =========================
if(ENABLE_TEST_HWINFO)
  add_executable(test_hwinfo test_hwinfo.cpp)
  target_link_libraries(test_hwinfo PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_hwinfo
  )
  add_test(NAME TestHwinfo COMMAND test_hwinfo)
  gtest_discover_tests(test_hwinfo)
endif()

# ========================= test sql =========================
if(ENABLE_TEST_SQL)
  add_executable(test_sqlite test_sqlite.cpp)
  target_link_libraries(test_sqlite PRIVATE
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    ya_sqldriver
    sqlite3
  )
  add_test(NAME TestSqlite COMMAND test_sqllite)
  gtest_discover_tests(test_sqlite)

  add_executable(test_mysql test_mysql.cpp)
  target_link_libraries(test_mysql PRIVATE
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    ya_sqldriver
    mysql::concpp
  )
  add_test(NAME TestMySQL COMMAND test_mysql)
  gtest_discover_tests(test_mysql)
endif()
