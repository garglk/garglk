#ifndef _STATE_H_
#define _STATE_H_
/*----------------------------------------------------------------------*\

  state.h

  Header file for instruction state and undo handling in Alan interpreter

\*----------------------------------------------------------------------*/

/* Data: */
extern Bool gameStateChanged;

/* Functions: */
extern void initUndoStack(void);
extern void undo(void);
extern void pushGameState(void);
extern void forgetGameState(void);
extern void rememberCommands(void);
extern Bool popGameState(void);

#endif
