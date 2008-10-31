/*======================================================================*\

  exeTest.c

  Unit tests for exe module in the Alan interpreter

\*======================================================================*/

#include "set.h"
#include "exe.c"


static void testCountTrailingBlanks() {
  char *threeBlanks = "h   ";
  char *fiveBlanks = "     ";
  char *empty = "";
  ASSERT(countTrailingBlanks(threeBlanks, strlen(threeBlanks)-1) == 3);
  ASSERT(countTrailingBlanks(threeBlanks, 1) == 1);
  ASSERT(countTrailingBlanks(threeBlanks, 0) == 0);
  ASSERT(countTrailingBlanks(fiveBlanks, strlen(fiveBlanks)-1) == 5);
  ASSERT(countTrailingBlanks(empty, strlen(empty)-1) == 0);
}

static void testSkipWordForwards() {
  char *string = "a string of words";

  ASSERT(skipWordForwards(string, 0) == 1);
  ASSERT(skipWordForwards(string, 2) == 8);
  ASSERT(skipWordForwards(string, 9) == 11);
  ASSERT(skipWordForwards(string, 12) == 17);
  ASSERT(skipWordForwards(string, strlen(string)-1) == strlen(string));
}

static void testSkipWordBackwards() {
  char *string = "a string of words";
  char *emptyString = "";

  ASSERT(skipWordBackwards(string, 0) == 0);
  ASSERT(skipWordBackwards(string, 4) == 2);
  ASSERT(skipWordBackwards(string, 10) == 9);
  ASSERT(skipWordBackwards(string, strlen(string)) == 12);

  ASSERT(skipWordBackwards(emptyString, strlen(emptyString)) == 0);
}

static void testStripCharsFromString() {
  char *characters;
  char *rest;
  char *result;

  characters = "abcdef";
  result = stripCharsFromStringForwards(3, characters, &rest);
  ASSERT(strcmp(result, "abc")==0);
  ASSERT(strcmp(rest, "def")==0);

  characters = "ab";
  result = stripCharsFromStringForwards(3, characters, &rest);
  ASSERT(strcmp(result, "ab")==0);
  ASSERT(strcmp(rest, "")==0);

  characters = "";
  result = stripCharsFromStringForwards(3, characters, &rest);
  ASSERT(strcmp(result, "")==0);
  ASSERT(strcmp(rest, "")==0);

  characters = "abcdef";
  result = stripCharsFromStringBackwards(3, characters, &rest);
  ASSERT(strcmp(result, "def")==0);
  ASSERT(strcmp(rest, "abc")==0);

  characters = "ab";
  result = stripCharsFromStringBackwards(3, characters, &rest);
  ASSERT(strcmp(result, "ab")==0);
  ASSERT(strcmp(rest, "")==0);

  characters = "";
  result = stripCharsFromStringBackwards(3, characters, &rest);
  ASSERT(strcmp(result, "")==0);
  ASSERT(strcmp(rest, "")==0);
}

static void testStripWordsFromString() {
  char *testString = "this is four  words";
  char *empty = "";
  char *result;
  char *rest;

  result = stripWordsFromStringForwards(3, empty, &rest);
  ASSERT(strcmp(result, "") == 0);
  ASSERT(strcmp(rest, "") == 0);

  result = stripWordsFromStringForwards(3, testString, &rest);
  ASSERT(strcmp(result, "this is four") == 0);
  ASSERT(strcmp(rest, "words") == 0);

  result = stripWordsFromStringForwards(7, testString, &rest);
  ASSERT(strcmp(result, "this is four  words") == 0);
  ASSERT(strcmp(rest, "") == 0);

  result = stripWordsFromStringBackwards(3, empty, &rest);
  ASSERT(strcmp(result, "") == 0);
  ASSERT(strcmp(rest, "") == 0);

  result = stripWordsFromStringBackwards(3, testString, &rest);
  ASSERT(strcmp(result, "is four  words") == 0);
  ASSERT(strcmp(rest, "this") == 0);

  result = stripWordsFromStringBackwards(7, testString, &rest);
  ASSERT(strcmp(result, "this is four  words") == 0);
  ASSERT(strcmp(rest, "") == 0);

  testString = " an initial space";
  result = stripWordsFromStringBackwards(7, testString, &rest);
  ASSERT(strcmp(result, "an initial space") == 0);
  ASSERT(strcmp(rest, "") == 0);
}


static char testFileName[] = "getStringTestFile";
static FILE *testFile;
static void writeAndOpenGetStringTestFile(int fpos, char *testString)
{
  int i;

  testFile = fopen(testFileName, "wb");
  for (i = 0; i < fpos; i++) fputc(' ', testFile);
  fprintf(testFile, testString);
  fclose(testFile);
  textFile = fopen(testFileName, "rb");
}


void testGetString()
{
  int fpos = 55;
  char testString[] = "hejhopp";

  writeAndOpenGetStringTestFile(fpos, testString);
  header->pack = FALSE;
  header->stringOffset = 0;
  ASSERT(strcmp(getStringFromFile(fpos, strlen(testString)), testString)==0);
  header->stringOffset = 1;
  ASSERT(strcmp(getStringFromFile(fpos, strlen(testString)-1), &testString[1])==0);
  fclose(textFile);
  unlink(testFileName);
}


static void testIncreaseEventQueue()
{
  eventQueueSize = 0;
  eventQueue = NULL;
  eventQueueTop = 0;

  increaseEventQueue();

  ASSERT(eventQueueSize != 0);
  ASSERT(eventQueue != NULL);
}

static void testWhereIllegalId() {
  header->instanceMax = 1;
  expectSyserr = TRUE;
  hadSyserr = FALSE;
  if (setjmp(syserr_label) == 0)
    where(0, TRUE);
  ASSERT(hadSyserr);
  hadSyserr = FALSE;
  if (setjmp(syserr_label) == 0)
    where(2, TRUE);
  ASSERT(hadSyserr);
  expectSyserr = FALSE;
}

static void testHereIllegalId() {
  header->instanceMax = 1;
  expectSyserr = TRUE;
  hadSyserr = FALSE;
  if (setjmp(syserr_label) == 0)
    isHere(0, FALSE);
  ASSERT(hadSyserr);
  hadSyserr = FALSE;
  if (setjmp(syserr_label) == 0)
    isHere(2, FALSE);
  ASSERT(hadSyserr);
  expectSyserr = FALSE;
}

static void testLocateIllegalId() {
  header->instanceMax = 1;
  expectSyserr = TRUE;
  hadSyserr = FALSE;
  if (setjmp(syserr_label) == 0)
    locate(0, 1);
  ASSERT(hadSyserr);
  hadSyserr = FALSE;
  if (setjmp(syserr_label) == 0)
    locate(2, 1);
  ASSERT(hadSyserr);
  hadSyserr = FALSE;
  if (setjmp(syserr_label) == 0)
    locate(1, 0);
  ASSERT(hadSyserr);
  hadSyserr = FALSE;
  if (setjmp(syserr_label) == 0)
    locate(1, 2);
  ASSERT(hadSyserr);
  expectSyserr = FALSE;
}


static void testWhere() {
  admin = allocate(5*sizeof(AdminEntry));
  instance = allocate(5*sizeof(InstanceEntry));
  class = allocate(5*sizeof(ClassEntry));
  header = allocate(sizeof(ACodeHeader));

  header->locationClassId = 1;
  header->instanceMax = 4;

  instance[1].parent = 1;	/* A location */
  admin[1].location = 3;
  ASSERT(where(1, TRUE) == 0);	/* Locations are always nowhere */
  ASSERT(where(1, FALSE) == 0);

  instance[2].parent = 0;	/* Not a location */
  admin[2].location = 1;	/* At 1 */
  ASSERT(where(2, TRUE) == 1);
  ASSERT(where(2, FALSE) == 1);

  instance[3].parent = 0;	/* Not a location */
  admin[3].location = 2;	/* In 2 which is at 1*/
  ASSERT(where(3, TRUE) == 2);
  ASSERT(where(3, FALSE) == 1);

  instance[4].parent = 0;	/* Not a location */
  admin[4].location = 3;	/* In 3 which is in 2 which is at 1*/
  ASSERT(where(4, TRUE) == 3);
  ASSERT(where(4, FALSE) == 1);

  free(admin);
  free(instance);
  free(class);
  free(header);
}

static void testGetMembers() {
  Set *set = newSet(0);
  Aword code[] = {0,	/* Dummy */
		  (Aword) set,
		  INSTRUCTION(I_SETSIZE),
		  INSTRUCTION(I_RETURN)};

  memory = code;
  memTop = 100;
  interpret(1);
  ASSERT(pop() == 0);
}


void testContainerSize() {
  header = allocate(sizeof(ACodeHeader));
  instance = allocate(4*sizeof(InstanceEntry));
  admin = allocate(4*sizeof(AdminEntry));

  header->instanceMax = 3;
  instance[1].container = 1;
  admin[1].location = 0;
  admin[2].location = 1;
  admin[3].location = 2;

  ASSERT(containerSize(1, TRUE) == 1);
  ASSERT(containerSize(1, FALSE) == 2);

  free(admin);
  free(instance);
  free(header);
}

void registerExeUnitTests() {
  registerUnitTest(testCountTrailingBlanks);
  registerUnitTest(testSkipWordForwards);
  registerUnitTest(testSkipWordBackwards);
  registerUnitTest(testStripCharsFromString);
  registerUnitTest(testStripWordsFromString);
  registerUnitTest(testGetString);
  registerUnitTest(testIncreaseEventQueue);
  registerUnitTest(testWhereIllegalId);
  registerUnitTest(testHereIllegalId);
  registerUnitTest(testLocateIllegalId);
  registerUnitTest(testWhere);
  registerUnitTest(testGetMembers);
  registerUnitTest(testContainerSize);
}
