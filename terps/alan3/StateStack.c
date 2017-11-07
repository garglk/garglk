/*----------------------------------------------------------------------*\

  StateStack.c

\*----------------------------------------------------------------------*/
#include "StateStack.h"

/* IMPORTS: */
#include "syserr.h"
#include "memory.h"
#include "state.h"


/* CONSTANTS: */
#define EXTENT 10


/* PRIVATE TYPES: */
typedef struct StateStackStructure {
	void **states;
	char **commands;
	int stackSize;
	int stackPointer;	/* Points above used stack, 0 initially */
	int elementSize;	/* Size of elements in the stack */
} StateStackStructure;


/*----------------------------------------------------------------------*/
static void *reallocate(void *from, int newSize)
{
    void *newArea = realloc(from, newSize*sizeof(void*));
    if (newArea == NULL)
        syserr("Out of memory in 'reallocateStack()'");
    return newArea;
}

/*======================================================================*/
StateStackP createStateStack(int elementSize) {
    StateStackP stack = NEW(StateStackStructure);
    stack->stackSize = 0;
    stack->stackPointer = 0;
    stack->elementSize = elementSize;
    return stack;
}


/*======================================================================*/
void deleteStateStack(StateStackP stateStack) {
    if (stateStack != NULL) {
        while (stateStack->stackPointer > 0) {
            stateStack->stackPointer--;
            deallocateGameState(stateStack->states[stateStack->stackPointer]);
            deallocate(stateStack->states[stateStack->stackPointer]);
            deallocate(stateStack->commands[stateStack->stackPointer]);
        }
        if (stateStack->stackSize > 0) {
            deallocate(stateStack->states);
            deallocate(stateStack->commands);
        }
        deallocate(stateStack);
    }
}


/*======================================================================*/
bool stateStackIsEmpty(StateStackP stateStack) {
	return stateStack->stackPointer == 0;
}


/*----------------------------------------------------------------------*/
static void ensureSpaceForGameState(StateStackP stack)
{
    if (stack->stackPointer == stack->stackSize) {
    	stack->states = reallocate(stack->states, stack->stackSize+EXTENT);
    	stack->commands = reallocate(stack->commands, stack->stackSize+EXTENT);
    	stack->stackSize += EXTENT;
    }
}


/*======================================================================*/
void pushGameState(StateStackP stateStack, void *gameState) {
	void *element = allocate(stateStack->elementSize);
	memcpy(element, gameState, stateStack->elementSize);
    ensureSpaceForGameState(stateStack);
    stateStack->commands[stateStack->stackPointer] = NULL;
    stateStack->states[stateStack->stackPointer++] = element;
}


/*======================================================================*/
void attachPlayerCommandsToLastState(StateStackP stateStack, char *playerCommands) {
	stateStack->commands[stateStack->stackPointer-1] = strdup(playerCommands);
}


/*======================================================================*/
void popGameState(StateStackP stateStack, void *gameState, char** playerCommand) {
    if (stateStack->stackPointer == 0)
        syserr("Popping GameState from empty stack");
    else {
        stateStack->stackPointer--;
        memcpy(gameState, stateStack->states[stateStack->stackPointer], stateStack->elementSize);
        deallocate(stateStack->states[stateStack->stackPointer]);
        *playerCommand = stateStack->commands[stateStack->stackPointer];
    }
}
