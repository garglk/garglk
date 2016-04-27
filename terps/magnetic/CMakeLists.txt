set(SRCS Generic/emu.c Glk/glk.c)

interpreter(magnetic "${SRCS}")
target_include_directories(magnetic PRIVATE Generic)
