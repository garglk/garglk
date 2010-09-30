/*----------------------------------------------------------------------*\

  STACK.C

  Stack Handling for Alan interpreter

\*----------------------------------------------------------------------*/


#include "types.h"
#include "main.h"

#include "stack.h"


/* PRIVATE DATA */

#define STACKSIZE 100

/* The AMACHINE STACK */
static Aptr stack[STACKSIZE];
static int stackp = 0;


#ifdef _PROTOTYPES_
void push(Aptr i)
#else
void push(i)
     Aptr i;
#endif
{
  if (stackp == STACKSIZE)
    syserr("Out of stack space.");
  stack[stackp++] = i;
}


#ifdef _PROTOTYPES_
Aptr pop(void)
#else
Aptr pop()
#endif
{
  if (stackp == 0)
    syserr("Stack underflow.");
  return(stack[--stackp]);
}


#ifdef _PROTOTYPES_
Aptr top(void)
#else
Aptr top()
#endif
{
  return(stack[stackp-1]);
}
