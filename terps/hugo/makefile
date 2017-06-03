TERP=hugo
INCL=-I$(TOP)/terps/hugo/source -I$(TOP)/terps/hugo/heglk
DEFMACRO=-DGLK -DCOMPILE_V25 -DNO_KEYPRESS_CURSOR -DHUGO_INLINE="static inline"
CFLAGS=$(INCL) $(DEFMACRO)
SRC=heglk/heglk.c heglk/heglkunix.c \
    source/he.c source/heexpr.c source/hemisc.c source/heobject.c source/heparse.c \
    source/heres.c source/herun.c source/heset.c source/stringfn.c
include ../terp.mk
