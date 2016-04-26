set(SRCS main.c files.c vm.c exec.c funcs.c operand.c string.c glkop.c heap.c serial.c search.c gestalt.c osdepend.c unixstrt.c accel.c profile.c float.c)

interpreter(glulxe "${SRCS}")
target_link_libraries(glulxe m)
target_compile_definitions(glulxe PRIVATE FLOAT_COMPILE_SAFER_POWF)

if(UNIX)
  target_compile_definitions(glulxe PRIVATE OS_UNIX)
endif()
