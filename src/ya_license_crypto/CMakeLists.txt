add_library(ya_license_crypto STATIC
  src/crypto.cpp
  src/handshake.cpp
)
add_library(YoAuthorize::crypto ALIAS ya_license_crypto)

target_compile_features(ya_license_crypto PUBLIC cxx_std_20)
target_include_directories(ya_license_crypto PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(ya_license_crypto PUBLIC OpenSSL::Crypto)
