cmake_minimum_required(VERSION 3.14)
project(ya-net VERSION 0.0.1 LANGUAGES CXX)

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

add_subdirectory(src/ya_log)
target_link_libraries(${PROJECT_NAME} PRIVATE ya_log)

add_subdirectory(src/ya_utils)
target_link_libraries(${PROJECT_NAME} PRIVATE ya_utils)

add_subdirectory(src/ya_config)
target_link_libraries(${PROJECT_NAME} PRIVATE ya_config)

add_subdirectory(src/ya_communicate)
target_link_libraries(${PROJECT_NAME} PRIVATE ya_communicate)

add_subdirectory(src/ya_json)
target_link_libraries(${PROJECT_NAME} PRIVATE ya_json)

add_subdirectory(src/ya_hwinfo)
target_link_libraries(${PROJECT_NAME} PRIVATE ya_hwinfo)

add_subdirectory(src/ya_sqldriver)
target_link_libraries(${PROJECT_NAME} PRIVATE ya_sqldriver)

add_subdirectory(src/global)
target_link_libraries(${PROJECT_NAME} PRIVATE global_headers)

add_subdirectory(test)

# spdlog
add_subdirectory(vendor/spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

# nng
add_subdirectory(vendor/nng EXCLUDE_FROM_ALL)
target_link_libraries(${PROJECT_NAME} PRIVATE nng)

# tomlplusplus
add_subdirectory(vendor/tomlplusplus)
target_link_libraries(${PROJECT_NAME} PRIVATE tomlplusplus::tomlplusplus)

# googletest
add_subdirectory(vendor/gtest)
target_link_libraries(${PROJECT_NAME} PRIVATE gtest gtest_main)

add_subdirectory(vendor/simdjson)
target_link_libraries(${PROJECT_NAME} PRIVATE simdjson)

add_subdirectory(vendor/sqlite-amalgamation)
target_link_libraries(${PROJECT_NAME} PRIVATE sqlite3)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vendor/mysql-connector-cpp/build/")
find_package(mysql-concpp REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE mysql::concpp)

# brpc
# add_subdirectory(vendor/brpc)
# target_link_libraries(${PROJECT_NAME} PRIVATE brpc)
