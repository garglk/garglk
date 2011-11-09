/*----------------------------------------------------------------------*\

  debug.c

  Debugger unit in Alan interpreter ARUN
  
  \*----------------------------------------------------------------------*/

#include "debug.h"


/* Imports: */

#include "sysdep.h"
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

#include "alan.version.h"

#ifdef USE_READLINE
#include "readline.h"
#endif

#include "lists.h"
#include "inter.h"
#include "current.h"
#include "options.h"
#include "utils.h"
#include "instance.h"
#include "memory.h"
#include "output.h"
#include "class.h"
#include "event.h"
#include "exe.h"

#ifdef HAVE_GLK
#include "glk.h"
#define MAP_STDIO_TO_GLK
#include "glkio.h"
#endif

#define BREAKPOINTMAX 50


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

    if (attributes == 0)
        return;

    i = 1;
    for (at = attributes; !isEndOfArray(at); at++) {
        sprintf(str, "$i$t%s[%d] = %d", (char *) pointerTo(at->id), at->code, (int)at->value);
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
            sprintf(str, "[%d] ", i);
            output(str);
        }
    }
    if (!found)
        output("nothing");
}


/*----------------------------------------------------------------------*/
static void showInstanceLocation(int ins, char *prefix) {
    char buffer[1000];

    if (admin[ins].location == 0)
        return;
    else {
        output(prefix);
        if (isALocation(admin[ins].location)) {
            output("at");
            say(admin[ins].location);
            sprintf(buffer, "[%d]", admin[ins].location);
            output(buffer);
        } else if (isContainer(admin[ins].location)) {
		  
            if (isObject(admin[ins].location))
                output("in");
            else if (isActor(admin[ins].location))
                output("carried by");
            say(admin[ins].location);
            sprintf(buffer, "[%d]", admin[ins].location);
            output(buffer);

        } else
            output("Illegal location!");
    }
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
        if (instances[ins].container)
            output("(container)");
        showInstanceLocation(ins, ", ");
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
    sprintf(str, "[%d]", ins);
    output(str);
    if (instances[ins].parent) {
        sprintf(str, "Isa %s[%d]", idOfClass(instances[ins].parent), instances[ins].parent);
        output(str);
    }

    if (!isA(ins, header->locationClassId) || (isA(ins, header->locationClassId) && admin[ins].location != 0)) {
        sprintf(str, "$iLocation: ");
        output(str);
        showInstanceLocation(ins, " ");
    }

    output("$iAttributes:");
    showAttributes(admin[ins].attributes);

    if (instances[ins].container)
        showContents(ins);

    if (isA(ins, header->actorClassId)) {
        if (admin[ins].script == 0)
            output("$iIs idle");
        else {
            sprintf(str, "$iExecuting script: %d, Step: %d", admin[ins].script, admin[ins].step);
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

#ifdef UNDEF_WHEN_NEEDED
/*----------------------------------------------------------------------*/
static void showcnts(void)
{
    char str[80];
    int cnt;

    output("Containers:");
    for (cnt = 1; cnt <= header->containerMax; cnt++) {
        sprintf(str, "$i%3d: ", cnt);
        output(str);
        if (containers[cnt].owner != 0)
            say(containers[cnt].owner);
    }

}

/*----------------------------------------------------------------------*/
static void showContainer(int cnt)
{
    char str[80];

    if (cnt < 1 || cnt > header->containerMax) {
        sprintf(str, "Container number out of range. Between 1 and %d, please.", header->containerMax);
        output(str);
        return;
    }

    sprintf(str, "Container %d :", cnt);
    output(str);
    if (containers[cnt].owner != 0) {
        cnt = containers[cnt].owner;
        say(cnt);
        sprintf(str, "$iLocation: %d", where(cnt, TRUE));
        output(str);
    }
    showContents(cnt);
}
#endif


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



/*----------------------------------------------------------------------*/
static void printClassName(int c) {
    output(idOfClass(c));
}


/*----------------------------------------------------------------------*/
static void showClassInheritance(int c) {
    char str[80];

    if (classes[c].parent != 0) {
        output(", Isa");
        printClassName(classes[c].parent);
        sprintf(str, "[%d]", classes[c].parent);
        output(str);
    }
}


/*----------------------------------------------------------------------*/
static void showClass(int c)
{
    char str[80];

    output("$t");
    printClassName(c);
    sprintf(str, "[%d]", c);
    output(str);
    showClassInheritance(c);
}


/*----------------------------------------------------------------------*/
static void listClass(int c)
{
    char str[80];

    sprintf(str, "%3d: ", c);
    output(str);
    printClassName(c);
    showClassInheritance(c);
}


/*----------------------------------------------------------------------*/
static void showClassHierarchy(int this, int depth)
{
    int i;
    int child;

    output("$i");
    for (i=0; i < depth; i++)
        output("$t");
    listClass(this);
    for (child = 1; child <= header->classMax; child++) {
        if (classes[child].parent == this) {
            showClassHierarchy(child, depth+1);
        }
    }
}


/*----------------------------------------------------------------------*/
static void showLocations(void)
{
    char str[80];
    int loc;

    output("Locations:");
    for (loc = 1; loc <= header->instanceMax; loc++) {
        if (isALocation(loc)) {
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


    if (!isALocation(loc)) {
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
            sprintf(str, "$i%3d: ", act);
            output(str);
            say(act);
            if (instances[act].container)
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
    bool scheduled;

    output("Events:");
    for (event = 1; event <= header->eventMax; event++) {
        sprintf(str, "$i%d [%s]:", event, (char *)pointerTo(events[event].id));
#if ISO == 0
        fromIso(str, str);
#endif
        output(str);
        scheduled = FALSE;
        for (i = 0; i < eventQueueTop; i++)
            if ((scheduled = (eventQueue[i].event == event)))
                break;
        if (scheduled) {
            sprintf(str, "Scheduled for +%d, at ", eventQueue[i].after);
            output(str);
            say(eventQueue[i].where);
        } else
            output("Not scheduled.");
    }
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
void listLines() {
    SourceLineEntry *entry;
    for (entry = pointerTo(header->sourceLineTable); *((Aword*)entry) != EOF; entry++)
        printf("  %s:%d\n", sourceFileName(entry->file), entry->line);
}


/*----------------------------------------------------------------------*/
static int findSourceLineIndex(SourceLineEntry *entry, int file, int line) {
    /* Will return index to the closest line available */
    int i = 0;

    while (!isEndOfArray(&entry[i]) && entry[i].file != file)
        i++;
    while (!isEndOfArray(&entry[i]) && entry[i].file == file  && entry[i].line < line)
        i++;
    if (isEndOfArray(entry) || entry[i].file != file)
        return i-1;
    else
        return i;
}


/*----------------------------------------------------------------------*/
static void listBreakpoints() {
    int i;
    bool found = FALSE;

    for (i = 0; i < BREAKPOINTMAX; i++)
        if (breakpoint[i].line != 0) {
            if (!found)
                printf("Breakpoints set:\n");
            found = TRUE;
            printf("    %s:%d\n", sourceFileName(breakpoint[i].file), breakpoint[i].line);
        }
    if (!found)
        printf("No breakpoints set\n");
}


/*======================================================================*/
int breakpointIndex(int file, int line) {
    int i;

    for (i = 0; i < BREAKPOINTMAX; i++)
        if (breakpoint[i].line == line && breakpoint[i].file == file)
            return i;
    return -1;
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
        printf("Breakpoint already set at %s:%d\n", sourceFileName(file), line);
    else {
        i = availableBreakpointSlot();
        if (i == -1)
            printf("No room for more breakpoints. Delete one first.\n");
        else {
            int lineIndex = findSourceLineIndex(pointerTo(header->sourceLineTable), file, line);
            SourceLineEntry *entry = pointerTo(header->sourceLineTable);
            char leadingText[100] = "Breakpoint";
            if (entry[lineIndex].file == EOF) {
                printf("Line %d not available\n", line);
            } else {
                if (entry[lineIndex].line != line)
                    sprintf(leadingText, "Line %d not available, breakpoint instead", line);
                breakpoint[i].file = entry[lineIndex].file;
                breakpoint[i].line = entry[lineIndex].line;
                printf("%s set at %s:%d\n", leadingText, sourceFileName(entry[lineIndex].file), entry[lineIndex].line);
                showSourceLine(entry[lineIndex].file, entry[lineIndex].line);
                printf("\n");
            }
        }
    }
}


/*----------------------------------------------------------------------*/
static void deleteBreakpoint(int line, int file) {
    int i = breakpointIndex(file, line);

    if (i == -1)
        printf("No breakpoint set at %s:%d\n", sourceFileName(file), line);
    else {
        breakpoint[i].line = 0;
        printf("Breakpoint at %s:%d deleted\n", sourceFileName(file), line);
    }
}



static bool trc, stp, cap, psh, stk;
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

#define HELP_COMMAND 'H'
#define QUIT_COMMAND 'Q'
#define EXIT_COMMAND 'X'
#define GO_COMMAND 'G'
#define FILES_COMMAND 'F'
#define INSTANCES_COMMAND 'I'
#define CLASSES_COMMAND 'C'
#define OBJECTS_COMMAND 'O'
#define ACTORS_COMMAND 'A'
#define LOCATIONS_COMMAND 'L'
#define EVENTS_COMMAND 'E'
#define BREAK_COMMAND 'B'
#define DELETE_COMMAND 'D'
#define SECTION_TRACE_COMMAND 'T'
#define INSTRUCTION_TRACE_COMMAND 'S'
#define NEXT_COMMAND 'N'
#define UNKNOWN_COMMAND '?'
#define AMBIGUOUS_COMMAND '-'

typedef struct DebugParseEntry {
    char *command;
	char *parameter;
    char code;
	char *helpText;
} DebugParseEntry;

static DebugParseEntry commandEntries[] = {
    {"help", "", HELP_COMMAND, "this help"},
    {"?", "", HELP_COMMAND, "d:o"},
    {"break", "[file:[n]]", BREAK_COMMAND, "set breakpoint at source line [n] in [file]"},
    {"delete", "[file:[n]]", DELETE_COMMAND, "delete breakpoint at source line [n] in [file]"},
    {"files", "", FILES_COMMAND, "list source files"},
    {"events", "", EVENTS_COMMAND, "show events"},
    {"classes", "", CLASSES_COMMAND, "show class hierarchy"},
    {"instances", "[n]", INSTANCES_COMMAND, "show instance(s)"},
    {"objects", "[n]", OBJECTS_COMMAND, "show instance(s) that are objects"},
    {"actors", "[n]", ACTORS_COMMAND, "show instance(s) that are actors"},
    {"locations", "[n]", LOCATIONS_COMMAND, "show instances that are locations"},
    {"section", "", SECTION_TRACE_COMMAND, "toggle section trace"},
    {"single", "", INSTRUCTION_TRACE_COMMAND, "toggle single instruction trace"},
    {"next", "", NEXT_COMMAND, "execute to next source line"},
    {"go", "", GO_COMMAND, "go another player turn"},
    {"exit", "", EXIT_COMMAND, "exit debug mode and return to game, get back with 'debug' as player input"},
    {"x", "", EXIT_COMMAND, "d:o"},
    {"quit", "", QUIT_COMMAND, "quit game"},
    {NULL, NULL}
};


static char *spaces(int length) {
	static char buf[200];
	int i;

	for (i = 0; i<length; i++)
		buf[i] = ' ';
	buf[i] = '\0';
	return buf;
}


static char *padding(DebugParseEntry *entry, int maxLength) {
	return spaces(maxLength-strlen(entry->command)-strlen(entry->parameter));
}


static void handleHelpCommand() {
	output(alan.longHeader);
	DebugParseEntry *entry = commandEntries;

	int maxLength = 0;
	for (entry = commandEntries; entry->command != NULL; entry++) {
		if (strlen(entry->command)+strlen(entry->parameter) > maxLength)
			maxLength = strlen(entry->command)+strlen(entry->parameter);
	}

	output("$nADBG Commands (can be abbreviated):");
	for (entry = commandEntries; entry->command != NULL; entry++) {
		char buf[200];
		sprintf(buf, "$i%s %s %s -- %s", entry->command, entry->parameter, padding(entry, maxLength), entry->helpText);
		output(buf);
	}
}


/*----------------------------------------------------------------------*/
static DebugParseEntry *findEntry(char *command, DebugParseEntry *entry) {
    while (entry->command != NULL) {
        if (strncasecmp(command, entry->command, strlen(command)) == 0)
            return entry;
        entry++;
    }
    return NULL;
}


/*----------------------------------------------------------------------*/
static char parseDebugCommand(char *command) {
    DebugParseEntry *entry = findEntry(command, commandEntries);
    if (entry != NULL) {
        if (strlen(command) < strlen(entry->command)) {
            if (findEntry(command, entry+1) != NULL)
                return AMBIGUOUS_COMMAND;
        }
        return entry->code;
    } else
        return UNKNOWN_COMMAND;
}


/*----------------------------------------------------------------------*/
static void readCommand(char buf[]) {
	char c;

	capitalize = FALSE;
	if (anyOutput) newline();
	do {
		output("adbg> ");

#ifdef USE_READLINE
		(void) readline(buf);
#else
		fgets(buf, 255, stdin);
#endif
		lin = 1;
		c = buf[0];
	} while (c == '\0');
}


/*----------------------------------------------------------------------*/
static void displaySourceLocation(int line, int fileNumber) {
	char *cause;
	if (anyOutput) newline();
	if (breakpointIndex(fileNumber, line) != -1)
		cause = "Breakpoint hit at";
	else
		cause = "Stepping to";
	printf("%s %s %s:%d\n", debugPrefix, cause, sourceFileName(fileNumber), line);
	showSourceLine(fileNumber, line);
	printf("\n");
	anyOutput = FALSE;
}


/*----------------------------------------------------------------------*/
static void toggleSectionTrace() {
	if ((trc = !trc))
		printf("Section trace on.");
	else
		printf("Section trace off.");
}


/*----------------------------------------------------------------------*/
static void handleBreakCommand(int fileNumber) {
	char *parameter = strtok(NULL, ":");
	if (parameter != NULL && isalpha((int)parameter[0])) {
		fileNumber = sourceFileNumber(parameter);
		if (fileNumber == -1) {
			printf("No such file: '%s'\n", parameter);
			return;
		}
		parameter = strtok(NULL, "");
	}
	if (parameter == NULL)
		listBreakpoints();
	else
		setBreakpoint(fileNumber, atoi(parameter));
}


/*----------------------------------------------------------------------*/
static void handleDeleteCommand(bool calledFromBreakpoint, int line, int fileNumber) {
	char *parameter = strtok(NULL, "");
	if (parameter == NULL) {
		if (calledFromBreakpoint)
			deleteBreakpoint(line, fileNumber);
		else
			printf("No current breakpoint to delete\n");
	} else
		deleteBreakpoint(atoi(parameter), fileNumber);
}


/*----------------------------------------------------------------------*/
static void handleNextCommand(bool calledFromBreakpoint) {
	stopAtNextLine = TRUE;
	debugOption = FALSE;
	if (!calledFromBreakpoint)
		current.sourceLine = 0;
	restoreInfo();
}


/*----------------------------------------------------------------------*/
static void toggleInstructionTrace() {
	if ((stp = !stp))
		printf("Single instruction trace on.");
	else
		printf("Single instruction trace off.");
}


/*----------------------------------------------------------------------*/
static void handleLocationsCommand() {
	char *parameter = strtok(NULL, "");
	if (parameter == 0)
		showLocations();
	else
		showLocation(atoi(parameter));
}


/*----------------------------------------------------------------------*/
static void handleActorsCommand() {
	char *parameter = strtok(NULL, "");
	if (parameter == NULL)
		showActors();
	else
		showActor(atoi(parameter));
}


/*----------------------------------------------------------------------*/
static void handleClassesCommand() {
	char *parameter = strtok(NULL, "");
	if (parameter == NULL) {
		output("Classes:");
		showClassHierarchy(1, 0);
	} else
		showClass(atoi(parameter));
}


/*----------------------------------------------------------------------*/
static void handleObjectsCommand() {
	char *parameter = strtok(NULL, "");
	if (parameter == NULL)
		showObjects();
	else
		showObject(atoi(parameter));
}


/*----------------------------------------------------------------------*/
static void handleInstancesCommand() {
	char *parameter = strtok(NULL, "");

	if (parameter == NULL)
		showInstances();
	else
		showInstance(atoi(parameter));
}


/*======================================================================*/
void debug(bool calledFromBreakpoint, int line, int fileNumber)
{
    saveInfo();

#ifdef HAVE_GLK
    glk_set_style(style_Preformatted);
#endif

    if (calledFromBreakpoint)
        displaySourceLocation(line, fileNumber);

    while (TRUE) {

        char commandLine[200];
        readCommand(commandLine);
        char *command = strtok(commandLine, " ");
        char commandCode = parseDebugCommand(command);

        switch (commandCode) {
		case AMBIGUOUS_COMMAND: output("Ambiguous ADBG command abbreviation. ? for help."); break;
        case ACTORS_COMMAND: handleActorsCommand(); break;
        case BREAK_COMMAND: handleBreakCommand(fileNumber); break;
        case CLASSES_COMMAND: handleClassesCommand(); break;
        case DELETE_COMMAND: handleDeleteCommand(calledFromBreakpoint, line, fileNumber); break;
        case EVENTS_COMMAND: showEvents(); break;
        case EXIT_COMMAND: debugOption = FALSE; restoreInfo(); goto exit_debug;
        case FILES_COMMAND: listFiles(); break;
        case GO_COMMAND: restoreInfo(); goto exit_debug;
        case HELP_COMMAND: handleHelpCommand(); break;
        case INSTANCES_COMMAND: handleInstancesCommand(); break;
        case INSTRUCTION_TRACE_COMMAND: toggleInstructionTrace(); break;
        case LOCATIONS_COMMAND: handleLocationsCommand(); break;
        case NEXT_COMMAND: handleNextCommand(calledFromBreakpoint); goto exit_debug;
        case OBJECTS_COMMAND: handleObjectsCommand(); break;
        case QUIT_COMMAND: terminate(0); break;
        case SECTION_TRACE_COMMAND: toggleSectionTrace(); break;
        default: output("Unknown ADBG command. ? for help."); break;
        }
    }

 exit_debug:
#ifdef HAVE_GLK
    glk_set_style(style_Normal);
#endif
    ;
}


/*======================================================================*/
void traceSay(int item)
{
    /*
      Say something, but make sure we don't disturb anything and that it is
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
