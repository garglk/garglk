# David Picton's modified makefile for Solaris, etc.

# Hugo makefile for GCC and Unix by Bill Lash, modified by Kent Tessman
#
# (Anything really ugly in this makefile has come about since I
# got my hands on it.  --KT)
#
# Bill's (modified) notes:
#
# Make the executables hc, he, and hd by (editing this appropriately and)
# typing 'make'.
#
# Two of the features of this version of Hugo may cause some problems on
# some terminals.  These features are "special character support" and
# "color handling".
#
# "Special character support" allows some international characters to be
# printed, (e.g. grave-accented characters, characters with umlauts, ...).
# Unfortunately it is was beyond my ability to determine if a particular
# terminal supports these characters.  For example, the xterm that I use
# at work under SunOS 4.1.3 does not support these characters in the default
# "fixed" font family, but if started with xterm -fn courier, to use the
# "courier" font family, it does support these characters.  On my Linux box
# at home, the "fixed" font does support these characters.
#
# "Color handling" allows the game to control the color of text.
# This can only be enabled if using ncurses.  The color handling works pretty
# well on the Linux console, but in a color xterm and setting the TERM
# environment variable to xterm-color, annoying things can sometimes happen.
# I am not sure if the problem lies in the code here, the ncurses library,
# xterm, or the terminfo entry for xterm-color.  Using color rxvt for the
# terminal emulator produces the same results so I suspect the code here, ncurses
# or the terminfo entry.  If the TERM environment variable is xterm, no
# color will be used (although "bright" colors will appear bold).
#
# Note: After looking into this further, it seems that if one runs a
#       color xterm and sets the TERM environment variable to pcansi,
#       the color handling works very well (assuming that you use a
#       background color of black and a forground color of white or gray).
#       The command line to do this is:
#               color_xterm -bg black -fg white -tn pcansi
#       or
#               xterm-color -bg black -fg white -tn pcansi
#
# The following defines set configuration options:
#       DO_COLOR        turns on color handling (requires ncurses)
CFG_OPTIONS=-DDO_COLOR

# Define your optimization flags.  Most compilers understand -O and -O2,
# Debugging
#CFLAGS=-Wall -g 
# Standard optimizations
# Pentium with gcc 2.7.0 or better
#CFLAGS0=-O2 -Wall -fomit-frame-pointer -malign-functions=2 -malign-loops=2 \
-malign-jumps=2
CFLAGS0=-O2 -Wall
# If you are using the ncurses package, define NCURSES for the compiler
CFLAGS=$(CFLAGS0) $(ENV_CFLAGS) -DNCURSES -DGCC_UNIX -DUSE_TEMPFILES \
-DCOMPILE_V25
# For building the engine and/or debugger, you probably also want to add:
# -DNO_LATIN1_CHARSET

# If you need a special include path to get the right curses header, specify
# it.  Note that -fwritable-strings is required for this package.
CC=gcc -fwritable-strings -I/usr/local/include -Isource $(CFG_OPTIONS) $(CFLAGS)

# If using ncurses you need -lncurses, otherwise use -lcurses.  You may also
# need -ltermcap or -ltermlib.  If you need to specify a particular path to
# the curses library, do so here.
#HE_LIBS=-L/usr/lib -lcurses 
#HE_LIBS=-lcurses -ltermcap
#HE_LIBS=-lcurses -ltermlib
#HE_LIBS=-lcurses 
HE_LIBS=-lncurses 

# Shouldn't need to change anything below here.
HC_H=source/hcheader.h source/htokens.h
HE_H=source/heheader.h
HD_H=$(HE_H) source/hdheader.h source/hdinter.h source/htokens.h
HC_OBJS = hc.o hcbuild.o hccode.o hcdef.o hcfile.o hclink.o \
hcmisc.o hccomp.o hcpass.o hcres.o stringfn.o hcgcc.o
HE_OBJS = he.o heexpr.o hemisc.o heobject.o heparse.o herun.o \
heres.o heset.o stringfn.o hegcc.o
HD_OBJS = hd.o hddecode.o hdmisc.o hdtools.o hdupdate.o \
hdval.o hdwindow.o hdgcc.o

all: hc he hd

clean:
	rm -f $(HC_OBJS) $(HD_OBJS) $(HE_OBJS)

hc: $(HC_OBJS)
#	gcc -g -o hc $(HC_OBJS)
	gcc -o hc $(HC_OBJS)

he:
	-$(RM) he*.o
	$(MAKE) HE

HE: $(HE_OBJS)
#       gcc -g -static -o he $(HE_OBJS) $(HE_LIBS)
#       gcc -g -o he $(HE_OBJS) $(HE_LIBS)
	gcc -o he $(HE_OBJS) $(HE_LIBS)

hd:
	-$(RM) he*.o
	ENV_CFLAGS='-DDEBUGGER -DFRONT_END' $(MAKE) HD

HD: $(HE_OBJS) $(HD_OBJS)
#       gcc -g -static -o hd $(HD_OBJS) $(HE_LIBS)
#       gcc -g -o hd $(HD_OBJS) $(HE_LIBS)
	gcc -o hd $(HE_OBJS) $(HD_OBJS) $(HE_LIBS)

iotest: source/iotest.c gcc/hegcc.c $(HE_H)
	$(CC) -o iotest source/iotest.c hegcc.o stringfn.o $(HE_LIBS)

# Portable sources:

hc.o: source/hc.c $(HC_H)
	$(CC) -c source/hc.c

hcbuild.o: source/hcbuild.c $(HC_H)
	$(CC) -c source/hcbuild.c

hccode.o: source/hccode.c $(HC_H)
	$(CC) -c source/hccode.c

hccomp.o: source/hccomp.c $(HC_H)
	$(CC) -c source/hccomp.c

hcdef.o: source/hcdef.c $(HC_H)
	$(CC) -c source/hcdef.c

hcfile.o: source/hcfile.c $(HC_H)
	$(CC) -c source/hcfile.c

hclink.o: source/hclink.c $(HC_H)
	$(CC) -c source/hclink.c

hcmisc.o: source/hcmisc.c $(HC_H)
	$(CC) -c source/hcmisc.c

hcpass.o: source/hcpass.c $(HC_H)
	$(CC) -c source/hcpass.c

hcres.o: source/hcres.c $(HC_H)
	$(CC) -c source/hcres.c

hd.o: source/hd.c $(HD_H) $(HE_H)
	$(CC) -c source/hd.c

hddecode.o: source/hddecode.c $(HD_H) $(HE_H)
	$(CC) -c source/hddecode.c

hdmisc.o: source/hdmisc.c $(HD_H) $(HE_H)
	$(CC) -c source/hdmisc.c

hdtools.o: source/hdtools.c $(HD_H) $(HE_H)
	$(CC) -c source/hdtools.c

hdupdate.o: source/hdupdate.c $(HD_H) $(HE_H)
	$(CC) -c source/hdupdate.c

hdval.o: source/hdval.c $(HD_H) $(HE_H)
	$(CC) -c source/hdval.c

hdwindow.o: source/hdwindow.c $(HD_H) $(HE_H)
	$(CC) -c source/hdwindow.c

he.o: source/he.c $(HE_H)
	$(CC) -c source/he.c

heexpr.o: source/heexpr.c $(HE_H)
	$(CC) -c source/heexpr.c

hemisc.o: source/hemisc.c $(HE_H)
	$(CC) -c source/hemisc.c

heobject.o: source/heobject.c $(HE_H)
	$(CC) -c source/heobject.c

heparse.o: source/heparse.c $(HE_H)
	$(CC) -c source/heparse.c

heres.o: source/heres.c $(HE_H)
	$(CC) -c source/heres.c

herun.o: source/herun.c $(HE_H)
	$(CC) -c source/herun.c

heset.o: source/heset.c $(HE_H)
	$(CC) -c source/heset.c

stringfn.o: source/stringfn.c
	$(CC) -c source/stringfn.c


# Non-portable sources:

hcgcc.o: gcc/hcgcc.c $(HC_H)
	$(CC) -c gcc/hcgcc.c

hegcc.o: gcc/hegcc.c $(HE_H)
	$(CC) -c gcc/hegcc.c

hdgcc.o: gcc/hdgcc.c $(HD_H)
	$(CC) -c gcc/hdgcc.c
