add_library(module
    http.cpp
    http.h
    client.cpp
    cleint.h
    server.cpp
    server.h
)

target_include_directories(module PUBLIC ${CMAKE_SOURCE_DIR}/include)

