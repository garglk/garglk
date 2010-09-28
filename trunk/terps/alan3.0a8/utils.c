#include "utils.h"


/* Imports: */
#include "alan.version.h"
#include "options.h"
#include "memory.h"
#include "output.h"


/*======================================================================

  terminate()

  Terminate the execution of the adventure, e.g. close windows,
  return buffers...

 */
void terminate(int code)
{
#ifdef __amiga__
#ifdef AZTEC_C
#include <fcntl.h>
  extern struct _dev *_devtab;
  char buf[85];

  if (con) { /* Running from WB, created a console so kill it */
    /* Running from WB, so we created a console and
       hacked the Aztec C device table to use it for all I/O
       so now we need to make it close it (once!) */
    _devtab[1].fd = _devtab[2].fd = 0;
  } else
#else
  /* Geek Gadgets GCC */
#include <workbench/startup.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>

  if (_WBenchMsg != NULL) {
    Close(window);
    if (_WBenchMsg->sm_ArgList != NULL)
      UnLock(CurrentDir(cd));
  } else
#endif
#endif
    newline();
  if (memory)
      free(memory);
  if (transcriptOption|| logOption)
#ifdef HAVE_GLK
    glk_stream_close(logFile, NULL);
#else
    fclose(logFile);
#endif

#ifdef SMARTALLOC
    sm_dump(1);
#endif

#ifdef HAVE_GLK
  glk_exit();
#else
  exit(code);
#endif
}


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
    printf("Usage:\n\n");
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


