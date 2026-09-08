include(GoogleTest)

add_executable(test_license_protocol
  test_envelope.cpp
  test_frame.cpp
)

target_include_directories(test_license_protocol PRIVATE
  ${PROJECT_BINARY_DIR}/generated
)
target_link_libraries(test_license_protocol PRIVATE
  GTest::gtest_main
  YoAuthorize::protocol
)

gtest_discover_tests(test_license_protocol)
