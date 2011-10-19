#ifndef CURRENT_H_
#define CURRENT_H_
/*----------------------------------------------------------------------*\

	current

\*----------------------------------------------------------------------*/

/* IMPORTS */
#include "types.h"

/* CONSTANTS */


/* TYPES */
typedef struct CurVars {
  int
    verb,
    location,
    actor,
    instance,
    tick,
    score,
    visits,
    sourceLine,
    sourceFile;
} CurVars;


/* DATA */
extern CurVars current;
extern bool gameStateChanged;


/* FUNCTIONS */

#endif /* CURRENT_H_ */
