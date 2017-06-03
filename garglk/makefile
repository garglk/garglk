TOP=..
EXTCFLAGS=`pkg-config freetype2 gtk+-x11-2.0 gdk-x11-2.0 gobject-2.0 glib-2.0 fontconfig --cflags`
INCL=-I/usr/include/SDL -I$(TOP)/support/babel -I$(TOP)/garglk -I$(TOP)/support/sdl -I$(TOP)/support/sdl_sound
DEFMACRO=-DGARGLKINI=\"/etc/garglk.ini\" -DGARGLKPRE=\"\" -DBABEL_HANDLER -DBUNDLED_FONTS
LIBBABEL=$(TOP)/support/babel/babel_static.a
EXTLIBS=`pkg-config freetype2 gtk+-x11-2.0 gdk-x11-2.0 gobject-2.0 glib-2.0 fontconfig --libs` \
					-ljpeg -lpng -lz -lrt -lSDL_mixer -lSDL_sound -lSDL
BUILDTYPE?=-O2
CFLAGS=$(BUILDTYPE) $(EXTCFLAGS) -fPIC $(INCL) $(DEFMACRO)
ARFLAGS=rcs

SRC=gi_blorb.c gi_dispa.c cggestal.c cgblorb.c cgfref.c cgmisc.c cgstyle.c cgstream.c cgunicod.c cgdate.c window.c \
	winblank.c winpair.c wingrid.c wintext.c wingfx.c winmask.c event.c draw.c config.c imgload.c imgscale.c fontdata.c \
	babeldata.c sndsdl.c sysgtk.c fontfc.c
OBJ=$(SRC:.c=.o)

all: release

debug: BUILDTYPE=-g -DGARGLK_DEBUG
debug: release
release: libgarglkmain.a libgarglk.so gargoyle

libgarglkmain.a: main.o
	$(AR) $(ARFLAGS) $@ $<

libgarglk.so: $(OBJ) $(LIBBABEL)
	$(CC) -shared -Wl,-soname,$@ -o $@ $(OBJ) $(LIBBABEL) $(EXTLIBS)

gargoyle: launcher.o launchgtk.o libgarglk.so
	$(CC) -o $@ $^ $(EXTLIBS)

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $<

clean:
	-rm *.o libgarglkmain.a libgarglk.so gargoyle 2>/dev/null ; echo >/dev/null

.PHONY: clean
