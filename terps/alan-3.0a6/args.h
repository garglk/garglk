/*----------------------------------------------------------------------*\

  args.h

  Argument handling 

\*----------------------------------------------------------------------*/
#include "types.h"

#ifndef PROGNAME
#define PROGNAME "arun"
#endif

extern char *gameName(char fullPathName[]);
extern void args(int argc, char *argv[]);
