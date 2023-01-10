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
static int countInContainer(Aid containerIndex)	/* IN - the container to count in */
{
    int j = 0;

    for (int instanceIndex = 1; instanceIndex <= header->instanceMax; instanceIndex++)
        if (isIn(instanceIndex, containerIndex, DIRECT))
            /* Then it's in this container also */
            j++;
    return j;
}


/*----------------------------------------------------------------------*/
static int sumAttributeInContainer(
    Aid containerIndex,        /* IN - the container to sum */
    Aid attributeIndex			/* IN - the attribute to sum over */
) {
    int sum = 0;

    for (int instanceIndex = 1; instanceIndex <= header->instanceMax; instanceIndex++)
        if (isIn(instanceIndex, containerIndex, TRANSITIVE)) {
            /* We might find surprises in nested containers... */
            if (hasAttribute(instanceIndex, attributeIndex))
                sum = sum + getInstanceAttribute(instanceIndex, attributeIndex);
        }
    return sum;
}


/*----------------------------------------------------------------------*/
static bool containerIsEmpty(int container)
{
    for (int i = 1; i <= header->instanceMax; i++)
        if (isDescribable(i) && isIn(i, container, TRANSITIVE))
            return false;
    return true;
}


/*======================================================================*/
void describeContainer(int container)
{
    if (!containerIsEmpty(container) && !isOpaque(container))
        list(container);
}


/*======================================================================*/
bool passesContainerLimits(Aint theContainerInstance, Aint theAddedInstance) {
    Aint containerProps;

    if (!isAContainer(theContainerInstance))
        syserr("Checking limits for a non-container.");

    /* Find the container properties */
    containerProps = instances[theContainerInstance].container;

    if (containers[containerProps].limits != 0) { /* Any limits at all? */
        for (LimitEntry *limit = (LimitEntry *) pointerTo(containers[containerProps].limits); !isEndOfArray(limit); limit++)
            if (limit->atr == 1-I_COUNT) { /* TODO This is actually some encoding of the attribute number, right? */
                if (countInContainer(theContainerInstance) >= limit->val) {
                    interpret(limit->stms);
                    return false;
                }
            } else {
                /* First check nested containers... */
                int currentSum = sumAttributeInContainer(theContainerInstance, limit->atr);
                int addedSum = getInstanceAttribute(theAddedInstance, limit->atr);
                if (isAContainer(theAddedInstance))
                    addedSum += sumAttributeInContainer(theAddedInstance, limit->atr);
                if (currentSum + addedSum > limit->val) {
                    interpret(limit->stms);
                    return false;
                }
            }
    }
    /* Then check any possible containing containers upwards using recursion... */
    Aid location = where(theContainerInstance, DIRECT);
    if (isAContainer(location))
        return passesContainerLimits(location, theAddedInstance);
    return true;
}


/*======================================================================*/
int containerSize(int container, ATrans trans) {
    Aint count = 0;

    for (Aid i = 1; i <= header->instanceMax; i++) {
        if (isIn(i, container, trans))
            count++;
    }
    return count;
}

/*======================================================================*/
void list(int container)
{
    Aword containerProps;
    Aword foundInstance[2] = {0,0};
    int found = 0;
    Aint previousThis = current.instance;

    current.instance = container;

    /* Find container table entry */
    containerProps = instances[container].container;
    if (containerProps == 0) syserr("Trying to list something not a container.");

    for (Aid i = 1; i <= header->instanceMax; i++) {
        if (isDescribable(i)) {
            /* We can only see objects and actors directly in this container... */
            if (admin[i].location == container) { /* Yes, it's in this container */
                if (found == 0) {
                    if (containers[containerProps].header != 0)
                        interpret(containers[containerProps].header);
                    else {
                        if (isAActor(containers[containerProps].owner))
                            printMessageWithInstanceParameter(M_CARRIES, containers[containerProps].owner);
                        else
                            printMessageWithInstanceParameter(M_CONTAINS, containers[containerProps].owner);
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
        if (containers[containerProps].empty != 0)
            interpret(containers[containerProps].empty);
        else {
            if (isAActor(containers[containerProps].owner))
                printMessageWithInstanceParameter(M_EMPTYHANDED, containers[containerProps].owner);
            else
                printMessageWithInstanceParameter(M_EMPTY, containers[containerProps].owner);
        }
    }
    needSpace = true;
    current.instance = previousThis;
}
