cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

include_directories(${PROJECT_SOURCE_DIR}/src)

include(GoogleTest)

option(ENABLE_TEST_YA_SQL_SQLITE "Test sql sqlite" ON)
option(ENABLE_TEST_YA_SQL_MYSQL "Test sql mysql" ON)
option(ENABLE_TEST_YA_SQL_MONGODB "Test sql mongodb" ON)
option(ENABLE_TEST_YA_SQL_POSTGRESQL "Test sql postgresql" ON)

# ========================= test sql sqlite =========================
if(ENABLE_TEST_YA_SQL_SQLITE)
    add_executable(test_sql_sqlite test_sqlite.cpp)
    target_link_libraries(test_sql_sqlite PRIVATE
        GTest::gtest
        GTest::gtest_main
        ya_sql
        GTest::gmock
    )
    add_test(NAME TestSqlSqlite COMMAND test_sql_sqlite)
    gtest_discover_tests(test_sql_sqlite)
endif()

# ========================= test sql mysql =========================
if(ENABLE_TEST_YA_SQL_MYSQL)
    add_executable(test_sql_mysql test_mysql.cpp)
    target_link_libraries(test_sql_mysql PRIVATE
        GTest::gtest
        GTest::gtest_main
        ya_sql
    )
    add_test(NAME TestSqlMySQL COMMAND test_sql_mysql)
    gtest_discover_tests(test_sql_mysql)
endif()

# ========================= test sql mongodb =========================
if(ENABLE_TEST_YA_SQL_MONGODB)
    add_executable(test_sql_mongodb test_mongodb.cpp)
    target_link_libraries(test_sql_mongodb PRIVATE
        GTest::gtest
        GTest::gtest_main
        ya_sql
    )
    add_test(NAME TestSqlMongoDB COMMAND test_sql_mongodb)
    gtest_discover_tests(test_sql_mongodb)
endif()

# ========================= test sql postgresql =========================
if(ENABLE_TEST_YA_SQL_POSTGRESQL)
    add_executable(test_sql_postgresql test_postgresql.cpp)
    target_link_libraries(test_sql_postgresql PRIVATE
        GTest::gtest
        GTest::gtest_main
        ya_sql
    )
    add_test(NAME TestSqlPostgreSQL COMMAND test_sql_postgresql)
    gtest_discover_tests(test_sql_postgresql)
endif()

