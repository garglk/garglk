#ifndef _STACK_H_
#define _STACK_H_
/*----------------------------------------------------------------------*\

  stack.h

  Header file for stack handler in Alan interpreter

\*----------------------------------------------------------------------*/

/* IMPORTS: */
#include "acode.h"


/* TYPES: */

typedef struct StackStructure *Stack;


/* FUNCTIONS: */

extern Stack createStack(int size);
extern void deleteStack(Stack stack);
extern void dumpStack(Stack theStack);
extern Aword pop(Stack stack);
extern void push(Stack stack, Aword item);
extern Aword top(Stack theStack);
extern int stackDepth(Stack theStack);

extern void newFrame(Stack theStack, Aint noOfLocals);
extern void setLocal(Stack theStack, Aint blocksBelow, Aint variableNumber, Aword value);
extern Aword getLocal(Stack theStack, Aint level, Aint variable);
extern void endFrame(Stack theStack);

#endif
