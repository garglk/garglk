set(SRCS heglk/heglk.c heglk/heglkunix.c source/he.c source/hemisc.c source/heparse.c source/herun.c source/heexpr.c source/heobject.c source/heres.c source/heset.c source/stringfn.c)

interpreter(hugo "${SRCS}")
target_include_directories(hugo PRIVATE source)
target_compile_definitions(hugo PRIVATE GLK COMPILE_V25 NO_KEYPRESS_CURSOR "HUGO_INLINE=static inline")
