cmake_minimum_required(VERSION 3.14)
project(YoAuthorize VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.h
)

include(CTest)

find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
find_package(Threads REQUIRED)

set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_INSTALL OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_FLATLIB OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_FLATC ON CACHE BOOL "" FORCE)
add_subdirectory(vendor/flatbuffers EXCLUDE_FROM_ALL)
add_subdirectory(vendor/tomlplusplus EXCLUDE_FROM_ALL)

if(BUILD_TESTING)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  add_subdirectory(vendor/gtest EXCLUDE_FROM_ALL)
endif()

option(YOAUTHORIZE_BUILD_SERVICE "Build the license service" ON)
option(YOAUTHORIZE_BUILD_SDK "Build the native C++ SDK" ON)
option(YOAUTHORIZE_BUILD_TOOLS "Build license tools" OFF)
option(YOAUTHORIZE_BUILD_LEGACY "Build legacy prototype modules" OFF)

add_subdirectory(src/global)
add_subdirectory(src/ya_license_crypto)
add_subdirectory(src/ya_license_protocol)
add_subdirectory(src/ya_license_transport)
add_subdirectory(src/ya_license_core)

if(YOAUTHORIZE_BUILD_SERVICE)
  add_subdirectory(src/ya_license_service)
endif()

if(BUILD_TESTING)
  add_subdirectory(test/authorization)
endif()

if(YOAUTHORIZE_BUILD_SDK)
  add_subdirectory(src/ya_license_sdk)
endif()

if(YOAUTHORIZE_BUILD_TOOLS)
  message(STATUS "YoAuthorize tools are not implemented yet")
endif()

if(YOAUTHORIZE_BUILD_LEGACY)
  add_executable(ya-net src/main.cpp)
  target_include_directories(ya-net PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(ya-net PRIVATE global_headers)

  option(ENABLE_YA_LOG "Build ya_log module" OFF)
  option(ENABLE_YA_UTILS "Build ya_utils module" OFF)
  option(ENABLE_YA_CONFIG "Build ya_config module" OFF)
  option(ENABLE_YA_COMMUNICATE "Build ya_communicate module" OFF)
  option(ENABLE_YA_JSON "Build ya_json module" OFF)
  option(ENABLE_YA_HWINFO "Build ya_hwinfo module" OFF)
  option(ENABLE_YA_SQL "Build ya_sql module" OFF)
  option(ENABLE_YA_SHELL "Build ya_shell module" OFF)
  option(ENABLE_YA_TEST "Build legacy tests" OFF)

  if(ENABLE_YA_LOG)
    add_subdirectory(vendor/spdlog)
    add_subdirectory(src/ya_log)
    target_link_libraries(ya-net PRIVATE ya_log spdlog::spdlog)
  endif()

  if(ENABLE_YA_UTILS)
    find_package(OpenSSL REQUIRED)
    add_subdirectory(src/ya_utils)
    target_link_libraries(ya-net PRIVATE ya_utils OpenSSL::SSL OpenSSL::Crypto)
  endif()

  if(ENABLE_YA_CONFIG)
    add_subdirectory(src/ya_config)
    target_link_libraries(ya-net PRIVATE ya_config tomlplusplus::tomlplusplus)
  endif()

  if(ENABLE_YA_COMMUNICATE)
    add_subdirectory(vendor/nng EXCLUDE_FROM_ALL)
    add_subdirectory(src/ya_communicate)
    target_link_libraries(ya-net PRIVATE ya_communicate nng)
  endif()

  if(ENABLE_YA_JSON)
    add_subdirectory(vendor/json)
    add_subdirectory(src/ya_json)
    target_link_libraries(ya-net PRIVATE ya_json nlohmann_json::nlohmann_json)
  endif()

  if(ENABLE_YA_HWINFO)
    add_subdirectory(src/ya_hwinfo)
    target_link_libraries(ya-net PRIVATE ya_hwinfo)
  endif()

  if(ENABLE_YA_SQL)
    add_subdirectory(vendor/sqlite-amalgamation)
    find_package(mysql-concpp REQUIRED CONFIG
      PATHS "${CMAKE_SOURCE_DIR}/vendor/mysql-connector-cpp/build"
      NO_DEFAULT_PATH
    )
    find_package(bson 2.0.0 REQUIRED CONFIG
      PATHS "${CMAKE_SOURCE_DIR}/vendor/mongo-cxx-driver/build/lib64/cmake/bson-2.0.0"
      NO_DEFAULT_PATH
    )
    find_package(bsoncxx 4.1.0 REQUIRED CONFIG
      PATHS "${CMAKE_SOURCE_DIR}/vendor/mongo-cxx-driver/build/lib64/cmake/bsoncxx-4.1.0"
      NO_DEFAULT_PATH
    )
    find_package(mongoc 2.0.0 REQUIRED CONFIG
      PATHS "${CMAKE_SOURCE_DIR}/vendor/mongo-cxx-driver/build/lib64/cmake/mongoc-2.0.0"
      NO_DEFAULT_PATH
    )
    find_package(mongocxx 4.1.0 REQUIRED CONFIG
      PATHS "${CMAKE_SOURCE_DIR}/vendor/mongo-cxx-driver/build/lib64/cmake/mongocxx-4.1.0"
      NO_DEFAULT_PATH
    )
    add_subdirectory(src/ya_sql)
    target_link_libraries(ya-net PRIVATE ya_sql sqlite mysql::concpp
      mongo::mongocxx_shared mongo::bsoncxx_shared)
  endif()

  if(ENABLE_YA_SHELL)
    add_subdirectory(src/ya_shell)
    target_link_libraries(ya-net PRIVATE ya_shell)
  endif()

  if(ENABLE_YA_TEST)
    add_subdirectory(test)
  endif()
endif()
