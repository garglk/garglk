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
    printf("    -v       verbose mode\n");
    printf("    -l       log transcript to a file\n");
    printf("    -c       log player commands to a file\n");
    printf("    -n       no Status Line\n");
    printf("    -d       enter debug mode\n");
    printf("    -t[<n>]  trace game execution, higher <n> gives more trace\n");
    printf("    -i       ignore version and checksum errors\n");
    printf("    -r       make regression test easier (don't timestamp, page break, randomize...)\n");
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
