#ifndef LITERAL_H_
#define LITERAL_H_
/*----------------------------------------------------------------------*\

	literal

\*----------------------------------------------------------------------*/

/* IMPORTS */
#include "acode.h"


/* CONSTANTS */


/* TYPES */
typedef enum LiteralType {
  NO_LITERAL, NUMERIC_LITERAL, STRING_LITERAL
} LiteralType;

typedef struct LiteralEntry {   /* LITERAL */
  Aint class;           /* Class id of the literal type */
  LiteralType type;
  Aptr value;
} LiteralEntry;


/* DATA */
extern int litCount;
extern LiteralEntry *literals;


/* FUNCTIONS */
extern void createIntegerLiteral(int integerValue);
extern void createStringLiteral(char *unquotedString);
extern void freeLiterals(void);
extern int literalFromInstance(int instance);
extern int instanceFromLiteral(int literal);



#endif /* LITERAL_H_ */
