/*----------------------------------------------------------------------

  lists.h

  Various utility functions for handling lists and arrays

  ----------------------------------------------------------------------*/
#ifndef LISTS_H

#include "acode.h"
#include "types.h"

#define isEndOfArray(x) implementationOfIsEndOfList((Aword *) (x))
extern bool implementationOfIsEndOfList(Aword *adr);
#define setEndOfArray(x) implementationOfSetEndOfArray((Aword *) (x))
extern void implementationOfSetEndOfArray(Aword *adr);

#endif
