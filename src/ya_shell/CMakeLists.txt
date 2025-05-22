add_library(ya_shell STATIC
    yashell.cpp
    yashell.h
)

target_include_directories(ya_shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
