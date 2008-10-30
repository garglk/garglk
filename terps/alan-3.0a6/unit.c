/*======================================================================*\

  unit.c

  A unit test main program for the Alan interpreter

\*======================================================================*/

#include "sysdep.h"
#include "acode.h"
#include "reverse.h"

#include <setjmp.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

typedef struct Case {
  void (*theCase)();
  struct Case *next;
} Case;

static Case *caseList = NULL;
static Case *lastCase = NULL;


static void registerUnitTest(void (*aCase)());


Aword convertFromACD(Aword w)
{
  Aword s;                      /* The swapped ACODE word */
  char *wp, *sp;
  int i;
  
  wp = (char *) &w;
  sp = (char *) &s;

  for (i = 0; i < sizeof(Aword); i++)
    sp[sizeof(Aword)-1 - i] = wp[i];

  return s;
}


#include "syserr.h"

static Bool hadSyserr = FALSE;
jmp_buf syserr_label;
jmp_buf uniterr_label;
Bool expectSyserr = FALSE;

void syserr(char msg[])
{
  if (!expectSyserr) {
    printf("*** SYSTEM ERROR: %s ***\n", msg);
    longjmp(uniterr_label, TRUE);
  } else {
    hadSyserr = TRUE;
    longjmp(syserr_label, TRUE);
  }
}

void apperr(char msg[]) {
  syserr(msg);
}



#define RUNNING_UNITTESTS

void unitAssert(int x, char sourceFile[], int lineNumber);
#define ASSERT(x) (unitAssert((x), __FILE__, __LINE__))

#include "exeTest.c"
#include "saveTest.c"
#include "stateTest.c"
#include "parseTest.c"
#include "stackTest.c"
#include "interTest.c"
#include "reverseTest.c"
#include "sysdepTest.c"
#include "setTest.c"
#include "mainTest.c"

#include <stdio.h>

static int passed = 0;
static int failed = 0;

static void unitFail(char sourceFile[], int lineNumber)
{
  printf("%s:%d: unit test failed!\n", sourceFile, lineNumber);
  failed++;
}


static void unitReportProgress(failed, passed)
{
  return;
  printf("failed: %d, passed: %d\n", failed, passed);
}


/* Assert a particular test */
void unitAssert(int x, char sourceFile[], int lineNumber)
{
  (x)? passed++ : unitFail(sourceFile, lineNumber);
  unitReportProgress(failed, passed);
}


/* Run the tests in the test case array*/
static void unitTest(void)
{
  Case *current;
  int casesRun = 0;

  for (current = caseList; current != NULL; current = current->next) {
    (*current->theCase)();
#ifdef DMALLOC
    dmalloc_verify(0);
#endif
    casesRun++;
  }
  if (failed == 0)
    printf("All %d unit tests PASSED!!\n", passed);
  else {
    printf("******************************\n");
    printf("%d of %d unit tests FAILED!!\n", failed, passed+failed);
    printf("******************************\n");
  }
}


int main()
{
#ifdef DMALLOC
  /*
   * Get environ variable DMALLOC_OPTIONS and pass the settings string
   * on to dmalloc_debug_setup to setup the dmalloc debugging flags.
   */
  dmalloc_debug_setup(getenv("DMALLOC_OPTIONS"));
  printf("DMALLOC_OPTIONS=%s\n", getenv("DMALLOC_OPTIONS"));
#endif

  if (setjmp(uniterr_label) == 0) {
    registerExeUnitTests();
    registerSaveUnitTests();
    registerStateUnitTests();
    registerParseUnitTests();
    registerStackUnitTests();
    registerInterUnitTests();
    registerReverseUnitTests();
    registerSysdepUnitTests();
    registerSetUnitTests();
    registerMainUnitTests();

    unitTest();
  }
  return 0;
}

void registerUnitTest(void (*aCase)())
{
  if (lastCase == NULL) {
    caseList = calloc(sizeof(Case), 1);
    caseList->theCase = aCase;
    lastCase = caseList;
  } else {
    lastCase->next = calloc(sizeof(Case), 1);
    lastCase = lastCase->next;
    lastCase->theCase = aCase;
  }
  lastCase->next = NULL;
}


#ifdef NEED_LOADACD
static void loadACD(char fileName[])
{
  ACodeHeader tmphdr;
  FILE *acdFile = fopen(fileName, "r");

  fread(&tmphdr, sizeof(tmphdr), 1, acdFile);

  if (littleEndian())
    reverseHdr(&tmphdr);

  memory = calloc(4*tmphdr.size, 1);

  rewind(acdFile);
  fread(memory, sizeof(Aword), tmphdr.size, acdFile);

}
#endif

