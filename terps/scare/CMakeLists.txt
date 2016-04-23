set(srcs sctafpar.c sctaffil.c scprops.c scvars.c scexpr.c scprintf.c scinterf.c scparser.c sclibrar.c scrunner.c scevents.c scnpcs.c scobjcts.c sctasks.c screstrs.c scgamest.c scserial.c scresour.c scutils.c scdebug.c sclocale.c scmemos.c os_glk.c)

interpreter(scare "${srcs}")

if(UNIX)
  include_directories(${ZLIB_INCLUDE_DIRS})
  target_link_libraries(scare ${ZLIB_LIBRARIES})
elseif(WIN32)
  include_directories(../../support/zlib)
  target_link_libraries(scare ${CMAKE_CURRENT_SOURCE_DIR}/../../support/zlib/zlib1.dll)
endif()
