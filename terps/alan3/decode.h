#ifndef _DECODE_H_
#define _DECODE_H_
/*----------------------------------------------------------------------*\

  decode.h

  Arithmetic decoding module in Arun

\*----------------------------------------------------------------------*/

/* IMPORTS */
#include "types.h"

/* TYPES */

/* DATA */
extern Aword *freq;     /* Cumulated character frequencies for text decoding */

/* FUNCTIONS */

extern void startDecoding(void);
extern int decodeChar(void);
extern void *pushDecode(void);
extern void popDecode(void *info);

#endif

