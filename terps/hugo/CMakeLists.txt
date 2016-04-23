set(srcs heglk/heglk.c heglk/heglkunix.c source/he.c source/hemisc.c source/heparse.c source/herun.c source/heexpr.c source/heobject.c source/heres.c source/heset.c source/stringfn.c)

interpreter(hugo "${srcs}")
include_directories(source)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGLK -DCOMPILE_V25 -DNO_KEYPRESS_CURSOR -DHUGO_INLINE=\"static inline\"")
