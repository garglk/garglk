TOP=..
DEFMACRO=-DGARGOYLE -DVMGLOB_STRUCT -DGLK_UNICODE -DGLK_TIMERS -DTC_TARGET_T3 -DOS_USHORT_DEFINED -DOS_UINT_DEFINED -DOS_ULONG_DEFINED
INCL=-Itads2 -Itads3 -I. -I../garglk
BUILDTYPE?=-O2
CFLAGS=$(INCL) $(DEFMACRO)
CXXFLAGS=-std=c++11 $(INCL) $(DEFMACRO)
ARFLAGS=rcs

CXXSRC=t23run.cpp t3askf.cpp t3indlg.cpp vmuni_cs.cpp
SRC=osansi1.c osansi2.c osansi3.c osbuffer.c osparse.c osglk.c osglkban.c t2askf.c t2indlg.c memicmp.c
LIBGARGLK=$(TOP)/garglk/libgarglkmain.a
LIBGARGLK_SO=$(TOP)/garglk/libgarglk.so
EXTLIBS=-lz -lm `pkg-config freetype2 gtk+-x11-2.0 gdk-x11-2.0 gobject-2.0 glib-2.0 fontconfig --libs`
SRCTADS2=tads2/osifc.c tads2/osrestad.c tads2/oem.c tads2/argize.c tads2/bif.c \
         tads2/bifgdum.c tads2/cmap.c tads2/cmd.c tads2/dat.c tads2/dbgtr.c \
         tads2/errmsg.c tads2/execmd.c tads2/fio.c tads2/fioxor.c tads2/getstr.c \
         tads2/ler.c tads2/linfdum.c tads2/lst.c tads2/mch.c tads2/mcm.c \
         tads2/mcs.c tads2/obj.c tads2/oserr.c tads2/os0.c tads2/out.c \
         tads2/output.c tads2/ply.c tads2/qas.c tads2/regex.c tads2/run.c \
         tads2/runstat.c tads2/suprun.c tads2/trd.c tads2/voc.c tads2/vocab.c

SRCTADS3=tads3/vmcrc.cpp tads3/vmmain.cpp tads3/std.cpp tads3/charmap.cpp \
         tads3/resload.cpp tads3/resldexe.cpp tads3/vminit.cpp \
         tads3/vmini_nd.cpp tads3/vmconsol.cpp tads3/vmconhmp.cpp \
         tads3/vminitim.cpp tads3/vmcfgmem.cpp tads3/vmobj.cpp tads3/vmundo.cpp \
         tads3/vmtobj.cpp tads3/vmpat.cpp tads3/vmstrcmp.cpp tads3/vmdict.cpp \
         tads3/vmgram.cpp tads3/vmstr.cpp tads3/vmcoll.cpp tads3/vmiter.cpp \
         tads3/vmlst.cpp tads3/vmsort.cpp tads3/vmsortv.cpp tads3/vmbignum.cpp \
         tads3/vmvec.cpp tads3/vmintcls.cpp tads3/vmanonfn.cpp tads3/vmlookup.cpp \
         tads3/vmbytarr.cpp tads3/vmcset.cpp tads3/vmfilobj.cpp tads3/vmstack.cpp \
         tads3/vmerr.cpp tads3/vmerrmsg.cpp tads3/vmpool.cpp tads3/vmpoolim.cpp \
         tads3/vmtype.cpp tads3/vmtypedh.cpp tads3/utf8.cpp tads3/vmglob.cpp \
         tads3/vmrun.cpp tads3/vmfunc.cpp tads3/vmmeta.cpp tads3/vmsa.cpp \
         tads3/vmbiftio.cpp tads3/vmbif.cpp tads3/vmbifl.cpp tads3/vmimage.cpp \
         tads3/vmimg_nd.cpp tads3/vmrunsym.cpp tads3/vmsrcf.cpp tads3/vmfile.cpp \
         tads3/vmbiftad.cpp tads3/vmsave.cpp tads3/vmbift3.cpp tads3/vmbt3_nd.cpp \
         tads3/vmregex.cpp tads3/vmhosttx.cpp tads3/vmhostsi.cpp tads3/vmhash.cpp \
         tads3/vmmcreg.cpp tads3/vmbifreg.cpp tads3/vmtmpfil.cpp \
         tads3/vmnetfillcl.cpp tads3/vmdynfunc.cpp tads3/vmstrbuf.cpp \
         tads3/vmpack.cpp tads3/vmfref.cpp tads3/vmlog.cpp tads3/vmisaac.cpp \
         tads3/md5.cpp tads3/sha2.cpp tads3/tcmain.cpp tads3/tcgen.cpp \
         tads3/tcsrc.cpp tads3/tctok.cpp tads3/tcglob.cpp tads3/tcprs.cpp \
         tads3/tcprsstm.cpp tads3/tcprs_rt.cpp tads3/tcprsnf.cpp tads3/tcprsnl.cpp \
         tads3/tct3.cpp tads3/tct3unas.cpp tads3/tct3stm.cpp tads3/tct3nl.cpp \
         tads3/tct3_d.cpp tads3/tcerr.cpp tads3/tcerrmsg.cpp tads3/vmconnom.cpp \
         vmglk.cpp

OBJ=$(SRC:.c=.o)
OBJTADS2=$(SRCTADS2:.c=.o)
OBJTADS3=$(SRCTADS3:.cpp=.o)
CXXOBJ=$(CXXSRC:.cpp=.o)

all: release
debug: BUILDTYPE=-g -DGARGLK_DEBUG
debug: release
release: tadsr

tadsr: $(OBJ) $(CXXOBJ) libtads2.a libtads3.a $(LIBGARGLK) $(LIBGARGLK_SO)
	$(CXX) -o $@ $^ $(EXTLIBS)

libtads2.a: $(OBJTADS2)
	$(AR) $(ARFLAGS) $@ $^

libtads3.a: $(OBJTADS3)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) -c -o $@ $(BUILDTYPE) $(CFLAGS) $<

%.o: %.cc
	$(CXX) -c -o $@ $(BUILDTYPE) $(CXXFLAGS) $<

clean:
	-rm tadsr libtads2.a libtads3.a $(OBJ) $(CXXOBJ) $(OBJTADS2) $(OBJTADS3) 2>/dev/null ; echo >/dev/null

.PHONY: clean all release debug
