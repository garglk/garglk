set(srcs blorb.c branch.c dict.c iff.c io.c math.c meta.c memory.c objects.c osdep.c process.c random.c screen.c sound.c stack.c unicode.c util.c zoom.c zterp.c glkstart.c)

interpreter(bocfel "${srcs}")
c99(bocfel)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DZTERP_GLK")

if(UNIX)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DZTERP_UNIX")
elseif(WIN32)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DZTERP_WIN32")
endif()
