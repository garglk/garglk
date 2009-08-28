# Makefile for Alan interpreter to be built with CygWin32/Mingw32
# Note: uses a lot of special stuff, if you are porting, start from
# one of the other makefiles, such as Makefile.unix

VERSION = `cd ..; venum alan -print "\\$$v.\\$$r\\$$s\\$$c"`

CYGWIN	= -mno-cygwin
CYGWIN	= 

OPTIMIZE = -ggdb
EXE	= arun.exe

COMPILEFLAGS = $(OPTIMIZE) $(CYGWIN) -Wall -funsigned-char $(CRCLOG)
CFLAGS	= $(COMPILEFLAGS)
ARCH	= $(CYGWIN)$(OPTIMIZE)$(GLKDEFS)$(DMALLOC)
LIBS	=

#Settings for Windows GLK variant
WINGLKROOT = ../../WindowsGLK
WINGLKDEFS = -DHAVE_WINGLK
WINGLKINCLUDE = -I$(WINGLKROOT)/Include
WINGLKLIB = $(WINGLKROOT)/Glk.lib

# Settings for TermGLK
TERMGLKROOT = ../../glkterm
TERMGLKDEFS = -DHAVE_GLK
TERMGLKINCLUDE = -I$(TERMGLKROOT)
TERMGLKLIB = -L$(TERMGLKROOT) -lglkterm -lncurses

#Settings for Gargoyle GLK variant (experimental!)
GARGLKROOT = ../../Gargoyle/garglk
GARGLKDEFS = -DHAVE_GARGLK -DHAVE_GLK -mno-cygwin -mwindows
GARGLKINCLUDE = -I$(GARGLKROOT)
GARGLKLIB = -L$(GARGLKROOT)/../build/debug/garglk -lgarglk -lpng -lgtk -lgdk `pkg-config --libs freetype2` -ljpeg ../../../fmodapi374win/api/lib/libfmod.a


#Set up for specific target
winarun: GLKDEFS = $(WINGLKDEFS)
winarun: CFLAGS = $(COMPILEFLAGS) $(WINGLKINCLUDE) $(GLKDEFS)
winarun: CYGWIN = -mno-cygwin -mwindows
winarun: LIBS = $(WINGLKLIB)
winarun: EXE = winarun.exe

glkarun: GLKDEFS = $(TERMGLKDEFS)
glkarun: CFLAGS = $(COMPILEFLAGS) $(TERMGLKINCLUDE) $(GLKDEFS)
glkarun: CYGWIN = 
glkarun: LIBS = $(TERMGLKLIB)
glkarun: EXE = glkarun.exe

gararun: GLKDEFS = $(GARGLKDEFS)
gararun: CFLAGS = $(COMPILEFLAGS) $(GARGLKINCLUDE) $(GLKDEFS)
gararun: CYGWIN = 
gararun: LIBS = $(GARGLKLIB)
gararun: EXE = gararun.exe

dmalloc: DMALLOC = -DDMALLOC
dmalloc: CFLAGS = $(COMPILEFLAGS) $(DMALLOC)
dmalloc: LIBS = /usr/local/lib/libdmalloc.a
release: OPTIMIZE = -O4

# Define this to make interpreter log each crc calculation step
#CRCLOG = -DCRCLOG


CPP	= gcc -MM -ansi

LDFLAGS = $(CYGWIN) $(VERBOSE)

GLKSRCS = glkstart.c glkio.c


# Include common source lists 
include sources.mk


arun: checkTarget $(ARUNOBJECTS) unitTest dumpacd Makefile dependencies.mk
	gcc -o $@ $(OPTIMIZE) $(ARUNOBJECTS) $(LDFLAGS) $(LIBS)
	-@if test -f ../bin; then :; else mkdir ../bin 2> /dev/null ; fi
	cp $@ ../bin

all:
	$(MAKE) arun
	$(MAKE) winarun
#	$(MAKE) glkarun

release:
	$(MAKE) "OPTIMIZE=-O4" arun
	$(MAKE) "OPTIMIZE=-O4" winarun

winarun: checkTarget $(WINARUNOBJECTS) winglk.o arun.res
	gcc -o $@ $(OPTIMIZE) $(WINARUNOBJECTS) winglk.o arun.res $(LDFLAGS) $(LIBS)
	-@if test -f ../bin; then :; else mkdir ../bin 2> /dev/null ; fi
	cp $@ ../bin

glkarun: checkTarget $(TERMARUNOBJECTS) arun.res
	gcc -o $@ $(OPTIMIZE) $(TERMARUNOBJECTS) arun.res $(LDFLAGS) $(LIBS)
	-@if test -f ../bin; then :; else mkdir ../bin 2> /dev/null ; fi
	cp $@ ../bin

dmalloc: checkTarget $(ARUNOBJECTS) unitTest
	gcc -o arun $(OPTIMIZE) $(ARUNOBJECTS) $(LDFLAGS) $(LIBS)
	-@if test -f ../bin; then :; else mkdir ../bin 2> /dev/null ; fi
	cp arun.exe ../bin

arun.res: arun.rc arun.ico resources.h alan.version.c
	echo $(VERSION)
	sed "s/WINARUNVERSION/$(VERSION)/" arun.rc > arun.rc2
	windres arun.rc2 -O coff -o $@


about: about.c arun.res
	gcc -o $@ -ggdb -mno-cygwin -mwindows about.c arun.res

headerGenerator: headerGenerator.c

clean:
	-rm *.o

.PHONY: checkTarget
checkTarget:
	-@if test -f .arch; then :; else echo "none" > .arch; fi
	-@if test "`cat .arch`" != "$(ARCH)"; then \
		echo Removing objects for \'`cat .arch`\', re-building for \'$(ARCH)\'... ; \
		rm *.o; \
		/bin/echo -n $(ARCH) > .arch; \
	fi


#
# Version number generation
#
alan.version.h :
	cd ..; venum -write always -all alan
	cp -p ../alan.version.h .

version.h :
	cd ..; venum -write always -all alan
	cp -p ../version.h .

../alan.version: $(VERSIONSRCS) 
	cd ..; venum alan time

alan.version.c:  ../alan.version
	cp -p ../alan.version.c .

#++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#
# Unit testing
#

unit : checkTarget $(TESTOBJECTS)
	gcc -o unit $(TESTOBJECTS) $(LDFLAGS) $(LIBS)

unitTest: unit
	@./unit

# Run all tests!
test: unitTest


#
#
#
#
dumpacd: dumpacd.c reverse.o ../compiler/spa.c acode.h sysdep.c
	gcc -Wall -o dumpacd -ggdb -DSTDIONONCONST dumpacd.c ../compiler/spa.c reverse.c sysdep.c
	cp $@ ../bin




#
# Dependencies are generate only for command line case
# Dependencies on libraries are only in the way for other cases,
# so special dependencies that we need are set explicitly below
#
depend:
	@-for f in $(UNITSRCS) $(BUILDSRCS) $(MAINSRCS) $(GLKSRCS) dumpacd.c reverse.c; \
	  do \
	  $(CPP) $(CFLAGS) $$f >> dependencies.new; \
	done
	mv dependencies.new dependencies.mk

# Extra dependencies for WinGLK case
readline.o : resources.h

include dependencies.mk
