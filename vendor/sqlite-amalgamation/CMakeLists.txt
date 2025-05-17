enable_language(C)
set(CMAKE_C_STANDARD 11)

add_library(sqlite STATIC
    sqlite3.c
)

target_include_directories(sqlite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

