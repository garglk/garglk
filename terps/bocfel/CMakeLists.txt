set(SRCS blorb.c branch.c dict.c iff.c io.c math.c meta.c memory.c objects.c osdep.c process.c random.c screen.c sound.c stack.c unicode.c util.c zoom.c zterp.c glkstart.c)

interpreter(bocfel "${SRCS}")
c99(bocfel)

target_compile_definitions(bocfel PRIVATE ZTERP_GLK)

if(UNIX)
  target_compile_definitions(bocfel PRIVATE ZTERP_UNIX)
elseif(WIN32)
  target_compile_definitions(bocfel PRIVATE ZTERP_WIN32)
endif()
