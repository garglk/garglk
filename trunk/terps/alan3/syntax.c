/*----------------------------------------------------------------------*\

	syntax

\*----------------------------------------------------------------------*/
#include "syntax.h"

/* IMPORTS */
#include "word.h"
#include "msg.h"
#include "lists.h"
#include "compatibility.h"


/* CONSTANTS */


/* PUBLIC DATA */
SyntaxEntry *stxs;      /* Syntax table pointer */


/* PRIVATE TYPES & DATA */


/*+++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*======================================================================*/
ElementEntry *elementTreeOf(SyntaxEntry *stx) {
    return (ElementEntry *) pointerTo(stx->elms);
}


/*----------------------------------------------------------------------*/
static SyntaxEntry *findSyntaxEntryForPreBeta2(int verbCode, SyntaxEntry *foundStx) {
    SyntaxEntryPreBeta2 *stx;
    for (stx = (SyntaxEntryPreBeta2*)stxs; !isEndOfArray(stx); stx++)
        if (stx->code == verbCode) {
            foundStx = (SyntaxEntry *)stx;
            break;
        }
    return(foundStx);
}


/*----------------------------------------------------------------------*/
static SyntaxEntry *findSyntaxEntry(int verbCode, SyntaxEntry *foundStx) {
    SyntaxEntry *stx;
    for (stx = stxs; !isEndOfArray(stx); stx++)
        if (stx->code == verbCode) {
            foundStx = stx;
            break;
        }
    return(foundStx);
}


/*======================================================================*/
SyntaxEntry *findSyntaxTreeForVerb(int verbCode) {
    SyntaxEntry *foundStx = NULL;
    if (isPreBeta2(header->version)) {
	foundStx = findSyntaxEntryForPreBeta2(verbCode, foundStx);
    } else {
	foundStx = findSyntaxEntry(verbCode, foundStx);
    }
    if (foundStx == NULL)
        /* No matching syntax */
        error(M_WHAT);
    return foundStx;
}


/*======================================================================*/
char *parameterNameInSyntax(int parameterNumber) {
    SyntaxEntry *syntax = findSyntaxTreeForVerb(verbWordCode);
    Aaddr *parameterNameTable = (Aaddr *)pointerTo(syntax->parameterNameTable);
    if (isPreBeta2(header->version) || syntax->parameterNameTable != 0) {
        return stringAt(parameterNameTable[parameterNumber-1]);
	} else
        return NULL;
}
