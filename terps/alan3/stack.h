#ifndef _STACK_H_
#define _STACK_H_
/*----------------------------------------------------------------------*\

  stack.h

  Header file for stack handler in Alan interpreter

\*----------------------------------------------------------------------*/

/* TYPES */

/* Functions: */

extern void dumpStack();
extern Aptr pop(void);
extern void push(Aptr item);
extern Aptr top(void);
extern void newFrame(Aint noOfLocals);
extern void setLocal(Aint blocksBelow, Aint variableNumber, Aptr value);
extern Aptr getLocal(Aint level, Aint variable);
extern void endFrame(void);

#endif
