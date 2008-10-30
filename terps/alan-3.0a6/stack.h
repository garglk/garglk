#ifndef _STACK_H_
#define _STACK_H_
/*----------------------------------------------------------------------*\

  stack.h

  Header file for stack handler in Alan interpreter

\*----------------------------------------------------------------------*/

/* TYPES */

/* Functions: */

extern void dumpStack();
extern Aword pop(void);
extern void push(Aword item);
extern Aword top(void);
extern void newFrame(Aint noOfLocals);
extern void setLocal(Aint blocksBelow, Aint variableNumber, Aword value);
extern Aword getLocal(Aint level, Aint variable);
extern void endFrame(void);

#endif
