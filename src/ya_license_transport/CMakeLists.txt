add_library(ya_license_transport STATIC
  src/unix_socket.cpp
)
add_library(YoAuthorize::transport ALIAS ya_license_transport)

target_compile_features(ya_license_transport PUBLIC cxx_std_20)
target_include_directories(ya_license_transport
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(ya_license_transport PUBLIC Threads::Threads)
