/*======================================================================*\

  stackTest.c

  Unit tests for stack module in the Alan interpreter

\*======================================================================*/

#include "stack.c"


static void testNewFrame()
{
  Aint originalSp;

  framePointer = 47;
  originalSp = stackp;

  /* Add a block with four local variables */
  newFrame(4);

  ASSERT(stackp == originalSp + 1/*old fp*/ + 4/*Locals*/);
  ASSERT(framePointer == originalSp + 1);

  ASSERT(getLocal(0,1) == 0);
  setLocal(0,1,14);
  ASSERT(getLocal(0,1) == 14);
  ASSERT(stack[stackp - 4] == 14);
  ASSERT(stack[stackp - 5] == 47);

  endFrame();
  ASSERT(stackp == originalSp);
  ASSERT(framePointer == 47);
}  

  
static void testFrameInFrame()
{
  Aint originalSp;

  framePointer = 47;
  originalSp = stackp;

  /* Add a block with one local variables */
  newFrame(1);
  setLocal(0,1,14);
  ASSERT(getLocal(0,1) == 14);
  newFrame(1);
  setLocal(0,1,15);
  ASSERT(getLocal(0,1) == 15);
  ASSERT(getLocal(1,1) == 14);
  endFrame();
  endFrame();
}  

  

void registerStackUnitTests()
{
  registerUnitTest(testNewFrame);
  registerUnitTest(testFrameInFrame);
}
