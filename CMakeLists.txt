cmake_minimum_required(VERSION 3.0)

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

  # CMake doesn't know how to set C99 mode for icc.
  if("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
    target_compile_options(${TARGET} PUBLIC -std=c99)
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
