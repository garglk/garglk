#ifndef _STACK_H_
#define _STACK_H_
/*----------------------------------------------------------------------*\

  stack.h

  Header file for stack handler in Alan interpreter

\*----------------------------------------------------------------------*/

/* TYPES */

#ifdef _PROTOTYPES_

extern Aptr pop(void);
extern void push(Aptr item);
extern Aptr top(void);

#else
extern Aptr pop();
extern void push();
extern Aptr top();
#endif

#endif
