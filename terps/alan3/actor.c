/*----------------------------------------------------------------------*\

	actor

\*----------------------------------------------------------------------*/
#include "actor.h"

/* IMPORTS */
#include "instance.h"
#include "memory.h"
#include "lists.h"
#include "inter.h"
#include "msg.h"
#include "Container.h"


/* CONSTANTS */


/* PUBLIC DATA */


/* PRIVATE TYPES & DATA */


/*+++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*======================================================================*/
ScriptEntry *scriptOf(int actor) {
    ScriptEntry *scr;

    if (admin[actor].script != 0) {
        for (scr = (ScriptEntry *) pointerTo(header->scriptTableAddress); !isEndOfArray(scr); scr++)
            if (scr->code == admin[actor].script)
                break;
        if (!isEndOfArray(scr))
            return scr;
    }
    return NULL;
}


/*======================================================================*/
StepEntry *stepOf(int actor) {
    StepEntry *step;
    ScriptEntry *scr = scriptOf(actor);

    if (scr == NULL) return NULL;

    step = (StepEntry*)pointerTo(scr->steps);
    step = &step[admin[actor].step];

    return step;
}


/*======================================================================*/
void describeActor(int actor)
{
    ScriptEntry *script = scriptOf(actor);

    if (script != NULL && script->description != 0)
        interpret(script->description);
    else if (hasDescription(actor))
        describeAnything(actor);
    else {
        printMessageWithInstanceParameter(M_SEE_START, actor);
        printMessage(M_SEE_END);
        if (instances[actor].container != 0)
            describeContainer(actor);
    }
    admin[actor].alreadyDescribed = TRUE;
}


