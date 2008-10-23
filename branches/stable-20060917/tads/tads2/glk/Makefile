# Makefile for GlkTADS

# Unfortunately for us, there are two porting variables which must be
# taken into account. One: which OS are you compiling under? Two:
# which Glk library are you using?  You must uncomment the proper
# sections below for your choice of Glk library and OS. If your OS
# and/or Glk library aren't listed below and you get a port working
# anyway, please let me know at stephen@granades.com and I'll
# incorporate the changes into the files using lots of #ifdefs.
#
# There are also a few variables having to do with TADS and directories
# which you will need to set.

# First, the Glk library defines. GLKINCLUDEDIR must be the directory
# containing glk.h and the Make.library file. GLKLIBDIR must be the
# directory containing the library.a file. GLKMAKEFILE must be the name
# of the Make.library file. And GLKFLAGS are the #defines which specify
# which Glk library you're using, along with any other #defines you need
# to make the port work.

#
# GlkTerm
#
#GLKINCLUDEDIR = ./glkterm/
#GLKLIBDIR = ./glkterm/
#GLKMAKEFILE = Make.glkterm
#GLKEXEFILE = tadsr
#GLKFLAGS = -DGLKTERM -DGLKUNIX -DGLK

#
# X Glk
#
#GLKINCLUDEDIR = ./xglk/
#GLKLIBDIR = ./xglk/
#GLKMAKEFILE = Make.xglk
#GLKEXEFILE = xtads
#GLKFLAGS = -DXGLK -DGLKUNIX -DGLK

#
# CheapGlk
#
#GLKINCLUDEDIR = ./cheapglk/
#GLKLIBDIR = ./cheapglk/
#GLKMAKEFILE = Make.cheapglk
#GLKEXEFILE = tadsr
#GLKFLAGS = -DCHEAPGLK -DGLKUNIX -DGLK


# Now the OS-specific defines. OSFLAGS are the OS-specific #defines and
# OPTIMIZE are optimization options for the compiler.

#
# Linux
#
# For Slackware distributions, use
#OSFLAGS = -DLINUX -DOS_TYPES_DEFINED
# For Red Hat, use
#OSFLAGS = -DLINUX
# For all, use the following:
#OPTIMIZE = -O2 -funroll-loops -fomit-frame-pointer -m486
#OSLIBS = -lc

#
# Sun Solaris
#
#OSFLAGS = -DSOLARIS -DOS_TYPES_DEFINED
#OPTIMIZE = -O2
#OSLIBS =

#
# NetBSD
#
#OSFLAGS = -DOS_TYPES_DEFINED
#OPTIMIZE = -O2


# Finally, some other variables. With TADS 3, we now have to know where
# standard include files, libraries, and resources live. There are three
# ways of doing this. The first is to define the variables RESDIR, LIBDIR,
# and INCDIR. The second is to set the environment variables
# TADS3_RESDIR, TADS3_LIBDIR, TADS3_INCLUDEDIR. The third is to do nothing
# and let the interpreter assume that everything lives in the same
# directory.

#RESDIR='"/home/userdir/tads/"'
#LIBDIR='"/home/userdir/tads/"'
#INCDIR='"/home/userdir/tads/"'
#DIRFLAGS = -DDEFINEDIRS -DRESDIR=$(RESDIR) -DLIBDIR=$(LIBDIR) -DINCDIR=$(INCDIR)
DIRFLAGS = 

# Pick a C compiler, any C compiler.
#CC = cc
CC = gcc -ansi -DOSANSI

# Where I keep my .h include files
INCLUDES = -I. -I..


#---------------------------------------------------#
# From here on out, it's all TADS objects and such. #
# Alter at your own risk.                           #
#---------------------------------------------------#

# The machine-dependent OS files.
MACHDEP=oem_glk.o os_glk.o oss_glk.o glkstart.o

# The machine-dependent OS files specific to the runtime
MACHDEPTR=$(MACHDEP)

# The machine-dependent OS files specific to the debugger
MACHDEPTD=$(MACHDEP)

# Now for all the nasty TADS object files
LIBOBJS= ler.o

# added osifc.o
# objects in common to all executables
COMMONOBJS= $(LIBOBJS) mcm.o mcs.o mch.o obj.o cmd.o \
            errmsg.o dummy.o fioxor.o oserr.o runstat.o \
	    fio.o getstr.o cmap.o askf_os.o indlg_tx.o osifc.o

# objects common to the runtime objects (debugger and runtime)
CMNRUNOBJS= $(COMMONOBJS) dat.o lst.o run.o out.o voc.o bif.o output.o \
	    suprun.o regex.o

# objects specific to the compiler (tadsc) and the debugger (tdb). Even
# though we don't build the compiler, we need these for the debugger
COMPOBJS= linf.o prs.o prscomp.o emt.o sup.o fiowrt.o tok.o objcomp.o \
          osgen.o tokth.o dbg.o voccomp.o

# objects specific to the run-time (tadsr) and the debugger (tdb)
RUNOBJS= vocab.o execmd.o ply.o qas.o trd.o dbgtr.o linfdum.o osrestad.o

# objects specific to the debugger (tdb)
DEBUGOBJS= tcgdum.o tdd.o dbgu.o dbgrun.o

# extra objects for TADS (mostly stubs for the now-defunct TADS/Graphic)
CHAROBJS= bifgdum.o

# Finally, the definitive list of TADS objects for the interpreter
TROBJS= $(CMNRUNOBJS) $(RUNOBJS) $(CHAROBJS) $(MACHDEPTR)

# The list of TADS objects for the debugger
DBGOBJS= $(CMNRUNOBJS) $(RUNOBJS) $(COMPOBJS) $(CHAROBJS) $(DEBUGOBJS) \
         $(MACHDEPTD)

# Stick all of our options together
OPTIONS= $(GLKFLAGS) $(OSFLAGS) $(OPTIMIZE) $(DIRFLAGS)


include $(GLKINCLUDEDIR)/$(GLKMAKEFILE)

CFLAGS = $(OPTIONS) -I$(GLKINCLUDEDIR)
LIBS = -L$(GLKLIBDIR) $(OSLIBS) $(GLKLIB) $(LINKLIBS)

all: $(GLKEXEFILE)

$(GLKEXEFILE): $(TROBJS)
	$(CC) $(CFLAGS) $(TROBJS) -o $(GLKEXEFILE) $(LIBS)

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDES) $*.c

#
# Copy files from the main TADS dir
#
askf_os.c: ../askf_os.c
	cp ../askf_os.c askf_os.c

askf_tx.c: ../askf_tx.c
	cp ../askf_tx.c askf_tx.c

bif.c: ../bif.c
	cp ../bif.c bif.c

bifgdum.c: ../bifgdum.c
	cp ../bifgdum.c bifgdum.c

cmd.c: ../cmd.c
	cp ../cmd.c cmd.c

cmap.c: ../cmap.c
	cp ../cmap.c cmap.c

dat.c: ../dat.c
	cp ../dat.c dat.c

dbgtr.c: ../dbgtr.c
	cp ../dbgtr.c dbgtr.c

dummy.c: ../dummy.c
	cp ../dummy.c dummy.c

errmsg.c: ../errmsg.c
	cp ../errmsg.c errmsg.c

execmd.c: ../execmd.c
	cp ../execmd.c execmd.c

fio.c: ../fio.c
	cp ../fio.c fio.c

fioxor.c: ../fioxor.c
	cp ../fioxor.c fioxor.c

getstr.c: ../getstr.c
	cp ../getstr.c getstr.c

indlg_os.c: ../indlg_os.c
	cp ../indlg_os.c indlg_os.c

indlg_tx.c: ../indlg_tx.c
	cp ../indlg_tx.c indlg_tx.c

ler.c: ../ler.c
	cp ../ler.c ler.c

linfdum.c: ../linfdum.c
	cp ../linfdum.c linfdum.c

lst.c: ../lst.c
	cp ../lst.c lst.c

mch.c: ../mch.c
	cp ../mch.c mch.c

mcm.c: ../mcm.c
	cp ../mcm.c mcm.c

mcs.c: ../mcs.c
	cp ../mcs.c mcs.c

obj.c: ../obj.c
	cp ../obj.c obj.c

oserr.c: ../oserr.c
	cp ../oserr.c oserr.c

osrestad.c: ../osrestad.c
	cp ../osrestad.c osrestad.c

out.c: ../out.c
	cp ../out.c out.c

output.c: ../output.c
	cp ../output.c output.c

ply.c: ../ply.c
	cp ../ply.c ply.c

qas.c: ../qas.c
	cp ../qas.c qas.c

osifc.c: ../osifc.c
	cp ../osifc.c osifc.c

regex.c: ../regex.c
	cp ../regex.c regex.c

run.c: ../run.c
	cp ../run.c run.c

runstat.c: ../runstat.c
	cp ../runstat.c runstat.c

suprun.c: ../suprun.c
	cp ../suprun.c suprun.c

trd.c: ../trd.c
	cp ../trd.c trd.c

voc.c: ../voc.c
	cp ../voc.c voc.c

vocab.c: ../vocab.c
	cp ../vocab.c vocab.c


clean:
	rm -f *~ *.o $(GLKEXEFILE)
	rm -f askf_os.c
	rm -f askf_tx.c
	rm -f bif.c
	rm -f bifgdum.c
	rm -f cmd.c
	rm -f cmap.c
	rm -f dat.c
	rm -f dbgtr.c
	rm -f dummy.c
	rm -f errmsg.c
	rm -f execmd.c
	rm -f fio.c
	rm -f fioxor.c
	rm -f getstr.c
	rm -f indlg_os.c
	rm -f indlg_tx.c
	rm -f ler.c
	rm -f linfdum.c
	rm -f lst.c
	rm -f mch.c
	rm -f mcm.c
	rm -f mcs.c
	rm -f obj.c
	rm -f oserr.c
	rm -f osifc.c
	rm -f osrestad.c
	rm -f out.c
	rm -f output.c
	rm -f ply.c
	rm -f qas.c
	rm -f regex.c
	rm -f run.c
	rm -f runstat.c
	rm -f suprun.c
	rm -f trd.c
	rm -f voc.c
	rm -f vocab.c
