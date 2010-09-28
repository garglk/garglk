/*----------------------------------------------------------------------*\

  args.h

  Argument handling

\*----------------------------------------------------------------------*/
#include "types.h"

#ifndef PROGNAME
#define PROGNAME "arun"
#endif

/* DATA */
extern char *adventureName; /* The name of the game */
extern char *adventureFileName;

/* FUNCTIONS */
extern char *gameName(char fullPathName[]);
extern void args(int argc, char *argv[]);
