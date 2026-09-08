add_library(ya_license_core STATIC
  src/license.cpp
  src/machine_identity.cpp
  src/session.cpp
)
add_library(YoAuthorize::core ALIAS ya_license_core)

target_compile_features(ya_license_core PUBLIC cxx_std_20)
target_include_directories(ya_license_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_BINARY_DIR}/generated
)
target_link_libraries(ya_license_core
  PUBLIC YoAuthorize::crypto
  PRIVATE YoAuthorize::protocol
)
