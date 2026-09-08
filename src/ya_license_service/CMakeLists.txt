add_library(ya_license_service STATIC
  src/config.cpp
  src/connection.cpp
  src/key_store.cpp
  src/service.cpp
)
add_library(YoAuthorize::service ALIAS ya_license_service)

target_include_directories(ya_license_service PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(ya_license_service PUBLIC
  YoAuthorize::core
  YoAuthorize::crypto
  YoAuthorize::protocol
  YoAuthorize::transport
)
target_link_libraries(ya_license_service PRIVATE
  tomlplusplus::tomlplusplus
)

add_executable(yoauthorize-service main.cpp)

target_include_directories(yoauthorize-service PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries(yoauthorize-service PRIVATE
  YoAuthorize::core
  YoAuthorize::crypto
  YoAuthorize::protocol
  YoAuthorize::transport
  YoAuthorize::service
)
