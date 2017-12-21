#ifndef COMPATIBILITY_H_
#define COMPATIBILITY_H_
/*----------------------------------------------------------------------*\

    compatibility.h

\*----------------------------------------------------------------------*/

/* IMPORTS: */
#include "types.h"


/* TYPES: */


/* FUNCTIONS: */
extern bool isPreAlpha5(char version[4]);
extern bool isPreBeta2(char version[4]);
extern bool isPreBeta3(char version[4]);
extern bool isPreBeta4(char version[4]);
extern bool isPreBeta5(char version[4]);
extern char *decodedGameVersion(char version[]);

#endif
