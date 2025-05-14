add_library(ya_utils STATIC
    yautils.h
    yautils.cpp
)

target_include_directories(ya_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
