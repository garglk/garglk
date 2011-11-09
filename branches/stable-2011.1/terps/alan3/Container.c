#include "Container.h"

#include "instance.h"
#include "syserr.h"
#include "inter.h"
#include "lists.h"
#include "memory.h"
#include "current.h"
#include "msg.h"
#include "output.h"


/* PUBLIC DATA */
ContainerEntry *containers;  /* Container table pointer */


/*----------------------------------------------------------------------*/
static int countInContainer(int containerIndex)	/* IN - the container to count in */
{
    int instanceIndex, j = 0;

    for (instanceIndex = 1; instanceIndex <= header->instanceMax; instanceIndex++)
        if (in(instanceIndex, containerIndex, TRUE))
            /* Then it's in this container also */
            j++;
    return(j);
}


/*----------------------------------------------------------------------*/
static int sumAttributeInContainer(
                                   Aint containerIndex,			/* IN - the container to sum */
                                   Aint attributeIndex			/* IN - the attribute to sum over */
                                   ) {
    int instanceIndex;
    int sum = 0;

    for (instanceIndex = 1; instanceIndex <= header->instanceMax; instanceIndex++)
        if (in(instanceIndex, containerIndex, TRUE)) {	/* Then it's directly in this cont */
            if (instances[instanceIndex].container != 0)	/* This is also a container! */
                sum = sum + sumAttributeInContainer(instanceIndex, attributeIndex);
            sum = sum + getInstanceAttribute(instanceIndex, attributeIndex);
        }
    return(sum);
}


/*----------------------------------------------------------------------*/
static bool containerIsEmpty(int container)
{
    int i;

    for (i = 1; i <= header->instanceMax; i++)
        if (isDescribable(i) && in(i, container, FALSE))
            return FALSE;
    return TRUE;
}


/*======================================================================*/
void describeContainer(int container)
{
    if (!containerIsEmpty(container) && !getInstanceAttribute(container, OPAQUEATTRIBUTE))
        list(container);
}


/*======================================================================*/
bool passesContainerLimits(Aint theContainer, Aint theAddedInstance) {
    LimitEntry *limit;
    Aword props;

    if (!isContainer(theContainer))
        syserr("Checking limits for a non-container.");

    /* Find the container properties */
    props = instances[theContainer].container;

    if (containers[props].limits != 0) { /* Any limits at all? */
        for (limit = (LimitEntry *) pointerTo(containers[props].limits); !isEndOfArray(limit); limit++)
            if (limit->atr == 1-I_COUNT) { /* TODO This is actually some encoding of the attribute number, right? */
                if (countInContainer(theContainer) >= limit->val) {
                    interpret(limit->stms);
                    return(FALSE);
                }
            } else {
                if (sumAttributeInContainer(theContainer, limit->atr) + getInstanceAttribute(theAddedInstance, limit->atr) > limit->val) {
                    interpret(limit->stms);
                    return(FALSE);
                }
            }
    }
    return(TRUE);
}


/*======================================================================*/
int containerSize(int container, bool directly) {
    Aint i;
    Aint count = 0;

    for (i = 1; i <= header->instanceMax; i++) {
        if (in(i, container, directly))
            count++;
    }
    return(count);
}

/*======================================================================*/
void list(int container)
{
    int i;
    Aword props;
    Aword foundInstance[2] = {0,0};
    int found = 0;
    Aint previousThis = current.instance;

    current.instance = container;

    /* Find container table entry */
    props = instances[container].container;
    if (props == 0) syserr("Trying to list something not a container.");

    for (i = 1; i <= header->instanceMax; i++) {
        if (isDescribable(i)) {
            /* We can only see objects and actors directly in this container... */
            if (admin[i].location == container) { /* Yes, it's in this container */
                if (found == 0) {
                    if (containers[props].header != 0)
                        interpret(containers[props].header);
                    else {
                        if (isActor(containers[props].owner))
                            printMessageWithInstanceParameter(M_CARRIES, containers[props].owner);
                        else
                            printMessageWithInstanceParameter(M_CONTAINS, containers[props].owner);
                    }
                    foundInstance[0] = i;
                } else if (found == 1)
                    foundInstance[1] = i;
                else {
                    printMessageWithInstanceParameter(M_CONTAINS_COMMA, i);
                }
                found++;
            }
        }
    }

    if (found > 0) {
        if (found > 1)
            printMessageWithInstanceParameter(M_CONTAINS_AND, foundInstance[1]);
        printMessageWithInstanceParameter(M_CONTAINS_END, foundInstance[0]);
    } else {
        if (containers[props].empty != 0)
            interpret(containers[props].empty);
        else {
            if (isActor(containers[props].owner))
                printMessageWithInstanceParameter(M_EMPTYHANDED, containers[props].owner);
            else
                printMessageWithInstanceParameter(M_EMPTY, containers[props].owner);
        }
    }
    needSpace = TRUE;
    current.instance = previousThis;
}


