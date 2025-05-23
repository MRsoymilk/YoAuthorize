add_library(ya_json STATIC
    yajson.cpp
    yajson.h
)

target_include_directories(ya_json PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE simdjson nlohmann_json::nlohmann_json)
