add_library(ya_communicate STATIC
  node_def.h
  yacommunicate.h
  yacommunicate.cpp
  arch/p2p.h
  arch/p2p.cpp
  arch/single.h
  arch/single.cpp
  module/http.h
  module/http.cpp
  module/pipeline.h
  module/pipeline.cpp
  module/requester.h
  module/requester.cpp
  module/responder.h
  module/responder.cpp
  module/publisher.h
  module/publisher.cpp
  module/subscriber.h
  module/subscriber.cpp
  module/survey.h
  module/survey.cpp
  module/bus.h
  module/bus.cpp
)

target_include_directories(ya_communicate PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ya_communicate PRIVATE nng)
