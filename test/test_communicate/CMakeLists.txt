cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

include_directories(${PROJECT_SOURCE_DIR}/src)

include(GoogleTest)

option(ENABLE_TEST_YA_COMMUNICATE_HTTP "Test module http" ON)
option(ENABLE_TEST_YA_COMMUNICATE_PIPELINE "Test module pipeline" ON)
option(ENABLE_TEST_YA_COMMUNICATE_PUBSUB "Test module publish-subscribe" ON)
option(ENABLE_TEST_YA_COMMUNICATE_REQREP "Test module reqest-response" ON)
option(ENABLE_TEST_YA_COMMUNICATE_BUS "Test module bus" ON)
option(ENABLE_TEST_YA_COMMUNICATE_SURVEY "Test module survey" ON)

# ========================= test module http =========================
if(ENABLE_TEST_YA_COMMUNICATE_HTTP)
  add_executable(test_module_http test_http.cpp)
  target_link_libraries(test_module_http PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_communicate
  )
  add_test(NAME TestModuleHttp COMMAND test_module_http)
  gtest_discover_tests(test_module_http)
endif()

# ========================= test module pipeline =========================
if(ENABLE_TEST_YA_COMMUNICATE_PIPELINE)
  add_executable(test_module_pipeline test_pipeline.cpp)
  target_link_libraries(test_module_pipeline PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_communicate
  )
  add_test(NAME TestModulePipeline COMMAND test_module_pipeline)
  gtest_discover_tests(test_module_pipeline)
endif()

# ========================= test module pubsub =========================
if(ENABLE_TEST_YA_COMMUNICATE_PUBSUB)
  add_executable(test_module_pubsub test_pubsub.cpp)
  target_link_libraries(test_module_pubsub PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_communicate
  )
  add_test(NAME TestModulePubsub COMMAND test_module_pubsub)
  gtest_discover_tests(test_module_pubsub)
endif()

# ========================= test module reqrep =========================
if(ENABLE_TEST_YA_COMMUNICATE_REQREP)
  add_executable(test_module_reqrep test_reqrep.cpp)
  target_link_libraries(test_module_reqrep PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_communicate
  )
  add_test(NAME TestModuleReqrep COMMAND test_module_reqrep)
  gtest_discover_tests(test_module_reqrep)
endif()

# ========================= test module survey =========================
if(ENABLE_TEST_YA_COMMUNICATE_SURVEY)
  add_executable(test_module_survey test_survey.cpp)
  target_link_libraries(test_module_survey PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_communicate
  )
  add_test(NAME TestModuleSurvey COMMAND test_module_survey)
  gtest_discover_tests(test_module_survey)
endif()

# ========================= test module bus =========================
if(ENABLE_TEST_YA_COMMUNICATE_BUS)
  add_executable(test_module_bus test_bus.cpp)
  target_link_libraries(test_module_bus PRIVATE
    GTest::gtest
    GTest::gtest_main
    ya_communicate
  )
  add_test(NAME TestModuleBus COMMAND test_module_bus)
  gtest_discover_tests(test_module_bus)
endif()
