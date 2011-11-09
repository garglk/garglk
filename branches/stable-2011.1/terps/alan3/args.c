/*----------------------------------------------------------------------*\

  args - Argument handling for arun

  Handles the various startup methods on all machines.

  Main function args() will set up global variable adventureName and the flags,
  the terminal will also be set up and connected.

  \*----------------------------------------------------------------------*/

#include "sysdep.h"
#include "args.h"

#include "options.h"
#include "memory.h"
#include "utils.h"

#ifdef HAVE_GLK
#include "glk.h"
#include "glkio.h"
#endif

#ifdef __windows__
#include <windows.h>
#endif


/* PUBLIC DATA */
/* The files and filenames */
char *adventureName;        /* The name of the game */
char *adventureFileName;

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*======================================================================*/
char *gameName(char *fullPathName) {
    char *foundGameName = "";

    if (fullPathName != NULL) {
        foundGameName = strdup(baseNameStart(fullPathName));
        foundGameName[strlen(foundGameName)-4] = '\0'; /* Strip off .A3C */
    }

    if (foundGameName[0] == '.' && foundGameName[1] == '/')
        strcpy(foundGameName, &foundGameName[2]);

    return foundGameName;
}


/*----------------------------------------------------------------------*/
static char *removeQuotes(char *argument) {
    char *str = strdup(&argument[1]);
    str[strlen(str)-1] = '\0';
    return str;
}


/*----------------------------------------------------------------------*/
static bool isQuoted(char *argument) {
    return argument[0] == '"' && strlen(argument) > 2;
}


/*----------------------------------------------------------------------*/
static char *addAcodeExtension(char *adventureFileName) {
    if (compareStrings(&adventureFileName[strlen(adventureFileName)-4], ACODEEXTENSION) != 0) {
        adventureFileName = realloc(adventureFileName, strlen(adventureFileName)+5);
        strcat(adventureFileName, ACODEEXTENSION);
    }
    return adventureFileName;
}



/*----------------------------------------------------------------------*/
static void switches(int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; i++) {
        char *argument = argv[i];

        if (argument[0] == '-') {
            switch (toLower(argument[1]))
                {
                case 'i':
                    ignoreErrorOption = TRUE;
                    break;
                case 't':
                    sectionTraceOption = TRUE;
                    switch (argument[2]) {
                    case '9':
                    case '8':
                    case '7':
                    case '6':
                    case '5' : traceStackOption = TRUE;
                    case '4' : tracePushOption = TRUE;
                    case '3' : singleStepOption = TRUE;
                    case '2' : traceSourceOption = TRUE;
                    case '\0':
                    case '1': sectionTraceOption = TRUE;
                    }
                    break;
                case 'd':
                    debugOption = TRUE;
                    break;
                case 'l':
                    transcriptOption = TRUE;
                    logOption = FALSE;
                    break;
                case 'v':
                    verboseOption = TRUE;
                    break;
                case 'n':
                    statusLineOption = FALSE;
                    break;
                case 'c':
                    logOption = TRUE;
                    transcriptOption = FALSE;
                    break;
                case 'r':
                    regressionTestOption = TRUE;
                    break;
                default:
                    printf("Unrecognized switch, -%c\n", argument[1]);
                    usage(argv[0]);
                    terminate(0);
                }
        } else {

            if (isQuoted(argument))
                adventureFileName = removeQuotes(argument);
            else
                adventureFileName = strdup(argument);

            adventureFileName = addAcodeExtension(adventureFileName);

            adventureName = gameName(adventureFileName);

        }
    }
}


/*----------------------------------------------------------------------*/
static bool differentInterpreterName(char *string) {
    return strcasecmp(string, PROGNAME) != 0;
}


/*======================================================================*/
#if defined(__dos__) || defined(__windows__) || defined(__cygwin__)
#include "winargs.c"
#else
#if defined(__unix__) || defined(__macosx__) || defined(__APPLE__)
#include "unixargs.c"
#else
#ifdef __mac__
#include "macargs.c"
#else
/***********************************************************************\

   UNIMPLEMENTED OS

\***********************************************************************/
#endif
#endif
#endif
