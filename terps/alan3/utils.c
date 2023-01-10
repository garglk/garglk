#include "utils.h"


/* Imports: */
#include "alan.version.h"
#include "options.h"
#include "memory.h"
#include "output.h"
#include "exe.h"
#include "state.h"
#include "lists.h"

#include "fnmatch.h"

/*======================================================================

  terminate()

  Terminate the execution of the adventure, e.g. close windows,
  return buffers...

 */
void terminate(int code)
{
    newline();

    terminateStateStack();

    stopTranscript();

    if (memory)
        deallocate(memory);

#ifdef HAVE_GLK
    glk_exit();
#else
    exit(code);
#endif
}

#ifdef HAVE_GLK
#include "glkio.h"
#endif

/*======================================================================*/
void printVersion(int buildNumber) {
    printf("Arun - Adventure Language Interpreter version %s", alan.version.string);
    if (buildNumber != 0) printf("-%d", buildNumber);
    printf(" (%s %s)", alan.date, alan.time);
#ifdef GIT_VERSION
    printf("\nBuilt from git %s", GIT_VERSION);
#endif

}

/*======================================================================*/
void printIFIDs(char *adventureName) {
    IfidEntry *ifidEntry;

    printf("'%s' contains the following IFIDs:\n", adventureName);
    for (ifidEntry = pointerTo(header->ifids); !isEndOfArray(ifidEntry); ifidEntry++) {
        printf("  %s:\t%s\n", (unsigned char *)pointerTo(ifidEntry->nameAddress),
               (unsigned char *)pointerTo(ifidEntry->valueAddress));
    }
}


/*======================================================================*/
void usage(char *programName)
{
#if (BUILD+0) != 0
    printVersion(BUILD);
#else
    printVersion(0);
#endif
    printf("\n\nUsage:\n\n");
    printf("    %s [<switches>] <adventure>\n\n", programName);
    printf("where the possible optional switches are:\n");
#ifdef HAVE_GLK
    glk_set_style(style_Preformatted);
#endif
    printf("    -u        use UTF-8 encoding for input and output\n");
    printf("    -i        use ISO8859-1 encoding for input and output\n");
    printf("    -h        this help\n");
    printf("    -v        verbose mode\n");
    printf("    -l        log game transcript to a file ('.a3t')\n");
    printf("    -c        log player command input (the solution) to a file ('.a3s')\n");
    printf("    -n        don't show the Status Line\n");
    printf("    -p        don't page output\n");
    printf("    -d        enter debug mode immediately\n");
    printf("    -t[<n>]   trace game execution, higher <n> gives more trace\n");
    printf("    -r        make regression testing easier (don't timestamp, page break, randomize...)\n");
    printf("    -e        ignore version and checksum errors (dangerous)\n");
    printf("    --version print version and exit\n");
#ifdef HAVE_GLK
    glk_set_style(style_Normal);
#endif
}


#ifndef FNM_CASEFOLD
#define FNM_CASEFOLD 0
#endif
/*======================================================================*/
bool match(char *pattern, char *input) {
    return fnmatch(pattern, input, FNM_CASEFOLD) == 0;
}
