set(srcs Glk/glk.c bitmap.c level9.c)

interpreter(level9 "${srcs}")
target_link_libraries(level9 m)
include_directories(.)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBITMAP_DECODER -DNEED_STRICMP_PROTOTYPE -Dstricmp=gln_strcasecmp -Dstrnicmp=gln_strncasecmp")
