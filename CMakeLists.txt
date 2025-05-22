cmake_minimum_required(VERSION 3.14)
project(ya-net VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if (MSVC)
    foreach(flag_var
            CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE)
        string (REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        string (REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
    endforeach()
endif()

add_executable(${PROJECT_NAME}
    src/main.cpp
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(src/global)
target_link_libraries(${PROJECT_NAME} PRIVATE global_headers)

option(ENABLE_YA_LOG "Build ya_log module" OFF)
option(ENABLE_YA_UTILS "Build ya_utils module" OFF)
option(ENABLE_YA_CONFIG "Build ya_config module" OFF)
option(ENABLE_YA_COMMUNICATE "Build ya_communicate module" OFF)
option(ENABLE_YA_JSON "Build ya_json module" OFF)
option(ENABLE_YA_HWINFO "Build ya_hwinfo module" OFF)
option(ENABLE_YA_SQLDRIVER "Build ya_sqldriver module" OFF)
option(ENABLE_YA_SHELL "Build ya_shell module" OFF)
option(ENABLE_TEST "Build test" OFF)


if(ENABLE_YA_LOG)
  add_subdirectory(src/ya_log)
  target_link_libraries(${PROJECT_NAME} PRIVATE ya_log)
  # spdlog
  add_subdirectory(vendor/spdlog)
  target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

endif()

if(ENABLE_YA_UTILS)
  add_subdirectory(src/ya_utils)
  target_link_libraries(${PROJECT_NAME} PRIVATE ya_utils)
endif()

if(ENABLE_YA_CONFIG)
  add_subdirectory(src/ya_config)
  target_link_libraries(${PROJECT_NAME} PRIVATE ya_config)
  # tomlplusplus
  add_subdirectory(vendor/tomlplusplus)
  target_link_libraries(${PROJECT_NAME} PRIVATE tomlplusplus::tomlplusplus)
endif()

if(ENABLE_YA_COMMUNICATE)
  add_subdirectory(src/ya_communicate)
  target_link_libraries(${PROJECT_NAME} PRIVATE ya_communicate)
  # nng
  add_subdirectory(vendor/nng EXCLUDE_FROM_ALL)
  target_link_libraries(${PROJECT_NAME} PRIVATE nng)
endif()

if(ENABLE_YA_JSON)
  add_subdirectory(src/ya_json)
  target_link_libraries(${PROJECT_NAME} PRIVATE ya_json)
  # simdjson
  add_subdirectory(vendor/simdjson)
  target_link_libraries(${PROJECT_NAME} PRIVATE simdjson)
endif()

if(ENABLE_YA_HWINFO)
  add_subdirectory(src/ya_hwinfo)
  target_link_libraries(${PROJECT_NAME} PRIVATE ya_hwinfo)
endif()

if(ENABLE_YA_SQLDRIVER)
  add_subdirectory(src/ya_sqldriver)
  target_link_libraries(${PROJECT_NAME} PRIVATE ya_sqldriver)

  # sql driver: sqlit
  add_subdirectory(vendor/sqlite-amalgamation)
  target_link_libraries(${PROJECT_NAME} PRIVATE sqlite3)

  # sql driver: MySQL
  find_package(mysql-concpp REQUIRED CONFIG
    PATHS "${CMAKE_SOURCE_DIR}/vendor/mysql-connector-cpp/build"
    NO_DEFAULT_PATH
  )
target_link_libraries(${PROJECT_NAME} PRIVATE mysql::concpp)

# sql driver: MongoDB
find_package(bsoncxx 4.1 REQUIRED CONFIG
  PATHS "${CMAKE_SOURCE_DIR}/vendor/mongo-cxx-driver/build/lib64/cmake/bsoncxx-4.1.0"
  NO_DEFAULT_PATH
)
target_link_libraries(${PROJECT_NAME} PRIVATE mongo::mongocxx_shared mongo::bsoncxx_shared)

find_package(mongocxx 4.1 REQUIRED CONFIG
  PATHS "${CMAKE_SOURCE_DIR}/vendor/mongo-cxx-driver/build/lib64/cmake/mongocxx-4.1.0"
  NO_DEFAULT_PATH
)
endif()

if(ENABLE_YA_SHELL)
  add_subdirectory(src/ya_shell)
  target_link_libraries(${PROJECT_NAME} PRIVATE ya_shell)
endif()

if(ENABLE_TEST)
  add_subdirectory(test)
  # googletest
  add_subdirectory(vendor/gtest)
  target_link_libraries(${PROJECT_NAME} PRIVATE gtest gtest_main)
endif()

# brpc
# add_subdirectory(vendor/brpc)
# target_link_libraries(${PROJECT_NAME} PRIVATE brpc)
