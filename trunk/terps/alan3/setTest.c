/*======================================================================*\

  setTest.c

  Unit tests for set type module in the Alan interpreter

\*======================================================================*/

#include "set.c"


static void testAddToSet() {
  Set aSet = {0, 0, NULL};
  int i;

  /* Add a single value */
  addToSet(&aSet, 0);
  ASSERT(aSet.size == 1);
  ASSERT(aSet.members[0] == 0);
  /* Add it again, should not extend the set */
  addToSet(&aSet, 0);
  ASSERT(aSet.size == 1);

  /* Add a number of elements so that we need to extend */
  for (i = 1; i<6; i++) {
    addToSet(&aSet, i);
    ASSERT(aSet.size == i+1);
    ASSERT(aSet.members[i] == i);
  }
  ASSERT(aSet.size == 6);
}  

static void testSetUnion() {
  Set *set0 = newSet(0);
  Set *set678 = newSet(3);
  Set *set456 = newSet(3);
  Set *theUnion;

  /* Test adding an empty set */
  theUnion = setUnion(set0, set0);
  ASSERT(setSize(theUnion)==0);

  addToSet(set678, 6);
  addToSet(set678, 7);
  addToSet(set678, 8);
  theUnion = setUnion(set0, set678);
  ASSERT(setSize(theUnion)==3);
  ASSERT(inSet(theUnion, 6));
  ASSERT(inSet(theUnion, 7));
  ASSERT(inSet(theUnion, 8));

  addToSet(set456, 4);
  addToSet(set456, 5);
  addToSet(set456, 6);
  theUnion = setUnion(set456, set678);
  ASSERT(setSize(theUnion)==5);
  ASSERT(inSet(theUnion, 4));
  ASSERT(inSet(theUnion, 5));
  ASSERT(inSet(theUnion, 6));
  ASSERT(inSet(theUnion, 7));
  ASSERT(inSet(theUnion, 8));
}

static void testSetRemove() {
  Set *aSet = newSet(0);
  int i;

  /* Add a number of elements to remove */
  for (i = 1; i<6; i++)
    addToSet(aSet, i);

  ASSERT(aSet->size == 5);

  removeFromSet(aSet, 1);
  ASSERT(aSet->size == 4);

  /* Do it again, should not change... */
  removeFromSet(aSet, 1);
  ASSERT(aSet->size == 4);

  removeFromSet(aSet, 5);
  ASSERT(aSet->size == 3);
  removeFromSet(aSet, 4);
  ASSERT(aSet->size == 2);
  removeFromSet(aSet, 3);
  ASSERT(aSet->size == 1);
  removeFromSet(aSet, 2);
  ASSERT(aSet->size == 0);

  removeFromSet(aSet, 1);
  ASSERT(aSet->size == 0);
}  

static void testInSet() {
  Set *aSet = newSet(0);
  int i;

  ASSERT(!inSet(aSet, 0));
  for (i = 6; i>0; i--)
    addToSet(aSet, i);
  for (i = 1; i<7; i++)
    ASSERT(inSet(aSet, i));
}

static void testClearSet() {
  Set *aSet = newSet(0);
  int i;

  for (i = 6; i>0; i--)
    addToSet(aSet, i);
  clearSet(aSet);
  ASSERT(setSize(aSet) == 0);
}


static void testCompareSets() {
  Set *set1 = newSet(0);
  Set *set2 = newSet(0);
  Set *set3 = newSet(0);
  Set *set4 = newSet(0);
  
  addToSet(set3, 4);
  addToSet(set4, 4);

  ASSERT(equalSets(set1, set2));
  ASSERT(!equalSets(set1, set3));
  ASSERT(equalSets(set3, set4));
}


void registerSetUnitTests()
{
  registerUnitTest(testAddToSet);
  registerUnitTest(testSetUnion);
  registerUnitTest(testSetRemove);
  registerUnitTest(testInSet);
  registerUnitTest(testClearSet);
  registerUnitTest(testCompareSets);
}
