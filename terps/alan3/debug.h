#ifndef _DEBUG_H_
#define _DEBUG_H_
/*----------------------------------------------------------------------*\

  debug.h

  Header file for debug handler in Alan interpreter

\*----------------------------------------------------------------------*/

/* Imports: */
#include "types.h"


/* Types: */

typedef struct Breakpoint {
  int line;
  int file;
} Breakpoint;


/* Data: */
extern int breakpointCount;
extern Breakpoint breakpoint[];

/* Functions: */
extern void saveInfo(void);
extern void restoreInfo(void);
extern int breakpointIndex(int file, int line);
extern char *sourceFileName(int file);
extern char *readSourceLine(int file, int line);
extern void showSourceLine(int fileNumber, int line);
extern void debug(bool calledFromBreakpoint, int line, int fileNumber);
extern void traceSay(int item);

#endif
