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
int litCount = 0;
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
static char *copyAndQuoteDollarSigns(char *unquotedString) {
    char *quotedString = (char *)allocate(strlen(unquotedString)*2+1);
    char *source = unquotedString;
    char *destination = quotedString;
    while (*source != '\0') {
        if (*source == '$') {
            *destination++ = '$';
            *destination = '_';
        } else
            *destination = *source;
        source++;
        destination++;
    }
    return quotedString;
}


/*----------------------------------------------------------------------*/
void createStringLiteral(char *unquotedString) {
    litCount++;
    literals[litCount].class = header->stringClassId;
    literals[litCount].type = STRING_LITERAL;
    literals[litCount].value = toAptr(copyAndQuoteDollarSigns(unquotedString));
}

/*----------------------------------------------------------------------*/
void freeLiterals() {
    int i;

    for (i = 0; i <= litCount; i++)
        if (literals[i].type == STRING_LITERAL && literals[i].value != 0) {
            deallocate((void *)fromAptr(literals[i].value));
        }
    litCount = 0;}



/*======================================================================*/
int literalFromInstance(int instance) {
    return instance - header->instanceMax;
}
