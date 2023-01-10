/*----------------------------------------------------------------------*\

	Syntax

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
static SyntaxEntry *findSyntaxEntry(int verbCode) {
    SyntaxEntry *stx;
    for (stx = stxs; !isEndOfArray(stx); stx++)
        if (stx->code == verbCode) {
            return stx;
            break;
        }
    return NULL;
}


/*======================================================================*/
SyntaxEntry *findSyntaxTreeForVerb(int verbCode) {
    SyntaxEntry *foundStx = NULL;
    if (isPreBeta2(header->version)) {
        foundStx = findSyntaxEntryForPreBeta2(verbCode, foundStx);
    } else {
        foundStx = findSyntaxEntry(verbCode);
    }
    if (foundStx == NULL)
        /* No matching syntax */
        error(M_WHAT);
    return foundStx;
}


/*======================================================================*/
char *parameterNameInSyntax(int syntaxNumber, int parameterNumber) {
    Aaddr adr = addressAfterTable(header->parameterMapAddress, sizeof(ParameterMapEntry));
    Aaddr *syntaxParameterNameTable = pointerTo(memory[adr]);
    Aaddr *parameterNameTable = (Aaddr *)pointerTo(syntaxParameterNameTable[syntaxNumber-1]);
    return stringAt(parameterNameTable[parameterNumber-1]);
}



