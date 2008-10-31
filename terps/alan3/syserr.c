#include "syserr.h"

#include "main.h"
#include "inter.h"
#include "debug.h"


/*----------------------------------------------------------------------*/
static void runtimeError(char *errorClassification, char *errorDescription) {
  output("$n$nAs you enter the twilight zone of Adventures, you stumble \
and fall to your knees. In front of you, you can vaguely see the outlines \
of an Adventure that never was.$n$n");
  output(errorClassification);
  output(errorDescription);
  output("$n$n");

  if (current.sourceLine != 0) {
    printf("At source line %d in '%s':\n", current.sourceLine, sourceFileName(current.sourceFile));
    printf("%s", readSourceLine(current.sourceFile, current.sourceLine));
  }

  if (transcriptOption || logOption)
#ifdef HAVE_GLK
    glk_stream_close(logFile, NULL);
#else
  fclose(logFile);
#endif
  newline();

#ifdef __amiga__
#ifdef AZTEC_C
  {
    char buf[80];

    if (con) { /* Running from WB, wait for user ack. */
      printf("press RETURN to quit");
      gets(buf);
    }
  }
#endif
#endif

  terminate(0);
}


/*======================================================================*/
void syserr(char *description)
{
  runtimeError("SYSTEM ERROR: ", description);
}


/*======================================================================*/
void apperr(char *description)
{
  runtimeError("APPLICATION ERROR: ", description);
}
