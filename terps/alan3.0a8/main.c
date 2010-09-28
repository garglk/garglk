/*----------------------------------------------------------------------*\
 
  MAIN.C
 
  Main module of interpreter for ALAN Adventure Language
 
  \*----------------------------------------------------------------------*/
#include "main.h"

/* Imports: */
#include "state.h"
#include "lists.h"
#include "syserr.h"
#include "options.h"
#include "utils.h"
#include "args.h"
#include "parse.h"
#include "scan.h"
#include "inter.h"
#include "rules.h"
#include "reverse.h"
#include "debug.h"
#include "exe.h"
#include "term.h"
#include "instance.h"
#include "memory.h"
#include "output.h"
#include "Container.h"
#include "Location.h"
#include "dictionary.h"
#include "class.h"
#include "score.h"
#include "decode.h"
#include "msg.h"
#include "event.h"
#include "syntax.h"
#include "current.h"
#include "literal.h"


#include <time.h>
#ifdef USE_READLINE
#include "readline.h"
#endif

#include "alan.version.h"


#ifdef HAVE_GLK
#include "glk.h"
#include "glkio.h"
#ifdef HAVE_WINGLK
#include "winglk.h"
#endif
#endif

/* PUBLIC DATA */

/* Amachine structures - Static */
VerbEntry *vrbs;		/* Verb table pointer */


/* PRIVATE DATA */
#define STACKSIZE 100



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




/*----------------------------------------------------------------------*
 *
 * Event Handling
 *
 * eventchk()
 *
 *----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
static void runPendingEvents(void)
{
    int i;
	
    while (eventQueueTop != 0 && eventQueue[eventQueueTop-1].after == 0) {
        eventQueueTop--;
        if (isALocation(eventQueue[eventQueueTop].where))
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
static char logFileName[256] = "";


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
    char errorMessage[200];
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
    Bool developmentVersion;
    Bool alphaVersion;
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
    int i;
    char err[100];
	
    rewind(codfil);
    fread(&tmphdr, sizeof(tmphdr), 1, codfil);
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
                crc, tmphdr.acdcrc);
        if (!ignoreErrorOption)
            syserr(err);
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
	
    if (debugOption || regressionTestOption) /* If debugging... */
        srand(1);			/* ... use no randomization */
    else
        srand(time(0));		/* Else seed random generator */
}


/*----------------------------------------------------------------------*/
static void initStaticData(void)
{
    /* Dictionary */
    dictionary = (DictionaryEntry *) pointerTo(header->dictionary);
    /* Find out number of entries in dictionary */
    for (dictionarySize = 0; !isEndOfArray(&dictionary[dictionarySize]); dictionarySize++);
	
    /* Scores */
	
	
    /* All addresses to tables indexed by ids are converted to pointers,
       then adjusted to point to the (imaginary) element before the
       actual table so that [0] does not exist. Instead indices goes
       from 1 and we can use [1]. */
	
    if (header->instanceTableAddress == 0)
        syserr("Instance table pointer == 0");
    instances = (InstanceEntry *) pointerTo(header->instanceTableAddress);
    instances--;			/* Back up one so that first is no. 1 */
	
	
    if (header->classTableAddress == 0)
        syserr("Class table pointer == 0");
    classes = (ClassEntry *) pointerTo(header->classTableAddress);
    classes--;			/* Back up one so that first is no. 1 */
	
    if (header->containerTableAddress != 0) {
        containers = (ContainerEntry *) pointerTo(header->containerTableAddress);
        containers--;
    }
	
    if (header->eventTableAddress != 0) {
        events = (EventEntry *) pointerTo(header->eventTableAddress);
        events--;
    }
	
    /* Scores, if already allocated, copy initial data */
    if (scores == NULL)
        scores = duplicate((Aword *) pointerTo(header->scores), header->scoreCount*sizeof(Aword));
    else
        memcpy(scores, pointerTo(header->scores), header->scoreCount*sizeof(Aword));
	
    if (literals == NULL)
        literals = allocate(sizeof(Aword)*(MAXPARAMS+1));
	
    stxs = (SyntaxEntry *) pointerTo(header->syntaxTableAddress);
    vrbs = (VerbEntry *) pointerTo(header->verbTableAddress);
    ruls = (RuleEntry *) pointerTo(header->ruleTableAddress);
    msgs = (MessageEntry *) pointerTo(header->messageTableAddress);
	
    if (header->pack)
        freq = (Aword *) pointerTo(header->freq);
}


/*----------------------------------------------------------------------*/
static void initStrings(void)
{
    StringInitEntry *init;
	
    for (init = (StringInitEntry *) pointerTo(header->stringInitTable); !isEndOfArray(init); init++)
        setInstanceAttribute(init->instanceCode, init->attributeCode, (Aword)getStringFromFile(init->fpos, init->len));
}

/*----------------------------------------------------------------------*/
static Aint sizeOfAttributeData(void)
{
    int i;
    int size = 0;
	
    for (i=1; i<=header->instanceMax; i++) {
        AttributeEntry *attribute = pointerTo(instances[i].initialAttributes);
        while (!isEndOfArray(attribute)) {
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
        AttributeEntry *originalAttribute = pointerTo(instances[i].initialAttributes);
        admin[i].attributes = (AttributeEntry *)currentAttributeArea;
        while (!isEndOfArray(originalAttribute)) {
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
        admin[instanceId].location = instances[instanceId].initialLocation;
}


/*----------------------------------------------------------------------*/
static void runInheritedInitialize(Aint theClass) {
    if (theClass == 0) return;
    runInheritedInitialize(classes[theClass].parent);
    if (classes[theClass].initialize)
        interpret(classes[theClass].initialize);
}


/*----------------------------------------------------------------------*/
static void runInitialize(Aint theInstance) {
    runInheritedInitialize(instances[theInstance].parent);
    if (instances[theInstance].initialize != 0)
        interpret(instances[theInstance].initialize);
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

    if (where(HERO, FALSE) == startloc)
        look();
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
        sprintf(logFileName, "%s%d%s.log", adventureName, (int)tick, usr);
#ifdef HAVE_GLK
        glui32 fileUsage = transcriptOption?fileusage_Transcript:fileusage_InputRecord;
        frefid_t logFileRef = glk_fileref_create_by_name(fileUsage, logFileName, 0);
        logFile = glk_stream_open_file(logFileRef, filemode_Write, 0);
#else
        logFile = fopen(logFileName, "w");
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
        sprintf(str, "version %ld.%ld%s%ld",
                (long)alan.version.version,
                (long)alan.version.revision,
                alan.version.state[0]=='\0'?".":alan.version.state,
                (long)alan.version.correction);
        output(str);
	if (BUILD != 0)
	    sprintf(str, "- build %d!>$n", BUILD);
	else
	    sprintf(str, "!>$n");
	output(str);
    }
	
    /* Initialise some status */
    eventQueueTop = 0;			/* No pending events */
    initStaticData();
    initDynamicData();
    initParsing();
    checkDebug();
	
    getPageSize();
	
    /* Find first conjunction and use that for ',' handling */
    for (i = 0; i < dictionarySize; i++)
        if (isConjunction(i)) {
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



/*----------------------------------------------------------------------*/
static Bool traceActor(int theActor)
{
    if (sectionTraceOption) {
        printf("\n<ACTOR ");
        traceSay(theActor);
        printf("[%d]", theActor);
	if (current.location != 0) {
	    printf(" (at ");
	    traceSay(current.location);
	} else
	    printf(" (nowhere");
        printf("[%d])", current.location);
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
            parse(globalParameters);
            capitalize = TRUE;
            fail = FALSE;			/* fail only aborts one actor */
        }
    } else if (admin[theActor].script != 0) {
        for (scr = (ScriptEntry *) pointerTo(header->scriptTableAddress); !isEndOfArray(scr); scr++) {
            if (scr->code == admin[theActor].script) {
                /* Find correct step in the list by indexing */
                step = (StepEntry *) pointerTo(scr->steps);
                step = (StepEntry *) &step[admin[theActor].step];
                /* Now execute it, maybe. First check wait count */
                if (admin[theActor].waitCount > 0) { /* Wait some more ? */
                    if (traceActor(theActor))
                        printf(", SCRIPT %s[%ld], STEP %ld, Waiting another %ld turns>\n",
                               scriptName(theActor, admin[theActor].script),
                               admin[theActor].script, admin[theActor].step+1,
                               admin[theActor].waitCount);
                    admin[theActor].waitCount--;
                    break;
                }
                /* Then check possible expression to wait for */
                if (step->exp != 0) {
                    if (traceActor(theActor))
                        printf(", SCRIPT %s[%ld], STEP %ld, Evaluating:>\n",
                               scriptName(theActor, admin[theActor].script),
                               admin[theActor].script, admin[theActor].step+1);
                    if (!evaluate(step->exp))
                        break;		/* Break loop, don't execute step*/
                }
                /* OK, so finally let him do his thing */
                admin[theActor].step++;		/* Increment step number before executing... */
                if (!isEndOfArray(step+1) && (step+1)->after != 0) {
                    admin[theActor].waitCount = evaluate((step+1)->after);
                }
                if (traceActor(theActor))
                    printf(", SCRIPT %s[%ld], STEP %ld, Executing:>\n",
                           scriptName(theActor, admin[theActor].script),
                           admin[theActor].script,
                           admin[theActor].step);
                interpret(step->stms);
                step++;
                /* ... so that we can see if he is USEing another script now */
                if (admin[theActor].step != 0 && isEndOfArray(step))
                    /* No more steps in this script, so stop him */
                    admin[theActor].script = 0;
                fail = FALSE;			/* fail only aborts one actor */
                break;			/* We have executed a script so leave loop */
            }
        }
        if (isEndOfArray(scr))
            syserr("Unknown actor script.");
    } else {
        if (traceActor(theActor)) {
            printf(", Idle>\n");
        }
    }
	
    current.instance = previousInstance;
}

#define RESTARTED (setjmp(restartLabel) != NO_JUMP_RETURN)
#define ERROR_RETURNED (setjmp(returnLabel) != NO_JUMP_RETURN)

/*======================================================================*/
void run(void)
{
    int i;
    Bool playerChangedState;
    Stack theStack = NULL;
	
    openFiles();
    load();			/* Load program */
	
    if (RESTARTED) {
        deleteStack(theStack);
    }
	
    theStack = createStack(STACKSIZE);
    setInterpreterStack(theStack);
	
    initStateStack();
	
    if (!ERROR_RETURNED)   /* Can happen in start section to... */
        init();			   /* Initialise and start the adventure */
	
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
            //printf("ERROR_RETURN\n");
            forgetGameState();
            forceNewPlayerInput();
            break;
        case UNDO_RETURN:
            forceNewPlayerInput();
            break;
        default:
            syserr("Unexpected longjmp() return value");
        }
		
        recursionDepth = 0;
		
        /* Move all characters, hero first */
        rememberGameState();
		
        /* TODO: Why 'playerChangedState' since gameStateChanged is sufficient
         * Actually it isn't since it might have been one of the events or other actors
         * that changed the state. Why is this important?
         * Yes, because for UNDO we want to undo the last command the player did that
         * changed the state, not any of the others. */
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
#ifdef SMARTALLOC
    sm_dump(1);
#endif
}
