/*----------------------------------------------------------------------*\

  parse.c

  Command line parser unit for Alan interpreter ARUN

\*----------------------------------------------------------------------*/

#include <stdio.h>
#include <ctype.h>

#include "types.h"

#ifdef USE_READLINE
#include "readline.h"
#endif

#include "main.h"
#include "inter.h"
#include "exe.h"
#include "state.h"
#include "act.h"
#include "term.h"
#include "debug.h"
#include "params.h"
#include "syserr.h"
#include "parse.h"

#ifdef HAVE_GLK
#include "glkio.h"
#endif


#define LISTLEN 100
/* PUBLIC DATA */

/* List of parsed words, index into dictionary */
WordEntry playerWords[LISTLEN/2] = {{EOF, NULL, NULL}};
int wordIndex;			/* An index into it that list */
int firstWord;			/* The first word for this command */
int lastWord;			/* ... and the last. */

Bool plural = FALSE;


/* Syntax Parameters */
int paramidx;			/* Index in params */
ParamEntry *parameters;		/* List of params */
static ParamEntry *previousMultipleList;	/* Previous multiple list */

/* Literals */
LiteralEntry literal[MAXPARAMS+1];
int litCount;

/* What did the user say? */
int verbWord;			/* The word he used, dictionary index */
int verbWordCode;		/* The code for that verb */


/*======================================================================*/
void forceNewPlayerInput() {
  playerWords[wordIndex].code = EOF;		/* Force new player input */
}


/*----------------------------------------------------------------------*\

  SCAN DATA & PROCEDURES

  All procedures for getting a command and turning it into a list of
  dictionary entries are placed here.

\*----------------------------------------------------------------------*/

/* PRIVATE DATA */

static char buf[LISTLEN+1];	/* The input buffer */
static char isobuf[LISTLEN+1];	/* The input buffer in ISO */
static Bool eol = TRUE;		/* Looking at End of line? Yes, initially */
static char *token;



/*----------------------------------------------------------------------*/
static void unknown(char token[]) {
  char *str = strdup(token);

#if ISO == 0
  fromIso(str, str);
#endif
  setupParameterForString(1, str);
  printMessage(M_UNKNOWN_WORD);
  free(str);
  error(MSGMAX);		/* Error return without any output */
}


/*----------------------------------------------------------------------*/
static int lookup(char wrd[]) {
  int i;

  for (i = 0; !endOfTable(&dictionary[i]); i++) {
    if (compareStrings(wrd, (char *) pointerTo(dictionary[i].string)) == 0)
      return (i);
  }
  unknown(wrd);
  return(EOF);
}


/*----------------------------------------------------------------------*/
static int number(char token[]) {
  int i;

  sscanf(token, "%d", &i);
  return i;
}


/*----------------------------------------------------------------------*/
static Bool isWordCharacter(int ch) {
  return isISOLetter(ch)||isdigit(ch)||ch=='\''||ch=='-'||ch=='_';
}


/*----------------------------------------------------------------------*/
static char *gettoken(char *buf) {
  static char *marker;
  static char oldch;

  if (buf == NULL)
    *marker = oldch;
  else
    marker = buf;
  while (*marker != '\0' && isSpace(*marker) && *marker != '\n') marker++;
  buf = marker;
  if (isISOLetter(*marker))
    while (*marker&&isWordCharacter(*marker)) marker++;
  else if (isdigit(*marker))
    while (isdigit(*marker)) marker++;
  else if (*marker == '\"') {
    marker++;
    while (*marker != '\"') marker++;
    marker++;
  } else if (*marker == '\0' || *marker == '\n' || *marker == ';')
    return NULL;
  else
    marker++;
  oldch = *marker;
  *marker = '\0';
  return buf;
}

/*----------------------------------------------------------------------*/
static void getLine(void)
{
  para();
  do {
#if defined(HAVE_ANSI) || defined(HAVE_GLK)
    statusline();
#endif
    printAndLog("> ");
#ifdef USE_READLINE
    if (!readline(buf)) {
      newline();
      quitGame();
    }
#else
    if (fgets(buf, LISTLEN, stdin) == NULL) {
      newline();
      quitGame();
    }
#endif
    getPageSize();
    anyOutput = FALSE;
    if (transcriptOption || logOption) {
#ifdef HAVE_GLK
      glk_put_string_stream(logFile, buf);
      glk_put_char_stream(logFile, '\n');
#else
#ifdef __amiga__
      fprintf(logFile, "%s", buf);
#else
      fprintf(logFile, "%s\n", buf);
#endif
#endif
    }
    /* If the player input an empty command he forfeited his command */
    if (strlen(buf) == 0) {
      playerWords[0].code = EOF;
      longjmp(forfeitLabel, 0);
    }

#if ISO == 0
    toIso(isobuf, buf, NATIVECHARSET);
#else
    strcpy(isobuf, buf);
#endif
    token = gettoken(isobuf);
    if (token != NULL) {
      if (strcmp("debug", token) == 0 && header->debug) {
	debugOption = TRUE;
	debug(FALSE, 0, 0);
	token = NULL;
      } else if (strcmp("undo", token) == 0) {
	token = gettoken(NULL);
	if (token != NULL) /* More tokens? */
	  error(M_WHAT);
	undo();
      }
    }
  } while (token == NULL);
  eol = FALSE;
}


/*======================================================================*/
int literalFromInstance(Aint instance) {
  return instance-header->instanceMax;
}

/*======================================================================*/
Aint instanceFromLiteral(int literalIndex) {
  return literalIndex+header->instanceMax;
}

/*----------------------------------------------------------------------*/
static void createIntegerLiteral(int integerValue) {
  litCount++;
  if (litCount > MAXPARAMS)
    syserr("Too many player command parameters.");

  literal[litCount].class = header->integerClassId;
  literal[litCount].type = NUMERIC_LITERAL;
  literal[litCount].value = integerValue;
}


/*----------------------------------------------------------------------*/
static void createStringLiteral(char *unquotedString) {
  litCount++;
  if (litCount > MAXPARAMS)
    syserr("Too many player command parameters.");
  literal[litCount].class = header->stringClassId;
  literal[litCount].type = STRING_LITERAL;
  literal[litCount].value = (Aptr) strdup(unquotedString);
}

static Bool continued = FALSE;
/*----------------------------------------------------------------------*/
static void scan(void)
{
  int i;
  int w;

  if (continued) {
    /* Player used '.' to separate commands. Read next */
    para();
    token = gettoken(NULL);
    if (token == NULL)
      getLine();
    continued = FALSE;
  } else
    getLine();

  playerWords[0].code = 0;
  for (i = 0; i < litCount; i++)
    if (literal[i].type == STRING_LITERAL && literal[i].value != 0)
      free((char *) literal[i].value);
  i = 0;
  litCount = 0;
  do {
    playerWords[i].start = token;
    playerWords[i].end = strchr(token, '\0');
    if (isISOLetter(token[0])) {
      w = lookup(token);
      if (!isNoise(w))
	playerWords[i++].code = w;
    } else if (isdigit(token[0]) || token[0] == '\"') {
      if (isdigit(token[0])) {
	createIntegerLiteral(number(token));
      } else {
	char *unquotedString = strdup(token);
	unquotedString[strlen(token)-1] = '\0';
	createStringLiteral(&unquotedString[1]);
	free(unquotedString);
      }
      playerWords[i++].code = dictsize+litCount; /* Word outside dictionary = literal */
    } else if (token[0] == ',') {
      playerWords[i++].code = conjWord;
    } else if (token[0] == '.') {
      continued = TRUE;
      playerWords[i].code = EOF;
      eol = TRUE;
      break;
    } else
      unknown(token);
    playerWords[i].code = EOF;
    eol = (token = gettoken(NULL)) == NULL;
  } while (!eol);
}


/*----------------------------------------------------------------------*\

  Parameter allocation, saving and restoring

\*----------------------------------------------------------------------*/


/*======================================================================*/
void allocateParameters(ParamEntry **area) {
  if (*area == NULL) {
    *area = (ParamEntry *) allocate(sizeof(ParamEntry)*(header->instanceMax+1));
    (*area)->instance = EOF;
  }
}

static ParamEntry savedParameters[3];

/*----------------------------------------------------------------------*/
static void setupParameterForWord(int parameter, int playerWordIndex) {

  /* Trick message handling to output the word, create a string literal */
  createStringLiteral(pointerTo(dictionary[playerWords[playerWordIndex].code].string));

  allocateParameters(&parameters);

  parameters[parameter-1].instance = instanceFromLiteral(litCount);	/* A faked literal */
  parameters[parameter-1].useWords = TRUE;
  parameters[parameter-1].firstWord = parameters[parameter-1].lastWord = playerWordIndex;
  parameters[parameter].instance = EOF;
}


/*======================================================================*/
void setupParameterForInstance(int parameter, Aint instance) {

  if (parameter > 2)
    syserr("Saving more parameters than expected");

  allocateParameters(&parameters);

  savedParameters[parameter-1] = parameters[parameter-1];
  savedParameters[parameter].instance = EOF;

  parameters[parameter-1].instance = instance;
  parameters[parameter-1].useWords = FALSE;
  parameters[parameter].instance = EOF;
}


/*======================================================================*/
void setupParameterForInteger(int parameter, Aint value) {

  if (parameter > 2)
    syserr("Saving more parameters than expected");

  allocateParameters(&parameters);

  savedParameters[parameter-1] = parameters[parameter-1];
  savedParameters[parameter].instance = EOF;

  createIntegerLiteral(value);
  parameters[parameter-1].instance = instanceFromLiteral(litCount);
  parameters[parameter-1].useWords = FALSE;
  parameters[parameter].instance = EOF;
}


/*======================================================================*/
void setupParameterForString(int parameter, char *value) {

  if (parameter > 2)
    syserr("Saving more parameters than expected");

  allocateParameters(&parameters);

  savedParameters[parameter-1] = parameters[parameter-1];
  savedParameters[parameter].instance = EOF;

  createStringLiteral(value);
  parameters[parameter-1].instance = instanceFromLiteral(litCount);
  parameters[parameter-1].useWords = FALSE;
  parameters[parameter].instance = EOF;
}


/*======================================================================*/
void restoreParameters() {
  int p = 0;

  while (savedParameters[p].instance != EOF) {
    parameters[p] = savedParameters[p];
    p++;
  }
  parameters[p] = savedParameters[p];
}



/*----------------------------------------------------------------------*\

  PARSE DATA & PROCEDURES

  All procedures and data for getting a command and parsing it

\*---------------------------------------------------------------------- */


/* Private Types */

typedef struct PronounEntry {	/* To remember parameter/pronoun relations */
  int pronoun;
  int instance;
} PronounEntry;

static PronounEntry *pronounList = NULL;
static int allWordIndex;	/* Word index of the ALL_WORD found */
static int butWordIndex;	/* Word index of the BUT_WORD found */
static int allLength;		/* No. of objects matching 'all' */


/*----------------------------------------------------------------------*/
static void nonverb(void) {
  if (isDir(playerWords[wordIndex].code)) {
    wordIndex++;
    if (playerWords[wordIndex].code != EOF && !isConj(playerWords[wordIndex].code))
      error(M_WHAT);
    else
      go(dictionary[playerWords[wordIndex-1].code].code);
    if (playerWords[wordIndex].code != EOF)
      wordIndex++;
  } else
    error(M_WHAT);
}


/*----------------------------------------------------------------------*/
static void errorWhich(ParamEntry alternative[]) {
  int p;			/* Index into the list of alternatives */

  parameters[1].instance = EOF;
  parameters[0] = alternative[0];
  printMessage(M_WHICH_ONE_START);
  for (p = 1; !endOfTable(&alternative[p+1]); p++) {
    parameters[0] = alternative[p];
    printMessage(M_WHICH_ONE_COMMA);
  }
  parameters[0] = alternative[p];
  printMessage(M_WHICH_ONE_OR);
  error(MSGMAX);		/* Return with empty error message */
}


/*----------------------------------------------------------------------*/
static void errorWhichPronoun(int pronounWordIndex, ParamEntry alternative[]) {
  int p;			/* Index into the list of alternatives */

  setupParameterForWord(1, pronounWordIndex);
  printMessage(M_WHICH_PRONOUN_START);

  parameters[1].instance = EOF;
  parameters[0] = alternative[0];
  printMessage(M_WHICH_PRONOUN_FIRST);

  for (p = 1; !endOfTable(&alternative[p+1]); p++) {
    parameters[0] = alternative[p];
    printMessage(M_WHICH_ONE_COMMA);
  }
  parameters[0] = alternative[p];
  printMessage(M_WHICH_ONE_OR);
  error(MSGMAX);		/* Return with empty error message */
}


/*----------------------------------------------------------------------*/
static void errorWhat(int playerWordIndex) {
  setupParameterForWord(1, playerWordIndex);
  error(M_WHAT_WORD);
}


/*----------------------------------------------------------------------*/
static void errorAfterBut() {
  setupParameterForWord(1, butWordIndex);
  error(M_AFTER_BUT);
}

/*----------------------------------------------------------------------*/
static int fakePlayerWordForAll() {
  /* Look through the dictionary and find any ALL_WORD, then add a
     player word so that it can be used in the message */
  int p, d;

  for (p = 0; playerWords[p].code != EOF; p++);
  playerWords[p+1].code = EOF;	/* Make room for one more word */
  allWordIndex = p;
  for (d = 0; d < dictsize; d++)
    if (isAll(d)) {
      playerWords[p].code = d;
      return p;
    }
  syserr("No ALLWORD found");
  return 0;
}

/*----------------------------------------------------------------------*/
static void errorButAfterAll(int butWordIndex) {
  setupParameterForWord(1, butWordIndex);
  setupParameterForWord(2, fakePlayerWordForAll());
  error(M_BUT_ALL);
}


/*----------------------------------------------------------------------*/
static Aint findInstanceForNoun(int wordIndex) {
  /* Assume the last word used is the noun, then find any instance
     with this noun */
  DictionaryEntry *d = &dictionary[wordIndex];
  if (d->nounRefs == 0 || d->nounRefs == EOF)
    syserr("No references for noun");
  return *(Aint*)pointerTo(d->nounRefs);
}


/*----------------------------------------------------------------------*/
static void errorNoSuch(ParamEntry parameter) {
  parameters[0] = parameter;
  if (parameters[0].instance == 0)
    parameters[0].instance = findInstanceForNoun(playerWords[parameter.lastWord].code);
  parameters[0].useWords = TRUE; /* Indicate to use words and not names */
  parameters[1].instance = EOF;
  error(M_NO_SUCH);
}


/*----------------------------------------------------------------------*/
static void buildAll(ParamEntry list[]) {
  int o, i = 0;
  Bool found = FALSE;
  
  for (o = 1; o <= header->instanceMax; o++)
    if (isHere(o, FALSE)) {
      found = TRUE;
      list[i].instance = o;
      list[i++].firstWord = EOF;
    }
  if (!found)
    errorWhat(wordIndex);
  else
    list[i].instance = EOF;
  allWordIndex = wordIndex;
}


/*----------------------------------------------------------------------*/
static int getPronounInstances(int word, ParamEntry instances[]) {
  /* Find the instance that the pronoun word could refer to, return 0
     if none or multiple */
  int p;
  int i = 0;

  instances[0].instance = EOF;
  for (p = 0; pronounList[p].instance != EOF; p++)
    if (dictionary[word].code == pronounList[p].pronoun) {
      instances[i].instance = pronounList[p].instance;
      instances[i].useWords = FALSE; /* Can't use words since they are gone, pronouns
					refer to parameters in previous command */
      instances[++i].instance = EOF;
    }
  return i;
}


/*----------------------------------------------------------------------*/
static Bool inOpaqueContainer(int originalInstance)
{
  int instance = admin[originalInstance].location;

  while (isContainer(instance)) {
    if (attributeOf(instance, OPAQUEATTRIBUTE))
      return TRUE;
    instance = admin[instance].location;
  }
  return FALSE;
}


/*----------------------------------------------------------------------*/
static Bool reachable(int instance)
{
  if (isA(instance, THING) || isA(instance, LOCATION))
    return isHere(instance, FALSE) && !inOpaqueContainer(instance);
  else
    return TRUE;
}
    
	
/*----------------------------------------------------------------------*/
static void resolve(ParamEntry plst[])
{
  /* In case the syntax did not indicate omnipotent powers (allowed
     access to remote object), we need to remove non-present
     parameters */

  int i;

  if (allLength > 0) return;	/* ALL has already done this */
  /* TODO: NO IT HASN'T ALWAYS SINCE THIS CAN BE ANOTHER PARAMETER!!! */

  /* Resolve ambiguities by presence */
  for (i=0; plst[i].instance != EOF; i++) {
    if (isLiteral(plst[i].instance))	/* Literals are always 'here' */
      continue;
    if (instance[plst[i].instance].parent == header->entityClassId)
      /* and so are pure entities */
      continue;
    if (!reachable(plst[i].instance)) {
      errorNoSuch(plst[i]);
    }
  }
}


/*----------------------------------------------------------------------*/
static void unambig(ParamEntry plst[])
{
  int i;
  Bool foundNoun = FALSE;	/* Adjective or noun found ? */
  static ParamEntry *refs;	/* Entities referenced by word */
  static ParamEntry *savlst;	/* Saved list for backup at EOF */
  int firstWord, lastWord;	/* The words the player used */

  if (refs == NULL)
    refs = (ParamEntry *)allocate((MAXENTITY+1)*sizeof(ParamEntry));

  if (savlst == NULL)
    savlst = (ParamEntry *)allocate((MAXENTITY+1)*sizeof(ParamEntry));

  if (isLiteralWord(playerWords[wordIndex].code)) {
    /* Transform the word into a reference to the literal value */
    /* words > dictsize are literals with index = word-dictsize */
    plst[0].instance = (playerWords[wordIndex].code-dictsize) + header->instanceMax;
    plst[0].firstWord = EOF;	/* No words used! */
    plst[1].instance = EOF;
    wordIndex++;
    return;
  }

  plst[0].instance = EOF;		/* Make an empty parameter list */
  if (isPronoun(playerWords[wordIndex].code)) {
    ParameterList pronounInstances;
    int pronounMatches = getPronounInstances(playerWords[wordIndex].code, pronounInstances);
    if (pronounMatches == 0)
      errorWhat(wordIndex);
    else if (pronounMatches > 1) {
      /* Set up parameters for error message... */
      parameters[0].instance = 0; /* Just make it anything != EOF */
      parameters[1].instance = EOF; /* Terminate list after one */
      errorWhichPronoun(wordIndex, pronounInstances);
    }
    wordIndex++;		/* Consume the pronoun */
    plst[0].instance = pronounInstances[0].instance;
    plst[0].firstWord = EOF;	/* No words used! */
    plst[1].instance = EOF;
    return;
  }

  firstWord = wordIndex;
  while (playerWords[wordIndex].code != EOF && isAdjective(playerWords[wordIndex].code)) {
    /* If this word can be a noun and there is no noun following break loop */
    if (isNoun(playerWords[wordIndex].code) && (playerWords[wordIndex+1].code == EOF || !isNoun(playerWords[wordIndex+1].code)))
      break;
    copyReferences(refs, (Aword *)pointerTo(dictionary[playerWords[wordIndex].code].adjectiveRefs));
    copyParameterList(savlst, plst);	/* To save it for backtracking */
    if (foundNoun)
      intersect(plst, refs);
    else {
      copyParameterList(plst, refs);
      foundNoun = TRUE;
    }
    wordIndex++;
  }
  if (playerWords[wordIndex].code != EOF) {
    if (isNoun(playerWords[wordIndex].code)) {
      copyReferences(refs, (Aword *)pointerTo(dictionary[playerWords[wordIndex].code].nounRefs));
      if (foundNoun)
	intersect(plst, refs);
      else {
	copyParameterList(plst, refs);
	foundNoun = TRUE;
      }
      wordIndex++;
    } else
      error(M_NOUN);
  } else if (foundNoun) {
    if (isNoun(playerWords[wordIndex-1].code)) {
      /* Perhaps the last word was also a noun? */
      copyParameterList(plst, savlst);	/* Restore to before last adjective */
      copyReferences(refs, (Aword *)pointerTo(dictionary[playerWords[wordIndex-1].code].nounRefs));
      if (plst[0].instance == EOF)
	copyParameterList(plst, refs);
      else
	intersect(plst, refs);
    } else
      error(M_NOUN);
  }
  lastWord = wordIndex-1;

  /* Allow remote objects, but resolve ambiguities by reachability */
  if (listLength(plst) > 1) {
    for (i=0; plst[i].instance != EOF; i++)
      if (!reachable(plst[i].instance))
	plst[i].instance = 0;
    compact(plst);
  }
#ifdef DISAMBIGUATE_USING_CHECKS
  if (listLength(plst) > 1)
    disambiguateUsingChecks(plst, parameterPosition);
  /* We don't have the parameterPosition here.
     Maybe we can do this later? */
#endif

  if (listLength(plst) > 1 || (foundNoun && listLength(plst) == 0)) {
    /* This is an error so have to set up parameters for error message... */
    parameters[0].instance = 0;	/* Just make it anything != EOF */
    parameters[0].useWords = TRUE; /* Remember words for errors below */
    parameters[0].firstWord = firstWord;
    parameters[0].lastWord = lastWord;
    parameters[1].instance = EOF;	/* Terminate list after one */
    if (listLength(plst) > 1)
      errorWhich(plst);
    else if (foundNoun && listLength(plst) == 0)
      errorNoSuch(parameters[0]);
  } else {
    plst[0].firstWord = firstWord;
    plst[0].lastWord = lastWord;
  }
}
  
  
/*----------------------------------------------------------------------*/
static void simple(ParamEntry olst[]) {
  static ParamEntry *tlst = NULL;
  int savidx = wordIndex;
  Bool savplur = FALSE;
  int i;

  if (tlst == NULL)
    tlst = (ParamEntry *) allocate(sizeof(ParamEntry)*(MAXENTITY+1));
  tlst[0].instance = EOF;

  for (;;) {
    /* Special handling here since THEM_WORD is a common pronoun, so
       we check if it is also a pronoun, if it is but there is a list of
       multi parameters from the previous command, we assume that is
       what he ment */
    if (isThem(playerWords[wordIndex].code)
	&& ((isPronoun(playerWords[wordIndex].code) && listLength(previousMultipleList) > 0)
	    || !isPronoun(playerWords[wordIndex].code))) {
      plural = TRUE;
      for (i = 0; previousMultipleList[i].instance != EOF; i++)
	if (!reachable(previousMultipleList[i].instance))
	  previousMultipleList[i].instance = 0;
      compact(previousMultipleList);
      if (listLength(previousMultipleList) == 0)
	errorWhat(wordIndex);
      copyParameterList(olst, previousMultipleList);
      olst[0].firstWord = EOF;	/* No words used */
      wordIndex++;
    } else {
      unambig(olst);		/* Look for unambigous noun phrase */
      if (listLength(olst) == 0) {	/* Failed! */
	copyParameterList(olst, tlst);
	wordIndex = savidx;
	plural = savplur;
	return;
      }
    }
    mergeLists(tlst, olst);
    if (playerWords[wordIndex].code != EOF
	&& (isConj(playerWords[wordIndex].code) &&
	    (isAdjective(playerWords[wordIndex+1].code) || isNoun(playerWords[wordIndex+1].code)))) {
      /* More parameters in a conjunction separated list ? */
      savplur = plural;
      savidx = wordIndex;
      wordIndex++;
      plural = TRUE;
    } else {
      copyParameterList(olst, tlst);
      return;
    }
  }
}

  
/*----------------------------------------------------------------------*/
static void complex(ParamEntry olst[])
{
  static ParamEntry *alst = NULL;

  if (alst == NULL)
    alst = (ParamEntry *) allocate((MAXENTITY+1)*sizeof(ParamEntry));

  if (isAll(playerWords[wordIndex].code)) {
    plural = TRUE;
    buildAll(alst);		/* Build list of all objects */
    wordIndex++;
    if (playerWords[wordIndex].code != EOF && isBut(playerWords[wordIndex].code)) {
      butWordIndex = wordIndex;
      wordIndex++;
      simple(olst);
      if (listLength(olst) == 0)
	errorAfterBut();
      subtractListFromList(alst, olst);
      if (listLength(alst) == 0)
	error(M_NOT_MUCH);
    }
    copyParameterList(olst, alst);
    allLength = listLength(olst);
  } else
    simple(olst);		/* Look for simple noun group */
}


/*----------------------------------------------------------------------*/
static Bool restrictionCheck(RestrictionEntry *restriction)
{
  Bool ok = FALSE;

  if (restriction->class == RESTRICTIONCLASS_CONTAINER)
    ok = instance[parameters[restriction->parameter-1].instance].container != 0;
  else
    ok = isA(parameters[restriction->parameter-1].instance, restriction->class);
  return ok;
}


/*----------------------------------------------------------------------*/
static void runRestriction(RestrictionEntry *restriction)
{
  if (sectionTraceOption)
    printf("\n<SYNTAX parameter #%ld Is Not of class %ld:>\n",
	   restriction->parameter,
	   restriction->class);
  if (restriction->stms)
    interpret(restriction->stms);
  else
    error(M_CANT0);
}


/*----------------------------------------------------------------------*/
static Aint mapSyntax(Aint syntaxNumber)
{
  /* 
     Find the syntax map, use the verb code from it and remap the parameters
   */
  ParameterMapEntry *parameterMapTable;
  Aword *parameterMap;
  Aint parameterIndex;
  ParamEntry originalParameters[MAXPARAMS];

  for (parameterMapTable = pointerTo(header->parameterMapAddress); !endOfTable(parameterMapTable); parameterMapTable++)
    if (parameterMapTable->syntaxNumber == syntaxNumber)
      break;
  if (endOfTable(parameterMapTable)) syserr("Could not find syntax in mapping table.");

  parameterMap = pointerTo(parameterMapTable->parameterMapping);
  copyParameterList(originalParameters, parameters);
  for (parameterIndex = 1; originalParameters[parameterIndex-1].instance != EOF;
       parameterIndex++)
    parameters[parameterIndex-1] = originalParameters[parameterMap[parameterIndex-1]-1];

  return parameterMapTable->verbCode;
}


/*----------------------------------------------------------------------*/
static void parseParameter(Aword flags, Bool *anyPlural, ParamEntry multipleList[]) {
  static ParamEntry *parsedParameters; /* List of parameters parsed */

  /* Allocate large enough paramlists */
  allocateParameters(&parsedParameters);

  plural = FALSE;
  complex(parsedParameters);
  if (listLength(parsedParameters) == 0) /* No object!? */
    error(M_WHAT);
  if ((flags & OMNIBIT) == 0) /* Omnipotent parameter? */
    /* If its not an omnipotent parameter, resolve by presence */
    resolve(parsedParameters);
  if (plural) {
    if ((flags & MULTIPLEBIT) == 0)	/* Allowed multiple? */
      error(M_MULTIPLE);
    else {
      /* Mark this as the multiple position in which to insert actual
	 parameter values later */
      parameters[paramidx++].instance = 0;
      copyParameterList(multipleList, parsedParameters);
      *anyPlural = TRUE;
    }
  } else
    parameters[paramidx++] = parsedParameters[0];
  parameters[paramidx].instance = EOF;
}


/*----------------------------------------------------------------------*/
static ElementEntry *matchParameterElement(ElementEntry *elms) {
  /* Require a parameter if elms->code == 0! */
  while (!endOfTable(elms) && elms->code != 0)
    elms++;
  if (endOfTable(elms))
    return NULL;
  return(elms);
}


/*----------------------------------------------------------------------*/
static ElementEntry *matchEndOfSyntax(ElementEntry *elms) {
  while (!endOfTable(elms) && elms->code != EOS)
    elms++;
  if (endOfTable(elms))		/* No match for EOS! */
    return NULL;
  return(elms);
}

/*----------------------------------------------------------------------*/
static ElementEntry *matchWordElement(ElementEntry *elms, Aint wordCode) {
  while (!endOfTable(elms) && elms->code != wordCode)
    elms++;
  if (endOfTable(elms))
    return NULL;
  return(elms);
}

/*----------------------------------------------------------------------*/
static Bool isParameterWord(int word) {
  return isNoun(word) || isAdjective(word) || isAll(word)
    || isLiteralWord(word) || isIt(word) || isThem(word) || isPronoun(word);
}


/*----------------------------------------------------------------------*/
static ElementEntry *matchParseTree(ParamEntry multipleList[],
				    ElementEntry *elms,
				    Bool *anyPlural) {
  ElementEntry *currentEntry = elms;

  while (TRUE) {		/* Traverse the possible branches to
				   find a match */
    ElementEntry *elms;

    /* End of input? */
    if (playerWords[wordIndex].code == EOF || isConj(playerWords[wordIndex].code)) {
      elms = matchEndOfSyntax(currentEntry);
      if (elms == NULL)
	return NULL;
      else
	return elms;
    }

    if (isParameterWord(playerWords[wordIndex].code)) {
      elms = matchParameterElement(currentEntry);
      if (elms != NULL) {
	parseParameter(elms->flags, anyPlural, multipleList);
	currentEntry = (ElementEntry *) pointerTo(elms->next);
	continue;
      } /* didn't allow a parameter so fall through
	   and try with a preposition... */
    }

    if (isPreposition(playerWords[wordIndex].code)) {
      /* A preposition? Or rather, an intermediate word? */
      elms = matchWordElement(currentEntry, dictionary[playerWords[wordIndex].code].code);
      if (elms != NULL) {
	wordIndex++;		/* Word matched, go to next */
	currentEntry = (ElementEntry *) pointerTo(elms->next);
	continue;
      }
    }

    if (isBut(playerWords[wordIndex].code))
      errorButAfterAll(wordIndex);

    /* If we get here we couldn't match anything... */
    return NULL;
  }
  /* And this should never happen ... */
  syserr("Fall-through in 'matchParseTree()'");
  return NULL;
}


/*----------------------------------------------------------------------*/
static SyntaxEntry *findSyntax(int verbCode) {
  SyntaxEntry *stx;
  for (stx = stxs; !endOfTable(stx); stx++)
    if (stx->code == verbCode)
      return(stx);
  return(NULL);
}

/*----------------------------------------------------------------------*/
static void disambiguate(ParamEntry candidates[], int position) {
  int i;
  for (i = 0; candidates[i].instance != EOF; i++) {
    if (candidates[i].instance != 0) {	/* Already empty? */
      parameters[position] = candidates[i];
      if (!reachable(candidates[i].instance) || !possible())
	candidates[i].instance = 0;	/* Remove this from list */
    }
  }
  compact(candidates);
}


/*----------------------------------------------------------------------*/
static void try(ParamEntry multipleParameters[])
{
  ElementEntry *elms;		/* Pointer to element list */
  SyntaxEntry *stx;		/* Pointer to syntax parse list */
  RestrictionEntry *restriction; /* Pointer to class restrictions */
  Bool anyPlural = FALSE;	/* Any parameter that was plural? */
  int i, multiplePosition;
  static ParamEntry *tlst = NULL; /* List of params found by complex() */
  static Bool *checked = NULL; /* Corresponding parameter checked? */

  if (tlst == NULL) {
    tlst = (ParamEntry *) allocate((MAXENTITY+1)*sizeof(ParamEntry));
    checked = (Bool *) allocate((MAXENTITY+1)*sizeof(Bool));
  }

  stx = findSyntax(verbWordCode);
  if (stx == NULL)
    error(M_WHAT);

  elms = matchParseTree(tlst, (ElementEntry *) pointerTo(stx->elms), &anyPlural);
  if (elms == NULL)
    error(M_WHAT);
  if (anyPlural)
    copyParameterList(multipleParameters, tlst);
  
  /* Set verb code and map parameters */
  current.verb = mapSyntax(elms->flags); /* Flags of EOS element is
					    actually syntax number! */

  /* Now perform class restriction checks */
  if (elms->next == 0)	/* No verb code, verb not declared! */
    error(M_CANT0);

  for (multiplePosition = 0; parameters[multiplePosition].instance != EOF; multiplePosition++) /* Mark all parameters unchecked */
    checked[multiplePosition] = FALSE;
  for (restriction = (RestrictionEntry *) pointerTo(elms->next); !endOfTable(restriction); restriction++) {
    if (parameters[restriction->parameter-1].instance == 0) {
      /* This was a multiple parameter, so check all and remove failing */
      for (i = 0; multipleParameters[i].instance != EOF; i++) {
	parameters[restriction->parameter-1] = multipleParameters[i];
	if (!restrictionCheck(restriction)) {
	  /* Multiple could be both an explicit list of params and an ALL */
	  if (allLength == 0) {
	    char marker[80];
	    /*
	       It wasn't ALL, we need to say something about it, so
	       prepare a printout with $1/2/3
	     */
	    sprintf(marker, "($%ld)", (long) restriction->parameter); 
	    output(marker);
	    runRestriction(restriction);
	    para();
	  }
	  multipleParameters[i].instance = 0;	  /* In any case remove it from the list */
	}
      }
      parameters[restriction->parameter-1].instance = 0;
    } else {
      if (!restrictionCheck(restriction)) {
	runRestriction(restriction);
	error(MSGMAX);		/* Return to player without saying anything */
      }
    }
    checked[restriction->parameter-1] = TRUE; /* Remember that it's already checked */
  }
  /* Now check the rest of the parameters, must be objects */
  for (multiplePosition = 0; parameters[multiplePosition].instance != EOF; multiplePosition++)
    if (!checked[multiplePosition]) {
      if (parameters[multiplePosition].instance == 0) {
	/* This was a multiple parameter, check all and remove failing */
	for (i = 0; multipleParameters[i].instance != EOF; i++)
	  if (multipleParameters[i].instance != 0) /* Skip any empty slots */
	    if (!isObject(multipleParameters[i].instance))
	      multipleParameters[i].instance = 0;
      } else if (!isObject(parameters[multiplePosition].instance))
	error(M_CANT0);
    }

  /* Finally, if ALL was used, try to find out what was applicable */
  if (allLength > 0) {
    for (multiplePosition = 0; parameters[multiplePosition].instance != 0;
	 multiplePosition++)
      ; /* Iterate over parameters to find multiple position */
    disambiguate(multipleParameters, multiplePosition);
    parameters[multiplePosition].instance = 0;		/* Restore multiple marker */
    if (listLength(multipleParameters) == 0) {
      parameters[0].instance = EOF;
      errorWhat(allWordIndex);
    }
  } else if (anyPlural) {
    compact(multipleParameters);
    if (listLength(multipleParameters) == 0)
      /* If there where multiple parameters but non left, exit without a */
      /* word, assuming we have already said enough */
      error(MSGMAX);
  }
  plural = anyPlural;		/* Remember that we found plural objects */
}


/*----------------------------------------------------------------------*/  
static void match(ParamEntry *mlst) /* OUT - List of params allowed by multiple */
{
  try(mlst);			/* ... to understand what he said */
  if (playerWords[wordIndex].code != EOF && !isConj(playerWords[wordIndex].code))
    error(M_WHAT);
  if (playerWords[wordIndex].code != EOF)	/* More on this line? */
    wordIndex++;			/* If so skip the AND */
}


/*======================================================================*/
void initParse(void) {
  int dictionaryIndex;
  int pronounIndex = 0;

  wordIndex = 0;
  continued = FALSE;
  playerWords[0].code = EOF;

  if (pronounList == NULL)
    pronounList = allocate((MAXPARAMS+1)*sizeof(PronounEntry));

  for (dictionaryIndex = 0; dictionaryIndex < dictsize; dictionaryIndex++)
    if (isPronoun(dictionaryIndex)) {
      pronounList[pronounIndex].pronoun = dictionary[dictionaryIndex].code;
      pronounList[pronounIndex].instance = 0;
      pronounIndex++;
    }
}


/*----------------------------------------------------------------------*/
static int pronounForInstance(int instance) {
  /* Scan through the dictionary to find any pronouns that can be used
     for this instance */
  int w;

  for (w = 0; w < dictsize; w++)
    if (isPronoun(w)) {
      Aword *reference = pointerTo(dictionary[w].pronounRefs);
      while (*reference != EOF) {
	if (*reference == instance)
	  return dictionary[w].code;
	reference++;
      }
    }
  return 0;
}


/*----------------------------------------------------------------------*/
static void enterPronoun(int pronoun, int instanceCode) {
  int pronounIndex;

  for (pronounIndex = 0; pronounList[pronounIndex].instance != EOF; pronounIndex++)
    ;
  pronounList[pronounIndex].pronoun = pronoun;
  pronounList[pronounIndex].instance = instanceCode;
  pronounList[pronounIndex+1].instance = EOF;
}


/*----------------------------------------------------------------------*/
static void clearPronounEntries() {
  pronounList[0].instance = EOF;
}


/*----------------------------------------------------------------------*/
static void notePronounParameters(ParamEntry *parameters) {
  /* For all parameters note which ones can be refered to by a pronoun */
  ParamEntry *p;

  clearPronounEntries();
  for (p = parameters; !endOfTable(p); p++) {
    int pronoun = pronounForInstance(p->instance);
    if (pronoun > 0)
      enterPronoun(pronoun, p->instance);
  }
}


/*======================================================================*/
void parse(void) {
  static ParamEntry *parsedParameters; /* List of parameters parsed */
  static ParamEntry *multipleList;	/* Multiple objects list */
  static ParamEntry *previousParameters;	/* Previous parameter list */

  /* Allocate large enough paramlists */
  allocateParameters(&parsedParameters);
  allocateParameters(&parameters);
  allocateParameters(&previousParameters);
  allocateParameters(&multipleList);
  allocateParameters(&previousMultipleList);

  if (playerWords[wordIndex].code == EOF) {
    wordIndex = 0;
    scan();
  } else if (anyOutput)
    para();

  capitalize = TRUE;
  allLength = 0;
  paramidx = 0;
  copyParameterList(previousParameters, parameters);
  parameters[0].instance = EOF;
  copyParameterList(previousMultipleList, multipleList);
  multipleList[0].instance = EOF;
  firstWord = wordIndex;
  if (isVerb(playerWords[wordIndex].code)) {
    verbWord = playerWords[wordIndex].code;
    verbWordCode = dictionary[verbWord].code;
    wordIndex++;
    match(multipleList);
    notePronounParameters(parameters);
    action(multipleList);		/* contains possible multiple params */
  } else {
    parameters[0].instance = EOF;
    previousMultipleList[0].instance = EOF;
    nonverb();
  }
  lastWord = wordIndex-1;
  if (isConj(playerWords[lastWord].code)) lastWord--;
}
