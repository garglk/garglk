cmake_minimum_required(VERSION 3.0)

project(garglk VERSION 2017)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")

include(Compilers)

option(WITH_BABEL "Display Treaty of Babel-derived author and title if possible" ON)
option(WITH_SDL "Use SDL for sound support." ON)

add_subdirectory(garglk)
add_subdirectory(terps)
add_subdirectory(tads)

if(WITH_BABEL)
  add_subdirectory(support/babel)
endif()

if(WITH_SDL AND MINGW)
  add_subdirectory(support/sdl_sound)
endif()
