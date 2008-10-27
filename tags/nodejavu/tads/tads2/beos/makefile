#
# Makefile for BeOS-based TADS 2
# Ported by Christopher Tate, <ctate@acm.org>
#
# Compile TADS2 for BeOS from within the 'beos' subdirectory
# of the main TADS2 tree.

#
# Places to look for include files.
#
INCLUDES = -I. -I..

#
# Current patchlevel of BeOS sources.  Note that this is a *string*.
#
BEOSPATCHLEVEL= -DBEOSPATCHLEVEL=\"1\"

#
# Name of the BeOS port maintainers
# Add your name here if you do a new BeOS port.
#
CTATE=Christopher Tate <ctate@acm.org>

#
# BeOS x86
#
ifdef COMPILE_FOR_DEBUG
OPTIMIZE=-g -O0
else
OPTIMIZE=-O3
endif

CC=gcc
BEOSFLAGS=-Wall -W -Wno-multichar -Wno-unused -DOSANSI
SYSFLAGS= -DSYSPL=\"1\" -DPORTER=\""$(CTATE)"\" -DSYSMAINTAINER=\""BeOS port maintained by $(CTATE)\n"\"
CFLAGS= $(BEOSFLAGS) $(SYSFLAGS)
LIBS=-lbe
CP=copyattr --data
MAKEXEC=mimeset -f

#
# compilation rules
#
%.o : %.cpp
	$(CC) -c $< $(OPTIMIZE) $(BEOSPATCHLEVEL) $(CFLAGS) $(INCLUDES) -o $@

# look in the main TADS2 directory for source files first
%.o : ../%.c
	$(CC) -c $< $(OPTIMIZE) $(BEOSPATCHLEVEL) $(CFLAGS) $(INCLUDES) -o $@

# compile out of this directory only if it isn't in the main TADS2 directory
%.o : %.c
	$(CC) -c $< $(OPTIMIZE) $(BEOSPATCHLEVEL) $(CFLAGS) $(INCLUDES) -o $@

#
# Various groups of object files to be linked together in various combinations
#

# objects in common to all executables
COMMONOBJS= ler.o mcm.o mcs.o mch.o \
	obj.o cmd.o errmsg.o dummy.o fioxor.o os0.o argize.o \
	oserr.o cmap.o regex.o osifc.o osbeos.o

# common run objects for everything with a run-time
CMNRUNOBJS_NOHTML = $(COMMONOBJS) \
	dat.o lst.o out.o voc.o bif.o suprun.o run.o

# common objects for all executables incorporating a run-time and no HTML
CMNRUNOBJS= $(CMNRUNOBJS_NOHTML) output.o

# common objects for run-times with HTML
CMNRUNOBJS_HTML = $(CMNRUNOBJS_NOHTML)

# extra objects beyond the common ones required for the runtime executable
RUNOBJS_EXTRA= \
	vocab.o execmd.o runstat.o ply.o trd.o  linfdum.o fio.o \
	getstr.o qas.o dbgtr.o askf_os.o 

# objects for the run-time system executable with no HTML
RUNOBJS= $(CMNRUNOBJS) $(RUNOBJS_EXTRA) osbeos_nohtml.o

# objects for an HTML run-time executable
RUNOBJS_HTML= $(CMNRUNOBJS_HTML) $(RUNOBJS_EXTRA) run.o

# extra objects for character-mode run-time
CHAROBJS= \
	osnoui.o os0tr_beos.o bifgdum.o indlg_tx.o

# objects for compiler (includes run-time system)
COMPOBJS= $(CMNRUNOBJS) \
	askf_tx.o bifgdum.o dbg.o emt.o fio.o fiowrt.o \
	getstr.o  indlg_tx.o linf.o objcomp.o prs.o prscomp.o \
	sup.o tcd.o tok.o tokth.o voccomp.o \
	osnoui.o os0tc_beos.o osbeos_nohtml.o osbeos.o

TCOBJS= $(COMPOBJS) tcgdum.o

# extra objects for all builds of the debugger
DBG_EXTRA = \
         vocab.o execmd.o runstat.o \
         ply.o fio.o sup.o \
         linf.o prs.o emt.o \
         tok.o tdd.o os0td_beos.o \
         dbg.o dbgrun.o \
         objcomp.o tokth.o \
         voccomp.o getstr.o \
         qas.o run.o \
         askf_os.o 

# objects for the standard debugger
DBGOBJS= $(CMNRUNOBJS) $(DBG_EXTRA) \
         osnoui.o bifgdum.o osr.o \
         osgenr.o dbgu.o \
         indlg_tx.o

# objects for the html debugger
DBGOBJS_HTML = $(CMNRUNOBJS_HTML) $(DBG_EXTRA) 

# extra objects for BeOS HTML libraries
WIN32_HTML_LIB_EXTRA = \
        bifgdum.o osnoui.o

# identifying strings
TR_OEM_OBJ = oem_beos.o

#
# Targets
#

all: tadsr tadsc tadsr.stripped tadsc.stripped trh.a tdbh.a #tdb

# TADS runtime library for BeOS HTML
trh.a: $(RUNOBJS_HTML) $(WIN32_HTML_LIB_EXTRA)
	ar -rc $@ $^

# TADS debugger library for BeOS HTML
tdbh.a: $(DBGOBJS_HTML) $(WIN32_HTML_LIB_EXTRA) 
	ar -rc $@ $^

# the TADS compiler - tadsc
tadsc: $(TCOBJS)
	$(CC) -o $@ $(CFLAGS) $^ $(LIBS)
	$(MAKEXEC) $@

tadsc.stripped: tadsc
	$(CP) $< $@
	strip $@

# The TADS runtime interpreter - tadsr
tadsr: $(RUNOBJS) $(CHAROBJS) $(TR_OEM_OBJ)
	$(CC) -o $@ $(CFLAGS) $^ $(LIBS)
	$(MAKEXEC) $@

tadsr.stripped: tadsr
	$(CP) $< $@
	strip $@

# The TADS debugger - tdb
# There's no debugger support for BeOS yet.  Be patient.
tdb: $(DBGOBJS) $(TR_OEM_OBJ)
	$(CC) -o $@ $(CFLAGS) $^ $(LIBS)
	$(MAKEXEC) $@

tdb.stripped: tdb
	$(CP) $< $@
	strip $@

tadsr.static: $(RUNOBJS) $(CHAROBJS) $(TR_OEM_OBJ)
	$(CC) -o $@ $(CFLAGS) $^ -static $(LIBS)
	$(MAKEXEC) $@

tadsc.static: $(TCOBJS)
	$(CC) -o $@ $(CFLAGS) $^ -static $(LIBS)
	$(MAKEXEC) $@

tdb.static: $(DBGOBJS) $(TR_OEM_OBJ)
	$(CC) -o $@ $(CFLAGS) $^ -static $(LIBS)
	$(MAKEXEC) $@

#
# Clean up. 
#
# In addition to removing .o's and binaries, this
# also removes files copied from the general source directory.
#
clean:
	rm -f *.o
	rm -f tadsc tadsr tdb trh.a tdbh.a
	rm -f tadsc.static tadsr.static
	rm -f tadsc.stripped tadsr.stripped tdb.stripped
