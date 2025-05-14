add_library(ya_communicate STATIC
  yacommunicate.h
  yacommunicate.cpp
)

target_include_directories(ya_communicate PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ya_communicate PRIVATE nng)
