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

#ifdef _PROTOTYPES_
extern void args(int argc, char *argv[]);
#else
extern void args();
#endif

