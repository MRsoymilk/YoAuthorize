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

add_executable(test_license_crypto
  test_crypto.cpp
  test_handshake.cpp
)
target_link_libraries(test_license_crypto PRIVATE
  GTest::gtest_main
  YoAuthorize::crypto
)

gtest_discover_tests(test_license_crypto)
