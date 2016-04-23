set(srcs geasfile.cc geas-runner.cc geas-state.cc geas-util.cc geasglk.cc geasglkterm.c istring.cc readfile.cc)

interpreter(geas "${srcs}")
target_compile_definitions(geas PRIVATE USE_INLINE)
