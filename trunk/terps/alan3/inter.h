#ifndef _INTER_H_
#define _INTER_H_
/*----------------------------------------------------------------------*\

  inter.h

  The interpreter of Acode.

\*----------------------------------------------------------------------*/

/* Types: */

/* Data: */

extern Bool stopAtNextLine;
extern int currentLine;
extern int depth;


/* Functions: */

#ifdef _PROTOTYPES_

extern void interpret(Aaddr adr);

#else
extern void interpret();
#endif

#endif
