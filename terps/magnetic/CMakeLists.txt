set(srcs Generic/emu.c Glk/glk.c)

interpreter(magnetic "${srcs}")
include_directories(Generic)
