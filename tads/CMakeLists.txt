include_directories(../garglk)

macro(tadsdefs target)
  target_compile_definitions(${target} PRIVATE GARGOYLE VMGLOB_STRUCT GLK_UNICODE GLK_TIMERS TC_TARGET_T3)
endmacro()

include_directories(.)
include_directories(tads2)
include_directories(tads3)

add_subdirectory(tads2)
add_subdirectory(tads3)

set(SRCS t23run.cpp osansi1.c osansi2.c osansi3.c osbuffer.c osparse.c osglk.c osglkban.c t2askf.c t2indlg.c t3askf.cpp t3indlg.cpp memicmp.c vmuni_cs.cpp vmglk.cpp)
interpreter(tadsr "${SRCS}")
tadsdefs(tadsr)

target_link_libraries(tadsr tads2 tads3)
