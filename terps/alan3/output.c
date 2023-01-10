#include "output.h"

/* IMPORTS */
#include "options.h"
#include "memory.h"
#include "word.h"
#include "lists.h"
#include "term.h"
#include "syserr.h"
#include "dictionary.h"
#include "current.h"
#include "msg.h"
#include "readline.h"
#include "instance.h"
#include "converter.h"


#ifdef HAVE_GLK
#include "glkio.h"
#endif

/* PUBLIC DATA */
bool anyOutput = false;
bool capitalize = false;
bool needSpace = false;
bool skipSpace = false;

/* Screen formatting info */
int col, lin;
int pageLength, pageWidth;

/* Logfiles */
#ifdef HAVE_GLK
strid_t commandLogFile;
strid_t transcriptFile;
#else
FILE *commandLogFile;
FILE *transcriptFile;
#endif


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


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
void setSubHeaderStyle(void) {
#ifdef HAVE_GLK
    glk_set_style(style_Subheader);
#endif
}


/*======================================================================*/
void setNormalStyle(void) {
#ifdef HAVE_GLK
    glk_set_style(style_Normal);
#endif
}

/*======================================================================*/
void newline(void)
{
#ifndef HAVE_GLK
    char buf[256];

    printAndLog("\n");
    if ((!nopagingOption && !regressionTestOption) && lin == pageLength - 1) {
        needSpace = false;
        col = 0;
        lin = 0;
        printMessage(M_MORE);
        statusline();
        fflush(stdout);
        if (fgets(buf, 256, stdin) == 0)
            /* ignore error */;
        getPageSize();
    }

    lin++;
#else
    printAndLog("\n");
#endif
    col = 1;
    needSpace = false;
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
    capitalize = true;
}


/*======================================================================*/
void clear(void)
{
#ifdef HAVE_GLK
    glk_window_clear(glkMainWin);
#else
#ifdef HAVE_ANSI_CONTROL
    if (!statusLineOption) return;
    printf("\x1b[2J");
    printf("\x1b[%d;1H", pageLength);
#endif
#endif
}


/*----------------------------------------------------------------------*/
static void capitalizeFirst(char *str) {
    int i = 0;

    /* Skip over space... */
    while (i < strlen(str) && isSpace(str[i])) i++;
    if (i < strlen(str)) {
        str[i] = toUpper(str[i]);
        capitalize = false;
    }
}



/*======================================================================*/
void printAndLog(char string[])
{
    /* TODO: clean up the GLK code to use same converted string */
#ifdef HAVE_GLK
    printf("%s", string);
#else
    char *outputString = ensureExternalEncoding(string);
    printf("%s", outputString);
#endif

#ifdef HAVE_GLK
    if (!onStatusLine && transcriptOption) {
        static int column = 0;
        char *stringCopy;
        char *stringPart;

        // For GLK terps we need to do the formatting of transcript
        // output here as GLK formats output for the terminal. Non-GLK
        // terps will format all output in justify().  Using 70-char
        // wide transcript lines.
        /* The transcript file should be generated in external coding even for GLK terps */
        stringCopy = strdup(string);  /* Make sure string is modifiable */
        stringPart = stringCopy;
        while (strlen(stringPart) > 70-column) {
            int p;
            for (p = 70-column; p>0 && !isspace((int)stringPart[p]); p--);
            stringPart[p] = '\0';
            char *converted = ensureExternalEncoding(stringPart);
            glk_put_string_stream(transcriptFile, converted);
            free(converted);
            glk_put_string_stream(transcriptFile, "\n");
            column = 0;
            stringPart = &stringPart[p+1];
        }
        char *converted = ensureExternalEncoding(stringPart);
        glk_put_string_stream(transcriptFile, converted);
        free(converted);
        column = updateColumn(column, stringPart);
        free(stringCopy);
    }
#else
    if (!onStatusLine && transcriptOption) {
        fprintf(transcriptFile, "%s", outputString);
        fflush(transcriptFile);
    }
    free(outputString);
#endif
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
        if (i == 0 && col == 1) /* If it doesn't fit at all */
            /* Wrap immediately after this word */
            while (!isSpace(str[i]) && str[i] != '\0')
                i++;
        if (i > 0) {        /* If it fits ... */
            ch = str[i];      /* Save space or NULL */
            str[i] = '\0';        /* Terminate string */
            printAndLog(str);     /* and print it */
            skipSpace = false;        /* If skipping, now we're done */
            str[i] = ch;      /* Restore character */
            /* Skip white after printed portion */
            for (str = &str[i]; isSpace(str[0]) && str[0] != '\0'; str++);
        }
        newline();          /* Then start a new line */
        while(isSpace(str[0])) str++; /* Skip any leading space on next part */
    }
    printAndLog(str);     /* Print tail */
#endif
    col = col + strlen(str);  /* Update column */
}


/*----------------------------------------------------------------------*/
static void space(void)
{
    if (skipSpace)
        skipSpace = false;
    else {
        if (needSpace) {
            printAndLog(" ");
            col++;
        }
    }
    needSpace = false;
}


/*----------------------------------------------------------------------*/
static void sayPlayerWordsForParameter(int p) {
    int i;

    for (i = globalParameters[p].firstWord; i <= globalParameters[p].lastWord; i++) {
        justify((char *)pointerTo(dictionary[playerWords[i].code].string));
        if (i < globalParameters[p].lastWord)
            justify(" ");
    }
}


/*----------------------------------------------------------------------*/
static void sayParameter(int p, int form)
{
    int i;

    for (i = 0; i <= p; i++)
        if (isEndOfArray(&globalParameters[i]))
            apperr("Nonexistent parameter referenced.");

#ifdef ALWAYS_SAY_PARAMETERS_USING_PLAYER_WORDS
    if (params[p].firstWord != EOF) /* Any words he used? */
        /* Yes, so use them... */
        sayPlayerWordsForParameter(p);
    else
        sayForm(params[p].code, form);
#else
    if (globalParameters[p].useWords) {
        /* Ambiguous instance referenced, so use the words he used */
        sayPlayerWordsForParameter(p);
    } else
        sayForm(globalParameters[p].instance, form);
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
  _ = interpret this as a single dollar, if in doubt or conflict with other symbols
*/
static char *printSymbol(char str[]) /* IN - The string starting with '$' */
{
    int advance = 2;

    if (*str == '\0') printAndLog("$");
    else switch (toLower(str[1])) {
        case 'n':
            newline();
            needSpace = false;
            break;
        case 'i':
            newline();
            printAndLog("    ");
            col = 5;
            needSpace = false;
            break;
        case 'o':
            space();
            sayParameter(0, 0);
            needSpace = true;       /* We did print something non-white */
            break;
        case '+':
        case '0':
        case '-':
        case '!':
            space();
            if (isdigit((int)str[2])) {
                int form;
                switch (str[1]) {
                case '+': form = SAY_DEFINITE; break;
                case '0': form = SAY_INDEFINITE; break;
                case '-': form = SAY_NEGATIVE; break;
                case '!': form = SAY_PRONOUN; break;
                default: form = SAY_SIMPLE; break;
                }
                sayParameter(str[2]-'1', form);
                needSpace = true;
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
            needSpace = true;       /* We did print something non-white */
            break;
        case 'l':
            space();
            say(current.location);
            needSpace = true;       /* We did print something non-white */
            break;
        case 'a':
            space();
            say(current.actor);
            needSpace = true;       /* We did print something non-white */
            break;
        case 'v':
            space();
            justify((char *)pointerTo(dictionary[verbWord].string));
            needSpace = true;       /* We did print something non-white */
            break;
        case 'p':
            para();
            needSpace = false;
            break;
        case 't': {
            int i;
            int spaces = 4-(col-1)%4;

            for (i = 0; i<spaces; i++) printAndLog(" ");
            col = col + spaces;
            needSpace = false;
            break;
        }
        case '$':
            skipSpace = true;
            capitalize = false;
            break;
        case '_':
            advance = 2;
            printAndLog("$");
            break;
        default:
            advance = 1;
            printAndLog("$");
            break;
        }

    return &str[advance];
}


/*----------------------------------------------------------------------*/
static bool inhibitSpace(char *str) {
    return str[0] != '\0' && str[0] == '$' && str[1] == '$';
}


/*----------------------------------------------------------------------*/
static bool isSpaceEquivalent(char str[]) {
    if (str[0] == ' ')
        return true;
    else
        return strncmp(str, "$p", 2) == 0
            || strncmp(str, "$n", 2) == 0
            || strncmp(str, "$i", 2) == 0
            || strncmp(str, "$t", 2) == 0;
}


/*----------------------------------------------------------------------*/
static bool punctuationNext(char *str) {
    char *punctuation = strchr(".,!?", str[0]);
    bool end = str[1] == '\0';
    bool space = isSpaceEquivalent(&str[1]);
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

    if (strlen(original) == 0)
        return;

    copy = strdup(original);
    str = copy;

    if (inhibitSpace(str) || punctuationNext(str))
        needSpace = false;
    else
        space();            /* Output space if needed (& not inhibited) */

    /* Output string up to symbol and handle the symbol */
    while ((symptr = strchr(str, '$')) != (char *) NULL) {
        ch = *symptr;       /* Terminate before symbol */
        *symptr = '\0';
        if (strlen(str) > 0) {
            skipSpace = false;    /* Only let skipSpace through if it is
                                     last in the string */
            if (lastCharOf(str) == ' ') {
                str[strlen(str)-1] = '\0'; /* Truncate space character */
                justify(str);       /* Output part before '$' */
                needSpace = true;
            } else {
                justify(str);       /* Output part before '$' */
                needSpace = false;
            }
        }
        *symptr = ch;       /* restore '$' */
        str = printSymbol(symptr);  /* Print the symbolic reference and advance */
    }

    if (str[0] != 0) {
        justify(str);           /* Output trailing part */
        skipSpace = false;
        if (lastCharOf(str) != ' ')
            needSpace = true;
    }
    if (needSpace)
        capitalize = strchr("!?.", str[strlen(str)-1]) != 0;
    anyOutput = true;
    free(copy);
}


/*======================================================================*/
bool confirm(MsgKind msgno)
{
    char buf[80];

    /* This is a bit of a hack since we really want to compare the input,
       it could be affirmative, but for now any input is NOT! */
    printMessage(msgno);

#ifdef USE_READLINE
    if (!readline(buf)) return true;
#else
    if (gets(buf) == NULL) return true;
#endif
    col = 1;

    return (buf[0] == '\0');
}
