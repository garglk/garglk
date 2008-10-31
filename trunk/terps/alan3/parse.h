#ifndef _PARSE_H_
#define _PARSE_H_
/*----------------------------------------------------------------------*\

  PARSE.H

  Parse data for ALAN interpreter module.

\*----------------------------------------------------------------------*/

/* Types: */

typedef struct WordEntry {
  int code;			/* The dictionary index for that word */
  char *start;			/* Where does it start */
  char *end;			/* .. and end */
} WordEntry;



/* Data: */

extern WordEntry playerWords[];	/* List of Parsed Word */
extern int wordIndex;		/* and an index into it */
extern int firstWord;
extern int lastWord;

extern ParamEntry *parameters;	/* List of parameters */
extern Bool plural;

extern LiteralEntry literal[];
extern int litCount;

extern int verbWord;


/* Functions: */

/* Parse a new player command */
extern void forceNewPlayerInput();
extern void parse(void);
extern void initParse(void);
extern int literalFromInstance(Aint instance);
extern Aint instanceFromLiteral(int literalIndex);
extern void setupParameterForInstance(int parameter, Aint instance);
extern void setupParameterForInteger(int parameter, Aint value);
extern void setupParameterForString(int parameter, char *value);
extern void restoreParameters();

#endif
