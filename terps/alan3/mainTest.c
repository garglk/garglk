/*======================================================================*\

  mainTest.c

  Unit tests for main module in the Alan interpreter

\*======================================================================*/

#include "main.c"

static void testUpdateColumn() {
  ASSERT(updateColumn(0, "\n") == 1);
  ASSERT(updateColumn(11, "123456789") == 20);
}

static void testCopyAttributes() {
  AttributeEntry *attributeArea;
  AttributeEntry *a = (AttributeEntry *)&memory[1];

  memory = allocate(6*sizeof(AttributeEntry));
  a = (AttributeEntry *)&memory[1];

  header = allocate(sizeof(ACodeHeader));
  header->instanceMax = 2;
  admin = allocate(3*sizeof(AdminEntry));
  instance = allocate(3*sizeof(InstanceEntry));

  /* Create two attribute lists which consists of two attributes each,
     each is terminated by EOF */
  a[0].code = 13;
  a[0].value = 15;
  a[0].stringAddress = 17;
  a[1].code = 19;
  a[1].value = 21;
  a[1].stringAddress = 23;
  a[2].code = EOF;

  a[3].code = 130;
  a[3].value = 150;
  a[3].stringAddress = 170;
  a[4].code = 190;
  a[4].value = 210;
  a[4].stringAddress = 230;
  a[5].code = EOF;

  instance[1].initialAttributes = 1;
  instance[2].initialAttributes = 10;

  attributeArea = initializeAttributes(5*sizeof(AttributeEntry)/sizeof(Aword));

  ASSERT(admin[1].attributes[0].code == 13);
  ASSERT(admin[1].attributes[0].value == 15);
  ASSERT(admin[1].attributes[0].stringAddress == 17);
  ASSERT(admin[1].attributes[1].code == 19);
  ASSERT(admin[1].attributes[1].value == 21);
  ASSERT(admin[1].attributes[1].stringAddress == 23);
  ASSERT(*(Aword*)&admin[1].attributes[2] == EOF);

  ASSERT(admin[2].attributes[0].code == 130);
  ASSERT(admin[2].attributes[0].value == 150);
  ASSERT(admin[2].attributes[0].stringAddress == 170);
  ASSERT(admin[2].attributes[1].code == 190);
  ASSERT(admin[2].attributes[1].value == 210);
  ASSERT(admin[2].attributes[1].stringAddress == 230);
  ASSERT(*(Aword*)&admin[2].attributes[2] == EOF);
}

static void testPunctuationNext() {
  ASSERT(punctuationNext("."));
  ASSERT(punctuationNext("!"));
  ASSERT(punctuationNext("?"));
  ASSERT(punctuationNext(".$p"));
  ASSERT(punctuationNext(".$n"));
  ASSERT(punctuationNext(".$t"));
  ASSERT(!punctuationNext("a."));
  ASSERT(!punctuationNext("$p."));
}

static void testSpaceEquivalent(){
  ASSERT(isSpaceEquivalent("$p"));
  ASSERT(isSpaceEquivalent("$pafdjljf"));
  ASSERT(isSpaceEquivalent("$t"));
  ASSERT(isSpaceEquivalent("$n"));
  ASSERT(isSpaceEquivalent("$i"));
  ASSERT(!isSpaceEquivalent("$$"));
  ASSERT(!isSpaceEquivalent("abc"));
  ASSERT(!isSpaceEquivalent("..."));
  ASSERT(!isSpaceEquivalent(""));
  ASSERT(isSpaceEquivalent(" "));
}

void registerMainUnitTests()
{
  registerUnitTest(testSpaceEquivalent);
  registerUnitTest(testPunctuationNext);
  registerUnitTest(testUpdateColumn);
  registerUnitTest(testCopyAttributes);
}
