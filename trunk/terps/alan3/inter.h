#ifndef _INTER_H_
#define _INTER_H_
/*----------------------------------------------------------------------*\

  inter.h

  The interpreter of Acode.

\*----------------------------------------------------------------------*/

#include "types.h"
#include "stack.h"

/* TYPES: */


/* DATA: */

extern Bool stopAtNextLine;
extern int currentLine;
extern int recursionDepth;

/* Global failure flag */
extern Bool fail;


/* FUNCTIONS: */

extern void setInterpreterStack(Stack stack);
extern void interpret(Aaddr adr);
extern Aword evaluate(Aaddr adr);

#endif
