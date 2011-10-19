#ifndef _STATE_H_
#define _STATE_H_
/*----------------------------------------------------------------------*\

  state.h

  Header file for instruction state and undo handling in Alan interpreter

\*----------------------------------------------------------------------*/

/* IMPORTS */
#include "types.h"


/* DATA */


/* FUNCTIONS */
extern bool anySavedState(void);
extern void initStateStack(void);
extern void rememberGameState(void);
extern void forgetGameState(void);
extern void rememberCommands(void);
extern void recallGameState(void);
extern char *recreatePlayerCommand(void);
#endif
