set(srcs geasfile.cc geas-runner.cc geas-state.cc geas-util.cc geasglk.cc geasglkterm.c istring.cc readfile.cc)

interpreter(geas "${srcs}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_INLINE")
