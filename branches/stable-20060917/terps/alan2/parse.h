/*----------------------------------------------------------------------*\

  PARSE.H

  Parse data for ALAN interpreter module.

\*----------------------------------------------------------------------*/

extern int wrds[];		/* List of Parsed Word */
extern int wrdidx;		/* and an index into it */

extern ParamElem *params;	/* List of parameters */
extern Boolean plural;

extern LitElem litValues[];
extern int litCount;

extern int vrbwrd;

#ifdef _PROTOTYPES_

/* Parse a new player command */
extern void parse(void);

#else
extern void parse();
#endif
