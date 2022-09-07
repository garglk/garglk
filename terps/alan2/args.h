/*----------------------------------------------------------------------*\

  args.h

  Argument handling 

\*----------------------------------------------------------------------*/

#ifdef __amiga__

#include <libraries/dosextens.h>

#ifdef AZTEC_C
extern struct FileHandle *con;
#else

#include <intuition/intuition.h>
#include <workbench/startup.h>

extern struct WBStartup *_WBenchMsg; /* From libnix */

extern BPTR window;
extern BPTR cd;

#endif
#endif

#ifndef PROGNAME
#define PROGNAME "alan2"
#endif

// _PROTOTYPES_ (which is a reserved identifier, but oh well) is defined in
// sysdep.h, so theoretically that file should be included here; but that
// causes, eventually, a conflicting definition of printf, as Alan defines
// printf as a macro to a function that's _not_ compatible with the actual
// printf(). Instead of fixing that, just define _PROTOTYPES_ here.
#define _PROTOTYPES_

#ifdef _PROTOTYPES_
extern void args(int argc, char *argv[]);
#else
extern void args();
#endif

