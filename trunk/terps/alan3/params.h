/*----------------------------------------------------------------------*\

  params.h

  Various utility functions for handling parameters

\*----------------------------------------------------------------------*/
#ifndef PARAMS_H
#include "types.h"

extern void compact(ParamEntry *a);
extern int listLength(ParamEntry *a);
extern Bool inList(ParamEntry *l, Aword e);
extern void copyParameterList(ParamEntry *to, ParamEntry *from);
extern void subtractListFromList(ParamEntry *a, ParamEntry *b);
extern void mergeLists(ParamEntry *a, ParamEntry *b);
extern void intersect(ParamEntry *a, ParamEntry *b);
extern void copyReferences(ParamEntry *p, Aword *r);
extern void allocateParameters(ParamEntry **areaPointer);
#endif
