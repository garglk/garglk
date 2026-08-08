# vi: set ts=8:
#
# Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License
# as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
# USA
#

VERSION = 1.4.0

#
# Compiler and Glk library setup.  Configure this section to match the
# system you're using to build Scarier, or override with VARIABLE= on the
# make command line.
#
# The Scarier interpreter is now C-like C++; its sources are *.cpp and are
# compiled with $(CXX).  Unlike upstream SCARE, which unzipped a bundled
# zlib123.zip at build time, this tree links the system zlib (-lz).
#

# Compiler and linker flags.
CC	= cc
CXX	= c++
DEBUG	=
CEXTRA	=
CFLAGS	= -O2 -fPIC $(DEBUG) $(CEXTRA)
CXXFLAGS = -O2 -fPIC -Wall -W $(DEBUG) $(CEXTRA)
LDLIBS	= -lz
LDEXTRA	=
LDFLAGS	= $(DEBUG) $(LDEXTRA)

# Glk library selection and library location.  The default matches this tree:
# terps/cheapglk is the one Glk library checked in beside us, which is enough
# to smoke-test both engines from the command line.  (The shipping macOS build
# goes through the Xcode project, not this Makefile.)
GLK	= cheapglk
#GLK	= glkterm
#GLK	= xglk
#GLK	= winglk
GLKDIR	= ..

# Glk platform selection; normally, the platform's compiler will define
# one of these automatically, but if not, pick one to suit your system.
#GLKARCH= -D__unix
#GLKARCH= -D_WIN32

#
# End of compiler and Glk library set.  The rest of the Makefile is Scarier
# generic.
#

# Set up link libraries and path to Glk.  The Glk library make inclusion
# sets up LINKLIBS and GLKLIB for us; if the file is missing (say, we're
# building only the ANSI interface version), the include should not fail.
# This will of course make GLKLIBS gibberish, but we'll check that later.
# If your make can't handle sinclude or -include, and you're not building
# for Glk, comment out the include line.
GLKPATH	= $(GLKDIR)/$(GLK)
sinclude $(GLKPATH)/Make.$(GLK)
GLKLIBS	= -L$(GLKPATH) $(GLKLIB) $(LINKLIBS)

# Always use the Bourne shell.
SHELL	= /bin/sh

# Miscellaneous tools, overridable by the command line.
RM	= rm
TEST	= test

# Build C-like C++ interpreter objects from .cpp sources.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# ADRIFT 5 engine objects live in adrift5/; -I. lets them pick up the bundled
# zlib headers unzipped into the Scarier root.
adrift5/%.o: adrift5/%.cpp
	$(CXX) $(CXXFLAGS) -I. -c -o $@ $<

# Scarier objects and headers.
OBJECTS	= sctafpar.o sctaffil.o scprops.o scvars.o scexpr.o scprintf.o	\
	  scinterf.o scparser.o sclibrar.o scrunner.o scevents.o	\
	  scnpcs.o scbattle.o scobjcts.o sctasks.o screstrs.o scgamest.o scserial.o\
	  scresour.o scmemos.o scutils.o sclocale.o scdebug.o
HEADERS	= scarier.h scprotos.h scgamest.h

# The map pane: scmap.cpp builds the ADRIFT 4 map model, a5map.cpp the ADRIFT 5
# one, and both draw into the engine-neutral rasteriser in mapdraw.cpp.  Only
# the Glk port has somewhere to put a map, so these stay out of $(OBJECTS).
MAPOBJECTS = mapdraw.o scmap.o
MAPHEADERS = mapdraw.h scmap.h

# The shared, seedable cross-platform RNG (erkyrath xoshiro128**) the v5 engine
# draws from, so seeded walkthroughs reproduce across platforms.  Plain C, and
# it pulls glui32 in from glk.h, so it is built only for the Glk port.
COMMON	= ../common_utils
RANDOBJ	= a5_randomness.o

# ADRIFT 5 engine objects and headers (driven by os_glk.cpp via gsc_a5_main
# when the loaded file is an ADRIFT 5 game).
A5DIR	= adrift5
A5OBJECTS = $(A5DIR)/a5blorb.o $(A5DIR)/a5deobf.o $(A5DIR)/a5xml.o	\
	  $(A5DIR)/a5model.o $(A5DIR)/a5state.o $(A5DIR)/a5restr.o	\
	  $(A5DIR)/a5expr.o $(A5DIR)/a5sexpr.o $(A5DIR)/a5text.o	\
	  $(A5DIR)/a5parse.o $(A5DIR)/a5rand.o $(A5DIR)/a5sb.o		\
	  $(A5DIR)/a5run_ref.o $(A5DIR)/a5run_events.o			\
	  $(A5DIR)/a5run_conv.o $(A5DIR)/a5run_resp.o			\
	  $(A5DIR)/a5run_sort.o						\
	  $(A5DIR)/a5run_action.o $(A5DIR)/a5run.o $(A5DIR)/a5map.o
A5HEADERS = $(A5DIR)/a5blorb.h $(A5DIR)/a5deobf.h $(A5DIR)/a5xml.h	\
	  $(A5DIR)/a5model.h $(A5DIR)/a5state.h $(A5DIR)/a5restr.h	\
	  $(A5DIR)/a5expr.h $(A5DIR)/a5sexpr.h $(A5DIR)/a5text.h	\
	  $(A5DIR)/a5parse.h $(A5DIR)/a5rand.h $(A5DIR)/a5sb.h		\
	  $(A5DIR)/a5run.h $(A5DIR)/a5run_internal.h $(A5DIR)/a5map.h

# Scarier verifier objects and headers.
XOBJECTS= sxmain.o sxutils.o sxglob.o sxtester.o sxscript.o sxfile.o	\
	  sxstubs.o
XHEADERS= sxprotos.h

# Default make target is the basic ANSI interpreter.
all:	scarier

$(OBJECTS): $(HEADERS)

scarier: $(OBJECTS) os_ansi.o
	$(CXX) $(LDFLAGS) -o scarier $(OBJECTS) os_ansi.o $(LDLIBS)

# Option to build the Scarier verifier.
$(XOBJECTS): $(XHEADERS)

sx: $(OBJECTS) $(XOBJECTS)
	$(CXX) $(LDFLAGS) -o sx $(OBJECTS) $(XOBJECTS) $(LDLIBS)

# Option to build the Glk port of Scarier.
glkscarier: $(OBJECTS) $(A5OBJECTS) $(MAPOBJECTS) $(RANDOBJ) os_glk.o
	@$(TEST) -n "$(GLKLIB)" \
		|| ( echo "Could not include $(GLKPATH)/Make.$(GLK)"; exit 1 )
	$(CXX) $(LDFLAGS) -o glkscarier $(OBJECTS) $(A5OBJECTS)		\
		$(MAPOBJECTS) $(RANDOBJ) os_glk.o $(LDLIBS) $(GLKLIBS)

$(A5OBJECTS): $(A5HEADERS)

$(MAPOBJECTS): $(MAPHEADERS)

$(RANDOBJ): $(COMMON)/randomness.c $(COMMON)/randomness.h
	$(CC) $(CFLAGS) -I$(GLKPATH) -c -o $@ $(COMMON)/randomness.c

os_glk.o: os_glk.cpp $(HEADERS) $(A5HEADERS) $(MAPHEADERS)
	$(CXX) $(GLKARCH) $(CXXFLAGS) -Wno-write-strings			\
		-I $(GLKDIR)/$(GLK) -c -o $@ $<

# Option to build an IFP plugin version of Scarier.
scarier_plugin.c: scarier.hdr
	ifphdr $< >$@

scarier_plugin.o: scarier_plugin.c
	@$(TEST) -n "$(GLKLIB)" \
		|| ( echo "Could not include $(GLKPATH)/Make.$(GLK)"; exit 1 )
	$(CC) -I$(GLKPATH) $(CFLAGS) -c -o $@ $<

PLUGIN_LIBS = -lifppi
PLUGIN_OBJS = scarier_plugin.o
scarier-$(VERSION).so: $(OBJECTS) $(A5OBJECTS) $(MAPOBJECTS) $(RANDOBJ)	\
		os_glk.o $(PLUGIN_OBJS)
	$(CXX) -shared -Wl,-u,ifpi_force_link -Wl,-Bsymbolic		\
		-o $@ $(OBJECTS) $(A5OBJECTS) $(MAPOBJECTS) $(RANDOBJ)	\
		os_glk.o $(PLUGIN_OBJS) $(PLUGIN_LIBS) -lc

# Cleanup.
clean:
	$(RM) -f scarier sx glkscarier *.o adrift5/*.o core core.* *.gcov *.gcno *.gcda
	$(RM) -f scarier_plugin.c scarier-$(VERSION).so

distclean: clean

clobber: clean

maintainer-clean: clobber

# Unimplemented targets.
dist check:
