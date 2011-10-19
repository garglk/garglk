/*----------------------------------------------------------------------*\

  exe.c

  Amachine instruction execution unit of Alan interpreter

\*----------------------------------------------------------------------*/
#include "exe.h"


/* IMPORTS */
#include <time.h>

#include "types.h"
#include "sysdep.h"

#include "lists.h"
#include "state.h"
#include "syserr.h"
#include "term.h"
#include "utils.h"
#include "instance.h"
#include "inter.h"
#include "decode.h"
#include "save.h"
#include "memory.h"
#include "output.h"
#include "score.h"
#include "event.h"
#include "current.h"
#include "word.h"
#include "msg.h"
#include "actor.h"
#include "options.h"
#include "args.h"


#ifdef USE_READLINE
#include "readline.h"
#endif

#ifdef HAVE_GLK
#include "glk.h"
#define MAP_STDIO_TO_GLK
#include "glkio.h"
#endif


/* PUBLIC DATA */

FILE *textFile;

/* Long jump buffers */
// TODO move to longjump.c? or error.c, and abstract them into functions?
jmp_buf restartLabel;       /* Restart long jump return point */
jmp_buf returnLabel;        /* Error (or undo) long jump return point */
jmp_buf forfeitLabel;       /* Player forfeit by an empty command */


/* PRIVATE CONSTANTS */

#define WIDTH 80


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static char logFileName[256] = "";

/*======================================================================*/
void setStyle(int style)
{
#ifdef HAVE_GLK
    switch (style) {
    case NORMAL_STYLE: glk_set_style(style_Normal); break;
    case EMPHASIZED_STYLE: glk_set_style(style_Emphasized); break;
    case PREFORMATTED_STYLE: glk_set_style(style_Preformatted); break;
    case ALERT_STYLE: glk_set_style(style_Alert); break;
    case QUOTE_STYLE: glk_set_style(style_BlockQuote); break;
    }
#endif
}

/*======================================================================*/
void print(Aword fpos, Aword len)
{
    char str[2*WIDTH];            /* String buffer */
    int outlen = 0;               /* Current output length */
    int ch = 0;
    int i;
    long savfp = 0;		/* Temporary saved text file position */
    static bool printFlag = FALSE; /* Printing already? */
    bool savedPrintFlag = printFlag;
    void *info = NULL;		/* Saved decoding info */


    if (len == 0) return;

    if (isHere(HERO, TRUE)) {	/* Check if the player will see it */
        if (printFlag) {            /* Already printing? */
            /* Save current text file position and/or decoding info */
            if (header->pack)
                info = pushDecode();
            else
                savfp = ftell(textFile);
        }
        printFlag = TRUE;           /* We're printing now! */

        /* Position to start of text */
        fseek(textFile, fpos+header->stringOffset, 0);

        if (header->pack)
            startDecoding();
        for (outlen = 0; outlen != len; outlen = outlen + strlen(str)) {
            /* Fill the buffer from the beginning */
            for (i = 0; i <= WIDTH || (i > WIDTH && ch != ' '); i++) {
                if (outlen + i == len)  /* No more characters? */
                    break;
                if (header->pack)
                    ch = decodeChar();
                else
                    ch = getc(textFile);
                if (ch == EOFChar)      /* Or end of text? */
                    break;
                str[i] = ch;
            }
            str[i] = '\0';
#if ISO == 0
            fromIso(str, str);
#endif
            output(str);
        }

        /* And restore */
        printFlag = savedPrintFlag;
        if (printFlag) {
            if (header->pack)
                popDecode(info);
            else
                fseek(textFile, savfp, 0);
        }
    }
}


/*======================================================================*/
void sys(Aword fpos, Aword len)
{
    char *command;

    command = getStringFromFile(fpos, len);
    system(command);
    free(command);
}


/*======================================================================*/
char *getStringFromFile(Aword fpos, Aword len)
{
    char *buf = allocate(len+1);
    char *bufp = buf;

    /* Position to start of text */
    fseek(textFile, fpos+header->stringOffset, 0);

    if (header->pack)
        startDecoding();
    while (len--)
        if (header->pack)
            *(bufp++) = decodeChar();
        else
            *(bufp++) = getc(textFile);

    /* Terminate string with zero */
    *bufp = '\0';

    return buf;
}



/*======================================================================*/
void score(Aword sc)
{
    if (sc == 0) {
        Parameter *messageParameters = allocateParameterArray(MAXENTITY);
        addParameterForInteger(messageParameters, current.score);
        addParameterForInteger(messageParameters, header->maximumScore);
        printMessageWithParameters(M_SCORE, messageParameters);
        free(messageParameters);
    } else {
        current.score += scores[sc-1];
        scores[sc-1] = 0;
        gameStateChanged = TRUE;
    }
}


/*======================================================================*/
void visits(Aword v)
{
    current.visits = v;
}


/*----------------------------------------------------------------------*/
static void sayUndoneCommand(char *words) {
    static Parameter *messageParameters = NULL;
    messageParameters = ensureParameterArrayAllocated(messageParameters);

    current.location = where(HERO, TRUE);
    clearParameterArray(messageParameters);
    addParameterForString(&messageParameters[0], words);
    setEndOfArray(&messageParameters[1]);
    printMessageWithParameters(M_UNDONE, messageParameters);
}


/*======================================================================*/
void undo(void) {
    forgetGameState();
    if (anySavedState()) {
        recallGameState();
        sayUndoneCommand(recreatePlayerCommand());
    } else {
        printMessage(M_NO_UNDO);
    }
    longjmp(returnLabel, UNDO_RETURN);
}


/*======================================================================*/
void quitGame(void)
{
    char buf[80];

    current.location = where(HERO, TRUE);
    para();
    while (TRUE) {
        col = 1;
        statusline();
        printMessage(M_QUITACTION);
#ifdef USE_READLINE
        if (!readline(buf)) terminate(0);
#else
        if (gets(buf) == NULL) terminate(0);
#endif
        if (strcmp(buf, "restart") == 0)
            longjmp(restartLabel, TRUE);
        else if (strcmp(buf, "restore") == 0) {
            restore();
            return;
        } else if (strcmp(buf, "quit") == 0) {
            terminate(0);
        } else if (strcmp(buf, "undo") == 0) {
            if (gameStateChanged) {
                rememberCommands();
                rememberGameState();
                undo();
            } else {
                if (anySavedState()) {
                    recallGameState();
                    sayUndoneCommand(playerWordsAsCommandString());
                } else
                    printMessage(M_NO_UNDO);
                longjmp(returnLabel, UNDO_RETURN);
            }
        }
    }
    syserr("Fallthrough in QUIT");
}



/*======================================================================*/
void restartGame(void)
{
    Aint previousLocation = current.location;

    current.location = where(HERO, TRUE);
    para();
    if (confirm(M_REALLY)) {
        longjmp(restartLabel, TRUE);
    }
    current.location = previousLocation;
}



/*======================================================================*/
void cancelEvent(Aword theEvent)
{
    int i;

    for (i = eventQueueTop-1; i>=0; i--)
        if (eventQueue[i].event == theEvent) {
            while (i < eventQueueTop-1) {
                eventQueue[i].event = eventQueue[i+1].event;
                eventQueue[i].after = eventQueue[i+1].after;
                eventQueue[i].where = eventQueue[i+1].where;
                i++;
            }
            eventQueueTop--;
            return;
        }
}


/*----------------------------------------------------------------------*/
static void increaseEventQueue(void)
{
    eventQueue = realloc(eventQueue, (eventQueueTop+2)*sizeof(EventQueueEntry));
    if (eventQueue == NULL) syserr("Out of memory in increaseEventQueue()");

    eventQueueSize = eventQueueTop + 2;
}


/*======================================================================*/
void schedule(Aword event, Aword where, Aword after)
{
    int i;

    if (event == 0) syserr("NULL event");

    cancelEvent(event);
    /* Check for overflow */
    if (eventQueue == NULL || eventQueueTop == eventQueueSize)
        increaseEventQueue();

    /* Bubble this event down */
    for (i = eventQueueTop; i >= 1 && eventQueue[i-1].after <= after; i--) {
        eventQueue[i].event = eventQueue[i-1].event;
        eventQueue[i].after = eventQueue[i-1].after;
        eventQueue[i].where = eventQueue[i-1].where;
    }

    eventQueue[i].after = after;
    eventQueue[i].where = where;
    eventQueue[i].event = event;
    eventQueueTop++;
}


// TODO Move to string.c?
/*======================================================================*/
Aptr concat(Aptr s1, Aptr s2)
{
    char *result = allocate(strlen((char*)s1)+strlen((char*)s2)+1);
    strcpy(result, (char*)s1);
    strcat(result, (char*)s2);
    free((char*)s1);
    free((char*)s2);
    return (Aptr)result;
}


/*----------------------------------------------------------------------*/
static char *stripCharsFromStringForwards(int count, char *initialString, char **theRest)
{
    int stripPosition;
    char *strippedString;
    char *rest;

    if (count > strlen(initialString))
        stripPosition = strlen(initialString);
    else
        stripPosition = count;
    rest = strdup(&initialString[stripPosition]);
    strippedString = strdup(initialString);
    strippedString[stripPosition] = '\0';
    *theRest = rest;
    return strippedString;
}

/*----------------------------------------------------------------------*/
static char *stripCharsFromStringBackwards(Aint count, char *initialString, char **theRest) {
    int stripPosition;
    char *strippedString;
    char *rest;

    if (count > strlen(initialString))
        stripPosition = 0;
    else
        stripPosition = strlen(initialString)-count;
    strippedString = strdup(&initialString[stripPosition]);
    rest = strdup(initialString);
    rest[stripPosition] = '\0';
    *theRest = rest;
    return strippedString;
}


/*----------------------------------------------------------------------*/
static int countLeadingBlanks(char *string, int position) {
    static char blanks[] = " ";
    return strspn(&string[position], blanks);
}


/*----------------------------------------------------------------------*/
static int skipWordForwards(char *string, int position)
{
    char separators[] = " .,?";

    int i;

    for (i = position; i<=strlen(string) && strchr(separators, string[i]) == NULL; i++)
        ;
    return i;
}


/*----------------------------------------------------------------------*/
static char *stripWordsFromStringForwards(Aint count, char *initialString, char **theRest) {
    int skippedChars;
    int position = 0;
    char *stripped;
    int i;

    for (i = count; i>0; i--) {
        /* Ignore any initial blanks */
        skippedChars = countLeadingBlanks(initialString, position);
        position += skippedChars;
        position = skipWordForwards(initialString, position);
    }

    stripped = (char *)allocate(position+1);
    strncpy(stripped, initialString, position);
    stripped[position] = '\0';

    skippedChars = countLeadingBlanks(initialString, position);
    *theRest = strdup(&initialString[position+skippedChars]);

    return(stripped);
}


/*----------------------------------------------------------------------*/
static int skipWordBackwards(char *string, int position)
{
    char separators[] = " .,?";
    int i;

    for (i = position; i>0 && strchr(separators, string[i-1]) == NULL; i--)
        ;
    return i;
}


/*----------------------------------------------------------------------*/
static int countTrailingBlanks(char *string, int position) {
    int skippedChars, i;
    skippedChars = 0;

    if (position > strlen(string)-1)
        syserr("position > length in countTrailingBlanks");
    for (i = position; i >= 0 && string[i] == ' '; i--)
        skippedChars++;
    return(skippedChars);
}


/*----------------------------------------------------------------------*/
static char *stripWordsFromStringBackwards(Aint count, char *initialString, char **theRest) {
    int skippedChars;
    char *stripped;
    int strippedLength;
    int position = strlen(initialString);
    int i;

    for (i = count; i>0 && position>0; i--) {
        position -= 1;
        /* Ignore trailing blanks */
        skippedChars = countTrailingBlanks(initialString, position);
        if (position - skippedChars < 0) break; /* No more words to strip */
        position -= skippedChars;
        position = skipWordBackwards(initialString, position);
    }

    skippedChars = countLeadingBlanks(initialString, 0);
    strippedLength = strlen(initialString)-position-skippedChars;
    stripped = (char *)allocate(strippedLength+1);
    strncpy(stripped, &initialString[position+skippedChars], strippedLength);
    stripped[strippedLength] = '\0';

    if (position > 0) {
        skippedChars = countTrailingBlanks(initialString, position-1);
        position -= skippedChars;
    }
    *theRest = strdup(initialString);
    (*theRest)[position] = '\0';
    return(stripped);
}



/*======================================================================*/
Aptr strip(bool stripFromBeginningNotEnd, int count, bool stripWordsNotChars, int id, int atr)
{
    char *initialString = (char *)getInstanceAttribute(id, atr);
    char *theStripped;
    char *theRest;

    if (stripFromBeginningNotEnd) {
        if (stripWordsNotChars)
            theStripped = stripWordsFromStringForwards(count, initialString, &theRest);
        else
            theStripped = stripCharsFromStringForwards(count, initialString, &theRest);
    } else {
        if (stripWordsNotChars)
            theStripped = stripWordsFromStringBackwards(count, initialString, &theRest);
        else
            theStripped = stripCharsFromStringBackwards(count, initialString, &theRest);
    }
    setInstanceStringAttribute(id, atr, theRest);
    return (Aptr)theStripped;
}


/*======================================================================*/
int getContainerMember(int container, int index, bool directly) {
    Aint i;
    Aint count = 0;

    for (i = 1; i <= header->instanceMax; i++) {
        if (in(i, container, directly)) {
            count++;
            if (count == index)
                return i;
        }
    }
    apperr("Index not in container in 'containerMember()'");
    return 0;
}


/***********************************************************************\

  Description Handling

\***********************************************************************/


/*======================================================================*/
void showImage(int image, int align)
{
#ifdef HAVE_GLK
    glui32 ecode;

    if ((glk_gestalt(gestalt_Graphics, 0) == 1) &&
        (glk_gestalt(gestalt_DrawImage, wintype_TextBuffer) == 1)) {
        glk_window_flow_break(glkMainWin);
        printf("\n");
        ecode = glk_image_draw(glkMainWin, image, imagealign_MarginLeft, 0);
    }
#endif
}


/*======================================================================*/
void playSound(int sound)
{
#ifdef HAVE_GLK
#ifdef GLK_MODULE_SOUND
    static schanid_t soundChannel = NULL;
    glui32 ecode;

    if (glk_gestalt(gestalt_Sound, 0) == 1) {
        if (soundChannel == NULL)
            soundChannel = glk_schannel_create(0);
        if (soundChannel != NULL) {
            glk_schannel_stop(soundChannel);
            ecode = glk_schannel_play(soundChannel, sound);
        }
    }
#endif
#endif
}



/*======================================================================*/
void empty(int cnt, int whr)
{
    int i;

    for (i = 1; i <= header->instanceMax; i++)
        if (in(i, cnt, TRUE))
            locate(i, whr);
}



/*======================================================================*/
void use(int actor, int script)
{
    char str[80];
    StepEntry *step;

    if (!isActor(actor)) {
        sprintf(str, "Instance is not an Actor (%d).", actor);
        syserr(str);
    }

    admin[actor].script = script;
    admin[actor].step = 0;
    step = stepOf(actor);
    if (step != NULL && step->after != 0) {
        admin[actor].waitCount = evaluate(step->after);
    }

    gameStateChanged = TRUE;
}

/*======================================================================*/
void stop(int act)
{
    char str[80];

    if (!isActor(act)) {
        sprintf(str, "Instance is not an Actor (%d).", act);
        syserr(str);
    }

    admin[act].script = 0;
    admin[act].step = 0;

    gameStateChanged = TRUE;
}




/*----------------------------------------------------------------------*/
int randomInteger(int from, int to)
{
    if (to == from)
        return to;
    else if (to > from)
        return (rand()/10)%(to-from+1)+from;
    else
        return (rand()/10)%(from-to+1)+to;
}



/*----------------------------------------------------------------------*/
bool btw(int val, int low, int high)
{
    if (high > low)
        return low <= val && val <= high;
    else
        return high <= val && val <= low;
}



/*======================================================================*/
bool contains(Aptr string, Aptr substring)
{
    bool found;

    strlow((char *)string);
    strlow((char *)substring);

    found = (strstr((char *)string, (char *)substring) != 0);

    free((char *)string);
    free((char *)substring);

    return(found);
}


/*======================================================================*/
bool streq(char a[], char b[])
{
    bool eq;

    strlow(a);
    strlow(b);

    eq = (strcmp(a, b) == 0);

    free(a);
    free(b);

    return(eq);
}



/*======================================================================*/
#include <sys/time.h>
void startTranscript() {
    time_t tick;

    if (logFile != NULL)
        return;

    time(&tick);

    struct timeval tv;
    struct tm *tm;
    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);

    sprintf(logFileName, "%s%d%02d%02d%02d%02d%02d%04d.log",
			adventureName, tm->tm_year+1900, tm->tm_mon+1,
			tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
			(int)tv.tv_usec);
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
    } else
        transcriptOption = TRUE;
}


/*======================================================================*/
void stopTranscript() {
    if (logFile == NULL)
        return;

	if (transcriptOption|| logOption)
#ifdef HAVE_GLK
		glk_stream_close(logFile, NULL);
#else
    fclose(logFile);
#endif
    logFile = NULL;
	transcriptOption = FALSE;
	logOption = FALSE;
}


