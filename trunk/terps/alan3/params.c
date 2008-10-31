/*----------------------------------------------------------------------*\

  params.c

  Various utility functions for handling parameters:

  compress()		Compact a list, i.e remove any NULL elements
  listlength()		Count number of elements
  inList()		Check if an element is in the list
  subtractList()	Subract two lists
  listCopy()		Copy one list onto another
  mergeLists()		Merge the paramElems of one list into the first
  intersect()		Take the intersection of two lists
  copyReferences()	Copy the refs (in dictionary) to a paramList

\*----------------------------------------------------------------------*/

#include "types.h"

#include "params.h"
#include "main.h"


/*======================================================================*/
void compress(ParamEntry theList[])
{
  int i, j;
  
  for (i = 0, j = 0; theList[j].instance != EOF; j++)
    if (theList[j].instance != 0)
      theList[i++] = theList[j];
  theList[i].instance = EOF;
}


/*======================================================================*/
int listLength(ParamEntry theList[])
{
  int i = 0;

  while (theList[i].instance != EOF)
    i++;
  return (i);
}


/*======================================================================*/
Bool inList(ParamEntry theList[], Aword theCode)
{
  int i;

  for (i = 0; theList[i].instance != EOF && theList[i].instance != theCode; i++);
  return (theList[i].instance == theCode);
}


/*======================================================================*/
void copyParameterList(ParamEntry to[], ParamEntry from[])
{
  int i;

  for (i = 0; from[i].instance != EOF; i++)
    to[i] = from[i];
  to[i].instance = EOF;
}


/*======================================================================*/
void subtractListFromList(ParamEntry theList[], ParamEntry remove[])
{
  int i;

  for (i = 0; theList[i].instance != EOF; i++)
    if (inList(remove, theList[i].instance))
      theList[i].instance = 0;		/* Mark empty */
  compress(theList);
}


/*======================================================================*/
void mergeLists(ParamEntry one[], ParamEntry other[])
{
  int i,last;

  for (last = 0; one[last].instance != EOF; last++); /* Find end of list */
  for (i = 0; other[i].instance != EOF; i++)
    if (!inList(one, other[i].instance)) {
      one[last++] = other[i];
      one[last].instance = EOF;
    }
}


/*======================================================================*/
void intersect(ParamEntry one[], ParamEntry other[])
{
  int i, last = 0;

  for (i = 0; one[i].instance != EOF; i++)
    if (inList(other, one[i].instance))
      one[last++] = one[i];
  one[last].instance = EOF;
}


/*======================================================================*/
void copyReferences(ParamEntry parameterList[], Aword references[])
{
  int i;

  for (i = 0; references[i] != EOF; i++) {
    parameterList[i].instance = references[i];
    parameterList[i].firstWord = EOF;
  }
  parameterList[i].instance = EOF;
}
