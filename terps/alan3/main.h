#ifndef _MAIN_H_
#define _MAIN_H_
/*----------------------------------------------------------------------*\

  Header file for main unit of ARUN Alan System interpreter

\*----------------------------------------------------------------------*/

#include "types.h"
#include "acode.h"


/* DATA */

/* Amachine data structures - Dynamic */

/* Amachine data structures - Static */

extern VerbEntry *vrbs;		/* Verb table pointer */


/* FUNCTIONS: */

/* Run the game! */
extern void run(void);

#endif
