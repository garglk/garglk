#ifndef _REVERSE_H_
#define _REVERSE_H_
/*----------------------------------------------------------------------*\

  reverse.h

  Header file for reverse-module in Alan Interpreter

\*----------------------------------------------------------------------*/

/* Import: */
#include "types.h"


/* Types: */


/* Functions: */

extern void reverseHdr(ACodeHeader *hdr);
extern void reverseACD(void);
extern void reverse(Aword *word);
extern Aword reversed(Aword word);

#endif
