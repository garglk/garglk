/*----------------------------------------------------------------------

  scan.c

  Player command line scanne unit for Alan interpreter ARUN

  ----------------------------------------------------------------------*/

#include "scan.h"


/* IMPORTS */
#include "lists.h"
#include "dictionary.h"
#include "output.h"
#include "memory.h"
#include "params.h"
#include "term.h"
#include "options.h"
#include "exe.h"
#include "word.h"
#include "literal.h"
#include "debug.h"
#include "msg.h"
#include "inter.h"
#include "converter.h"


#ifdef USE_READLINE
#include "readline.h"
#endif



/* PRIVATE CONSTANTS */


/* PUBLIC DATA */
bool continued = false;


/* PRIVATE DATA */
static char input_buffer[1000]; /* The input buffer */
static bool eol = true; /* Looking at End of line? Yes, initially */
static char *token = NULL;


/*======================================================================*/
void forceNewPlayerInput() {
    setEndOfArray(&playerWords[currentWordIndex]);
}


/*----------------------------------------------------------------------*/
static void unknown(char token[]) {
    Parameter *messageParameters = newParameterArray();

    addParameterForString(messageParameters, token);
    printMessageWithParameters(M_UNKNOWN_WORD, messageParameters);
    deallocate(messageParameters);
    abortPlayerCommand();
}


/*----------------------------------------------------------------------*/
static int number(char token[]) {
    int i;

    sscanf(token, "%d", &i);
    return i;
}


/*----------------------------------------------------------------------*/
static int lookup(char wrd[]) {
    int i;

    // TODO: Why do we start at 0, is there a word code == 0?
    for (i = 0; !isEndOfArray(&dictionary[i]); i++) {
        if (equalStrings(wrd, (char *) pointerTo(dictionary[i].string))) {
            return i;
        }
    }
    return EOF;
}


/*----------------------------------------------------------------------*/
static bool isWordCharacter(int ch) {
    return isLetter(ch) || isdigit(ch) || ch == '\'' || ch == '-' || ch == '_';
}

/*----------------------------------------------------------------------*/
static void readWord(char **markerp) {
    while (**markerp && isWordCharacter(**markerp))
        (*markerp)++;
}

/*----------------------------------------------------------------------*/
static void readNumber(char **markerp) {
    while (isdigit((int)**markerp))
        (*markerp)++;
}

/*----------------------------------------------------------------------*/
static void readString(char **markerp) {
    (*markerp)++;
    while (**markerp != '\"')
        (*markerp)++;
    (*markerp)++;
}


/*----------------------------------------------------------------------*/
static char *gettoken(char *buf) {
    static char *marker;
    static char oldch;

    if (buf == NULL)
        *marker = oldch;
    else
        marker = buf;
    while (*marker != '\0' && isSpace(*marker) && *marker != '\n')
        marker++;
    buf = marker;
    if (isLetter(*marker))
        readWord(&marker);
    else if (isdigit((int)*marker))
        readNumber(&marker);
    else if (*marker == '\"') {
        readString(&marker);
    } else if (*marker == '\0' || *marker == '\n' || *marker == ';')
        // End of input, either actually or by a comment start
        return NULL;
    else
        marker++;
    oldch = *marker;
    *marker = '\0';
    return buf;
}


static void printPrompt() {
    if (header->prompt) {
        anyOutput = false;
        interpret(header->prompt);
        if (anyOutput)
            printAndLog(" ");
        needSpace = false;
    } else
        printAndLog("> ");
}


/*----------------------------------------------------------------------*/
// TODO replace dependency to exe.c with injection of quitGame() and undo()
static void getLine(void) {
    para();
    do {
        statusline();
        printPrompt();

#ifdef USE_READLINE
        if (!readline(input_buffer))
#else
        fflush(stdout);
        if (fgets(buf, LISTLEN, stdin) == NULL)
#endif
        {
            newline();
            quitGame();
        }

        getPageSize();
        anyOutput = false;
        if (commandLogOption || transcriptOption) {
            /* Command log ("solution") needs to be in external encoding even for GLK terps */
            char *converted = ensureExternalEncoding(input_buffer);
            if (commandLogOption) {
#ifdef HAVE_GLK
                glk_put_string_stream(commandLogFile, converted);
                glk_put_char_stream(commandLogFile, '\n');
#else
                fprintf(commandLogFile, "%s\n", converted);
                fflush(commandLogFile);
#endif
            }
            if (transcriptOption) {
#ifdef HAVE_GLK
                glk_put_string_stream(transcriptFile, converted);
                glk_put_char_stream(transcriptFile, '\n');
#else
                fprintf(transcriptFile, "%s\n", converted);
                fflush(commandLogFile);
#endif
            }
            free(converted);
        }

        /* If the player inputs an empty command he forfeited his command */
        if (strlen(input_buffer) == 0) {
            clearWordList(playerWords);
            longjmp(forfeitLabel, 0);
        }

        token = gettoken(input_buffer);
        if (token != NULL) {
            if (strcmp("debug", token) == 0 && header->debug) {
                debugOption = true;
                debug(false, 0, 0);
                token = NULL;
            } else if (strcmp("undo", token) == 0) {
                token = gettoken(NULL);
                if (token != NULL) /* More tokens? */
                    error(M_WHAT);
                undo();
            }
        }
    } while (token == NULL);
    eol = false;
}


/*----------------------------------------------------------------------*/
static int try_elision(char *apostrophe, int *_i) {
    int i = *_i, w;
    // Handle elisions (contractions) with apostrophe,
    // e.g. Italian "l'acqua", and split that into two
    // words

    // First cut after the apostrophe
    int previous_char = apostrophe[1];
    apostrophe[1] = '\0';

    // Now try that word
    w = lookup(token);
    apostrophe[1] = previous_char;
    if (w == EOF) {
        // No cigar, so give up
        unknown(token);
    }

    // We found a word, save it
    if (!isNoise(w))
        playerWords[i++].code = w;
    // Then restore and point to next part
    token = &apostrophe[1];
    w = lookup(token);
    if (w == EOF) {
        // No cigar, so give up
        unknown(token);
    }

    // Prepare to store this word
    ensureSpaceForPlayerWords(i+1);
    playerWords[i].start = token;
    playerWords[i].end = strchr(token, '\0');

    *_i = i;
    return w;
}


/*----------------------------------------------------------------------*/
static int handle_literal(int i) {
    if (isdigit((int)token[0])) {
        createIntegerLiteral(number(token));
    } else {
        char *unquotedString = strdup(token);
        unquotedString[strlen(token) - 1] = '\0';
        createStringLiteral(&unquotedString[1]);
        free(unquotedString);
    }
    playerWords[i++].code = dictionarySize + litCount; /* Word outside dictionary = literal */
    return i;
}


/*----------------------------------------------------------------------*/
static int handle_word(int i) {
    int w = lookup(token);
    if (w == EOF) {
        char *apostrophe = strchr(token, '\'');
        if (apostrophe == NULL) {
            unknown(token);
        } else {
            w = try_elision(apostrophe, &i);
        }
    }
    if (!isNoise(w))
        playerWords[i++].code = w;
    return(i);
}


/*======================================================================*/
void scan(void) {
    int i;

    if (continued) {
        /* Player used '.' to separate commands. Read next */
        para();
        token = gettoken(NULL);
        if (token == NULL) /* Or did he just finish the command with a full stop? */
            getLine();
        continued = false;
    } else
        getLine();

    freeLiterals();
    playerWords[0].code = 0; // TODO This means what?
    i = 0;
    do {
        ensureSpaceForPlayerWords(i+1);
        playerWords[i].start = token;
        playerWords[i].end = strchr(token, '\0');

        if (isLetter(token[0])) {
            i = handle_word(i);
        } else if (isdigit((int)token[0]) || token[0] == '\"') {
            i = handle_literal(i);
        } else if (token[0] == ',') {
            playerWords[i++].code = conjWord;
        } else if (token[0] == '.') {
            continued = true;
            setEndOfArray(&playerWords[i]);
            eol = true;
            break;
        } else
            unknown(token);
        setEndOfArray(&playerWords[i]);
        eol = (token = gettoken(NULL)) == NULL;
    } while (!eol);
}
