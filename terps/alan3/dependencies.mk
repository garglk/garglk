unit.o: unit.c sysdep.h acode.h reverse.h types.h syserr.h exeTest.c \
  set.h exe.c act.h state.h debug.h params.h parse.h exe.h readline.h \
  main.h inter.h stack.h decode.h saveTest.c save.c stateTest.c state.c \
  parseTest.c parse.c term.h stackTest.c stack.c interTest.c inter.c \
  reverseTest.c reverse.c sysdepTest.c sysdep.c setTest.c set.c \
  mainTest.c main.c alan.version.h version.h args.h rules.h
rules.o: rules.c types.h sysdep.h acode.h main.h inter.h debug.h exe.h \
  stack.h rules.h
debug.o: debug.c types.h sysdep.h acode.h alan.version.h version.h \
  readline.h inter.h main.h parse.h stack.h exe.h debug.h
args.o: args.c sysdep.h args.h types.h acode.h main.h winargs.c
decode.o: decode.c main.h types.h sysdep.h acode.h decode.h syserr.h
term.o: term.c main.h types.h sysdep.h acode.h term.h
readline.o: readline.c sysdep.h readline.h types.h acode.h main.h
params.o: params.c types.h sysdep.h acode.h params.h main.h
act.o: act.c sysdep.h act.h types.h acode.h main.h inter.h exe.h stack.h \
  parse.h debug.h syserr.h
exe.o: exe.c sysdep.h types.h acode.h act.h set.h state.h debug.h \
  params.h parse.h syserr.h exe.h readline.h main.h inter.h stack.h \
  decode.h
sysdep.o: sysdep.c sysdep.h
stack.o: stack.c types.h sysdep.h acode.h main.h syserr.h stack.h
inter.o: inter.c types.h sysdep.h acode.h main.h parse.h exe.h stack.h \
  syserr.h debug.h set.h inter.h
reverse.o: reverse.c types.h sysdep.h acode.h main.h reverse.h
syserr.o: syserr.c main.h types.h sysdep.h acode.h inter.h debug.h
parse.o: parse.c types.h sysdep.h acode.h readline.h main.h inter.h exe.h \
  state.h act.h term.h debug.h params.h syserr.h parse.h
main.o: main.c sysdep.h acode.h types.h set.h state.h main.h syserr.h \
  parse.h readline.h alan.version.h version.h args.h inter.h rules.h \
  reverse.h debug.h stack.h exe.h term.h
set.o: set.c set.h acode.h types.h sysdep.h main.h syserr.h exe.h
arun.o: arun.c sysdep.h main.h types.h acode.h term.h alan.version.h \
  version.h args.h
state.o: state.c sysdep.h types.h acode.h syserr.h main.h set.h exe.h \
  parse.h
save.o: save.c sysdep.h types.h acode.h main.h exe.h set.h readline.h \
  syserr.h
dumpacd.o: dumpacd.c types.h sysdep.h acode.h reverse.h ../compiler/spa.h
reverse.o: reverse.c types.h sysdep.h acode.h main.h reverse.h
