/*----------------------------------------------------------------------*\

	literal

\*----------------------------------------------------------------------*/
#include "literal.h"

/* IMPORTS */
#include <strings.h>
#include "types.h"
#include "memory.h"


/* CONSTANTS */


/* PUBLIC DATA */
int litCount;
static LiteralEntry literalTable[100];
LiteralEntry *literals = literalTable;


/* PRIVATE TYPES & DATA */


/*+++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*======================================================================*/
int instanceFromLiteral(int literalIndex) {
    return literalIndex + header->instanceMax;
}

/*----------------------------------------------------------------------*/
void createIntegerLiteral(int integerValue) {
    litCount++;
    literals[litCount].class = header->integerClassId;
    literals[litCount].type = NUMERIC_LITERAL;
    literals[litCount].value = integerValue;
}

/*----------------------------------------------------------------------*/
void createStringLiteral(char *unquotedString) {
    litCount++;
    literals[litCount].class = header->stringClassId;
    literals[litCount].type = STRING_LITERAL;
    literals[litCount].value = (Aptr) strdup(unquotedString);
}

/*----------------------------------------------------------------------*/
void freeLiterals() {
    int i;

    for (i = 0; i < litCount; i++)
        if (literals[i].type == STRING_LITERAL && literals[i].value != 0)
            free((char*) literals[i].value);
    litCount = 0;
}


/*======================================================================*/
int literalFromInstance(int instance) {
    return instance - header->instanceMax;
}
