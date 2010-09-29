/*----------------------------------------------------------------------*\

  STACK.C

  Stack Handling for Alan interpreter

\*----------------------------------------------------------------------*/

#include "stack.h"

#include "types.h"
#include "syserr.h"
#include "memory.h"


/* ABSTRACT TYPE */
typedef struct StackStructure {
  Aint *stack;
  int stackSize;
  int stackp;
  int framePointer;
} StackStructure;


/* PRIVATE DATA */



/*======================================================================*/
Stack createStack(int size)
{
  StackStructure *theStack = NEW(StackStructure);

  theStack->stack = allocate(size*sizeof(Aptr));
  theStack->stackSize = size;
  theStack->framePointer = -1;

  return theStack;
}


/*======================================================================*/
void deleteStack(Stack theStack)
{
  if (theStack == NULL)
    syserr("deleting a NULL stack");

  free(theStack->stack);
  free(theStack);
}


/*======================================================================*/
int stackDepth(Stack theStack) {
  return theStack->stackp;
}


/*======================================================================*/
void dumpStack(Stack theStack)
{
  int i;

  if (theStack == NULL)
    syserr("NULL stack not supported anymore");

  printf("[");
  for (i = 0; i < theStack->stackp; i++)
    printf("%ld ", (unsigned long) theStack->stack[i]);
  printf("]");
}


/*======================================================================*/
void push(Stack theStack, Aptr i)
{
  if (theStack == NULL)
    syserr("NULL stack not supported anymore");

  if (theStack->stackp == theStack->stackSize)
    syserr("Out of stack space.");
  theStack->stack[(theStack->stackp)++] = i;
}


/*======================================================================*/
Aptr pop(Stack theStack)
{
  if (theStack == NULL)
    syserr("NULL stack not supported anymore");

  if (theStack->stackp == 0)
    syserr("Stack underflow.");
  return theStack->stack[--(theStack->stackp)];
}


/*======================================================================*/
Aptr top(Stack theStack)
{
  if (theStack == NULL)
    syserr("NULL stack not supported anymore");

  return(theStack->stack[theStack->stackp-1]);
}


/* The AMACHINE Block Frames */

/*======================================================================*/
void newFrame(Stack theStack, Aint noOfLocals)
{
  int n;

  if (theStack == NULL)
    syserr("NULL stack not supported anymore");

  push(theStack, theStack->framePointer);
  theStack->framePointer = theStack->stackp;
  for (n = 0; n < noOfLocals; n++)
    push(theStack, 0);
}


/*======================================================================*/
/* Local variables are numbered 1 and up and stored on their index-1 */
Aptr getLocal(Stack theStack, Aint framesBelow, Aint variableNumber)
{
  int frame;
  int frameCount;

  if (variableNumber < 1)
    syserr("Reading a non-existing block-local variable.");

  if (theStack == NULL)
    syserr("NULL stack not supported anymore");

  frame = theStack->framePointer;

  if (framesBelow != 0)
    for (frameCount = framesBelow; frameCount != 0; frameCount--)
      frame = theStack->stack[frame-1];

  return theStack->stack[frame + variableNumber-1];
}


/*======================================================================*/
void setLocal(Stack theStack, Aint framesBelow, Aint variableNumber, Aptr value)
{
  int frame;
  int frameCount;

  if (variableNumber < 1)
    syserr("Writing a non-existing block-local variable.");

  if (theStack == NULL)
    syserr("NULL stack not supported anymore");

  frame = theStack->framePointer;
  if (framesBelow != 0)
    for (frameCount = framesBelow; frameCount != 0; frameCount--)
      frame = theStack->stack[frame-1];

  theStack->stack[frame + variableNumber-1] = value;
}

/*======================================================================*/
void endFrame(Stack theStack)
{
  if (theStack == NULL)
    syserr("NULL stack not supported anymore");

  theStack->stackp = theStack->framePointer;
  theStack->framePointer = pop(theStack);
}


