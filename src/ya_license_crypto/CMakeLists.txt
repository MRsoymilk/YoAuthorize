add_library(ya_license_crypto INTERFACE)
add_library(YoAuthorize::crypto ALIAS ya_license_crypto)

target_compile_features(ya_license_crypto INTERFACE cxx_std_20)
