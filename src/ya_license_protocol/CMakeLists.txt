set(YOAUTHORIZE_SCHEMA_DIR ${PROJECT_SOURCE_DIR}/schema)
set(YOAUTHORIZE_GENERATED_DIR ${PROJECT_BINARY_DIR}/generated)
set(YOAUTHORIZE_GENERATED_HEADERS
  ${YOAUTHORIZE_GENERATED_DIR}/license_generated.h
  ${YOAUTHORIZE_GENERATED_DIR}/protocol_generated.h
)

add_custom_command(
  OUTPUT ${YOAUTHORIZE_GENERATED_HEADERS}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${YOAUTHORIZE_GENERATED_DIR}
  COMMAND $<TARGET_FILE:flatc> --cpp --scoped-enums
    -o ${YOAUTHORIZE_GENERATED_DIR}
    ${YOAUTHORIZE_SCHEMA_DIR}/license.fbs
    ${YOAUTHORIZE_SCHEMA_DIR}/protocol.fbs
  DEPENDS
    flatc
    ${YOAUTHORIZE_SCHEMA_DIR}/license.fbs
    ${YOAUTHORIZE_SCHEMA_DIR}/protocol.fbs
  VERBATIM
)

add_library(ya_license_protocol STATIC
  src/envelope.cpp
  src/frame.cpp
  src/record.cpp
  ${YOAUTHORIZE_GENERATED_HEADERS}
)
add_library(YoAuthorize::protocol ALIAS ya_license_protocol)

target_compile_features(ya_license_protocol PUBLIC cxx_std_20)
target_include_directories(ya_license_protocol
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${YOAUTHORIZE_GENERATED_DIR}
)
target_link_libraries(ya_license_protocol
  PUBLIC FlatBuffers::FlatBuffers
  PUBLIC YoAuthorize::crypto
)
