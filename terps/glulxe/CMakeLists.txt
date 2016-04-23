set(srcs main.c files.c vm.c exec.c funcs.c operand.c string.c glkop.c heap.c serial.c search.c gestalt.c osdepend.c unixstrt.c accel.c profile.c float.c)

interpreter(glulxe "${srcs}")
target_link_libraries(glulxe m)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFLOAT_COMPILE_SAFER_POWF")

if(NOT WIN32)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOS_UNIX")
endif()
