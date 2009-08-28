#ifndef _REVERSE_H_
#define _REVERSE_H_
/*----------------------------------------------------------------------*\

  reverse.h

  Header file for reverse-module

\*----------------------------------------------------------------------*/

/* TYPES */


#ifdef _PROTOTYPES_

extern void reverseHdr(AcdHdr *hdr);
extern void reverseACD(Boolean v25);
extern void reverse(Aword *word);
extern Aword reversed(Aword word);

#else
extern void reverseHdr();
extern void reverseACD();
extern void reverse();
extern Aword reversed();
#endif

#endif
