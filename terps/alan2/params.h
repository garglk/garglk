/*----------------------------------------------------------------------*\

  params.h

  Various utility functions for handling parameters

\*----------------------------------------------------------------------*/
#ifndef PARAMS_H
#include "types.h"


#ifdef _PROTOTYPES_
extern void compact(ParamElem *a);
extern int lstlen(ParamElem *a);
extern Boolean inlst(ParamElem *l, Aword e);
extern void lstcpy(ParamElem *a, ParamElem *b);
extern void sublst(ParamElem *a, ParamElem *b);
extern void mrglst(ParamElem *a, ParamElem *b);
extern void isect(ParamElem *a, ParamElem *b);
extern void cpyrefs(ParamElem *p, Aword *r);
#else
extern void compact();
extern int lstlen();
extern Boolean inlst();
extern void lstcpy();
extern void sublst();
extern void mrglst();
extern void isect();
extern void cpyrefs();
#endif

#endif
