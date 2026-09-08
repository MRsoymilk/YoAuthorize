add_executable(yoauthorize-service main.cpp)

target_include_directories(yoauthorize-service PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries(yoauthorize-service PRIVATE
  YoAuthorize::core
  YoAuthorize::crypto
  YoAuthorize::protocol
  YoAuthorize::transport
)
