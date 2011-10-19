#ifndef UTILS_H_
#define UTILS_H_
/*----------------------------------------------------------------------*\

    utils.h

    Utility functions for the Alan interpreter

\*----------------------------------------------------------------------*/

/* IMPORTS: */
#include "types.h"


/* TYPES: */


/* FUNCTIONS: */
extern void terminate(int code);
extern void usage(char *programName);
extern void printVersion(int buildNumber);

#endif /* UTILS_H_ */
