#ifndef _ACT_H_
#define _ACT_H_
/*----------------------------------------------------------------------*\

  ACT.H

  Header file for action unit of ARUN Alan System interpreter

\*----------------------------------------------------------------------*/

#include "types.h"
#include "acode.h"
#include <setjmp.h>


/* DATA */

/* trycheck() execute flags */
#define EXECUTE TRUE
#define DONT_EXECUTE FALSE

/* FUNCTIONS */

extern Bool checklim(Aword cnt, Aword obj);
extern Bool trycheck(Aaddr adr, Bool act);
extern Bool possible(void);
extern Bool exitto(int to, int from);
extern void action(ParamEntry *plst);
extern void go(int dir);

#endif
