/*----------------------------------------------------------------------*\

  reverse.c

  Handles all reversing of data on little-endian machines, like PC:s

\*----------------------------------------------------------------------*/
#include "reverse.h"

/* IMPORTS */

#include "types.h"
#include "lists.h"
#include "checkentry.h"
#include "rules.h"
#include "msg.h"
#include "utils.h"
#include "compatibility.h"


extern Aword *memory;


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

static Aword *addressesDone = NULL;
static int numberDone = 0;
static int doneSize = 0;

static bool alreadyDone(Aaddr address)
{
  int i;
  bool found = FALSE;

  if (address == 0) return TRUE;

  /* Have we already done it? */
  for (i = 0; i < numberDone; i++)
    if (addressesDone[i] == address) {
      found = TRUE;
      break;
    }
  if (found) return TRUE;

  if (doneSize == numberDone) {
    doneSize += 100;
    addressesDone = realloc(addressesDone, doneSize*sizeof(Aword));
  }
  addressesDone[numberDone] = address;
  numberDone++;

  return FALSE;
}



#define NATIVE(w)   \
    ( (((Aword)((w)[3])      ) & 0x000000ff)    \
    | (((Aword)((w)[2]) <<  8) & 0x0000ff00)    \
    | (((Aword)((w)[1]) << 16) & 0x00ff0000)    \
    | (((Aword)((w)[0]) << 24) & 0xff000000))

/*----------------------------------------------------------------------*/
Aword reversed(Aword w) /* IN - The ACODE word to swap bytes of */
{
#ifdef TRYNATIVE
  return NATIVE(&w);
#else
  Aword s;                      /* The swapped ACODE word */
  char *wp, *sp;
  int i;

  wp = (char *) &w;
  sp = (char *) &s;

  for (i = 0; i < sizeof(Aword); i++)
    sp[sizeof(Aword)-1 - i] = wp[i];

  return s;
#endif
}


void reverse(Aword *w)          /* IN - The ACODE word to reverse bytes in */
{
  *w = reversed(*w);
}


static void reverseTable(Aword adr, int len)
{
  Aword *e = &memory[adr];
  int i;

  if (adr == 0) return;

  while (!isEndOfArray(e)) {
    if (len < sizeof(Aword)) {
      printf("***Wrong size in 'reverseTable()' ***");
      exit(-1);
    }
    for (i = 0; i < len/sizeof(Aword); i++) {
      reverse(e);
      e++;
    }
  }
}


static void reverseStms(Aword adr)
{
  Aword *e = &memory[adr];

  if (alreadyDone(adr)) return;

  while (TRUE) {
    reverse(e);
    if (*e == ((Aword)C_STMOP<<28|(Aword)I_RETURN)) break;
    e++;
  }
}


static void reverseMsgs(Aword adr)
{
  MessageEntry *e = (MessageEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(MessageEntry));
    while (!isEndOfArray(e)) {
      reverseStms(e->stms);
      e++;
    }
  }
}


static void reverseDictionary(Aword adr)
{
    DictionaryEntry *e = (DictionaryEntry *) &memory[adr];

    if (alreadyDone(adr)) return;

    if (!isEndOfArray(e)) {
        reverseTable(adr, sizeof(DictionaryEntry));
        while (!isEndOfArray(e)) {
            if ((e->classBits & SYNONYM_BIT) == 0) { /* Do not do this for synonyms */
                reverseTable(e->adjectiveRefs, sizeof(Aword));
                reverseTable(e->nounRefs, sizeof(Aword));
                reverseTable(e->pronounRefs, sizeof(Aword));
            }
            e++;
        }
    }
}


static void reverseChks(Aword adr)
{
  CheckEntry *e = (CheckEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(CheckEntry));
    while (!isEndOfArray(e)) {
      reverseStms(e->exp);
      reverseStms(e->stms);
      e++;
    }
  }
}


static void reverseAlts(Aword adr)
{
  AltEntry *e = (AltEntry *)&memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(AltEntry));
    while (!isEndOfArray(e)) {
      reverseChks(e->checks);
      reverseStms(e->action);
      e++;
    }
  }
}


static void reverseVerbs(Aword adr)
{
  VerbEntry *e = (VerbEntry *)&memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(VerbEntry));
    while (!isEndOfArray(e)) {
      reverseAlts(e->alts);
      e++;
    }
  }
}


static void reverseSteps(Aword adr)
{
  StepEntry *e = (StepEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(StepEntry));
    while (!isEndOfArray(e)) {
      reverseStms(e->after);
      reverseStms(e->exp);
      reverseStms(e->stms);
      e++;
    }
  }
}


static void reverseScrs(Aword adr)
{
  ScriptEntry *e = (ScriptEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(ScriptEntry));
    while (!isEndOfArray(e)) {
      reverseStms(e->description);
      reverseSteps(e->steps);
      e++;
    }
  }
}


static void reverseExits(Aword adr)
{
  ExitEntry *e = (ExitEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(ExitEntry));
    while (!isEndOfArray(e)) {
      reverseChks(e->checks);
      reverseStms(e->action);
      e++;
    }
  }
}


static void reverseClasses(Aword adr)
{
  ClassEntry *e = (ClassEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(ClassEntry));
    while (!isEndOfArray(e)) {
      reverseStms(e->name);
      reverseStms(e->initialize);
      reverseChks(e->descriptionChecks);
      reverseStms(e->description);
      reverseStms(e->entered);
      reverseStms(e->definite.address);
      reverseStms(e->indefinite.address);
      reverseStms(e->negative.address);
      reverseStms(e->mentioned);
      reverseVerbs(e->verbs);
      e++;
    }
  }
}


static void reverseInstances(Aword adr)
{
  InstanceEntry *e = (InstanceEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(InstanceEntry));
    while (!isEndOfArray(e)) {
      reverseStms(e->name);
      reverseTable(e->initialAttributes, sizeof(AttributeHeaderEntry));
      reverseStms(e->initialize);
      reverseStms(e->definite.address);
      reverseStms(e->indefinite.address);
      reverseStms(e->negative.address);
      reverseStms(e->mentioned);
      reverseChks(e->checks);
      reverseStms(e->description);
      reverseVerbs(e->verbs);
      reverseStms(e->entered);
      reverseExits(e->exits);
      e++;
    }
  }
}


static void reverseRestrictions(Aword adr)
{
  RestrictionEntry *e = (RestrictionEntry *) &memory[adr];

  if (alreadyDone(adr)) return;
  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(RestrictionEntry));
    while (!isEndOfArray(e)) {
      reverseStms(e->stms);
      e++;
    }
  }
}


static void reverseElms(Aword adr)
{
  ElementEntry *e = (ElementEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(ElementEntry));
    while (!isEndOfArray(e)) {
      if (e->code == EOS) reverseRestrictions(e->next);
      else reverseElms(e->next);
      e++;
    }
  }
}


static void reverseSyntaxTableCurrent(Aword adr) {
    SyntaxEntry *e = (SyntaxEntry *) &memory[adr];

    if (!isEndOfArray(e)) {
        reverseTable(adr, sizeof(SyntaxEntry));
        while (!isEndOfArray(e)) {
            reverseElms(e->elms);
            reverseTable(e->parameterNameTable, sizeof(Aaddr));
            e++;
        }
    }
}


static void reverseSyntaxTablePreBeta2(Aword adr) {
    SyntaxEntryPreBeta2 *e = (SyntaxEntryPreBeta2 *) &memory[adr];

    if (!isEndOfArray(e)) {
        reverseTable(adr, sizeof(SyntaxEntryPreBeta2));
        while (!isEndOfArray(e)) {
            reverseElms(e->elms);
            e++;
        }
    }
}


static void reverseSyntaxTable(Aword adr, char version[])
{
  if (alreadyDone(adr)) return;

  if (isPreBeta2(version))
      reverseSyntaxTablePreBeta2(adr);
  else
      reverseSyntaxTableCurrent(adr);
}


static void reverseParameterTable(Aword adr)
{
  ParameterMapEntry *e = (ParameterMapEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(ParameterMapEntry));
    while (!isEndOfArray(e)) {
      reverseTable(e->parameterMapping, sizeof(Aword));
      e++;
    }
  }
}


static void reverseEvts(Aword adr)
{
  EventEntry *e = (EventEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(EventEntry));
    while (!isEndOfArray(e)) {
      reverseStms(e->code);
      e++;
    }
  }
}


static void reverseLims(Aword adr)
{
  LimitEntry *e = (LimitEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(LimitEntry));
    while (!isEndOfArray(e)) {
      reverseStms(e->stms);
      e++;
    }
  }
}


static void reverseContainers(Aword adr)
{
  ContainerEntry *e = (ContainerEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(ContainerEntry));
    while (!isEndOfArray(e)) {
      reverseLims(e->limits);
      reverseStms(e->header);
      reverseStms(e->empty);
      reverseChks(e->extractChecks);
      reverseStms(e->extractStatements);
      e++;
    }
  }
}


static void reverseRuls(Aword adr)
{
  RuleEntry *e = (RuleEntry *) &memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(RuleEntry));
    while (!isEndOfArray(e)) {
      reverseStms(e->exp);
      reverseStms(e->stms);
      e++;
    }
  }
}


static void reverseSetInitTable(Aaddr adr)
{
  SetInitEntry *e = (SetInitEntry *)&memory[adr];

  if (alreadyDone(adr)) return;

  if (!isEndOfArray(e)) {
    reverseTable(adr, sizeof(SetInitEntry));
    while (!isEndOfArray(e)) {
      reverseTable(e->setAddress, sizeof(Aword));
      e++;
    }
  }
}



/*----------------------------------------------------------------------*/
static void reversePreAlpha5Header(Pre3_0alpha5Header *hdr)
{
  int i;

  /* Reverse all words in the header except the tag */
  for (i = 1; i < sizeof(*hdr)/sizeof(Aword); i++)
    reverse(&((Aword *)hdr)[i]);
}


/*----------------------------------------------------------------------*/
static void reversePreAlpha5() {
    /* NOTE that the reversePreXXX() have different header definitions */
    Pre3_0alpha5Header *header = (Pre3_0alpha5Header *)memory;

    reversePreAlpha5Header(header);

    reverseDictionary(header->dictionary);
    reverseSyntaxTable(header->syntaxTableAddress, header->version);
    reverseParameterTable(header->parameterMapAddress);
    reverseVerbs(header->verbTableAddress);
    reverseClasses(header->classTableAddress);
    reverseInstances(header->instanceTableAddress);
    reverseScrs(header->scriptTableAddress);
    reverseContainers(header->containerTableAddress);
    reverseEvts(header->eventTableAddress);
    reverseRuls(header->ruleTableAddress);
    reverseTable(header->stringInitTable, sizeof(StringInitEntry));
    reverseSetInitTable(header->setInitTable);
    reverseTable(header->sourceFileTable, sizeof(SourceFileEntry));
    reverseTable(header->sourceLineTable, sizeof(SourceLineEntry));
    reverseStms(header->start);
    reverseMsgs(header->messageTableAddress);

    reverseTable(header->scores, sizeof(Aword));
    reverseTable(header->freq, sizeof(Aword));
}


/*----------------------------------------------------------------------*/
static void reversePreBeta2Header(Pre3_0beta2Header *hdr)
{
  int i;

  /* Reverse all words in the header except the tag */
  for (i = 1; i < sizeof(*hdr)/sizeof(Aword); i++)
    reverse(&((Aword *)hdr)[i]);
}


/*----------------------------------------------------------------------*/
static void reversePreBeta2() {
    /* NOTE that the reversePreXXX() have different header definitions */
    Pre3_0beta2Header *header = (Pre3_0beta2Header *)memory;

    reversePreBeta2Header(header);

    reverseDictionary(header->dictionary);
    reverseSyntaxTable(header->syntaxTableAddress, header->version);
    reverseParameterTable(header->parameterMapAddress);
    reverseVerbs(header->verbTableAddress);
    reverseClasses(header->classTableAddress);
    reverseInstances(header->instanceTableAddress);
    reverseScrs(header->scriptTableAddress);
    reverseContainers(header->containerTableAddress);
    reverseEvts(header->eventTableAddress);
    reverseRuls(header->ruleTableAddress);
    reverseTable(header->stringInitTable, sizeof(StringInitEntry));
    reverseSetInitTable(header->setInitTable);
    reverseTable(header->sourceFileTable, sizeof(SourceFileEntry));
    reverseTable(header->sourceLineTable, sizeof(SourceLineEntry));
    reverseStms(header->start);
    reverseMsgs(header->messageTableAddress);

    reverseTable(header->scores, sizeof(Aword));
    reverseTable(header->freq, sizeof(Aword));
}


/*======================================================================*/
void reverseHdr(ACodeHeader *hdr)
{
  int i;

  /* Reverse all words in the header except the tag and the version marking */
  for (i = 1; i < sizeof(*hdr)/sizeof(Aword); i++)
    reverse(&((Aword *)hdr)[i]);
}


/*----------------------------------------------------------------------*/
static void reverseNative() {
    /* NOTE that the reversePreXXX() have different header definitions */
    ACodeHeader *header = (ACodeHeader *)memory;

    reverseHdr(header);

    reverseDictionary(header->dictionary);
    reverseSyntaxTable(header->syntaxTableAddress, header->version);
    reverseParameterTable(header->parameterMapAddress);
    reverseVerbs(header->verbTableAddress);
    reverseClasses(header->classTableAddress);
    reverseInstances(header->instanceTableAddress);
    reverseScrs(header->scriptTableAddress);
    reverseContainers(header->containerTableAddress);
    reverseEvts(header->eventTableAddress);
    reverseRuls(header->ruleTableAddress);
    reverseTable(header->stringInitTable, sizeof(StringInitEntry));
    reverseSetInitTable(header->setInitTable);
    reverseTable(header->sourceFileTable, sizeof(SourceFileEntry));
    reverseTable(header->sourceLineTable, sizeof(SourceLineEntry));
    reverseStms(header->prompt);
    reverseStms(header->start);
    reverseMsgs(header->messageTableAddress);

    reverseTable(header->scores, sizeof(Aword));
    reverseTable(header->freq, sizeof(Aword));
}


/*======================================================================

  reverseACD()

  Traverse all the data structures and reverse all integers.
  Only performed in architectures with reversed byte ordering, which
  makes the .ACD files fully compatible across architectures

  */
void reverseACD(void)
{
  ACodeHeader *header = (ACodeHeader *)memory;
  char version[4];
  int i;

  /* Make a copy of the version marking to reverse */
  for (i = 0; i <= 3; i++)
      version[i] = header->version[i];
  reverse((Aword*)&version);

  if (isPreAlpha5(version))
      reversePreAlpha5();
  else if (isPreBeta2(version))
      reversePreBeta2();
  else
      reverseNative();

  free(addressesDone);
}
