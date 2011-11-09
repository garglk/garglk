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

VERSION = 1.3.10

#
# Compiler and Glk library setup.  Configure this section to match the
# system you're using to build SCARE, or override with VARIABLE= on the
# make command line.
#

# GCC compiler and linker flags.
CC	= gcc
DEBUG	=
CEXTRA	=
CFLAGS	= -O2 -D__NO_STRING_INLINES -fPIC -ansi -D_STRICT_ANSI_ -Wall -W   \
	  -pedantic -Wshadow -Wpointer-arith -Wstrict-prototypes	   \
	  -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls	   \
	  -Wwrite-strings -Wundef -Wbad-function-cast -Waggregate-return   \
	  -Wnested-externs						   \
	  $(DEBUG) $(CEXTRA)
LDLIBS	=
LDEXTRA	=
LDFLAGS	= $(DEBUG) $(LDEXTRA)

# Generic compiler and linker flags.
#CC	= cc
#DEBUG	=
#CEXTRA	=
#CFLAGS	=
#LDLIBS	=
#LDEXTRA=
#LDFLAGS=

# Glk library selection and library location.
GLK	= xglk
#GLK	= glkterm
#GLK	= cheapglk
#GLK	= winglk
GLKDIR	= ../..

# Glk platform selection; normally, the platform's compiler will define
# one of these automatically, but if not, pick one to suit your system.
#GLKARCH= -D__unix
#GLKARCH= -D_WIN32

#
# End of compiler and Glk library set.  The rest of the Makefile is SCARE
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
UNZIP	= unzip
RM	= rm
TEST	= test

# SCARE objects and headers.
OBJECTS	= sctafpar.o sctaffil.o scprops.o scvars.o scexpr.o scprintf.o	\
	  scinterf.o scparser.o sclibrar.o scrunner.o scevents.o	\
	  scnpcs.o scobjcts.o sctasks.o screstrs.o scgamest.o scserial.o\
	  scresour.o scmemos.o scutils.o sclocale.o scdebug.o
HEADERS	= scare.h scprotos.h scgamest.h

# SCARE verifier objects and headers.
XOBJECTS= sxmain.o sxutils.o sxglob.o sxtester.o sxscript.o sxfile.o	\
	  sxstubs.o
XHEADERS= sxprotos.h

# Zlib components used by SCARE.
ZLIBVER	= 123
ZLIBSRC	= zlib$(ZLIBVER).zip
ZOBJECTS= crc32.o inffast.o inflate.o inftrees.o zutil.o compress.o	\
	  adler32.o deflate.o trees.o
ZSOURCES= crc32.c inffast.c inflate.c inftrees.c zutil.c compress.c	\
	  adler32.c deflate.c trees.c
ZHEADERS= crc32.h inffast.h inflate.h inftrees.h inffixed.h zutil.h	\
	  zconf.h deflate.h trees.h zlib.h

# Default make target is the basic ANSI interpreter.
all:	scare

$(ZOBJECTS): $(ZHEADERS) $(ZSOURCES)

$(ZHEADERS) $(ZSOURCES): $(ZLIBSRC)
	@$(UNZIP) -uqqa $(ZLIBSRC) $@

$(OBJECTS): $(HEADERS)

scare: $(ZOBJECTS) $(OBJECTS) os_ansi.o
	$(CC) $(LDFLAGS) -o scare $(OBJECTS) $(ZOBJECTS) os_ansi.o $(LDLIBS)

# Option to build the SCARE verifier.
$(XOBJECTS): $(XHEADERS)

sx: $(ZOBJECTS) $(OBJECTS) $(XOBJECTS)
	$(CC) $(LDFLAGS) -o sx $(OBJECTS) $(ZOBJECTS) $(XOBJECTS) $(LDLIBS)

# Option to build the Glk port of SCARE.
glkscare: $(ZOBJECTS) $(OBJECTS) os_glk.o
	@$(TEST) -n "$(GLKLIB)" \
		|| ( echo "Could not include $(GLKPATH)/Make.$(GLK)"; exit 1 )
	$(CC) $(LDFLAGS) -o glkscare $(OBJECTS) $(ZOBJECTS) os_glk.o	\
		$(LDLIBS) $(GLKLIBS)

os_glk.o: os_glk.c $(HEADERS)
	$(CC) $(GLKARCH) $(CFLAGS) -Wno-write-strings			\
		-I $(GLKDIR)/$(GLK) -c -o $@ $<

# Option to build an IFP plugin version of SCARE.
scare_plugin.c: scare.hdr
	ifphdr $< >$@

scare_plugin.o: scare_plugin.c
	@$(TEST) -n "$(GLKLIB)" \
		|| ( echo "Could not include $(GLKPATH)/Make.$(GLK)"; exit 1 )
	$(CC) -I$(GLKPATH) $(CFLAGS) -c -o $@ $<

PLUGIN_LIBS = -lifppi
PLUGIN_OBJS = scare_plugin.o
scare-$(VERSION).so: $(ZOBJECTS) $(OBJECTS) os_glk.o $(PLUGIN_OBJS)
	$(CC) -shared -Wl,-u,ifpi_force_link -Wl,-Bsymbolic		\
		-o $@ $(ZOBJECTS) $(OBJECTS) os_glk.o $(PLUGIN_OBJS)	\
		$(PLUGIN_LIBS) -lc

# Cleanup.
clean:
	$(RM) -f scare sx glkscare *.o core core.* *.gcov *.gcno *.gcda
	$(RM) -f scare_plugin.c scare-$(VERSION).so
	$(RM) -f $(ZSOURCES)
	$(RM) -f $(ZHEADERS)

distclean: clean

clobber: clean

maintainer-clean: clobber

# Unimplemented targets.
dist check:

