set(SRCS Generic/emu.c Glk/glk.c)

interpreter(magnetic "${SRCS}")
include_directories(Generic)
