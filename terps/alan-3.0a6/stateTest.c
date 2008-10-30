/*======================================================================*\

  stateTest.c

  Unit tests for state module in the Alan interpreter

\*======================================================================*/

#include "set.h"
#include "state.c"

static void setupInstances(int instanceMax, int attributeCount) {
  int adminSize = (instanceMax+1)*sizeof(AdminEntry)/sizeof(Aword);
  int attributeAreaSize = (instanceMax+1)*attributeCount*sizeof(AttributeEntry)/sizeof(Aword);
  int i;

  header = allocate(sizeof(ACodeHeader));
  header->attributesAreaSize = attributeAreaSize;
  header->instanceMax = instanceMax;

  admin = allocate((instanceMax+1)*sizeof(AdminEntry));
  for (i = 0; i < adminSize; i++) ((Aword *)admin)[i] = i;

  attributes = allocate((instanceMax+1)*attributeCount*sizeof(AttributeEntry));
  for (i = 0; i < attributeAreaSize; i++) ((Aword *)attributes)[i] = i;

}

static void buildGameStateStack(int depth, int attributeCount) {
  int i;

  if (depth == 0)
    gameState = NULL;
  else {
    gameState = allocate(depth*sizeof(GameState));
    for (i = 0; i<depth; i++) {
      gameState[i].admin = allocate((header->instanceMax+1)*sizeof(AdminEntry));
      gameState[i].attributes = allocate((header->instanceMax+1)*attributeCount*sizeof(AttributeEntry));
    }
  }

  gameStateTop = depth;
  gameStateSize = depth;
}


static void testPushGameState() {
  int instanceCount = 3;
  int adminSize = (instanceCount+1)*sizeof(AdminEntry)/sizeof(Aword);
  int attributeCount = 5;
  int attributeAreaSize = attributeCount*instanceCount*sizeof(AttributeEntry)/sizeof(Aword);

  buildGameStateStack(0, instanceCount);
  setupInstances(instanceCount, attributeCount);

  eventQueueTop = 0;

  pushGameState();

  ASSERT(gameState != NULL);
  ASSERT(gameStateTop == 1);
  ASSERT(memcmp(gameState->attributes, attributes, attributeAreaSize*sizeof(Aword)) == 0);
  ASSERT(memcmp(gameState->admin, admin, adminSize*sizeof(Aword)) == 0);
}


static void testPushAndPopAttributesWithSet() {
  int instanceCount = 1;
  int attributeCount = 1;
  Set *originalSet = newSet(3);
  SetInitEntry *initEntry;

  setupInstances(instanceCount, attributeCount);

  admin[1].attributes = attributes;
  attributes[0].code = 1;
  attributes[0].value = (Aword)originalSet;
  addToSet(originalSet, 7);

  eventQueueTop = 0;

  /* Set up a set initialization */
  memory = allocate(sizeof(SetInitEntry)+2*sizeof(Aword));
  header->setInitTable = 1;
  initEntry = (SetInitEntry*)&memory[1];
  initEntry->instanceCode = 1;
  initEntry->attributeCode = 1;
  memory[1+sizeof(SetInitEntry)/sizeof(Aword)] = EOF;

  buildGameStateStack(0, instanceCount);

  pushGameState();

  ASSERT(gameState->sets[0] != (Aword)originalSet);
  ASSERT(equalSets((Set*)gameState->sets[0], originalSet));

  Set *modifiedSet = newSet(4);
  attributes[0].value = (Aword)modifiedSet;
  addToSet(modifiedSet, 11);
  addToSet(modifiedSet, 12);
  ASSERT(!equalSets((Set*)gameState->sets[0], modifiedSet));
  ASSERT(equalSets((Set*)attributes[0].value, modifiedSet));

  popGameState();

  ASSERT(attributes[0].value != (Aword)modifiedSet);
  ASSERT(attributes[0].value != (Aword)originalSet);
  ASSERT(equalSets((Set*)attributes[0].value, originalSet));
}

static void testPushAndPopAttributeState() {
  int instanceCount = 2;
  setupInstances(instanceCount, 3);
  buildGameStateStack(0, instanceCount);

  attributes[0].value = 12;
  attributes[2].value = 3;

  pushGameState();

  ASSERT(gameState != NULL);
  ASSERT(gameStateTop == 1);

  attributes[0].value = 11;
  attributes[2].value = 4;

  pushGameState();

  attributes[0].value = 55;
  attributes[2].value = 55;

  popGameState();

  ASSERT(attributes[0].value == 11);
  ASSERT(attributes[2].value == 4);

  popGameState();

  ASSERT(attributes[0].value == 12);
  ASSERT(attributes[2].value == 3);
}

static void testPushAndPopAdminState() {
  int instanceCount = 2;
  setupInstances(instanceCount, 3);
  buildGameStateStack(0, instanceCount);

  admin[1].location = 12;
  admin[2].script = 3;

  pushGameState();

  ASSERT(gameState != NULL);
  ASSERT(gameStateTop == 1);

  admin[2].location = 12;
  admin[2].alreadyDescribed = 2;
  admin[2].visitsCount = 13;
  admin[2].script = 33;
  admin[2].step = 3886;
  admin[2].waitCount = 38869878;

  pushGameState();

  admin[2].location = 55;
  admin[2].alreadyDescribed = 55;
  admin[2].visitsCount = 55;
  admin[2].script = 55;
  admin[2].step = 55;
  admin[2].waitCount = 55;

  popGameState();

  ASSERT(admin[2].location == 12);
  ASSERT(admin[2].alreadyDescribed == 2);
  ASSERT(admin[2].visitsCount == 13);
  ASSERT(admin[2].script == 33);
  ASSERT(admin[2].step == 3886);
  ASSERT(admin[2].waitCount == 38869878);

  popGameState();

  ASSERT(admin[1].location = 12);
  ASSERT(admin[2].script = 3);
}

static void testPushAndPopEvents() {
  int instanceCount = 1;

  buildGameStateStack(1, instanceCount);

  eventQueue = NULL;
  eventQueueTop = 0;

  pushGameState();

  eventQueue = allocate(5*sizeof(EventQueueEntry));
  eventQueueTop = 2;
  eventQueue[1].after = 47;

  pushGameState();

  eventQueueTop = 0;
  eventQueue[1].after = 1;

  popGameState();

  ASSERT(eventQueueTop == 2);
  ASSERT(eventQueue[1].after == 47);

  popGameState();

  ASSERT(eventQueueTop == 0);
}

static void testRememberCommand() {
  int instanceCount = 1;
  int i;
  char *command = "n, w, e and south";

  playerWords[0].code = EOF;
  for (i = 0; i < 4; i++)
    playerWords[i].code = i;
  playerWords[4].code = EOF;

  buildGameStateStack(1, instanceCount);

  firstWord = 0;
  lastWord = 3;
  playerWords[firstWord].start = command;
  playerWords[lastWord].end = &command[4];
  rememberCommands();

  ASSERT(strcmp(gameState[0].playerCommand, command) != 0);
  ASSERT(strncmp(gameState[0].playerCommand, command, 3) == 0);
}

static void testUndoStackFreesMemory() {
  int instanceCount = 1;

  buildGameStateStack(1, instanceCount);
  gameState[0].admin = allocate(100);

  initUndoStack();

  ASSERT(gameStateTop == 0);
  ASSERT(gameState[0].admin == NULL);
  ASSERT(gameState[0].attributes == NULL);
}

void registerStateUnitTests() {
  registerUnitTest(testUndoStackFreesMemory);
  registerUnitTest(testRememberCommand);
  registerUnitTest(testPushGameState);
  registerUnitTest(testPushAndPopAttributeState);
  registerUnitTest(testPushAndPopAdminState);
  registerUnitTest(testPushAndPopEvents);
  registerUnitTest(testPushAndPopAttributesWithSet);
}
