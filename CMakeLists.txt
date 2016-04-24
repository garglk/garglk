cmake_minimum_required(VERSION 3.5)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

option(WITH_BABEL "Use babel." ON)

macro(interpreter TARGET SRCS)
  add_executable(${TARGET} ${SRCS})
  set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME "${GARGLKPRE}${TARGET}")
  target_compile_definitions(${TARGET} PRIVATE GARGLK)
  target_link_libraries(${TARGET} glkmain garglk)
  install(TARGETS ${TARGET} DESTINATION libexec/gargoyle)
endmacro()

macro(c99 TARGET)
  set_property(TARGET ${TARGET} PROPERTY C_STANDARD 99)
  set_property(TARGET ${TARGET} PROPERTY C_STANDARD_REQUIRED ON)
  set_property(TARGET ${TARGET} PROPERTY C_EXTENSIONS OFF)

  # CMake doesn't know how to set C99 mode for icc.
  if("${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
    target_compile_options(${TARGET} PUBLIC -std=c99)
  endif()
endmacro()

add_subdirectory(garglk)
add_subdirectory(terps)
add_subdirectory(tads)

if(WITH_BABEL)
  add_subdirectory(support/babel)
endif()
