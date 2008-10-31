/*======================================================================*\

  reverseTest.c

  Unit tests for reverse module in the Alan interpreter

\*======================================================================*/

#include "reverse.c"


static void testAlreadyDone(void)
{
  addressesDone = NULL;
  doneSize = 0;
  numberDone = 0;

  ASSERT(alreadyDone(0));
  ASSERT(doneSize == 0);

  ASSERT(!alreadyDone(1));
  ASSERT(doneSize == 100);
  ASSERT(numberDone == 1);

  ASSERT(!alreadyDone(2));
  ASSERT(doneSize == 100);
  ASSERT(numberDone == 2);

  ASSERT(alreadyDone(1));
  ASSERT(doneSize == 100);
  ASSERT(numberDone == 2);
}


void registerReverseUnitTests()
{
  registerUnitTest(testAlreadyDone);
}
