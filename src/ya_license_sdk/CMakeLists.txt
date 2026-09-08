add_library(ya_license_sdk INTERFACE)
add_library(YoAuthorize::sdk ALIAS ya_license_sdk)

target_compile_features(ya_license_sdk INTERFACE cxx_std_20)
target_link_libraries(ya_license_sdk INTERFACE
  YoAuthorize::crypto
  YoAuthorize::protocol
  YoAuthorize::transport
)
