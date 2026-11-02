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

VERSION = 1.5.0

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
# -I. lets the command library in library/, the runner in runner/ and the
# front ends find the engine headers as "core/scarier.h" and the shared
# mapdraw.h / sessrec.h at the root.
CXXFLAGS = -O2 -fPIC -Wall -W $(DEBUG) $(CEXTRA) -I.
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

# -MMD writes a .d file beside each object naming the headers that source
# really includes, so a header edit rebuilds only the objects that use it;
# -MP keeps a deleted header from stopping the build.  The .d files are read
# back in at the bottom of this file.
DEPFLAGS = -MMD -MP

# Build C-like C++ interpreter objects from .cpp sources.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

# ADRIFT 5 engine objects live in adrift5/; -I. lets them include the engine
# headers (core/scarier.h, core/scprotos.h) and the shared ones at the root.
adrift5/%.o: adrift5/%.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -I. -c -o $@ $<

# Scarier objects: the ADRIFT 4 engine in core/ (every sc*.cpp)
# plus the library/ and runner/ topic files.  tools/ is the
# SCARIER_DUMP_TOOLS diagnostic instrumentation, compiled only by
# Makefile.headless; core/scmap.cpp is linked through MAPOBJECTS below.  A
# new .cpp in any of these three places is picked up without editing this
# file (the Xcode project still needs its own entry).
COREDIR	= core
LIBDIR	= library
RUNDIR	= runner
OBJECTS	= $(filter-out $(COREDIR)/scmap.o,				\
	    $(patsubst %.cpp,%.o,$(wildcard $(COREDIR)/sc*.cpp)))	\
	  $(patsubst %.cpp,%.o,$(wildcard $(LIBDIR)/*.cpp) $(wildcard $(RUNDIR)/*.cpp))

# The map pane: core/scmap.cpp builds the ADRIFT 4 map model, a5map.cpp the
# ADRIFT 5 one, and both draw into the engine-neutral rasteriser in
# mapdraw.cpp, which stays at the root with the other shared files.  Only the
# Glk port has somewhere to show a map, but the engine calls
# scmap_forget_game(), so every build links them.
MAPOBJECTS = mapdraw.o $(COREDIR)/scmap.o

# The shared, seedable cross-platform RNG (erkyrath xoshiro128**): the v5
# engine draws from it, so seeded walkthroughs reproduce across platforms, and
# the v4 engine's Runner-compatible generator is the same one.  Plain C, with
# no Glk dependency, so the ANSI build links it too.
COMMON	= ../common_utils
RANDOBJ	= a5_randomness.o

# ADRIFT 5 engine objects (driven by glk/os_glk_a5.cpp via
# gsc_a5_main when the loaded file is an ADRIFT 5 game).
A5DIR	= adrift5
A5SRCDIRS = $(A5DIR) $(A5DIR)/text $(A5DIR)/run
A5OBJECTS = $(patsubst %.cpp,%.o,$(wildcard $(addsuffix /*.cpp,$(A5SRCDIRS))))

# Default make target is the basic ANSI interpreter.
all:	scarier

scarier: $(OBJECTS) $(MAPOBJECTS) $(RANDOBJ) platform/os_ansi.o
	$(CXX) $(LDFLAGS) -o scarier $(OBJECTS) $(MAPOBJECTS) $(RANDOBJ)	\
		platform/os_ansi.o $(LDLIBS)

# Option to build the Glk port of Scarier.  The Glk front end lives in glk/:
# os_glk.cpp (module state, startup, event loop) plus the parts split
# out of it; os_glk_internal.h is what they share.
OSGLKDIR = glk
GLKOBJECTS = $(patsubst %.cpp,%.o,$(wildcard $(OSGLKDIR)/*.cpp))

glkscarier: $(OBJECTS) $(A5OBJECTS) $(MAPOBJECTS) $(RANDOBJ) $(GLKOBJECTS)
	@$(TEST) -n "$(GLKLIB)" \
		|| ( echo "Could not include $(GLKPATH)/Make.$(GLK)"; exit 1 )
	$(CXX) $(LDFLAGS) -o glkscarier $(OBJECTS) $(A5OBJECTS)		\
		$(MAPOBJECTS) $(RANDOBJ) $(GLKOBJECTS) $(LDLIBS) $(GLKLIBS)

$(RANDOBJ): $(COMMON)/randomness.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $(COMMON)/randomness.c

$(GLKOBJECTS): %.o: %.cpp
	$(CXX) $(GLKARCH) $(CXXFLAGS) $(DEPFLAGS) -Wno-write-strings	\
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
		$(GLKOBJECTS) $(PLUGIN_OBJS)
	$(CXX) -shared -Wl,-u,ifpi_force_link -Wl,-Bsymbolic		\
		-o $@ $(OBJECTS) $(A5OBJECTS) $(MAPOBJECTS) $(RANDOBJ)	\
		$(GLKOBJECTS) $(PLUGIN_OBJS) $(PLUGIN_LIBS) -lc

# Cleanup.  ALLOBJECTS is every object any target above builds; each has a
# .d file beside it.
ALLOBJECTS = $(OBJECTS) $(A5OBJECTS) $(MAPOBJECTS) $(RANDOBJ) $(GLKOBJECTS) \
	     platform/os_ansi.o $(PLUGIN_OBJS)
clean:
	$(RM) -f scarier glkscarier $(ALLOBJECTS) $(ALLOBJECTS:.o=.d)
	$(RM) -f *.gcov *.gcno *.gcda
	$(RM) -f scarier_plugin.c scarier-$(VERSION).so

distclean: clean

clobber: clean

maintainer-clean: clobber

# Unimplemented targets.
dist check:

# Header dependencies, as the compiler recorded them on the last build.
-include $(ALLOBJECTS:.o=.d)
