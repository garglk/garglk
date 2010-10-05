/*----------------------------------------------------------------------*\

  args.h

  Argument handling

\*----------------------------------------------------------------------*/
#include "types.h"

#ifndef PROGNAME
#ifdef HAVE_GARGLK
#define PROGNAME "alan3"
#else
#define PROGNAME "arun"
#endif
#endif

/* DATA */
extern char *adventureName; /* The name of the game */
extern char *adventureFileName;

/* FUNCTIONS */
extern char *gameName(char fullPathName[]);
extern void args(int argc, char *argv[]);
