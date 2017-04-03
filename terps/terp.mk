TOP=../..
INCL+=-I$(TOP)/terps/$(TERP) -I$(TOP)/garglk
LIBGARGLK=$(TOP)/garglk/libgarglkmain.a
LIBGARGLK_SO=$(TOP)/garglk/libgarglk.so
EXTLIBS=-lz -lm `pkg-config freetype2 gtk+-x11-2.0 gdk-x11-2.0 gobject-2.0 glib-2.0 fontconfig --libs`
LINK?=$(CC)
BUILDTYPE?=-O2

OBJ=$(SRC:.c=.o)
CXXOBJ=$(CXXSRC:.cc=.o)

all: release
debug: BUILDTYPE=-g -DGARGLK_DEBUG
debug: release
release: $(TERP)

$(TERP): $(OBJ) $(CXXOBJ) $(LIBGARGLK) $(LIBGARGLK_SO)
	$(LINK) -o $@ $(OBJ) $(CXXOBJ) $(LIBGARGLK) $(LIBGARGLK_SO) $(EXTLIBS)

%.o: %.c
	$(CC) -c -o $@ $(BUILDTYPE) $(CFLAGS) $<

%.o: %.cc
	$(CXX) -c -o $@ $(BUILDTYPE) $(CXXFLAGS) $<

clean:
	-rm $(OBJ) $(CXXOBJ) $(TERP) 2>/dev/null ; echo >/dev/null

.PHONY: clean all release debug
