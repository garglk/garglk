/*----------------------------------------------------------------------*\

  STACK.C

  Stack Handling for Alan interpreter

\*----------------------------------------------------------------------*/


#include "types.h"
#include "main.h"
#include "syserr.h"
#include "stack.h"


/* PRIVATE DATA */

#define STACKSIZE 100

/* The AMACHINE STACK */
static Aptr stack[STACKSIZE];
static int stackp = 0;

/*======================================================================*/
void resetStack(void)
{
  stackp = 0;
}


/*======================================================================*/
void dumpStack(void)
{
  int i;

  printf("[");
  for (i = 0; i < stackp; i++)
    printf("%ld ", (long) stack[i]);
  printf("]");
}


/*======================================================================*/
void push(Aptr i)
{
  if (stackp == STACKSIZE)
    syserr("Out of stack space.");
  stack[stackp++] = i;
}


/*======================================================================*/
Aptr pop(void)
{
  if (stackp == 0)
    syserr("Stack underflow.");
  return(stack[--stackp]);
}


/*======================================================================*/
Aptr top(void)
{
  return(stack[stackp-1]);
}


/* The AMACHINE Block Frames */
static int framePointer = -1;

/*======================================================================*/
void newFrame(Aint noOfLocals)
{
  push(framePointer);
  framePointer = stackp;
  for (;noOfLocals > 0; noOfLocals--) push(0);
}


/*======================================================================*/
/* Local variables are numbered 1 and up and stored on their index-1 */
Aptr getLocal(Aint framesBelow, Aint variableNumber)
{
  int frame = framePointer;
  int frameCount;

  if (framesBelow != 0)
    for (frameCount = framesBelow; frameCount != 0; frameCount--)
      frame = stack[frame-1];

  if (variableNumber < 1)
    syserr("Reading a non-existing block-local variable.");

  return stack[frame + variableNumber-1];
}

/*======================================================================*/
void setLocal(Aint framesBelow, Aint variableNumber, Aptr value)
{
  int frame = framePointer;
  int frameCount;

  if (framesBelow != 0)
    for (frameCount = framesBelow; frameCount != 0; frameCount--)
      frame = stack[frame-1];

  if (variableNumber < 1)
    syserr("Writing a non-existing block-local variable.");

  stack[frame + variableNumber-1] = value;
}

/*======================================================================*/
void endFrame(void)
{
  stackp = framePointer;
  framePointer = pop();
}


