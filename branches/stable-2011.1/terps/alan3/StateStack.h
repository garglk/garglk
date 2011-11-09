#ifndef STATESTACK_H_
#define STATESTACK_H_

/* IMPORTS */
#include "types.h"

/* CONSTANTS */


/* TYPES */
typedef struct StateStackStructure *StateStack;


/* DATA */


/* FUNCTIONS */
extern StateStack createStateStack(int elementSize);
extern bool stateStackIsEmpty(StateStack stateStack);
extern void pushGameState(StateStack stateStack, void *state);
extern void popGameState(StateStack stateStack, void *state, char **playerCommandPointer);
extern void attachPlayerCommandsToLastState(StateStack stateStack, char *playerCommand);
extern void deleteStateStack(StateStack stateStack);


#endif /* STATESTACK_H_ */
