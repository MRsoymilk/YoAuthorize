include(GoogleTest)

add_executable(test_license_protocol
  test_envelope.cpp
  test_frame.cpp
  test_protocol_handshake.cpp
  test_messages.cpp
  test_record.cpp
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

add_executable(test_license_core
  test_license.cpp
  test_machine_identity.cpp
  test_session.cpp
)
target_include_directories(test_license_core PRIVATE
  ${PROJECT_BINARY_DIR}/generated
)
target_link_libraries(test_license_core PRIVATE
  GTest::gtest_main
  YoAuthorize::core
  YoAuthorize::protocol
)

gtest_discover_tests(test_license_core)

add_executable(test_license_transport test_transport.cpp)
target_link_libraries(test_license_transport PRIVATE
  GTest::gtest_main
  YoAuthorize::transport
)

gtest_discover_tests(test_license_transport)

if(TARGET ya_license_service)
  add_executable(test_license_service
    test_service_config.cpp
    test_key_store.cpp
  )
  target_link_libraries(test_license_service PRIVATE
    GTest::gtest_main
    YoAuthorize::service
  )

  gtest_discover_tests(test_license_service)
endif()
