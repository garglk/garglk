/*----------------------------------------------------------------------*\

location.c

\*----------------------------------------------------------------------*/
#include "Location.h"

#include "instance.h"
#include "options.h"
#include "word.h"
#include "inter.h"
#include "lists.h"
#include "checkentry.h"
#include "debug.h"
#include "memory.h"
#include "dictionary.h"
#include "output.h"
#include "msg.h"
#include "current.h"

/*----------------------------------------------------------------------*/
static void traceExit(int location, int dir, char *what) {
    printf("\n<EXIT %s[%d] from ",
           (char *)pointerTo(dictionary[playerWords[currentWordIndex-1].code].string), dir);
    traceSay(location);
    printf("[%d], %s:>\n", location, what);
}



/*======================================================================*/
void go(int location, int dir)
{
    ExitEntry *theExit;
    bool ok;
    Aword oldloc;

    theExit = (ExitEntry *) pointerTo(instances[location].exits);
    if (instances[location].exits != 0)
	while (!isEndOfArray(theExit)) {
	    if (theExit->code == dir) {
		ok = TRUE;
		if (theExit->checks != 0) {
		    if (sectionTraceOption)
                        traceExit(location, dir, "Checking");
		    ok = !checksFailed(theExit->checks, EXECUTE_CHECK_BODY_ON_FAIL);
		}
		if (ok) {
		    oldloc = location;
		    if (theExit->action != 0) {
			if (sectionTraceOption)
                            traceExit(location, dir, "Executing");
			interpret(theExit->action);
		    }
		    /* Still at the same place? */
		    if (where(HERO, FALSE) == oldloc) {
			if (sectionTraceOption)
                            traceExit(location, dir, "Moving");
			locate(HERO, theExit->target);
		    }
                    return;
		} else
                    error(NO_MSG);
	    }
	    theExit++;
	}
    error(M_NO_WAY);
}


/*======================================================================*/
bool exitto(int to, int from)
{
    ExitEntry *theExit;

    if (instances[from].exits == 0)
	return FALSE; /* No exits */

    for (theExit = (ExitEntry *) pointerTo(instances[from].exits); !isEndOfArray(theExit); theExit++)
	if (theExit->target == to)
	    return TRUE;

    return FALSE;
}


/*======================================================================*/
void look(void)
{
    int i;

    /* Set describe flag for all objects and actors */
    for (i = 1; i <= header->instanceMax; i++)
        admin[i].alreadyDescribed = FALSE;

    if (anyOutput)
        para();

    setSubHeaderStyle();
    sayInstance(current.location);
    setNormalStyle();

    newline();
    capitalize = TRUE;
    if (describe(current.location))
        describeInstances();
}



