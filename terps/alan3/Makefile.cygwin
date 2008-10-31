# Makefile for Alan interpreter to be built with CygWin

#CYGWIN =  -DCOMPILER=\"[MinGWb20]\" -mno-cygwin -I/cygnus/mingw/include -L/cygnus/mingw/lib
#CYGWIN =  -DCOMPILER=\"[MinGWb20]\" -mno-cygwin
CYGWIN =  -DCOMPILER=\"[CygWin32]\"

# Define this to make interpreter log each crc calculation step
#CRCLOG = -DCRCLOG

CFLAGS = -g -Wall -funsigned-char $(CYGWIN) $(CRCLOG)

LDFLAGS = -g $(CYGWIN) $(VERBOSE)
LIB = ../../Glk/WinGlk/Glk.lib

OBJECTS = arun.o \
	main.o \
	debug.o \
	args.o \
	exe.o \
	inter.o \
	parse.o \
	rules.o \
	stack.o \
	decode.o \
	term.o \
	reverse.o \
	readline.o \
	params.o \
	sysdep.o \
	alan.version.o


arun.exe : $(OBJECTS)
	gcc -o arun.exe $(OBJECTS) $(LDFLAGS) $(LIB)
	cp arun.exe ../bin

dumpacd: dumpacd.o reverse.o spa.o
	$(CC) -o dumpacd $(CFLAGS) dumpacd.o reverse.o spa.o

spa.o: spa.c spa.h
	gcc $(CFLAGS) -DSTDIONONCONST -c spa.c -o spa.o 


strip:
	strip arun
	coff2exe arun

alan.version.c : ../alan.version.c alan.version.h
	cp ../alan.version.c .

alan.version.h : ../alan.version.h version.h
	cp ../alan.version.h .

version.h : ../version.h
	cp ../version.h .



#---<< GENERATED DEPENDENCIES FOLLOWS >>---
args.o: args.c args.h main.h types.h sysdep.h acode.h
arun.o: arun.c main.h types.h sysdep.h acode.h term.h version.h args.h
chartest.o: chartest.c
debug.o: debug.c types.h sysdep.h acode.h version.h readline.h inter.h \
 main.h parse.h exe.h debug.h
decode.o: decode.c main.h types.h sysdep.h acode.h decode.h
dumpacd.o: dumpacd.c types.h sysdep.h acode.h spa.h
exe.o: exe.c types.h sysdep.h acode.h readline.h main.h parse.h \
 inter.h stack.h decode.h exe.h
genalan.o: genalan.c spa.h
inter.o: inter.c types.h sysdep.h acode.h main.h parse.h exe.h stack.h \
 inter.h
main.o: main.c sysdep.h types.h acode.h main.h readline.h version.h \
 args.h parse.h inter.h rules.h debug.h stack.h exe.h term.h
params.o: params.c types.h sysdep.h acode.h params.h
parse.o: parse.c types.h sysdep.h acode.h readline.h main.h inter.h \
 exe.h term.h debug.h params.h parse.h
readline.o: readline.c readline.h types.h sysdep.h acode.h main.h
reverse.o: reverse.c types.h sysdep.h acode.h main.h reverse.h
rules.o: rules.c types.h sysdep.h acode.h main.h inter.h debug.h exe.h \
 stack.h rules.h
spa.o: spa.c spa.h
stack.o: stack.c types.h sysdep.h acode.h main.h stack.h
sysdep.o: sysdep.c sysdep.h
term.o: term.c main.h types.h sysdep.h acode.h term.h
version.o: version.c version.h
