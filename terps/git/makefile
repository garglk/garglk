TERP=git
DEFMACRO=-DUSE_INLINE -DUSE_OWN_POWF
CFLAGS=$(INCL) $(DEFMACRO)
SRC=git.c memory.c compiler.c opcodes.c operands.c peephole.c terp.c glkop.c search.c \
    git_unix.c savefile.c saveundo.c gestalt.c heap.c accel.c
include ../terp.mk
