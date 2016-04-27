cmake_minimum_required(VERSION 3.0)

include(CheckCCompilerFlag)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

option(WITH_BABEL "Use babel." ON)
option(WITH_SDL "Use SDL for sound." ON)

function(c99 TARGET)
  if(${CMAKE_MINOR_VERSION} EQUAL 0)
    if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
      target_compile_options(${TARGET} PUBLIC -std=c99)
    elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
      target_compile_options(${TARGET} PUBLIC -std=c99)
    endif()
  else()
    set_property(TARGET ${TARGET} PROPERTY C_STANDARD 99)
    set_property(TARGET ${TARGET} PROPERTY C_STANDARD_REQUIRED ON)
    set_property(TARGET ${TARGET} PROPERTY C_EXTENSIONS OFF)
  endif()

  # CMake doesn't know how to set C99 mode for Intel or Sun.
  if("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
    set(flag -std=c99)
  elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "SunPro")
    set(flag -xc99=all)
  endif()

  if(DEFINED flag)
    check_c_compiler_flag(${flag} HAS_C99)
    if(HAS_C99)
      target_compile_options(${TARGET} PUBLIC ${flag})
    endif()
  endif()
endfunction()

add_subdirectory(garglk)
add_subdirectory(terps)
add_subdirectory(tads)

if(WITH_BABEL)
  add_subdirectory(support/babel)
endif()

if(WITH_SDL AND MINGW)
  add_subdirectory(support/sdl_sound)
endif()
