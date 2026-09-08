add_library(ya_license_core INTERFACE)
add_library(YoAuthorize::core ALIAS ya_license_core)

target_compile_features(ya_license_core INTERFACE cxx_std_20)
target_link_libraries(ya_license_core INTERFACE
  YoAuthorize::crypto
  YoAuthorize::protocol
)
