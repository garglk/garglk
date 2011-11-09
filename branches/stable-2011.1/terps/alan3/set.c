/*----------------------------------------------------------------------*\

	Abstract datatype Set for Alan interpreter

	A set is implemented as a struct holding a size and a
	dynamically allocated array of members. Members can be
	integers or instance numbers. Attributes of Set type is
	allocated and the pointer to it is used as the attribute
	value. As members are only references, clearing a set can
	simply be done by setting the size to zero.

\*----------------------------------------------------------------------*/
#include "set.h"

/* Imports: */
#include "lists.h"
#include "syserr.h"
#include "memory.h"
#include "instance.h"

#define EXTENT 5


/*======================================================================*/
Set *newSet(int allocation) {
  Set *theSet = NEW(Set);

  if (allocation) {
    theSet->members = allocate(allocation*sizeof(theSet->members[0]));
    theSet->size = 0;
    theSet->allocated = allocation;
  }
  return theSet;
}


/*======================================================================*/
void initSets(SetInitEntry *initTable)
{
  SetInitEntry *init;
  int i;

  for (init = initTable; !isEndOfArray(init); init++) {
    Set *set = newSet(init->size);
    Aword *member = pointerTo(init->setAddress);
    for (i = 0; i < init->size; i++, member++)
      addToSet(set, *member);
    setInstanceAttribute(init->instanceCode, init->attributeCode, (Aptr)set);
  }
}


/*======================================================================*/
int setSize(Set *theSet) {
  return theSet->size;
}


/*======================================================================*/
void clearSet(Set *theSet) {
  theSet->size = 0;
}


/*======================================================================*/
Set *copySet(Set *theSet) {
  Set *new = newSet(theSet->size);
  int i;

  for (i = 1; i <= theSet->size; i++)
    addToSet(new, getSetMember(theSet, i));
  return new;
}


/*======================================================================*/
Aword getSetMember(Set *theSet, Aint theMember) {
  if (theMember > theSet->size || theMember < 1)
    apperr("Accessing nonexisting member in a set");
  return theSet->members[theMember-1];
}


/*======================================================================*/
bool inSet(Set *theSet, Aword member)
{
  int i;

  for (i = 1; i <= theSet->size; i++)
    if (getSetMember(theSet, i) == member)
      return TRUE;
  return FALSE;
}


/*=======================================================================*/
Set *setUnion(Set *set1, Set *set2)
{
  Set *theUnion = newSet(set1->size+set2->size);
  int i;

  for (i = 0; i < set1->size; i++)
    addToSet(theUnion, set1->members[i]);
  for (i = 0; i < set2->size; i++)
    addToSet(theUnion, set2->members[i]);
  return theUnion;
}


/*=======================================================================*/
void addToSet(Set *theSet, Aword newMember)
{
  if (inSet(theSet, newMember)) return;
  if (theSet->size == theSet->allocated) {
    theSet->allocated += EXTENT;
    theSet->members = realloc(theSet->members, theSet->allocated*sizeof(theSet->members[0]));
  }
  theSet->members[theSet->size] = newMember;
  theSet->size++;
}


/*=======================================================================*/
void removeFromSet(Set *theSet, Aword member)
{
  int i, j;

  if (!inSet(theSet, member)) return;

  for (i = 0; i < theSet->size; i++) {
    if ((Aword)theSet->members[i] == member) {
      for (j = i; j < theSet->size-1; j++)
	theSet->members[j] = theSet->members[j+1];
      theSet->size--;
      break;
    }
  }
}


/*=======================================================================*/
bool equalSets(Set *set1, Set *set2)
{
  int i;

  if (set1->size != set2->size) return FALSE;

  for (i = 0; i < set1->size; i++) {
    if (!inSet(set2, set1->members[i]))
      return FALSE;
  }
  return TRUE;
}


/*======================================================================*/
void freeSet(Set *theSet) {
  if (theSet->members != NULL)
    free(theSet->members);
  free(theSet);
}
