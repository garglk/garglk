/*----------------------------------------------------------------------*\

  state.c

  State and undo manager unit of Alan interpreter

\*----------------------------------------------------------------------*/
#include "state.h"

/* IMPORTS */

#include "syserr.h"
#include "current.h"
#include "word.h"
#include "StateStack.h"
#include "instance.h"
#include "attribute.h"
#include "memory.h"
#include "score.h"
#include "event.h"
#include "set.h"


/* PUBLIC DATA */


/* PRIVATE TYPES */

/* Implementation of the abstract type typedef struct game_state GameState */
struct game_state {
    /* Event queue */
    EventQueueEntry *eventQueue;
    int eventQueueTop;			/* Event queue top pointer */

    /* Scores */
    int score;
    Aword *scores;				/* Score table pointer */

    /* Instance data */
    AdminEntry *admin;			/* Administrative data about instances */
    AttributeEntry *attributes;	/* Attributes data area */
    /* Sets and strings are dynamically allocated areas for which the
       attribute is just a pointer to. So they are not catched by the
       saving of attributes, instead they require special storage */
    Set **sets;					/* Array of set pointers */
    char **strings;				/* Array of string pointers */
};

/* PRIVATE DATA */
static GameState gameState;     /* TODO: Make pointer, then we don't have to copy to stack, we can just use the pointer */
static StateStackP stateStack = NULL;

static char *playerCommand;


/*----------------------------------------------------------------------*/
static int countStrings(void) {
    StringInitEntry *entry;
    int count = 0;

    if (header->stringInitTable != 0)
        for (entry = pointerTo(header->stringInitTable); *(Aword *)entry != EOF; entry++)
            count++;
    return(count);
}


/*----------------------------------------------------------------------*/
static void deallocateStrings(GameState *gameState) {
    int count = countStrings();
    int i;

    for (i = 0; i < count; i++)
        deallocate(gameState->strings[i]);
    deallocate(gameState->strings);
}

/*----------------------------------------------------------------------*/
static int countSets(void) {
    SetInitEntry *entry;
    int count = 0;

    if (header->setInitTable != 0)
        for (entry = pointerTo(header->setInitTable); *(Aword *)entry != EOF; entry++)
            count++;
    return(count);
}


/*----------------------------------------------------------------------*/
static void deallocateSets(GameState *gameState) {
    int count = countSets();
    int i;

    for (i = 0; i < count; i++)
        freeSet(gameState->sets[i]);
    deallocate(gameState->sets);
}

/*======================================================================*/
void deallocateGameState(GameState *gameState) {

    deallocate(gameState->admin);
    deallocate(gameState->attributes);

    if (gameState->eventQueueTop > 0) {
        deallocate(gameState->eventQueue);
        gameState->eventQueue = NULL;
    }
    if (gameState->scores)
        deallocate(gameState->scores);

    deallocateStrings(gameState);
    deallocateSets(gameState);

    memset(gameState, 0, sizeof(GameState));
}


/*======================================================================*/
void forgetGameState(void) {
    char *playerCommand;
    popGameState(stateStack, &gameState, &playerCommand);
    deallocateGameState(&gameState);
    if (playerCommand != NULL)
        deallocate(playerCommand);
}


/*======================================================================*/
void initStateStack(void) {
    if (stateStack != NULL)
        deleteStateStack(stateStack);
    stateStack = createStateStack(sizeof(GameState));
}


/*======================================================================*/
void terminateStateStack(void) {
    deleteStateStack(stateStack);
    stateStack = NULL;
}


/*======================================================================*/
bool anySavedState(void) {
    return !stateStackIsEmpty(stateStack);
}


/*----------------------------------------------------------------------*/
static Set **collectSets(void) {
    SetInitEntry *entry;
    int count = countSets();
    Set **sets;
    int i;

    if (count == 0) return NULL;

    sets = allocate(count*sizeof(Set));

    entry = pointerTo(header->setInitTable);
    for (i = 0; i < count; i++)
        sets[i] = getInstanceSetAttribute(entry[i].instanceCode, entry[i].attributeCode);

    return sets;
}


/*----------------------------------------------------------------------*/
static char **collectStrings(void) {
    StringInitEntry *entry;
    int count = countStrings();
    char **strings;
    int i;

    if (count == 0) return NULL;

    strings = allocate(count*sizeof(char *));

    entry = pointerTo(header->stringInitTable);
    for (i = 0; i < count; i++)
        strings[i] = getInstanceStringAttribute(entry[i].instanceCode, entry[i].attributeCode);

    return strings;
}


/*======================================================================*/
void rememberCommands(void) {
    char *command = playerWordsAsCommandString();
    attachPlayerCommandsToLastState(stateStack, command);
    deallocate(command);
}


/*----------------------------------------------------------------------*/
static void collectEvents(void) {
    gameState.eventQueueTop = eventQueueTop;
    if (eventQueueTop > 0)
        gameState.eventQueue = duplicate(eventQueue, eventQueueTop*sizeof(EventQueueEntry));
}


/*----------------------------------------------------------------------*/
static void collectInstanceData(void) {
    gameState.admin = duplicate(admin, (header->instanceMax+1)*sizeof(AdminEntry));
    gameState.attributes = duplicate(attributes, header->attributesAreaSize*sizeof(Aword));
    gameState.sets = collectSets();
    gameState.strings = collectStrings();
}


/*----------------------------------------------------------------------*/
static void collectScores(void) {
    gameState.score = current.score;
    if (scores == NULL)
        gameState.scores = NULL;
    else
        gameState.scores = duplicate(scores, header->scoreCount*sizeof(Aword));
}


/*======================================================================*/
void rememberGameState(void) {
    collectEvents();
    collectInstanceData();
    collectScores();

    if (stateStack == NULL)
        initStateStack();

    pushGameState(stateStack, &gameState);
    gameStateChanged = FALSE;
}


/*----------------------------------------------------------------------*/
static void freeCurrentSetAttributes(void) {
    SetInitEntry *entry;

    if (header->setInitTable == 0) return;
    for (entry = pointerTo(header->setInitTable); *(Aword *)entry != EOF; entry++) {
        Aptr attributeValue = getAttribute(admin[entry->instanceCode].attributes, entry->attributeCode);
        freeSet((Set*)fromAptr(attributeValue));
    }
}


/*----------------------------------------------------------------------*/
static void recallSets(Set **sets) {
    SetInitEntry *entry;
    int count = countSets();
    int i;

    if (header->setInitTable == 0) return;

    entry = pointerTo(header->setInitTable);
    for (i = 0; i < count; i++) {
        setAttribute(admin[entry[i].instanceCode].attributes, entry[i].attributeCode, toAptr(sets[i]));
        sets[i] = NULL; /* Since we reuse the saved set, we need to clear the pointer */
    }
}


/*----------------------------------------------------------------------*/
static void freeCurrentStringAttributes(void) {
    StringInitEntry *entry;

    if (header->stringInitTable == 0) return;
    for (entry = pointerTo(header->stringInitTable); *(Aword *)entry != EOF; entry++) {
        Aptr attributeValue = getAttribute(admin[entry->instanceCode].attributes, entry->attributeCode);
        deallocate(fromAptr(attributeValue));
    }
}


/*----------------------------------------------------------------------*/
static void recallStrings(char **strings) {
    StringInitEntry *entry;
    int count = countStrings();
    int i;

    if (header->stringInitTable == 0) return;

    entry = pointerTo(header->stringInitTable);
    for (i = 0; i < count; i++) {
        setAttribute(admin[entry[i].instanceCode].attributes, entry[i].attributeCode, toAptr(strings[i]));
        strings[i] = NULL;      /* Since we reuse the saved, we need to clear the state */
    }
}


/*----------------------------------------------------------------------*/
static void recallEvents(void) {
    eventQueueTop = gameState.eventQueueTop;
    if (eventQueueTop > 0) {
        memcpy(eventQueue, gameState.eventQueue,
               (eventQueueTop+1)*sizeof(EventQueueEntry));
    }
}


/*----------------------------------------------------------------------*/
static void recallInstances(void) {

    if (admin == NULL)
        syserr("admin[] == NULL in recallInstances()");

    memcpy(admin, gameState.admin,
           (header->instanceMax+1)*sizeof(AdminEntry));

    freeCurrentSetAttributes();		/* Need to free previous set values */
    freeCurrentStringAttributes();	/* Need to free previous string values */

    memcpy(attributes, gameState.attributes,
           header->attributesAreaSize*sizeof(Aword));

    recallSets(gameState.sets);
    recallStrings(gameState.strings);
}


/*----------------------------------------------------------------------*/
static void recallScores(void) {
    current.score = gameState.score;
    memcpy(scores, gameState.scores, header->scoreCount*sizeof(Aword));
}


/*======================================================================*/
void recallGameState(void) {
    popGameState(stateStack, &gameState, &playerCommand);
    recallEvents();
    recallInstances();
    recallScores();
    deallocateGameState(&gameState);
}


/*======================================================================*/
char *recreatePlayerCommand(void) {
    return playerCommand;
}
