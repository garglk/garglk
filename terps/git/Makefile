# Unix makefile for Git (shamelessly ripped off from Glulxe's makefile)

# -----------------------------------------------------------------
# Step 1: pick a Glk library.

# Note: when using xglk, do NOT define USE_MMAP in step 2, below.

GLK = cheapglk
#GLK = glkterm
#GLK = xglk

GLKINCLUDEDIR = ../$(GLK)
GLKLIBDIR = ../$(GLK)
GLKMAKEFILE = Make.$(GLK)

# -----------------------------------------------------------------
# Step 2: pick a C compiler.

# Generic C compiler
CC = cc -O2
OPTIONS = 

# Settings for GCC
#CC = gcc -Wall -O3
#OPTIONS = -DUSE_DIRECT_THREADING -DUSE_MMAP -DUSE_INLINE

# Settings for Clang
#CC = clang -Wall -O3
#OPTIONS = -DUSE_DIRECT_THREADING -DUSE_MMAP -DUSE_INLINE

# -----------------------------------------------------------------
# Step 3: decide where you want to install the compiled executable.

INSTALLDIR = /usr/local/bin

# -----------------------------------------------------------------
# You shouldn't have to change anything from here on down.

MAJOR = 1
MINOR = 3
PATCH = 8

include $(GLKINCLUDEDIR)/$(GLKMAKEFILE)

CFLAGS = $(OPTIONS) -I$(GLKINCLUDEDIR)

LIBS = -L$(GLKLIBDIR) $(GLKLIB) $(LINKLIBS) -lm

HEADERS = version.h git.h config.h compiler.h \
	memory.h opcodes.h labels.inc

SOURCE = compiler.c gestalt.c git.c git_mac.c git_unix.c \
	git_windows.c glkop.c heap.c memory.c opcodes.c \
	operands.c peephole.c savefile.c saveundo.c \
	search.c terp.c accel.c

OBJS = git.o memory.o compiler.o opcodes.o operands.o \
	peephole.o terp.o glkop.o search.o git_unix.o \
	savefile.o saveundo.o gestalt.o heap.o accel.o

all: git

git: $(OBJS)
	$(CC) $(OPTIONS) -o git $(OBJS) $(LIBS)

install: git
	cp git $(INSTALLDIR)/git

clean:
	rm -f *~ *.o git

$(OBJS): $(HEADERS)

version.h: Makefile
	echo "// Automatically generated file -- do not edit!" > version.h
	echo "#define GIT_MAJOR" $(MAJOR) >> version.h
	echo "#define GIT_MINOR" $(MINOR) >> version.h
	echo "#define GIT_PATCH" $(PATCH) >> version.h

DISTZIP = git-$(MAJOR)$(MINOR)$(PATCH).zip

DISTDIR = git-$(MAJOR).$(MINOR).$(PATCH)

DISTFILES = README.txt Makefile $(HEADERS) $(SOURCE)

dist: $(DISTFILES)
	rm -rf $(DISTDIR)
	mkdir $(DISTDIR)
	cp -r $(DISTFILES) $(DISTDIR)
	mkdir $(DISTDIR)/win
	cp win/*.rc win/*.sln win/*.vcxproj win/*.vcxproj.filters $(DISTDIR)/win
	mkdir $(DISTDIR)/win/help
	cp win/help/*.htm win/help/*.css win/help/*.hh* $(DISTDIR)/win/help
	mkdir $(DISTDIR)/win/res
	cp win/res/*.ico win/res/*.manifest $(DISTDIR)/win/res
	find $(DISTDIR) -name "CVS" | xargs rm -rf
	rm -f $(DISTZIP)
	zip -r $(DISTZIP) $(DISTDIR)

