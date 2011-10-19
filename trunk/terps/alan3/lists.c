#include "lists.h"


/* How to know we are at end of a table or array, first Aword == EOF */
void implementationOfSetEndOfArray(Aword *adr)
{
  *adr = EOF;
}


bool implementationOfIsEndOfList(Aword *adr)
{
  return *adr == EOF;
}


