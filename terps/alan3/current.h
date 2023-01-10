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
    int syntax,
        verb,
        location,
        actor,
        instance,
        tick,
        score,
        visits,
        sourceLine,
        sourceFile;
    bool meta;
} CurVars;


/* DATA */
extern CurVars current;
extern bool gameStateChanged;


/* FUNCTIONS */

#endif /* CURRENT_H_ */
