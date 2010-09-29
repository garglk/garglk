/*----------------------------------------------------------------------*\

	syntax

\*----------------------------------------------------------------------*/
#include "syntax.h"

/* IMPORTS */


/* CONSTANTS */


/* PUBLIC DATA */
SyntaxEntry *stxs;      /* Syntax table pointer */


/* PRIVATE TYPES & DATA */


/*+++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*======================================================================*/
ElementEntry *elementTreeOf(SyntaxEntry *stx) {
    return (ElementEntry *) pointerTo(stx->elms);
}


