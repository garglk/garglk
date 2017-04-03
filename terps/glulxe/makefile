TERP=glulxe
DEFMACRO=-DFLOAT_COMPILE_SAFER_POWF -DOS_UNIX
CFLAGS=$(INCL) $(DEFMACRO)
SRC=main.c files.c vm.c exec.c funcs.c operand.c string.c glkop.c heap.c serial.c search.c \
    gestalt.c osdepend.c unixstrt.c accel.c profile.c float.c
include ../terp.mk
