! Makefile for arun on VAX		      Date: 93-10-24/THONI@_GOLLUM
!

CQ =/STANDARD=PORTABLE

OBJECTS = arun.obj args.obj debug.obj exe.obj inter.obj parse.obj rules.obj stack.obj params.obj term.obj decode.obj sysdep.obj reverse.obj

arun.obj : arun.c

args.obj : args.c

debug.obj : debug.c

exe.obj : exe.c

inter.obj : inter.c

parse.obj : parse.c

rules.obj : rules.c

stack.obj : stack.c

term.obj : term.c

params.obj : params.c

decode.obj : decode.c

reverse.obj : reverse.c

sysdep.obj : sysdep.c

version.obj : version.c

arun.exe : #(OBJECTS) version.obj
	$ link/exe=arun #(LQ) arun.obj,args.obj,debug.obj,exe.obj,inter.obj,parse.obj,rules.obj,stack.obj,params.obj,term.obj,decode.obj,reverse.obj,sysdep.obj,version.obj
	$ copy arun.exe <->

