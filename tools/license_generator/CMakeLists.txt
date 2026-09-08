add_library(ya_license_issuer STATIC issuer.cpp)
add_library(YoAuthorize::issuer ALIAS ya_license_issuer)

target_include_directories(ya_license_issuer
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_BINARY_DIR}/generated
)
target_link_libraries(ya_license_issuer PUBLIC
  YoAuthorize::crypto
  YoAuthorize::protocol
)

add_executable(yoauthorize-license-generator main.cpp)
target_link_libraries(yoauthorize-license-generator PRIVATE
  YoAuthorize::issuer
)
