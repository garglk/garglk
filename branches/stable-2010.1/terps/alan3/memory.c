/*----------------------------------------------------------------------*\

    The Amachine memory

\*----------------------------------------------------------------------*/
#include "memory.h"

/* Imports */
#include "types.h"
#include "syserr.h"


/* PUBLIC DATA */

Aword *memory = NULL;
static ACodeHeader dummyHeader; /* Dummy to use until memory allocated */
ACodeHeader *header = &dummyHeader;
int memTop = 0;         /* Top of load memory */


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


#ifndef SMARTALLOC
/*======================================================================*/
void *allocate(unsigned long lengthInBytes)
{
  void *p = (void *)calloc((size_t)lengthInBytes, 1);

  if (p == NULL)
    syserr("Out of memory.");

  return p;
}
#endif

/*======================================================================*/
void *duplicate(void *original, unsigned long len)
{
  void *p = allocate(len+1);

  memcpy(p, original, len);
  return p;
}
