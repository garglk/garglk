#ifndef STATESTACK_H_
#define STATESTACK_H_

/* IMPORTS */
#include "types.h"

/* CONSTANTS */


/* TYPES */
typedef struct StateStackStructure *StateStackP;


/* DATA */


/* FUNCTIONS */
extern StateStackP createStateStack(int elementSize);
extern bool stateStackIsEmpty(StateStackP stateStack);
extern void pushGameState(StateStackP stateStack, void *state);
extern void popGameState(StateStackP stateStack, void *state, char **playerCommandPointer);
extern void attachPlayerCommandsToLastState(StateStackP stateStack, char *playerCommand);
extern void deleteStateStack(StateStackP stateStack);


#endif /* STATESTACK_H_ */
