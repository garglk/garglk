#ifndef _DECODE_H_
#define _DECODE_H_
/*----------------------------------------------------------------------*\

  decode.h

  Arithmetic decoding module in Arun

\*----------------------------------------------------------------------*/

#include "types.h"

/* Types: */

/* Data: */

/* Functions: */

#ifdef _PROTOTYPES_
extern void startDecoding(void);
extern int decodeChar(void);
extern void *pushDecode(void);
extern void popDecode(void *info);
#else
extern void startDecoding();
extern int decodeChar();
extern void *pushDecode();
extern void popDecode();
#endif
#endif

