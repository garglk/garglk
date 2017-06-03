TERP=geas
DEFMACRO=-DUSE_INLINE
CFLAGS=$(INCL) $(DEFMACRO)
CXXFLAGS=-std=c++11 $(INCL) $(DEFMACRO)
LINK=$(CXX)
CXXSRC=geasfile.cc geas-runner.cc geas-state.cc geas-util.cc geasglk.cc istring.cc readfile.cc
SRC=geasglkterm.c
include ../terp.mk
