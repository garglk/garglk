/*----------------------------------------------------------------------*\
 
  act.c
 
  Action routines
 
\*----------------------------------------------------------------------*/

#include "act.h"

/* Import */

#include "AltInfo.h"
#include "output.h"
#include "msg.h"



/*----------------------------------------------------------------------*/
static void executeCommand(int verb, Parameter parameters[])
{
    AltInfo *altInfos;
    int altIndex;

    altInfos = findAllAlternatives(verb, parameters);
    if (anyCheckFailed(altInfos, EXECUTE_CHECK_BODY_ON_FAIL))
        return;

    /* Check for anything to execute... */
    if (!anythingToExecute(altInfos))
        error(M_CANT0);

    /* Now perform actions! First try any BEFORE or ONLY from inside out */
    for (altIndex = lastAltInfoIndex(altInfos); altIndex >= 0; altIndex--) {
        if (altInfos[altIndex].alt != 0) // TODO Can this ever be NULL? Why?
            if (altInfos[altIndex].alt->qual == (Aword)Q_BEFORE
                || altInfos[altIndex].alt->qual == (Aword)Q_ONLY) {
                if (!executedOk(&altInfos[altIndex]))
                    abortPlayerCommand();
                if (altInfos[altIndex].alt->qual == (Aword)Q_ONLY)
                    return;
            }
    }
        
    /* Then execute any not declared as AFTER, i.e. the default */
    for (altIndex = 0; !altInfos[altIndex].end; altIndex++) {
        if (altInfos[altIndex].alt != 0)
            if (altInfos[altIndex].alt->qual != (Aword)Q_AFTER)
                if (!executedOk(&altInfos[altIndex]))
                    abortPlayerCommand();
    }
        
    /* Finally, the ones declared as AFTER */
    for (altIndex = lastAltInfoIndex(altInfos); altIndex >= 0; altIndex--) {
        if (altInfos[altIndex].alt != 0)
            if (!executedOk(&altInfos[altIndex]))
                abortPlayerCommand();
    }
}


/*======================================================================
 
  action()
 
  Execute the command. Handles acting on multiple items
  such as ALL, THEM or lists of objects.
 
*/
void action(int verb, Parameter parameters[], Parameter multipleMatches[])
{
    int i, multiplePosition;
    char marker[10];
        
    multiplePosition = findMultiplePosition(parameters);
    if (multiplePosition != -1) {
        sprintf(marker, "($%d)", multiplePosition+1); /* Prepare a printout with $1/2/3 */
        for (i = 0; multipleMatches[i].instance != EOF; i++) {
            parameters[multiplePosition] = multipleMatches[i];
            output(marker);
			// TODO: if execution for one parameter aborts we should return here, not to top level
            executeCommand(verb, parameters);
            if (multipleMatches[i+1].instance != EOF)
                para();
        }
        parameters[multiplePosition].instance = 0;
    } else
        executeCommand(verb, parameters);
}
