add_library(ya_hwinfo STATIC
  info_def.h
  yahwinfo.h
  yahwinfo.cpp
  bios/yabios.h
  bios/yabios.cpp
  cpu/yacpu.h
  cpu/yacpu.cpp
  disk/yadisk.h
  disk/yadisk.cpp
  gpu/yagpu.h
  gpu/yagpu.cpp
  memory/yamemory.h
  memory/yamemory.cpp
  motherboard/yamotherboard.h
  motherboard/yamotherboard.cpp
  network/yanetwork.h
  network/yanetwork.cpp
  os/yaos.h
  os/yaos.cpp
)

target_include_directories(ya_hwinfo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

