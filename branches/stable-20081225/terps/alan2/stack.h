#ifndef _STACK_H_
#define _STACK_H_
/*----------------------------------------------------------------------*\

  stack.h

  Header file for stack handler in Alan interpreter

\*----------------------------------------------------------------------*/

/* TYPES */

#ifdef _PROTOTYPES_

extern Aword pop(void);
extern void push(Aword item);
extern Aword top(void);

#else
extern Aword pop();
extern void push();
extern Aword top();
#endif

#endif
