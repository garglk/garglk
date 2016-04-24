set(SRCS arun.c main.c debug.c args.c exe.c inter.c parse.c rules.c stack.c decode.c term.c reverse.c readline.c params.c sysdep.c glkstart.c glkio.c alan.version.c)

interpreter(alan2 "${SRCS}")
target_compile_definitions(alan2 PRIVATE GLK REVERSED)
