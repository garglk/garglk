/*----------------------------------------------------------------------*\

  MAIN.C

  Main module of interpreter for ALAN Adventure Language

\*----------------------------------------------------------------------*/

#include "sysdep.h"

#include "acode.h"
#include "types.h"
#include "set.h"
#include "state.h"
#include "main.h"
#include "syserr.h"
#include "parse.h"

#include <time.h>
#ifdef USE_READLINE
#include "readline.h"
#endif

#ifdef HAVE_SHORT_FILENAMES
#include "av.h"
#else
#include "alan.version.h"
#endif

#include "args.h"
#include "parse.h"
#include "inter.h"
#include "rules.h"
#include "reverse.h"
#include "debug.h"
#include "stack.h"
#include "exe.h"
#include "term.h"
#include "set.h"

#ifdef HAVE_GLK
#include "glk.h"
#include "glkio.h"
#ifdef HAVE_WINGLK
#include "winglk.h"
#endif
#endif

/* PUBLIC DATA */

/* The Amachine memory */
Aword *memory = NULL;
static ACodeHeader dummyHeader;	/* Dummy to use until memory allocated */
ACodeHeader *header = &dummyHeader;

int memTop = 0;			/* Top of load memory */

int conjWord;			/* First conjunction in dictonary, for ',' */


/* Amachine variables */
CurVars current;

/* The event queue */
int eventQueueSize = 0;
EventQueueEntry *eventQueue = NULL; /* Event queue */
Aint eventQueueTop = 0;		/* Event queue top pointer */

/* Amachine structures - Dynamic */
InstanceEntry *instance;	/* Instance table pointer */
AdminEntry *admin;		/* Administrative data about instances */
AttributeEntry *attributes;	/* Dynamic attribute values */
Aword *scores;			/* Score table pointer */

/* Amachine structures - Static */
ContainerEntry *container;	/* Container table pointer */
ClassEntry *class;		/* Class table pointer */
DictionaryEntry *dictionary;	/* Dictionary pointer */
VerbEntry *vrbs;		/* Verb table pointer */
SyntaxEntry *stxs;		/* Syntax table pointer */
RulEntry *ruls;			/* Rule table pointer */
EventEntry *events;		/* Event table pointer */
MessageEntry *msgs;			/* Message table pointer */
Aword *freq;			/* Cumulative character frequencies */

int dictsize;

Bool verbose = FALSE;
Bool ignoreErrorOption = FALSE;
Bool debugOption = FALSE;
Bool sectionTraceOption = FALSE;
Bool tracePushOption = FALSE;
Bool traceStackOption = FALSE;
Bool traceSourceOption = FALSE;
Bool singleStepOption = FALSE;
Bool transcriptOption = FALSE;
Bool logOption = FALSE;
Bool statusLineOption = TRUE;
Bool regressionTestOption = FALSE;
Bool fail = FALSE;
Bool anyOutput = FALSE;


/* The files and filenames */
char *adventureName;		/* The name of the game */
char *adventureFileName;
FILE *textFile;
#ifdef HAVE_GLK
strid_t logFile;
#else
FILE *logFile;
#endif

/* Screen formatting info */
int col, lin;
int pageLength, pageWidth;

Bool capitalize = FALSE;
Bool needSpace = FALSE;
Bool skipSpace = FALSE;

/* Restart jump buffer */
jmp_buf restartLabel;		/* Restart long jump return point */
jmp_buf returnLabel;		/* Error (or undo) long jump return point */
jmp_buf forfeitLabel;		/* Player forfeit by an empty command */


/* PRIVATE DATA */
static Bool onStatusLine = FALSE; /* Don't log when printing status */


/*======================================================================

  terminate()

  Terminate the execution of the adventure, e.g. close windows,
  return buffers...

 */
void terminate(int code)
{
#ifdef __amiga__
#ifdef AZTEC_C
#include <fcntl.h>
  extern struct _dev *_devtab;
  char buf[85];
  
  if (con) { /* Running from WB, created a console so kill it */
    /* Running from WB, so we created a console and
       hacked the Aztec C device table to use it for all I/O
       so now we need to make it close it (once!) */
    _devtab[1].fd = _devtab[2].fd = 0;
  } else
#else
  /* Geek Gadgets GCC */
#include <workbench/startup.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>

  if (_WBenchMsg != NULL) {
    Close(window);
    if (_WBenchMsg->sm_ArgList != NULL)
      UnLock(CurrentDir(cd));
  } else
#endif
#endif
    newline();
  free(memory);
  if (transcriptOption)
#ifdef HAVE_GLK
    glk_stream_close(logFile, NULL);
#else
    fclose(logFile);
#endif

#ifdef __MWERKS__
  printf("Command-Q to close window.");
#endif

#ifdef HAVE_GLK
  glk_exit();
#else
  exit(code);
#endif
}

/*======================================================================*/
void usage(void)
{
  printf("\nArun, Adventure Interpreter version %s (%s %s)\n\n",
	 alan.version.string, alan.date, alan.time);
  printf("Usage:\n\n");
  printf("    %s [<switches>] <adventure>\n\n", PROGNAME);
  printf("where the possible optional switches are:\n");
#ifdef HAVE_GLK
  glk_set_style(style_Preformatted);
#endif
  printf("    -v       verbose mode\n");
  printf("    -l       log transcript to a file\n");
  printf("    -c       log player commands to a file\n");
  printf("    -n       no Status Line\n");
  printf("    -d       enter debug mode\n");
  printf("    -t[<n>]  trace game execution, higher <n> gives more trace\n");
  printf("    -i       ignore version and checksum errors\n");
  printf("    -r       refrain from printing timestamps and paging (making regression testing easier)\n");
#ifdef HAVE_GLK
  glk_set_style(style_Normal);
#endif
}


/*======================================================================*/
void error(MsgKind msgno)	/* IN - The error message number */
{
  /* Print an error message and longjmp to main loop. */
  if (msgno != MSGMAX)
    printMessage(msgno);
  longjmp(returnLabel, ERROR_RETURN);
}


/*======================================================================*/
void statusline(void)
{
#ifdef HAVE_GLK
  glui32 glkWidth;
  char line[100];
  int pcol = col;

  if (!statusLineOption) return;
  if (glkStatusWin == NULL)
    return;

  glk_set_window(glkStatusWin);
  glk_window_clear(glkStatusWin);
  glk_window_get_size(glkStatusWin, &glkWidth, NULL);

int i;
glk_set_style(style_User1);
for (i = 0; i < glkWidth; i++)
    glk_put_char(' ');

  onStatusLine = TRUE;
  col = 1;
  glk_window_move_cursor(glkStatusWin, 1, 0);
  sayInstance(where(HERO, TRUE));

  if (header->maximumScore > 0)
    sprintf(line, "Score %d(%d)/%d moves", current.score, (int)header->maximumScore, current.tick);
  else
    sprintf(line, "%d moves", current.tick);
  glk_window_move_cursor(glkStatusWin, glkWidth-strlen(line)-1, 0);
  glk_put_string(line);
  needSpace = FALSE;

  col = pcol;
  onStatusLine = FALSE;

  glk_set_window(glkMainWin); 
#else
#ifdef HAVE_ANSI
  char line[100];
  int i;
  int pcol = col;

  if (!statusLineOption) return;
  /* ansi_position(1,1); ansi_bold_on(); */
  printf("\x1b[1;1H");
  printf("\x1b[7m");

  onStatusLine = TRUE;
  col = 1;
  sayInstance(where(HERO, FALSE));

  if (header->maximumScore > 0)
    sprintf(line, "Score %d(%ld)/%d moves", current.score, header->maximumScore, current.tick);
  else
    sprintf(line, "%ld moves", (long)current.tick);
  for (i=0; i < pageWidth - col - strlen(line); i++) putchar(' ');
  printf(line);
  printf("\x1b[m");
  printf("\x1b[%d;1H", pageLength);

  needSpace = FALSE;
  capitalize = TRUE;

  onStatusLine = FALSE;
  col = pcol;
#endif
#endif
}


#if defined(HAVE_GLK) || defined(RUNNING_UNITTESTS)
/*----------------------------------------------------------------------*/
static int updateColumn(int currentColumn, char *string) {
  char *newlinePosition = strrchr(string, '\n');
  if (newlinePosition != NULL)
    return &string[strlen(string)] - newlinePosition;
  else
    return currentColumn + strlen(string);
}
#endif


/*======================================================================*/
void printAndLog(char string[])
{
#ifdef HAVE_GLK
  static int column = 0;
  char *stringCopy;
  char *stringPart;
#endif

  printf(string);
  if (!onStatusLine && transcriptOption) {
#ifdef HAVE_GLK
    if (strlen(string) > 70-column) {
      stringCopy = strdup(string);	/* Make sure we can write NULLs */
      stringPart = stringCopy;
      while (strlen(stringPart) > 70-column) {
	int p;
	for (p = 70-column; p>0 && !isspace(stringPart[p]); p--);
	stringPart[p] = '\0';
	glk_put_string_stream(logFile, stringPart);
	glk_put_char_stream(logFile, '\n');
	column = 0;
	stringPart = &stringPart[p+1];
      }
      glk_put_string_stream(logFile, stringPart);
      column = updateColumn(column, stringPart);
      free(stringCopy);
    } else {
      glk_put_string_stream(logFile, string);
      column = updateColumn(column, string);
    }
#else
    fprintf(logFile, string);
#endif
  }
}



/*======================================================================*/
void newline(void)
{
#ifndef HAVE_GLK
  char buf[256];

  if (!regressionTestOption && lin >= pageLength - 1) {
    printAndLog("\n");
    needSpace = FALSE;
    printMessage(M_MORE);
    statusline();
    fflush(stdout);
    fgets(buf, 256, stdin);
    getPageSize();
    lin = 0;
  } else
    printAndLog("\n");
  
  lin++;
#else
  printAndLog("\n");
#endif
  col = 1;
  needSpace = FALSE;
}


/*======================================================================*/
void para(void)
{
  /* Make a new paragraph, i.e one empty line (one or two newlines). */

#ifdef HAVE_GLK
  if (glk_gestalt(gestalt_Graphics, 0) == 1)
    glk_window_flow_break(glkMainWin);
#endif
  if (col != 1)
    newline();
  newline();
  capitalize = TRUE;
}


/*======================================================================*/
void clear(void)
{
#ifdef HAVE_GLK
  glk_window_clear(glkMainWin);
#else
#ifdef HAVE_ANSI
  if (!statusLineOption) return;
  printf("\x1b[2J");
  printf("\x1b[%d;1H", pageLength);
#endif
#endif
}


#ifndef DMALLOC
/*======================================================================*/
void *allocate(unsigned long lengthInBytes)
{
  void *p = (void *)calloc((size_t)lengthInBytes, 1);

  if (p == NULL)
    syserr("Out of memory.");

  return p;
}
#endif


/*======================================================================*/
void *duplicate(void *original, unsigned long len)
{
  void *p = allocate(len);

  memcpy(p, original, len);
  return p;
}


/*----------------------------------------------------------------------*/
static void capitalizeFirst(char *str) {
  int i = 0;

  /* Skip over space... */
  while (i < strlen(str) && isSpace(str[i])) i++;
  if (i < strlen(str)) {
    str[i] = toUpper(str[i]);
    capitalize = FALSE;
  }
}


/*----------------------------------------------------------------------*/
static void justify(char str[])
{
  if (capitalize)
    capitalizeFirst(str);

#ifdef HAVE_GLK
  printAndLog(str);
#else
  int i;
  char ch;
  
  if (col >= pageWidth && !skipSpace)
    newline();

  while (strlen(str) > pageWidth - col) {
    i = pageWidth - col - 1;
    while (!isSpace(str[i]) && i > 0) /* First find wrap point */
      i--;
    if (i == 0 && col == 1)	/* If it doesn't fit at all */
      /* Wrap immediately after this word */
      while (!isSpace(str[i]) && str[i] != '\0')
	i++;
    if (i > 0) {		/* If it fits ... */
      ch = str[i];		/* Save space or NULL */
      str[i] = '\0';		/* Terminate string */
      printAndLog(str);		/* and print it */
      skipSpace = FALSE;		/* If skipping, now we're done */
      str[i] = ch;		/* Restore character */
      /* Skip white after printed portion */
      for (str = &str[i]; isSpace(str[0]) && str[0] != '\0'; str++);
    }
    newline();			/* Then start a new line */
    while(isSpace(str[0])) str++; /* Skip any leading space on next part */
  }
  printAndLog(str);		/* Print tail */
#endif
  col = col + strlen(str);	/* Update column */
}


/*----------------------------------------------------------------------*/
static void space(void)
{
  if (skipSpace)
    skipSpace = FALSE;
  else {
    if (needSpace) {
      printAndLog(" ");
      col++;
    }
  }
  needSpace = FALSE;
}


/*----------------------------------------------------------------------*/
static void sayPlayerWordsForParameter(int p) {
  int i;

  for (i = parameters[p].firstWord; i <= parameters[p].lastWord; i++) {
    justify((char *)pointerTo(dictionary[playerWords[i].code].string));
    if (i < parameters[p].lastWord)
      justify(" ");
  }
}


/*----------------------------------------------------------------------*/
static void sayParameter(int p, int form)
{
  int i;

  for (i = 0; i <= p; i++)
    if (parameters[i].instance == EOF)
      syserr("Nonexistent parameter referenced.");

#ifdef ALWAYS_SAY_PARAMETERS_USING_PLAYER_WORDS
  if (params[p].firstWord != EOF) /* Any words he used? */
    /* Yes, so use them... */
    sayPlayerWordsForParameter(p);
  else
    sayForm(params[p].code, form);
#else
  if (parameters[p].useWords) {
    /* Ambiguous instance referenced, so use the words he used */
    sayPlayerWordsForParameter(p);
  } else
    sayForm(parameters[p].instance, form);
#endif
}


/*----------------------------------------------------------------------

  Print an expanded symbolic reference.

  N = newline
  I = indent on a new line
  P = new paragraph
  L = current location name
  O = current object -> first parameter!
  <n> = n:th parameter
  +<n> = definite form of n:th parameter
  0<n> = indefinite form of n:th parameter
  !<n> = pronoun for the n:th parameter
  V = current verb
  A = current actor
  T = tabulation
  $ = no space needed after this, and don't capitalize
 */
static char *printSymbol(char *str)	/* IN - The string starting with '$' */
{
  int advance = 2;

  if (*str == '\0') printAndLog("$");
  else switch (toLower(str[1])) {
  case 'n':
    newline();
    needSpace = FALSE;
    break;
  case 'i':
    newline();
    printAndLog("    ");
    col = 5;
    needSpace = FALSE;
    break;
  case 'o':
    space();
    sayParameter(0, 0);
    needSpace = TRUE;		/* We did print something non-white */
    break;
  case '+':
  case '0':
  case '-':
  case '!':
    space();
    if (isdigit(str[2])) {
      int form;
      switch (str[1]) {
      case '+': form = SAY_DEFINITE; break;
      case '0': form = SAY_INDEFINITE; break;
      case '-': form = SAY_NEGATIVE; break;
      case '!': form = SAY_PRONOUN; break;
      default: form = SAY_SIMPLE; break;
      }
      sayParameter(str[2]-'1', form);
      needSpace = TRUE;
    }
    advance = 3;
    break;
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    space();
    sayParameter(str[1]-'1', SAY_SIMPLE);
    needSpace = TRUE;		/* We did print something non-white */
    break;
  case 'l':
    space();
    say(current.location);
    needSpace = TRUE;		/* We did print something non-white */
    break;
  case 'a':
    space();
    say(current.actor);
    needSpace = TRUE;		/* We did print something non-white */
    break;
  case 'v':
    space();
    justify((char *)pointerTo(dictionary[verbWord].string));
    needSpace = TRUE;		/* We did print something non-white */
    break;
  case 'p':
    para();
    needSpace = FALSE;
    break;
  case 't': {
    int i;
    int spaces = 4-(col-1)%4;
    
    for (i = 0; i<spaces; i++) printAndLog(" ");
    col = col + spaces;
    needSpace = FALSE;
    break;
  }
  case '$':
    skipSpace = TRUE;
    capitalize = FALSE;
    break;
  default:
    printAndLog("$");
    break;
  }

  return &str[advance];
}


/*----------------------------------------------------------------------*/
static Bool inhibitSpace(char *str) {
  return str[0] == '$' && str[1] == '$';
}


/*----------------------------------------------------------------------*/
static Bool isSpaceEquivalent(char str[]) {
  if (str[0] == ' ')
    return TRUE;
  else
    return strncmp(str, "$p", 2) == 0
      || strncmp(str, "$n", 2) == 0
      || strncmp(str, "$i", 2) == 0
      || strncmp(str, "$t", 2) == 0;
}


/*----------------------------------------------------------------------*/
static Bool punctuationNext(char *str) {
  char *punctuation = strchr(".,!?", str[0]);
  Bool end = str[1] == '\0';
  Bool space = isSpaceEquivalent(&str[1]);
  return (punctuation != NULL && (end || space));
}


/*----------------------------------------------------------------------*/
static char lastCharOf(char *str) {
  return str[strlen(str)-1];
}


/*======================================================================*/
void output(char original[])
{
  char ch;
  char *str, *copy;
  char *symptr;

  copy = strdup(original);
  str = copy;

  if (inhibitSpace(str) || punctuationNext(str))
    needSpace = FALSE;
  else
    space();			/* Output space if needed (& not inhibited) */

  /* Output string up to symbol and handle the symbol */
  while ((symptr = strchr(str, '$')) != (char *) NULL) {
    ch = *symptr;		/* Terminate before symbol */
    *symptr = '\0';
    if (strlen(str) > 0) {
      skipSpace = FALSE;	/* Only let skipSpace through if it is
				   last in the string */
      if (lastCharOf(str) == ' ') {
	str[strlen(str)-1] = '\0'; /* Truncate space character */
	justify(str);		/* Output part before '$' */
	needSpace = TRUE;
      } else {
	justify(str);		/* Output part before '$' */
	needSpace = FALSE;
      }
    }
    *symptr = ch;		/* restore '$' */
    str = printSymbol(symptr);	/* Print the symbolic reference and advance */
  }

  if (str[0] != 0) {
    justify(str);			/* Output trailing part */
    skipSpace = FALSE;
    if (lastCharOf(str) != ' ')
      needSpace = TRUE;
  }
  if (needSpace)
    capitalize = strchr("!?.", str[strlen(str)-1]) != 0;
  anyOutput = TRUE;
  free(copy);
}


/*======================================================================*/
void printMessage(MsgKind msg)		/* IN - message number */
{
  interpret(msgs[msg].stms);
}


/*----------------------------------------------------------------------*\

  Various check functions

  endOfTable()
  isObj, isLoc, isAct, IsCnt & isNum

\*----------------------------------------------------------------------*/

/* How to know we are at end of a table */
Bool eot(Aword *adr)
{
  return *adr == EOF;
}


Bool isObject(Aword x)
{
  return isA((x), OBJECT);
}

Bool isContainer(Aword x)
{
  return (x) != 0 && instance[x].container != 0;
}

Bool isActor(Aword x)
{
  return isA((x), ACTOR);
}

Bool isLocation(Aword x)
{
  return isA((x), LOCATION);
}


Bool isLiteral(Aword x)
{
  return (x) > header->instanceMax;
}

Bool isNumeric(Aword x)
{
  return isLiteral(x) && literal[(x)-header->instanceMax].type == NUMERIC_LITERAL;
}

Bool isString(Aword x)
{
  return isLiteral(x) && literal[(x)-header->instanceMax].type == STRING_LITERAL;
}


/* Word classes are numbers but in the dictionary they are generated as bits */
Bool isVerb(int word) {
  return word < dictsize && (dictionary[word].classBits&VERB_BIT)!=0;
}

Bool isConj(int word) {
  return word < dictsize && (dictionary[word].classBits&CONJUNCTION_BIT)!=0;
}

Bool isBut(int word) {
  return word < dictsize && (dictionary[word].classBits&EXCEPT_BIT)!=0;
}

Bool isThem(int word) {
  return word < dictsize && (dictionary[word].classBits&THEM_BIT)!=0;
}

Bool isIt(int word) {
  return word < dictsize && (dictionary[word].classBits&IT_BIT)!=0;
}

Bool isNoun(int word) {
  return word < dictsize && (dictionary[word].classBits&NOUN_BIT)!=0;
}

Bool isAdjective(int word) {
  return word < dictsize && (dictionary[word].classBits&ADJECTIVE_BIT)!=0;
}

Bool isPreposition(int word) {
  return word < dictsize && (dictionary[word].classBits&PREPOSITION_BIT)!=0;
}

Bool isAll(int word) {
  return word < dictsize && (dictionary[word].classBits&ALL_BIT)!=0;
}

Bool isDir(int word) {
  return word < dictsize && (dictionary[word].classBits&DIRECTION_BIT)!=0;
}

Bool isNoise(int word) {
  return word < dictsize && (dictionary[word].classBits&NOISE_BIT)!=0;
}

Bool isPronoun(int word) {
  return word < dictsize && (dictionary[word].classBits&PRONOUN_BIT)!=0;
}


Bool isLiteralWord(int word) {
  return word >= dictsize;
}


/*======================================================================*/
Bool exitto(int to, int from)
{
  ExitEntry *theExit;

  if (instance[from].exits == 0)
    return(FALSE); /* No exits */

  for (theExit = (ExitEntry *) pointerTo(instance[from].exits); !endOfTable(theExit); theExit++)
    if (theExit->target == to)
      return(TRUE);

  return(FALSE);
}
    
      

#ifdef CHECKOBJ
/*======================================================================

  checkobj()

  Check that the object given is valid, else print an error message
  or find out what he wanted.

  This routine is not used any longer, kept for sentimental reasons ;-)

  */
void checkobj(obj)
     Aword *obj;
{
  Aword oldobj;
  
  if (*obj != EOF)
    return;
  
  oldobj = EOF;
  for (cur.obj = OBJMIN; cur.obj <= OBJMAX; cur.obj++) {
    /* If an object is present and it is possible to perform his action */
    if (isHere(cur.obj) && possible())
      if (oldobj == EOF)
	oldobj = cur.obj;
      else
	error(WANT);          /* And we didn't find multiple objects */
    }
  
  if (oldobj == EOF)
    error(WANT);              /* But we found ONE */
  
  *obj = cur.obj = oldobj;    
  output("($o)");             /* Then he surely meant this object */
}
#endif




/*----------------------------------------------------------------------*\

  Event Handling

  eventchk()

\*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------

  runPendingEvents()

  Check if any events are pending. If so execute them.
  */
static void runPendingEvents(void)
{
  int i;

  while (eventQueueTop != 0 && eventQueue[eventQueueTop-1].after == 0) {
    eventQueueTop--;
    if (isLocation(eventQueue[eventQueueTop].where))
      current.location = eventQueue[eventQueueTop].where;
    else
      current.location = where(eventQueue[eventQueueTop].where, FALSE);
    if (sectionTraceOption) {
      printf("\n<EVENT %d (at ", eventQueue[eventQueueTop].event);
      traceSay(current.location);
      printf("):>\n");
    }
    interpret(events[eventQueue[eventQueueTop].event].code);
  }

  for (i = 0; i<eventQueueTop; i++)
    eventQueue[i].after--;
}





/*----------------------------------------------------------------------*\

  Main program and initialisation

\*----------------------------------------------------------------------*/


static FILE *codfil;
static char codfnm[256] = "";
static char txtfnm[256] = "";
static char logfnm[256] = "";


/*----------------------------------------------------------------------*/
static char *decodeState(int c) {
  static char state[2] = "\0\0";
  switch (c) {
  case 0: return ".";
  case 'd': return "dev";
  case 'a': return "alpha";
  case 'b': return "beta";
  default:
    state[0] = header->version[3];
    return state;
  }
}


/*----------------------------------------------------------------------*/
static void incompatibleDevelopmentVersion(ACodeHeader *header) {
  char str[80];
  sprintf(str, "Incompatible version of ACODE program. Development versions always require exact match. Game is %ld.%ld%s%ld, interpreter %ld.%ld%s%ld!",
	  (long)(header->version[0]),
	  (long)(header->version[1]),
	  decodeState(header->version[3]),
	  (long)(header->version[2]),
	  (long)alan.version.version,
	  (long)alan.version.revision,
	  alan.version.state,
	  (long)alan.version.correction);
  apperr(str);
}


/*----------------------------------------------------------------------*/
static void incompatibleVersion(ACodeHeader *header) {
  char str[80];
  sprintf(str, "Incompatible version of ACODE program. Game is %ld.%ld, interpreter %ld.%ld.",
	  (long)(header->version[0]),
	  (long)(header->version[1]),
	  (long)alan.version.version,
	  (long)alan.version.revision);
  apperr(str);
}


/*----------------------------------------------------------------------*/
static void alphaRunningLaterGame(char gameState) {
  output("<WARNING! You are running an alpha interpreter, but the game is generated by a");
  if (gameState == 'b')
    output("beta");
  else
    output("release");
  output("state compiler which was released later. This might cause the game to not work fully as intended. Look for an upgraded game file.>\n");
}

/*----------------------------------------------------------------------*/
static void nonDevelopmentRunningDevelopmentStateGame(char version[]) {
  char errorMessage[255];
  char versionString[100];

  strcpy(errorMessage, "Games generated by a development state compiler");
  sprintf(versionString, "(this game is v%d.%d.%d%s)", version[0], version[1],
	  version[2], decodeState(version[3]));
  strcat(errorMessage, versionString);
  strcat(errorMessage, "can only be run with a matching interpreter. Look for a game file generated with an alpha, beta or release state compiler.>\n");
  apperr(errorMessage);
}


/*----------------------------------------------------------------------*/
static void checkVersion(ACodeHeader *header)
{
  /* Strategy for version matching is:
     1) Development interpreters/games require exact match
     2) Alpha, Beta and Release interpreters will not run development games
     3) Alpha interpreters must warn if they run beta or release games
     4) Beta interpreters may introduce changes which are not alpha compatible,
        if the change is a strict addition (i.e. if not used will not affect
        alpha interpreters, example is introduction of a new opcode if it is
        done at the end of the list)
     5) Release interpreters should run alpha and beta games without problems
    */

  char interpreterVersion[4];
  Abool developmentVersion;
  Abool alphaVersion;
  int compareLength;
  char gameState = header->version[3];

  /* Construct our own version */
  interpreterVersion[0] = alan.version.version;
  interpreterVersion[1] = alan.version.revision;
  interpreterVersion[2] = alan.version.correction;
  interpreterVersion[3] = alan.version.state[0];

  /* Check version of .ACD file */
  if (debugOption && !regressionTestOption) {
    printf("<Version of '%s' is %d.%d%s%d>",
	   adventureFileName,
	   (int)(header->version[0]),
	   (int)(header->version[1]),
	   decodeState(gameState),
	   (int)(header->version[2]));
    newline();
  }

  /* Development version require exact match, else only 2 digit match */
  developmentVersion = (strcmp(alan.version.state, "dev") == 0);
  alphaVersion = (strcmp(alan.version.state, "alpha") == 0);
  compareLength = (developmentVersion? 3 : 2);

  if (gameState == 'd' && !developmentVersion)
    /* Development state game requires development state interpreter... */
    nonDevelopmentRunningDevelopmentStateGame(header->version);
  else {
    /* Compatible if version, revision (and correction if dev state) match... */
    if (memcmp(header->version, interpreterVersion, compareLength) != 0) {
      /* Mismatch! */
      if (!ignoreErrorOption) {
	if (developmentVersion)
	  incompatibleDevelopmentVersion(header);
	else
	  incompatibleVersion(header);
      } else
	output("<WARNING! Incompatible version of ACODE program.>\n");
    } else if (developmentVersion && gameState != 'd')
      /* ... unless interpreter is development and game not */
      incompatibleDevelopmentVersion(header);
    else if (alphaVersion && gameState != 'a') {
      /* If interpreter is alpha version and the game is later, warn! */
      alphaRunningLaterGame(gameState);
    }
  }
}

/*----------------------------------------------------------------------
  Calculate where the actual memory starts. Might be different for
  different versions.
*/
static int memoryStart(char version[4]) {
  /* Pre 3.0alpha5 had a shorter header */
  if (version[3] == 3 && version[2] == 0 && version[0] == 'a' && version[1] <5)
    return sizeof(Pre3_0alpha5Header)/sizeof(Aword);
  else
    return sizeof(ACodeHeader)/sizeof(Aword);
}



/*----------------------------------------------------------------------*/
static void load(void)
{
  ACodeHeader tmphdr;
  Aword crc = 0;
  int i,tmp;
  char err[100];

  rewind(codfil);
  tmp = fread(&tmphdr, sizeof(tmphdr), 1, codfil);
  rewind(codfil);
  if (strncmp((char *)&tmphdr, "ALAN", 4) != 0)
    apperr("Not an Alan game file, does not start with \"ALAN\"");

  checkVersion(&tmphdr);

  /* Allocate and load memory */

  if (littleEndian())
    reverseHdr(&tmphdr);

  if (tmphdr.size <= sizeof(ACodeHeader)/sizeof(Aword))
    syserr("Malformed game file. Too small.");

  /* No memory allocated yet? */
  if (memory == NULL) {
    memory = allocate(tmphdr.size*sizeof(Aword));
  }
  header = (ACodeHeader *) pointerTo(0);

  memTop = fread(pointerTo(0), sizeof(Aword), tmphdr.size, codfil);
  if (memTop != tmphdr.size)
    syserr("Could not read all ACD code.");

  /* Calculate checksum */
  for (i = memoryStart(tmphdr.version); i < memTop; i++) {
    crc += memory[i]&0xff;
    crc += (memory[i]>>8)&0xff;
    crc += (memory[i]>>16)&0xff;
    crc += (memory[i]>>24)&0xff;
#ifdef CRCLOG
    printf("%6x\t%6lx\t%6lx\n", i, crc, memory[i]);
#endif
  }
  if (crc != tmphdr.acdcrc) {
    sprintf(err, "Checksum error in Acode (.a3c) file (0x%lx instead of 0x%lx).",
	    (unsigned long) crc, (unsigned long) tmphdr.acdcrc);
    if (!ignoreErrorOption)
#if 0
      syserr(err);
#else
      output("[ Note: checksum was ignored when loading this game. ]$n");
#endif
    else {
      output("<WARNING! $$");
      output(err);
      output("$$ Ignored, proceed at your own risk.>$n");
    }
  }

  if (littleEndian()) {
    if (debugOption||sectionTraceOption||singleStepOption)
      output("<Hmm, this is a little-endian machine, fixing byte ordering....");
    reverseACD();			/* Reverse content of the ACD file */
    if (debugOption||sectionTraceOption||singleStepOption)
      output("OK.>$n");
  }
}


/*----------------------------------------------------------------------*/
static void checkDebug(void)
{
  /* Make sure he can't debug if not allowed! */
  if (!header->debug) {
    if (debugOption|sectionTraceOption|singleStepOption) {
      printf("<Sorry, '%s' is not compiled for debug!>\n", adventureFileName);
      terminate(0);
    }
    para();
    debugOption = FALSE;
    sectionTraceOption = FALSE;
    singleStepOption = FALSE;
    tracePushOption = FALSE;
  }

  if (debugOption)		/* If debugging... */
    srand(0);			/* ... use no randomization */
  else
    srand(time(0));		/* Else seed random generator */
}


/*----------------------------------------------------------------------*/
static void initStaticData(void)
{
  /* Dictionary */
  dictionary = (DictionaryEntry *) pointerTo(header->dictionary);
  /* Find out number of entries in dictionary */
  for (dictsize = 0; !endOfTable(&dictionary[dictsize]); dictsize++);

  /* Scores */
  

  /* All addresses to tables indexed by ids are converted to pointers,
     then adjusted to point to the (imaginary) element before the
     actual table so that [0] does not exist. Instead indices goes
     from 1 and we can use [1]. */

  if (header->instanceTableAddress == 0)
    syserr("Instance table pointer == 0");
  instance = (InstanceEntry *) pointerTo(header->instanceTableAddress);
  instance--;			/* Back up one so that first is no. 1 */


  if (header->classTableAddress == 0)
    syserr("Class table pointer == 0");
  class = (ClassEntry *) pointerTo(header->classTableAddress);
  class--;			/* Back up one so that first is no. 1 */

  if (header->containerTableAddress != 0) {
    container = (ContainerEntry *) pointerTo(header->containerTableAddress);
    container--;
  }

  if (header->eventTableAddress != 0) {
    events = (EventEntry *) pointerTo(header->eventTableAddress);
    events--;
  }

  /* Scores, if already allocated, copy initial data */
  if (scores == NULL)
    scores = duplicate((Aword *) pointerTo(header->scores), header->scoreCount*sizeof(Aword));
  else
    memcpy(scores, pointerTo(header->scores),
	   header->scoreCount*sizeof(Aword));


  stxs = (SyntaxEntry *) pointerTo(header->syntaxTableAddress);
  vrbs = (VerbEntry *) pointerTo(header->verbTableAddress);
  ruls = (RulEntry *) pointerTo(header->ruleTableAddress);
  msgs = (MessageEntry *) pointerTo(header->messageTableAddress);

  if (header->pack)
    freq = (Aword *) pointerTo(header->freq);
}


/*----------------------------------------------------------------------*/
static void initStrings(void)
{
  StringInitEntry *init;
  
  for (init = (StringInitEntry *) pointerTo(header->stringInitTable); !endOfTable(init); init++)
    setValue(init->instanceCode, init->attributeCode, (Aword)getStringFromFile(init->fpos, init->len));
}

/*----------------------------------------------------------------------*/
static Aint sizeOfAttributeData(void)
{
  int i;
  int size = 0;

  for (i=1; i<=header->instanceMax; i++) {
    AttributeEntry *attribute = pointerTo(instance[i].initialAttributes);
    while (!endOfTable(attribute)) {
      size += AwordSizeOf(AttributeEntry);
      attribute++;
    }
    size += 1;			/* For EOF */
  }

  if (size != header->attributesAreaSize)
    syserr("Attribute area size calculated wrong.");
  return size;
}


/*----------------------------------------------------------------------*/
static AttributeEntry *initializeAttributes(int awordSize)
{
  Aword *attributeArea = allocate(awordSize*sizeof(Aword));
  Aword *currentAttributeArea = attributeArea;
  int i;

  for (i=1; i<=header->instanceMax; i++) {
    AttributeEntry *originalAttribute = pointerTo(instance[i].initialAttributes);
    admin[i].attributes = (AttributeEntry *)currentAttributeArea;
    while (!endOfTable(originalAttribute)) {
      *((AttributeEntry *)currentAttributeArea) = *originalAttribute;
      currentAttributeArea += AwordSizeOf(AttributeEntry);
      originalAttribute++;
    }
    *((Aword*)currentAttributeArea) = EOF;
    currentAttributeArea += 1;
  }

  return (AttributeEntry *)attributeArea;
}




/*----------------------------------------------------------------------*/
static void initDynamicData(void)
{
  int instanceId;

  /* Allocate for administrative table */
  admin = (AdminEntry *)allocate((header->instanceMax+1)*sizeof(AdminEntry));

  /* Create game state copy of attributes */
  attributes = initializeAttributes(sizeOfAttributeData());

  /* Initialise string & set attributes */
  initStrings();
  initSets((SetInitEntry*)pointerTo(header->setInitTable));

  /* Set initial locations */
  for (instanceId = 1; instanceId <= header->instanceMax; instanceId++)
    admin[instanceId].location = instance[instanceId].initialLocation;
}


/*----------------------------------------------------------------------*/
static void runInheritedInitialize(Aint theClass) {
  if (theClass == 0) return;
  runInheritedInitialize(class[theClass].parent);
  if (class[theClass].initialize)
    interpret(class[theClass].initialize);
}


/*----------------------------------------------------------------------*/
static void runInitialize(Aint theInstance) {
  runInheritedInitialize(instance[theInstance].parent);
  if (instance[theInstance].initialize != 0)
    interpret(instance[theInstance].initialize);
}


/*----------------------------------------------------------------------*/
static void initializeInstances() {
  int instanceId;

  /* Set initial locations */
  for (instanceId = 1; instanceId <= header->instanceMax; instanceId++) {
    current.instance = instanceId;
    runInitialize(instanceId);
  }
}


/*----------------------------------------------------------------------*/
static void start(void)
{
  int startloc;

  current.tick = -1;
  current.location = startloc = where(HERO, FALSE);
  current.actor = HERO;
  current.score = 0;

  initializeInstances();

  if (sectionTraceOption)
    printf("\n<START:>\n");
  interpret(header->start);
  para();

  locate(HERO, startloc);
  rules();
}



/*----------------------------------------------------------------------*/
static void openFiles(void)
{
  char str[256];
  char *usr = "";
  time_t tick;

  /* Open Acode file */
  strcpy(codfnm, adventureFileName);
  if ((codfil = fopen(codfnm, READ_MODE)) == NULL) {
    strcpy(str, "Can't open adventure code file '");
    strcat(str, codfnm);
    strcat(str, "'.");
    apperr(str);
  }

{
char *s = strrchr(codfnm, '\\');
if (!s) s = strrchr(codfnm, '/');
garglk_set_story_name(s ? s + 1 : codfnm);
}

  /* Open Text file */
  strcpy(txtfnm, adventureFileName);
  if ((textFile = fopen(txtfnm, READ_MODE)) == NULL) {
    strcpy(str, "Can't open adventure text data file '");
    strcat(str, txtfnm);
    strcat(str, "'.");
    apperr(str);
  }

  /* If logging open log file */
  if (transcriptOption || logOption) {
    time(&tick);
    sprintf(logfnm, "%s%d%s.log", adventureName, (int)tick, usr);
#ifdef HAVE_GLK
    glui32 fileUsage = transcriptOption?fileusage_Transcript:fileusage_InputRecord;
    frefid_t logFileRef = glk_fileref_create_by_name(fileUsage, logfnm, 0);
    logFile = glk_stream_open_file(logFileRef, filemode_Write, 0);
#else
    logFile = fopen(logfnm, "w");
#endif
    if (logFile == NULL) {
      transcriptOption = FALSE;
      logOption = FALSE;
    }
  }
}
    

/*----------------------------------------------------------------------*/
static void init(void)
{
  int i;

  if (!regressionTestOption && (debugOption||sectionTraceOption||singleStepOption)) {
    char str[80];
    output("<Hi! This is Alan interactive fiction interpreter Arun,");
    sprintf(str, "version %ld.%ld%s%ld!>$n",
	    (long)alan.version.version,
	    (long)alan.version.revision,
	    alan.version.state[0]=='\0'?".":alan.version.state,
	    (long)alan.version.correction);
    output(str);
  }

  /* Initialise some status */
  eventQueueTop = 0;			/* No pending events */
  initStaticData();
  initDynamicData();
  initParse();
  checkDebug();

  getPageSize();

  /* Find first conjunction and use that for ',' handling */
  for (i = 0; i < dictsize; i++)
    if (isConj(i)) {
      conjWord = i;
      break;
    }

  /* Start the adventure */
  if (debugOption)
    debug(FALSE, 0, 0);
  else
    clear();

  start();
}



static Bool traceActor(int theActor)
{
  if (sectionTraceOption) {
    printf("\n<ACTOR %d, ", theActor);
    traceSay(theActor);
    printf(" (at ");
    traceSay(current.location);
  }
  return sectionTraceOption;
}


/*----------------------------------------------------------------------*/
static char *scriptName(int theActor, int theScript)
{
  ScriptEntry *scriptEntry = pointerTo(header->scriptTableAddress);

  while (theScript > 1) {
    scriptEntry++;
    theScript--;
  }
  return pointerTo(scriptEntry->stringAddress);
}


/*----------------------------------------------------------------------*/
static void moveActor(int theActor)
{
  ScriptEntry *scr;
  StepEntry *step;
  Aword previousInstance = current.instance;

  current.actor = theActor;
  current.instance = theActor;
  current.location = where(theActor, FALSE);
  if (theActor == HERO) {
    /* Ask him! */
    if (setjmp(forfeitLabel) == 0) {
      parse();
      capitalize = TRUE;
      fail = FALSE;			/* fail only aborts one actor */
    }
  } else if (admin[theActor].script != 0) {
    for (scr = (ScriptEntry *) pointerTo(header->scriptTableAddress); !endOfTable(scr); scr++) {
      if (scr->code == admin[theActor].script) {
	/* Find correct step in the list by indexing */
	step = (StepEntry *) pointerTo(scr->steps);
	step = (StepEntry *) &step[admin[theActor].step];
	/* Now execute it, maybe. First check wait count */
	if (admin[theActor].waitCount > 0) { /* Wait some more ? */
	  if (traceActor(theActor))
	    printf("), SCRIPT %s(%ld), STEP %ld, Waiting another %ld turns>\n",
		   scriptName(theActor, admin[theActor].script),
		   admin[theActor].script, admin[theActor].step+1,
		   admin[theActor].waitCount);
	  admin[theActor].waitCount--;
	  break;
	}
	/* Then check possible expression to wait for */
	if (step->exp != 0) {
	  if (traceActor(theActor))
	    printf("), SCRIPT %s(%ld), STEP %ld, Evaluating:>\n",
		   scriptName(theActor, admin[theActor].script),
		   admin[theActor].script, admin[theActor].step+1);
	  interpret(step->exp);
	  if (!(Abool)pop())
	    break;		/* Break loop, don't execute step*/
	}
	/* OK, so finally let him do his thing */
	admin[theActor].step++;		/* Increment step number before executing... */
	if (!endOfTable(step+1) && (step+1)->after != 0) {
	  interpret((step+1)->after);
	  admin[theActor].waitCount = pop();
	}
	if (traceActor(theActor))
	  printf("), SCRIPT %ld(%s), STEP %ld, Executing:>\n",
		 admin[theActor].script, 
		 scriptName(theActor, admin[theActor].script),
		 admin[theActor].step);
	interpret(step->stms);
	step++;
	/* ... so that we can see if he is USEing another script now */
	if (admin[theActor].step != 0 && endOfTable(step))
	  /* No more steps in this script, so stop him */
	  admin[theActor].script = 0;
	fail = FALSE;			/* fail only aborts one actor */
	break;			/* We have executed a script so leave loop */
      }
    }
    if (endOfTable(scr))
      syserr("Unknown actor script.");
  } else {
    if (sectionTraceOption) {
      printf("\n<ACTOR %d, ", theActor);
      traceSay(theActor);
      printf(" (at ");
      traceSay(current.location);
      printf("), Idle>\n");
    }
  }

  current.instance = previousInstance;
}

/*======================================================================*/
void run(void)
{
  int i;
  Bool playerChangedState;

  openFiles();
  load();			/* Load program */

  setjmp(restartLabel);	/* Return here if he wanted to restart */

  initUndoStack();

  if (setjmp(returnLabel) == NO_JUMP_RETURN)
    init();			/* Initialise and start the adventure */

  while (TRUE) {
    if (debugOption)
      debug(FALSE, 0, 0);

    runPendingEvents();
    current.tick++;

    /* Return here if error during execution */
    switch (setjmp(returnLabel)) {
    case NO_JUMP_RETURN:
      break;
    case ERROR_RETURN:
      forgetGameState();
      forceNewPlayerInput();
      break;
    case UNDO_RETURN:
      forceNewPlayerInput();
      break;
    default:
      syserr("Unexpected longjmp() return value");
    }

#ifdef DMALLOC
    dmalloc_verify(0);
#endif
    depth = 0;

    /* Move all characters, hero first */
    pushGameState();

    /* TODO: Why 'playerChangedState since gameStateChanged is sufficient */
    playerChangedState = FALSE;
    moveActor(header->theHero);
    playerChangedState = gameStateChanged;

    if (gameStateChanged)
      rememberCommands();
    else
      forgetGameState();

    rules();
    for (i = 1; i <= header->instanceMax; i++)
      if (i != header->theHero && isActor(i)) {
	moveActor(i);
	rules();
      }
  }
}
