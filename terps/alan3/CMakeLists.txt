set(srcs alan.version.c act.c actor.c args.c arun.c attribute.c checkentry.c class.c current.c debug.c decode.c dictionary.c event.c exe.c glkio.c glkstart.c instance.c inter.c lists.c literal.c main.c memory.c msg.c options.c output.c params.c parse.c readline.c reverse.c rules.c save.c scan.c score.c set.c stack.c state.c syntax.c sysdep.c syserr.c term.c utils.c word.c compatibility.c AltInfo.c Container.c Location.c ParameterPosition.c StateStack.c)

interpreter(alan3 "${srcs}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGLK -DHAVE_GARGLK -DBUILD=0")
