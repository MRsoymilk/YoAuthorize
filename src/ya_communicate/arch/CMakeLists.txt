add_library(arch
    p2p.cpp
    p2p.h
    single.cpp
    single.h
)

target_include_directories(arch PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(arch PUBLIC module)

