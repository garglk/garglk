/*----------------------------------------------------------------------*\

  params.c

  Various utility functions for handling parameters:

  compact()	Compact a list, i.e remove any NULL elements
  lstlen()	Count number of elements
  inlst()	Check if an element is in the list
  sublst()	Subract two lists
  lstcpy()	Copy one list onto another
  mrglst()	Merge the paramElems of one list into the first
  isect()	Take the intersection of two lists
  cpyrefs()	Copy the refs (in dictionary) to a paramList

\*----------------------------------------------------------------------*/

#include "types.h"

#include "params.h"


#ifdef _PROTOTYPES_
void compact(ParamElem a[])
#else
void compact(a)
     ParamElem a[];
#endif
{
  int i, j;
  
  for (i = 0, j = 0; a[j].code != EOF; j++)
    if (a[j].code != 0)
      a[i++] = a[j];
  a[i].code = EOF;
}


#ifdef _PROTOTYPES_
int lstlen(ParamElem a[])
#else
int lstlen(a)
     ParamElem a[];
#endif
{
  int i = 0;

  while (a[i].code != EOF)
    i++;
  return (i);
}


#ifdef _PROTOTYPES_
Boolean inlst(ParamElem l[], Aword e)
#else
Boolean inlst(l, e)
     ParamElem l[];
     Aword e;
#endif
{
  int i;

  for (i = 0; l[i].code != EOF && l[i].code != e; i++);
  return (l[i].code == e);
}


#ifdef _PROTOTYPES_
void lstcpy(ParamElem a[], ParamElem b[])
#else
void lstcpy(a, b)
     ParamElem a[], b[];
#endif
{
  int i;

  for (i = 0; b[i].code != EOF; i++)
    a[i] = b[i];
  a[i].code = EOF;
}


#ifdef _PROTOTYPES_
void sublst(ParamElem a[], ParamElem b[])
#else
void sublst(a, b)
     ParamElem a[], b[];
#endif
{
  int i;

  for (i = 0; a[i].code != EOF; i++)
    if (inlst(b, a[i].code))
      a[i].code = 0;		/* Mark empty */
  compact(a);
}


#ifdef _PROTOTYPES_
void mrglst(ParamElem a[], ParamElem b[])
#else
void mrglst(a, b)
     ParamElem a[], b[];
#endif
{
  int i,last;

  for (last = 0; a[last].code != EOF; last++); /* Find end of list */
  for (i = 0; b[i].code != EOF; i++)
    if (!inlst(a, b[i].code)) {
      a[last++] = b[i];
      a[last].code = EOF;
    }
}


#ifdef _PROTOTYPES_
void isect(ParamElem a[], ParamElem b[])
#else
void isect(a, b)
     ParamElem a[], b[];
#endif
{
  int i, last = 0;

  for (i = 0; a[i].code != EOF; i++)
    if (inlst(b, a[i].code))
      a[last++] = a[i];
  a[last].code = EOF;
}


#ifdef _PROTOTYPES_
void cpyrefs(ParamElem p[], Aword r[])
#else
void cpyrefs(p, r)
     ParamElem p[];
     Aword r[];
#endif
{
  int i;

  for (i = 0; r[i] != EOF; i++) {
    p[i].code = r[i];
    p[i].firstWord = EOF;
  }
  p[i].code = EOF;
}


