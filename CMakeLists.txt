cmake_minimum_required(VERSION 3.5)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

set(GARGLKINI "/etc/garglk.ini" CACHE STRING "Full path for garglk.ini.")
set(GARGLKPRE "" CACHE STRING "Binary prefix.")
set(GUI "GTK" CACHE STRING "Select GUI. Options: GTK, MINGW32.")

option(WITH_SDL "Use SDL for sound." ON)
option(WITH_BUNDLED_FONTS "Bundle fonts." OFF)
option(WITH_BABEL "Use babel." ON)

macro(interpreter name srcs)
  add_executable(${name} ${srcs})
  set_target_properties(${name} PROPERTIES OUTPUT_NAME "${GARGLKPRE}${name}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGARGLK")
  target_link_libraries(${name} glkmain garglk)
  install(TARGETS ${name} DESTINATION libexec/gargoyle)
endmacro()

macro(c99 name)
  set_property(TARGET ${name} PROPERTY C_STANDARD 99)
  set_property(TARGET ${name} PROPERTY C_STANDARD_REQUIRED ON)
  set_property(TARGET ${name} PROPERTY C_EXTENSIONS OFF)

  # CMake doesn't know how to set C99 mode for icc.
  if("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
  endif()
endmacro()

include_directories(garglk)

add_subdirectory(garglk)
add_subdirectory(terps)
add_subdirectory(tads)

if(WITH_BABEL)
  add_subdirectory(support/babel)
endif()
