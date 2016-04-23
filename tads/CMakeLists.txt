include_directories(../garglk)

set(MACROS "-DGARGOYLE -DVMGLOB_STRUCT -DGLK_UNICODE -DGLK_TIMERS -DTC_TARGET_T3")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MACROS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MACROS}")

include_directories(.)
include_directories(tads2)
include_directories(tads3)

set(srcs t23run.cpp osansi1.c osansi2.c osansi3.c osbuffer.c osparse.c osglk.c osglkban.c t2askf.c t2indlg.c t3askf.cpp t3indlg.cpp memicmp.c vmuni_cs.cpp vmglk.cpp)

add_subdirectory(tads2)
add_subdirectory(tads3)

interpreter(tadsr "${srcs}")
target_link_libraries(tadsr tads2 tads3)
