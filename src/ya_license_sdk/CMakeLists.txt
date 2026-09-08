add_library(ya_license_sdk STATIC src/client.cpp)
add_library(YoAuthorize::sdk ALIAS ya_license_sdk)

target_include_directories(ya_license_sdk PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(ya_license_sdk PUBLIC cxx_std_20)
target_link_libraries(ya_license_sdk PUBLIC
  YoAuthorize::crypto
  YoAuthorize::protocol
  YoAuthorize::transport
)
