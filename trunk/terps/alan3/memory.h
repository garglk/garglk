#ifndef MEMORY_H_
#define MEMORY_H_
/*----------------------------------------------------------------------*\

    The Amachine memory

\*----------------------------------------------------------------------*/

/* IMPORTS */
#include "sysdep.h"
#include "acode.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/* CONSTANTS */


/* TYPES */


/* DATA */
extern Aword *memory;
extern ACodeHeader *header;
extern int memTop;


/* FUNCTIONS */
#ifndef SMARTALLOC
extern void *allocate(unsigned long len);
#else
#define allocate(s) calloc(s, 1)
#endif
extern void *duplicate(void *original, unsigned long len);

#endif /* MEMORY_H_ */
