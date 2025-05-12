add_library(ya_config STATIC
    yaconfig.cpp
    yaconfig.h
)

target_include_directories(ya_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ya_config PUBLIC tomlplusplus::tomlplusplus)
