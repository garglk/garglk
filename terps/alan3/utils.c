#include "utils.h"


/* Imports: */
#include "alan.version.h"
#include "options.h"
#include "memory.h"
#include "output.h"
#include "exe.h"


/*======================================================================

  terminate()

  Terminate the execution of the adventure, e.g. close windows,
  return buffers...

 */
void terminate(int code)
{
    newline();
    if (memory)
        free(memory);

    stopTranscript();

#ifdef SMARTALLOC
    sm_dump(1);
#endif

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
    printf("Arun, Adventure Interpreter version %s", alan.version.string);
    if (buildNumber != 0) printf(" - build %d", buildNumber);
    printf(" (%s %s)", alan.date, alan.time);
}


/*======================================================================*/
void usage(char *programName)
{
    printVersion(BUILD);
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
    printf("    -r       refrain from printing timestamps and paging (making regression testing easier)\n");
#ifdef HAVE_GLK
    glk_set_style(style_Normal);
#endif
}


