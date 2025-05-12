add_library(ya_log STATIC
    yalog.cpp
    yalog.h
)

target_include_directories(ya_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ya_log PUBLIC spdlog::spdlog)

