TOP=../..
INCL=-I$(TOP)/terps/advsys -I$(TOP)/garglk
BUILDTYPE?=-O2
CFLAGS=-w $(INCL)
ARFLAGS=rcs

LIBGARGLK=$(TOP)/garglk/libgarglkmain.a
LIBGARGLK_SO=$(TOP)/garglk/libgarglk.so
EXTLIBS=-lz -lm `pkg-config freetype2 gtk+-x1 1-2.0 gdk-x11-2.0 gobject-2.0 glib-2.0 fontconfig --libs`

SRC= adrift.c advsys.c agt.c alan.c babel_handler.c blorb.c executable.c glulx.c hugo.c ifiction.c level9.c \
			magscrolls.c md5.c misc.c register.c register_ifiction.c tads2.c tads3.c tads.c zcode.c
OBJ=$(SRC:.c=.o)

all: release

debug: BUILDTYPE=-g -DGARGLK_DEBUG
debug: release
release: babel_static.a

babel_static.a: $(OBJ)
	$(AR) $(ARFLAGS) babel_static.a $(OBJ)

%.o: %.c
	$(CC) -c -o $@ $(BUILDTYPE) $(CFLAGS) $<

clean:
	-rm babel_static.a $(OBJ) 2>/dev/null ; echo >/dev/null

.PHONY: clean all debug release
