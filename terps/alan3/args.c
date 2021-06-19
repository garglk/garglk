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

#include "alan.version.h"

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
    if (strlen(adventureFileName) < strlen(ACODEEXTENSION)
        || compareStrings(&adventureFileName[strlen(adventureFileName)-4], ACODEEXTENSION) != 0) {
        adventureFileName = realloc(adventureFileName, strlen(adventureFileName)+strlen(ACODEEXTENSION)+1);
        strcat(adventureFileName, ACODEEXTENSION);
    }
    return adventureFileName;
}


/*----------------------------------------------------------------------*/
static void version(void) {
#if (BUILD+0) != 0
    printf("%s build %d", alan.version.string, BUILD);
#else
    printf("%s", alan.version.string);
#endif
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
                case 'h':
                    usage(argv[0]);
                    terminate(0);
                    break;
                case 'i':
                    ignoreErrorOption = TRUE;
                    break;
                case 't':
                    traceSectionOption = TRUE;
                    switch (argument[2]) {
                    case '9':
                    case '8':
                    case '7':
                    case '6':
                    case '5' : tracePushOption = TRUE;
                    case '4' : traceStackOption = TRUE;
                    case '3' : traceInstructionOption = TRUE;
                    case '2' : traceSourceOption = TRUE;
                    case '\0':
                    case '1': traceSectionOption = TRUE;
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
                    if (strcmp(argument, "-version") == 0) {
                        version();
                        terminate(0);
                    } else
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
                    statusLineOption = FALSE;
                    break;
                case '-':
                    if (strcasecmp(&argument[2], "version") == 0) {
                        version();
                        terminate(0);
                        break;
                    }
                    /* else fall-through */
                default:
                    printf("Unrecognized switch, -%s\n", &argument[1]);
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
bool differentInterpreterName(char *string) {
    return strcasecmp(string, PROGNAME) != 0;
}


/*======================================================================*/
#ifdef __windows__
#include <windows.h>
#endif


/*======================================================================*/
void args(int argc, char * argv[])
{
    char *programName;
    char *exePoint;

#ifdef ARGSDISPLAY
    int i;

    MessageBox(NULL, "Hello!", "Windows Arun interpreter", MB_OK);
    MessageBox(NULL, GetCommandLine(), "", MB_OK);
    for (i = 0; i < argc; i++) {
        char buf[199];
        sprintf(buf, "arg %d :\"%s\"", i, argv[i]);
        MessageBox(NULL, buf, "Alan V3 compiler", MB_OK);
    }
#endif

#ifdef HAVE_WINGLK
    argv[0] = GetCommandLine();
#endif
    if ((programName = strrchr(argv[0], '\\')) == NULL
        && (programName = strrchr(argv[0], '/')) == NULL
        && (programName = strrchr(argv[0], ':')) == NULL)
        programName = strdup(argv[0]);
    else
        programName = strdup(&programName[1]);

    if (strlen(programName) > 4 && (((exePoint = strstr(programName, ".EXE")) != NULL) || (exePoint = strstr(programName, ".exe")) != NULL))
        *exePoint = '\0';

    /* Now look at the switches and arguments */
    switches(argc, argv);

#ifdef ARGSDISPLAY
    {
        char buf[100];
        sprintf(buf, "programName = '%s'\nadventureFileName = '%s'", programName, adventureFileName);
        MessageBox(NULL, buf, "Alan V3 compiler", MB_OK);
    }
#endif

    if (adventureFileName == NULL) {
        /* No game given, try program name */
        if (differentInterpreterName(programName)) {
            // TODO break out as a function
            FILE *adventureFile;
            adventureFileName = allocate(strlen(programName)
                                         +strlen(ACODEEXTENSION)+1);
            strcpy(adventureFileName, programName);
            strcat(adventureFileName, ACODEEXTENSION);
            // TODO break out as utils::fileExists()
            if ((adventureFile = fopen(adventureFileName, "r")) == NULL) {
                free(adventureFileName);
                adventureFileName = NULL;
            } else
                fclose(adventureFile);
        }
    }
    adventureName = gameName(adventureFileName);
    free(programName);
}
