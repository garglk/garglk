/*  agilcomp.c-- Stubs for routines defined in agil but   */
/*    refered to elsewhere.                               */
/* Copyright (C) 1996-1999,2001  Robert Masenten          */
/* This program may be redistributed under the terms of the
   GNU General Public License, version 2; see agility.h for details. */

/* This is part of the source for AGiliTy: the (Mostly)   */
/*    Universal AGT Interpreter */

#include "agility.h"

void debugout(const char *s)
{
  fputs(s,stdout);
}

void writestr(const char *s)
{
  fputs(s,stdout);
}

void writeln(const char *s)
{
  printf("%s\n",s);
}

void agil_option(int optnum,char *optstr[], rbool setflag, rbool lastpass)
{
  /* Do nothing; this is just a place holder */
}

void close_interface(void)
{
}


