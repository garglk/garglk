# Basic makefile for the Hugo Engine

# Name of executable:
HE = he
# Compiler:
CC = gcc $(DEFINES)
DEFINES = -DGCC_UNIX

OBJECTS = he.o heblank.o heexpr.o hemisc.o heobject.o heparse.o heres.o herun.o heset.o stringfn.o

all : he

he : $(OBJECTS)

$(OBJECTS) : heheader.h
