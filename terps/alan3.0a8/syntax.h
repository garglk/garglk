#ifndef SYNTAX_H_
#define SYNTAX_H_
/*----------------------------------------------------------------------*\

	syntax

\*----------------------------------------------------------------------*/

/* IMPORTS */
#include "types.h"
#include "memory.h"


/* CONSTANTS */


/* TYPES */


/* DATA */
extern SyntaxEntry *stxs;   /* Syntax table pointer */


/* FUNCTIONS */
extern ElementEntry *elementTreeOf(SyntaxEntry *stx);


#endif /* SYNTAX_H_ */
