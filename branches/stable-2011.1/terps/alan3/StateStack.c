/*----------------------------------------------------------------------*\

  StateStack.c

\*----------------------------------------------------------------------*/
#include "StateStack.h"

/* IMPORTS: */
#include "syserr.h"
#include "memory.h"


/* CONSTANTS: */
#define EXTENT 10


/* PRIVATE TYPES: */
typedef struct StateStackStructure {
	void **stack;
	char **playerCommands;
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
StateStack createStateStack(int elementSize) {
  StateStack stack = NEW(StateStackStructure);
  stack->stackSize = 0;
  stack->stackPointer = 0;
  stack->elementSize = elementSize;
  return stack;
}


/*======================================================================*/
void deleteStateStack(StateStack stateStack) {
	while (stateStack->stackPointer >0)
		free(stateStack->stack[--stateStack->stackPointer]);
	if (stateStack->stackSize > 0) {
		free(stateStack->stack);
		free(stateStack->playerCommands);
	}
	free(stateStack);
}


/*======================================================================*/
bool stateStackIsEmpty(StateStack stateStack) {
	return stateStack->stackPointer == 0;
}


/*----------------------------------------------------------------------*/
static void ensureSpaceForGameState(StateStack stack)
{
    if (stack->stackPointer == stack->stackSize) {
    	stack->stack = reallocate(stack->stack, stack->stackSize+EXTENT);
    	stack->playerCommands = reallocate(stack->playerCommands, stack->stackSize+EXTENT);
    	stack->stackSize += EXTENT;
    }
}


/*======================================================================*/
void pushGameState(StateStack stateStack, void *gameState) {
	void *element = allocate(stateStack->elementSize);
	memcpy(element, gameState, stateStack->elementSize);
    ensureSpaceForGameState(stateStack);
    stateStack->playerCommands[stateStack->stackPointer] = NULL;
    stateStack->stack[stateStack->stackPointer++] = element;
}


/*======================================================================*/
void attachPlayerCommandsToLastState(StateStack stateStack, char *playerCommands) {
	stateStack->playerCommands[stateStack->stackPointer-1] = strdup(playerCommands);
}


/*======================================================================*/
void popGameState(StateStack stateStack, void *gameState, char** playerCommand) {
	if (stateStack->stackPointer == 0)
		syserr("Popping GameState from empty stack");
	else {
		stateStack->stackPointer--;
		memcpy(gameState, stateStack->stack[stateStack->stackPointer], stateStack->elementSize);
		free(stateStack->stack[stateStack->stackPointer]);
		*playerCommand = stateStack->playerCommands[stateStack->stackPointer];
	}
}
