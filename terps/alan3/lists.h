/*----------------------------------------------------------------------

  lists.h

  Various utility functions for handling lists and arrays

  ----------------------------------------------------------------------*/
#ifndef LISTS_H

#include "acode.h"
#include "types.h"

extern void initArray(void *array);

#define isEndOfArray(x) implementationOfIsEndOfList((Aword *) (x))
extern bool implementationOfIsEndOfList(Aword *adr);

#define setEndOfArray(x) implementationOfSetEndOfArray((Aword *) (x))
extern void implementationOfSetEndOfArray(Aword *adr);

#define lengthOfArray(array) lengthOfArrayImplementation((array), sizeof(*(array)))
extern int lengthOfArrayImplementation(void *array, int element_size_in_bytes);

#define addElement(array, element) addElementImplementation((array), (&element), sizeof(element))
extern void addElementImplementation(void *array_of_any_type, void *element_of_any_size, int element_size_in_bytes);
#endif
