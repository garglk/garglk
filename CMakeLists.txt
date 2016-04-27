cmake_minimum_required(VERSION 3.0)

include(CheckCCompilerFlag)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")

option(WITH_BABEL "Use babel." ON)
option(WITH_SDL "Use SDL for sound." ON)

function(c99 target)
  if(${CMAKE_MINOR_VERSION} EQUAL 0)
    if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
      target_compile_options(${target} PUBLIC -std=c99)
    elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
      target_compile_options(${target} PUBLIC -std=c99)
    endif()
  else()
    set_property(TARGET ${target} PROPERTY C_STANDARD 99)
    set_property(TARGET ${target} PROPERTY C_STANDARD_REQUIRED ON)
    set_property(TARGET ${target} PROPERTY C_EXTENSIONS OFF)
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
      target_compile_options(${target} PUBLIC ${flag})
    endif()
  endif()
endfunction()

function(lto target)
  if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    set(flags -flto)
  elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION 1)
  elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "SunPro")
    # Solaris Studio requires -xO4 or greater when using IPO, so set
    # here, cross fingers, and hope it overrides any other optimization
    # level (if -xO4 comes after others, it will).
    set(flags -xipo=2 -xO4)
  endif()

  if(DEFINED flags)
    foreach(flag ${flags})
      check_c_compiler_flag(${flag} HAS_LTO)
      if(HAS_LTO)
        target_compile_options(${target} PUBLIC ${flag})
        set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS " ${flag}")
      endif()
    endforeach()
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
