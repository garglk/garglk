/*----------------------------------------------------------------------*\

  debug.c

  Debugger unit in Alan interpreter ARUN

\*----------------------------------------------------------------------*/

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>


#include "types.h"
#ifdef HAVE_SHORT_FILENAMES
#include "av.h"
#else
#include "alan.version.h"
#endif


#ifdef USE_READLINE
#include "readline.h"
#endif

#include "types.h"
#include "inter.h"
#include "main.h"
#include "parse.h"
#include "stack.h"
#include "exe.h"

#include "debug.h"

#ifdef HAVE_GLK
#define MAP_STDIO_TO_GLK
#include "glkio.h"
#endif


/* PUBLIC: */
int breakpointCount = 0;
Breakpoint breakpoint[BREAKPOINTMAX];

#define debugPrefix "adbg: "

/*----------------------------------------------------------------------*/
static void showAttributes(AttributeEntry *attributes)
{
  AttributeEntry *at;
  int i;
  char str[80];

  if (attributes == 0) return;

  i = 1;
  for (at = attributes; !endOfTable(at); at++) {
    sprintf(str, "$i$t%s(%ld) = %ld", (char *) pointerTo(at->stringAddress), at->code, at->value);
#if ISO == 0
    fromIso(str, str);
#endif
    output(str);
    i++;
  }
}


/*----------------------------------------------------------------------*/
static void showContents(int cnt)
{
  int i;
  char str[80];
  Abool found = FALSE;

  output("$iContains:");
  for (i = 1; i <= header->instanceMax; i++) {
    if (in(i, cnt, TRUE)) { /* Yes, it's directly in this container */
      if (!found)
	found = TRUE;
      output("$i$t");
      say(i);
      sprintf(str, "(%d) ", i);
      output(str);
    }
  }
  if (!found)
    output("nothing");
}


/*----------------------------------------------------------------------*/
static void showInstanceLocation(int ins) {
  char buffer[1000];

  if (admin[ins].location == 0)
    output("nowhere (0)");
  else if (isLocation(admin[ins].location)) {
    output("at");
    say(admin[ins].location);
    sprintf(buffer, "(%ld)", admin[ins].location);
    output(buffer);
  } else if (isContainer(admin[ins].location)) {

    if (isObject(admin[ins].location))
      output("in");
    else if (isActor(admin[ins].location))
      output("carried by");
    say(admin[ins].location);
    sprintf(buffer, "(%ld)", admin[ins].location);
    output(buffer);

  } else
    output("Illegal location!");
}


/*----------------------------------------------------------------------*/
static void showInstances(void)
{
  char str[80];
  int ins;

  output("Instances:");
  for (ins = 1; ins <= header->instanceMax; ins++) {
    sprintf(str, "$i%3d: ", ins);
    output(str);
    say(ins);
    if (instance[ins].container)
      output("(container)");
    output(",");
    showInstanceLocation(ins);
  }
}

/*----------------------------------------------------------------------*/
static void showInstance(int ins)
{
  char str[80];

  if (ins > header->instanceMax) {
    sprintf(str, "Instance code %d is out of range", ins);
    output(str);
    return;
  }

  output("The");
  say(ins);
  sprintf(str, "(%d)", ins);
  output(str);
  if (instance[ins].parent) {
    sprintf(str, "Isa %s", (char *)pointerTo(class[instance[ins].parent].id));
    output(str);
  }

  if (!isA(ins, header->locationClassId)) {
    sprintf(str, "$iLocation: ");
    output(str);
    showInstanceLocation(ins);
  }

  output("$iAttributes:");
  showAttributes(admin[ins].attributes);

  if (instance[ins].container)
    showContents(ins);
      
  if (isA(ins, header->actorClassId)) {
    if (admin[ins].script == 0)
      output("$iIs idle");
    else {
      sprintf(str, "$iExecuting script: %ld, Step: %ld", admin[ins].script, admin[ins].step);
      output(str);
    }
  }
}


/*----------------------------------------------------------------------*/
static void showObjects(void)
{
  char str[80];
  int obj;

  output("Objects:");
  for (obj = 1; obj <= header->instanceMax; obj++) {
    if (isObject(obj)) {
      sprintf(str, "$i%3d: ", obj);
      output(str);
      say(obj);
    }
  }
}


/*----------------------------------------------------------------------*/
static void showObject(int obj)
{
  char str[80];


  if (!isObject(obj)) {
    sprintf(str, "Instance %d is not an object", obj);
    output(str);
    return;
  }

  showInstance(obj);

}


/*----------------------------------------------------------------------*/
static void showcnts(void)
{
  char str[80];
  int cnt;

  output("Containers:");
  for (cnt = 1; cnt <= header->containerMax; cnt++) {
    sprintf(str, "$i%3d: ", cnt);
    output(str);
    if (container[cnt].owner != 0)
      say(container[cnt].owner);
  }

}


/*----------------------------------------------------------------------*/
static void showClass(int c)
{
  char str[80];

  output("$t");
  output((char *)pointerTo(class[c].id));
  sprintf(str, "(%d)", c);
  output(str);
  if (class[c].parent != 0) {
    output("Isa");
    output((char *)pointerTo(class[class[c].parent].id));
    sprintf(str, "(%ld)", class[c].parent);
    output(str);
  }
}


/*----------------------------------------------------------------------*/
static void showClasses(void)
{
  int c;

  output("Classes:");
  for (c = 1; c <= header->classMax; c++) {
    output("$n");
    showClass(c);
  }
}


/*----------------------------------------------------------------------*/
static void showContainer(int cnt)
{
  char str[80];

  if (cnt < 1 || cnt > header->containerMax) {
    sprintf(str, "Container number out of range. Between 1 and %ld, please.", header->containerMax);
    output(str);
    return;
  }

  sprintf(str, "Container %d :", cnt);
  output(str);
  if (container[cnt].owner != 0) {
    cnt = container[cnt].owner;
    say(cnt);
    sprintf(str, "$iLocation: %ld", where(cnt, TRUE));
    output(str);
  }
  showContents(cnt);
}


/*----------------------------------------------------------------------*/
static void showLocations(void)
{
  char str[80];
  int loc;

  output("Locations:");
  for (loc = 1; loc <= header->instanceMax; loc++) {
    if (isLocation(loc)) {
      sprintf(str, "$i%3d: ", loc);
      output(str);
      say(loc);
    }
  }
}


/*----------------------------------------------------------------------*/
static void showLocation(int loc)
{
  char str[80];


  if (!isLocation(loc)) {
    sprintf(str, "Instance %d is not a location.", loc);
    output(str);
    return;
  }

  output("The ");
  say(loc);
  sprintf(str, "(%d) Isa location :", loc);
  output(str);

  output("$iAttributes =");
  showAttributes(admin[loc].attributes);
}


/*----------------------------------------------------------------------*/
static void showActors(void)
{
  char str[80];
  int act;

  output("Actors:");
  for (act = 1; act <= header->instanceMax; act++) {
    if (isActor(act)) {
      output("$i");
      say(act);
      sprintf(str, "(%d)", act);
      output(str);
      if (instance[act].container)
	output("(container)");
    }
  }
}


/*----------------------------------------------------------------------*/
static void showActor(int act)
{
  char str[80];
  
  if (!isActor(act)) {
    sprintf(str, "Instance %d is not an actor.", act);
    output(str);
    return;
  }
  
  showInstance(act);
}


/*----------------------------------------------------------------------*/
static void showEvents(void)
{
  int event, i;
  char str[80];
  Bool scheduled;

  output("Events:");
  for (event = 1; event <= header->eventMax; event++) {
    sprintf(str, "$i%d (%s):", event, (char *)pointerTo(events[event].stringAddress));
#if ISO == 0
    fromIso(str, str);
#endif
    output(str);
    scheduled = FALSE;
    for (i = 0; i < eventQueueTop; i++)
      if ((scheduled = (eventQueue[i].event == event)))
	break;
    if (scheduled) {
      sprintf(str, "Scheduled for +%d, at ", eventQueue[i].time-current.tick);
      output(str);
      say(eventQueue[i].where);
    } else
      output("Not scheduled.");
  }
}


/*----------------------------------------------------------------------*/
static int sourceFileNumber(char *fileName) {
  SourceFileEntry *entries = pointerTo(header->sourceFileTable);
  int n;

  for (n = 0; *(Aword*)&entries[n] != EOF; n++) {
    char *entryName;
    entryName = getStringFromFile(entries[n].fpos, entries[n].len);
    if (strcmp(entryName, fileName) == 0) return n;
    entryName = baseNameStart(entryName);
    if (strcmp(entryName, fileName) == 0) return n;
  }
  return -1;
}


/*======================================================================*/
char *sourceFileName(int fileNumber) {
  SourceFileEntry *entries = pointerTo(header->sourceFileTable);

  return getStringFromFile(entries[fileNumber].fpos, entries[fileNumber].len);
}


/*======================================================================*/
char *readSourceLine(int file, int line) {
  int count;
#define SOURCELINELENGTH 1000
  static char buffer[SOURCELINELENGTH];

#ifdef HAVE_GLK
  frefid_t sourceFileRef;
  strid_t sourceFile;

  sourceFileRef = glk_fileref_create_by_name(fileusage_TextMode, sourceFileName(file), 0);
  sourceFile = glk_stream_open_file(sourceFileRef, filemode_Read, 0);
#else
  FILE *sourceFile;

  sourceFile  = fopen(sourceFileName(file), "r");
#endif

  if (sourceFile != NULL) {
    for (count = 0; count < line; count++) {
      if (fgets(buffer, SOURCELINELENGTH, sourceFile) == 0)
	return NULL;
      /* If not read the whole line, or no newline, try to read again */
      while (strchr(buffer, '\n') == NULL)
	if (fgets(buffer, SOURCELINELENGTH, sourceFile) == 0)
	  return buffer;
    }
    fclose(sourceFile);
    return buffer;
  }
  return NULL;
}

/*======================================================================*/
void showSourceLine(int fileNumber, int line) {
  char *buffer = readSourceLine(fileNumber, line);
  if (buffer != NULL) {
    if (buffer[strlen(buffer)-1] == '\n')
      buffer[strlen(buffer)-1] = '\0';
    printf("<%05d>: %s", line, buffer);
  }
}


/*----------------------------------------------------------------------*/
static void listFiles() {
  SourceFileEntry *entry;
  int i = 0;
  for (entry = pointerTo(header->sourceFileTable); *((Aword*)entry) != EOF; entry++) {
    printf("  %2d : %s\n", i, sourceFileName(i));
    i++;
  }
}


/*----------------------------------------------------------------------*/
static void listLines() {
  SourceLineEntry *entry;
  for (entry = pointerTo(header->sourceLineTable); *((Aword*)entry) != EOF; entry++)
    printf("  %s:%ld\n", sourceFileName(entry->file), entry->line);
}


/*----------------------------------------------------------------------*/
static int findSourceLine(int file, int line) {
  /* Will return index to the closest line available */
  SourceLineEntry *entry = pointerTo(header->sourceLineTable);
  int i = 0;

  while (!endOfTable(&entry[i]) && entry[i].file < file && entry[i].line < line)
    i++;
  if (endOfTable(entry))
    return i-1;
  else
    return i;
}


/*----------------------------------------------------------------------*/
static void listBreakpoints() {
  int i;
  Bool found = FALSE;

  for (i = 0; i < BREAKPOINTMAX; i++)
    if (breakpoint[i].line != 0) {
      if (!found)
	printf("Breakpoints set:\n");
      found = TRUE;
      printf("    %s:%d\n", sourceFileName(breakpoint[i].file), breakpoint[i].line);
    }
  if (!found)
    printf("No breakpoints set.\n");
}


/*======================================================================*/
Bool breakpointIndex(int file, int line) {
  int i;

  for (i = 0; i < BREAKPOINTMAX; i++)
    if (breakpoint[i].line == line && breakpoint[i].file == file)
      return i;
  return(-1);
}


/*----------------------------------------------------------------------*/
static int availableBreakpointSlot() {
  int i;

  for (i = 0; i < BREAKPOINTMAX; i++)
    if (breakpoint[i].line == 0)
      return i;
  return -1;
}


/*----------------------------------------------------------------------*/
static void setBreakpoint(int file, int line) {
  int i = breakpointIndex(file, line);

  if (i != -1)
    printf("Breakpoint already set at %s:%d.\n", sourceFileName(file), line);
  else {
    i = availableBreakpointSlot();
    if (i == -1)
      printf("No room for more breakpoints. Delete one first.\n");
    else {
      int lineIndex = findSourceLine(file, line);
      SourceLineEntry *entry = pointerTo(header->sourceLineTable);
      breakpoint[i].file = entry[lineIndex].file;
      breakpoint[i].line = entry[lineIndex].line;
      printf("Breakpoint set at %s:%ld.\n", sourceFileName(entry[lineIndex].file), entry[lineIndex].line);
      showSourceLine(entry[lineIndex].file, entry[lineIndex].line);
    }
  }
}


/*----------------------------------------------------------------------*/
static void deleteBreakpoint(int line, int file) {
  int i = breakpointIndex(file, line);

  if (i == -1)
    printf("No breakpoint set at %s:%d.\n", sourceFileName(file), line);
  else {
    breakpoint[i].line = 0;
    printf("Breakpoint at %s:%d deleted.\n", sourceFileName(file), line);
  }
}



static Bool trc, stp, cap, psh, stk;
static int loc;

/*======================================================================*/
void saveInfo(void)
{
  /* Save some important things */
  cap = capitalize; capitalize = FALSE;
  trc = sectionTraceOption; sectionTraceOption = FALSE;
  stp = singleStepOption; singleStepOption = FALSE;
  psh = tracePushOption; tracePushOption = FALSE;
  stk = traceStackOption; traceStackOption = FALSE;
  loc = current.location; current.location = where(HERO, TRUE);
}


/*======================================================================*/
void restoreInfo(void)
{
  /* Restore! */
  capitalize = cap;
  sectionTraceOption = trc;
  singleStepOption = stp;
  tracePushOption = psh;
  traceStackOption = stk;
  current.location = loc;
}

/*======================================================================*/
void debug(Bool calledFromBreakpoint, int line, int fileNumber)
{
  char buf[256];
  char c;
  int i;

  saveInfo();

  if (calledFromBreakpoint) {
    char *cause;
    if (anyOutput) newline();
    if (breakpointIndex(fileNumber, line) != -1)
      cause = "Breakpoint hit at";
    else
      cause = "Stepping to";
    printf("%s %s %s:%d.\n", debugPrefix, cause, sourceFileName(fileNumber), line);
    showSourceLine(fileNumber, line);
    printf("\n");
    anyOutput = FALSE;
  }

  while (TRUE) {
    if (anyOutput) newline();
    do {
      capitalize = FALSE;
      output("adbg> ");
#ifdef USE_READLINE
      (void) readline(buf);
#else
      fgets(buf, 255, stdin);
#endif
      lin = 1;
      c = buf[0];
      i = 0;
      sscanf(&buf[1], "%d", &i);
    } while (buf && c == '\0');

    switch (toUpper(c)) {
    case 'H':
    case '?':
      output(alan.longHeader);
      output("$nADBG Commands:\
      $iB [n] -- set breakpoint at source line [n]\
      $iB     -- list breakpoints set\
      $iD [n] -- delete breakpoint at source line [n]\
      $iD     -- delete breakpoint at current source line [n]\
      $iF     -- list files\
      $iN     -- execute to next source line\
      $iX     -- exit debug mode and return to game, get back with 'debug'\
      $iQ     -- quit game\
      $iC     -- show class hierarchy\
      $iI [n] -- show instances or instance [n]\
      $iO [n] -- show instances that are object[s]\
      $iA [n] -- show instances that are actor[s]\
      $iL [n] -- show instances that are location[s]\
      $iE     -- show events\
      $iG     -- go another player turn\
      $iT     -- toggle trace mode\
      $iS     -- toggle single step trace mode\
");
      break;

    case 'Q':
      terminate(0);
      break;

    case 'X':
      debugOption = FALSE;		/* Fall through to 'G' */
    case 'G':
      restoreInfo();
      return;

    case 'F':
      listFiles();
      listLines();
      break;

    case 'I':
      if (i == 0)
	showInstances();
      else
	showInstance(i);
      break;

    case 'O':
      if (i == 0)
        showObjects();
      else
	showObject(i);
      break;
    case 'C':
      if (i == 0)
        showClasses();
      else
	showClass(i);
      break;
    case 'A':
      if (i == 0)
        showActors();
      else
	showActor(i);
      break;
    case 'L':
      if (i == 0)
        showLocations();
      else
	showLocation(i);
      break;
    case 'E':
      showEvents();
      break;
    case 'S':
      if ((stp = !stp))
        printf("Step on.");
      else
        printf("Step off.");
      break;      
    case 'T':
      if ((trc = !trc))
        printf("Trace on.");
      else
        printf("Trace off.");
      break;
    case 'B':
      if (i == 0)
	listBreakpoints();
      else
	setBreakpoint(fileNumber, i);
      break;
    case 'D':
      if (i == 0) {
	if (calledFromBreakpoint)
	  deleteBreakpoint(line, fileNumber);
	else
	  printf("No current breakpoint to delete.\n");
      } else
	deleteBreakpoint(i, fileNumber);
      break;
    case 'N':
      stopAtNextLine = TRUE;
      debugOption = FALSE;
      if (!calledFromBreakpoint)
	current.sourceLine = 0;
      restoreInfo();
      return;
    default:
      output("Unknown ADBG command. ? for help.");
      break;
    }
  }
}


/*======================================================================*/
void traceSay(int item)
{
  /*
    Say somethin, but make sure we don't disturb anything and that it is
    shown to the player. Needed for tracing. During debugging things are
    set up to avoid this problem.
  */

  saveInfo();
  needSpace = FALSE;
  col = 1;
  if (item == 0)
    printf("$null$");
  else
    say(item);
  needSpace = FALSE;
  col = 1;
  restoreInfo();
}
