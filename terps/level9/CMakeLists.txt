set(SRCS Glk/glk.c bitmap.c level9.c)

interpreter(level9 "${SRCS}")
target_link_libraries(level9 m)
include_directories(.)
target_compile_definitions(level9 PRIVATE BITMAP_DECODER NEED_STRICMP_PROTOTYPE stricmp=gln_strcasecmp strnicmp=gln_strncasecmp)
