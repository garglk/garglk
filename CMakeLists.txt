cmake_minimum_required(VERSION 3.9)

project(garglk VERSION 2017)
include(GNUInstallDirs)
include(CheckIPOSupported)

option(COMPILE_FOR_KINDLE "Compile for target platform Kindle" OFF)
option(ENABLE_ALT_MOUSE_HANDLING "Enable alternative gtk mouse handling optimized for touch devices like Kindle." OFF)
option(WITH_BABEL "Display Treaty of Babel-derived author and title if possible" ON)
option(WITH_SDL "Use SDL for sound support." ON)

if(ENABLE_ALT_MOUSE_HANDLING)
  message(STATUS "Alternative gtk mouse handling enabled.")
  add_definitions(-D_ALT_MOUSE_HANDLING)
endif(ENABLE_ALT_MOUSE_HANDLING)

if(COMPILE_FOR_KINDLE)
  message(STATUS "Setting definitions for Kindle target.")
  add_definitions(-D_KINDLE)
  add_definitions(-D_ALT_MOUSE_HANDLING)
endif(COMPILE_FOR_KINDLE)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  # Check for link-time-optimization support and enable if present.
  check_ipo_supported(RESULT hasIpoSupport OUTPUT ipoSupportCheckResultErrorMessage)
  if(hasIpoSupport)
    message(STATUS "IPO is supported and will be eabled.")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(WARNING "IPO is not supported: ${ipoSupportCheckResultErrorMessage}")
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")

include(Compilers)

add_subdirectory(garglk)
add_subdirectory(terps)
add_subdirectory(tads)

if(WITH_BABEL)
  add_subdirectory(support/babel)
endif()

if(WITH_SDL AND MINGW)
  add_subdirectory(support/sdl_sound)
endif()