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
static Aword stack[STACKSIZE];
static int stackp = 0;


#ifdef _PROTOTYPES_
void push(Aword i)
#else
void push(i)
     Aword i;
#endif
{
  if (stackp == STACKSIZE)
    syserr("Out of stack space.");
  stack[stackp++] = i;
}


#ifdef _PROTOTYPES_
Aword pop(void)
#else
Aword pop()
#endif
{
  if (stackp == 0)
    syserr("Stack underflow.");
  return(stack[--stackp]);
}


#ifdef _PROTOTYPES_
Aword top(void)
#else
Aword top()
#endif
{
  return(stack[stackp-1]);
}
