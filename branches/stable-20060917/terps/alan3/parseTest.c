/*======================================================================*\

  parseTest.c

  Unit tests for parse module in the Alan interpreter

\*======================================================================*/

#include "parse.c"

static void makeEOS(ElementEntry *element) {
  element->code = EOS;
}

static void makeEOF(ElementEntry *element) {
  *((Aword *)element) = EOF;	/* End of table */
}

static void makeParameterElement(ElementEntry *element) {
  element->code = 0;
}

static void makeWordElement(ElementEntry *element, int code, int next) {
  element->code = code;
  element->next = next;
}

static DictionaryEntry *makeDictionary(int size) {
  dictsize = size;
  return allocate(size*sizeof(DictionaryEntry));
}

static void makeDictionaryEntry(int index, int code, int classBits) {
  if (index > dictsize) syserr("makeDictionaryEntry() out of size");
  dictionary[index].code = code;
  dictionary[index].classBits = classBits;
}

static void testMatchEndOfSyntax() {
  ElementEntry *element;
  ElementEntry *elementTable;

  memory = allocate(100*sizeof(Aword));
  elementTable = (ElementEntry *)&memory[20];

  /* No words */
  playerWords[0].code = EOF;
  wordIndex = 0;

  /* First try an empty parse tree */
  makeEOF(elementTable);
  element = matchEndOfSyntax(elementTable);
  ASSERT(element == NULL);

  /* Then one with a single EOS */
  makeEOS(&elementTable[0]);
  makeEOF(&elementTable[1]);

  element = matchEndOfSyntax(elementTable);
  ASSERT(element != NULL);
  ASSERT(element->code == EOS);

  free(memory);
}


static void testMatchParameterElement() {
  ElementEntry *element;
  ElementEntry *elementTable;

  memory = allocate(100*sizeof(Aword));
  elementTable = (ElementEntry *)&memory[50];

  /* No words */
  playerWords[0].code = EOF;
  wordIndex = 0;

  /* First test an empty parse tree */
  makeEOF(elementTable);
  element = matchEndOfSyntax(elementTable);
  ASSERT(element == NULL);

  /* Then one with a single EOS */
  makeParameterElement(&elementTable[0]);
  makeEOF(&elementTable[1]);

  element = matchParameterElement(elementTable);
  ASSERT(element != NULL);
  ASSERT(element->code == 0);

  /* Parameter entry at the end */
  makeEOS(&elementTable[0]);
  makeParameterElement(&elementTable[1]);
  makeEOF(&elementTable[2]);
  element = matchParameterElement(elementTable);
  ASSERT(element != NULL);
  ASSERT(element->code == 0);

  free(memory);
}

static void testMatchParseTree() {
  ElementEntry *element;
  ElementEntry *elementTable;
  Bool plural;
  ParamEntry parameters[10];

  memory = allocate(100*sizeof(Aword));
  elementTable = (ElementEntry *)&memory[50];

  /* Emulate end of player input */
  playerWords[0].code = EOF;
  wordIndex = 0;

  /* First test EOF with empty parse tree */
  makeEOF(elementTable);
  element = matchParseTree(NULL, elementTable, &plural);
  ASSERT(element == NULL);

  /* Test EOF with EOS */
  makeEOS(&elementTable[0]);
  makeEOF(&elementTable[1]);
  element = matchParseTree(NULL, elementTable, &plural);
  ASSERT(element == elementTable);

  /* Test EOF with word, EOS */
  makeWordElement(&elementTable[0], 1, 0);
  makeEOS(&elementTable[1]);
  makeEOF(&elementTable[2]);
  element = matchParseTree(NULL, elementTable, &plural);
  ASSERT(element == &elementTable[1]);

  /* Test word, EOF with word, EOS */
  dictionary = makeDictionary(100);
  makeDictionaryEntry(0, 1, PREPOSITION_BIT);
  playerWords[0].code = 0;		/* A preposition with code 0 */
  playerWords[1].code = EOF;
  makeWordElement(&elementTable[0], 1, addressOf(&elementTable[1]));
  makeEOS(&elementTable[1]);
  makeEOF(&elementTable[2]);
  element = matchParseTree(parameters, elementTable, &plural);
  ASSERT(element == &elementTable[1]);
  free(dictionary);
  free(memory);
}

static void testNoOfPronouns() {
  int i;

  dictionary = makeDictionary(30);

  for (i = 0; i < dictsize; i++)
    if (i%3 == 0)
      makeDictionaryEntry(i, dictsize+i, PRONOUN_BIT);
    else
      makeDictionaryEntry(i, dictsize+1, VERB_BIT);

  ASSERT(noOfPronouns() == 10);

  free(dictionary);
}


static void testSetupParameterForWord() {
  ACodeHeader acdHeader;
  header = &acdHeader;
  header->maxParameters = 10;
  dictionary = makeDictionary(20);
  memory = allocate(40*sizeof(Aword));

  makeDictionaryEntry(2, 23, VERB_BIT);
  memcpy(&memory[12], "qwerty", 7);
  dictionary[2].string = 12;

  playerWords[1].code = 2;
  litCount = 0;
  setupParameterForWord(1, 1);

  ASSERT(parameters[0].instance == instanceFromLiteral(1));
  ASSERT(parameters[1].instance == EOF);

  free(dictionary);
  free(memory);
}


void registerParseUnitTests()
{
  registerUnitTest(testSetupParameterForWord);
  registerUnitTest(testMatchEndOfSyntax);
  registerUnitTest(testMatchParameterElement);
  registerUnitTest(testMatchParseTree);
  registerUnitTest(testNoOfPronouns);
}
