#include "syserr.h"

#include "debug.h"
#include "utils.h"
#include "current.h"
#include "output.h"


static void (*handler)(char *) = NULL;

/*----------------------------------------------------------------------*/
static void runtimeError(char *errorClassification, char *errorDescription, char *blurb) {
  output("$n$nAs you enter the twilight zone of Adventures, you stumble \
and fall to your knees. In front of you, you can vaguely see the outlines \
of an Adventure that never was.$n$n");
  output(errorClassification);
  output(errorDescription);
  newline();

  if (current.sourceLine != 0) {
    printf("At source line %d in '%s':\n", current.sourceLine, sourceFileName(current.sourceFile));
    printf("%s", readSourceLine(current.sourceFile, current.sourceLine));
  }

  newline();
  output(blurb);

  terminate(0);
}


/*======================================================================*/
void setSyserrHandler(void (*f)(char *))
{
  handler = f;
}


/*======================================================================*/
// TODO Make syserr() use ... as printf()
void syserr(char *description)
{
    if (handler == NULL) {
        char *blurb = "<If you are the creator of this piece of Interactive Fiction, \
please help debug this Alan system error. Collect *all* the sources, and, if possible, an \
exact transcript of the commands that let to this error, in a zip-file and send \
it to support@alanif.se. Thank you!>";
        runtimeError("SYSTEM ERROR: ", description, blurb);
    } else
        handler(description);
}


/*======================================================================*/
void apperr(char *description)
{
    if (handler == NULL) {
        char *blurb = "<If you are just playing this piece of Interactive Fiction, \
please help the author to debug this programming error. Send an exact \
transcript of the commands that led to this error to the author. Thank you! \
If you are the author, then you have to figure this out before releasing the game.>";
        runtimeError("APPLICATION ERROR: ", description, blurb);
    } else
        handler(description);
}

/*======================================================================*/
void playererr(char *description)
{
    if (handler == NULL) {
        char *blurb = "<You have probably done something that is not exactly right.>";
        runtimeError("PLAYER ERROR: ", description, blurb);
    } else
        handler(description);
}
