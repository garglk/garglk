#include "instance.h"

/* IMPORTS */
#include "memory.h"
#include "syserr.h"
#include "attribute.h"
#include "current.h"
#include "Container.h"
#include "options.h"
#include "debug.h"
#include "checkentry.h"
#include "inter.h"
#include "output.h"
#include "class.h"
#include "msg.h"
#include "actor.h"
#include "literal.h"
#include "dictionary.h"
#include "Location.h"
#include "compatibility.h"


/* PUBLIC DATA */

InstanceEntry *instances;   /* Instance table pointer */

AdminEntry *admin;      /* Administrative data about instances */
AttributeEntry *attributes; /* Dynamic attribute values */

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/* Instance query methods */

/*======================================================================*/
bool isA(int instance, int ancestor)
{
    int parent;

    if (isLiteral(instance))
        parent = literals[instance-header->instanceMax].class;
    else
        parent = instances[instance].parent;
    while (parent != 0 && parent != ancestor)
        parent = classes[parent].parent;

    return (parent != 0);
}


bool isAObject(int instance)
{
  return isA(instance, OBJECT);
}

bool isAContainer(int instance)
{
    return instance != 0 && !isLiteral(instance) && instances[instance].container != 0;
}

bool isAActor(int instance)
{
  return isA(instance, ACTOR);
}

bool isALocation(int instance)
{
  return isA(instance, LOCATION);
}


bool isLiteral(int instance)
{
  return instance > header->instanceMax;
}

bool isANumeric(int instance)
{
  return isLiteral(instance) && literals[literalFromInstance(instance)].type == NUMERIC_LITERAL;
}

bool isAString(int instance)
{
  return isLiteral(instance) && literals[literalFromInstance(instance)].type == STRING_LITERAL;
}


/*======================================================================*/
bool isOpaque(int container) {
    return getInstanceAttribute(container, OPAQUEATTRIBUTE);
}


/*======================================================================*/
void setInstanceAttribute(int instance, int attribute, Aptr value)
{
    char str[80];

    if (instance > 0 && instance <= header->instanceMax) {
        setAttribute(admin[instance].attributes, attribute, value);
        if (isALocation(instance) && attribute != VISITSATTRIBUTE)
            /* If it wasn't the VISITSATTRIBUTE the location may have
               changed so describe next time */
            admin[instance].visitsCount = 0;
    } else {
        sprintf(str, "Can't SET/MAKE instance (%d).", instance);
        syserr(str);
    }
}


/*======================================================================*/
void setInstanceStringAttribute(int instance, int attribute, char *string)
{
    deallocate(fromAptr(getInstanceAttribute(instance, attribute)));
    setInstanceAttribute(instance, attribute, toAptr(string));
}


/*======================================================================*/
void setInstanceSetAttribute(int instance, int attribute, Aptr set)
{
    freeSet((Set *)fromAptr(getInstanceAttribute(instance, attribute)));
    setInstanceAttribute(instance, attribute, set);
}


/*----------------------------------------------------------------------*/
static Aptr literalAttribute(int literal, int attribute)
{
    if (isPreBeta3(header->version)) {
        if (attribute == 1)
            return literals[literalFromInstance(literal)].value;
        else
            return 0;
    } else {
        if (attribute == 0)
            return literals[literalFromInstance(literal)].value;
        else
            return getAttribute(admin[header->instanceMax].attributes, attribute);
    }
    return(EOF);
}


/*======================================================================*/
Aptr getInstanceAttribute(int instance, int attribute)
{
    char str[80];

    if (isLiteral(instance))
        return literalAttribute(instance, attribute);
    else {
        if (instance > 0 && instance <= header->instanceMax) {
            if (attribute == -1)
                return locationOf(instance);
            else
                return getAttribute(admin[instance].attributes, attribute);
        } else {
            sprintf(str, "Can't ATTRIBUTE item (%d).", instance);
            syserr(str);
        }
    }
    return(EOF);
}


/*======================================================================*/
char *getInstanceStringAttribute(int instance, int attribute)
{
    return strdup((char *)fromAptr(getInstanceAttribute(instance, attribute)));
}


/*======================================================================*/
Set *getInstanceSetAttribute(int instance, int attribute)
{
    return copySet((Set *)fromAptr(getInstanceAttribute(instance, attribute)));
}


/*----------------------------------------------------------------------*/
static void verifyInstance(int instance, char *action) {
    char message[200];

    if (instance == 0) {
        sprintf(message, "Can't %s instance (%d).", action, instance);
        syserr(message);
    } else if (instance > header->instanceMax) {
        sprintf(message, "Can't %s instance (%d > instanceMax).", action, instance);
        syserr(message);
    }
}


/*======================================================================*/
bool isHere(int id, ATrans transitivity)
{
    verifyInstance(id, "HERE");

    return isAt(id, current.location, transitivity);
}


/*======================================================================*/
bool isNearby(int instance, ATrans transitivity)
{
    verifyInstance(instance, "NEARBY");

    if (isALocation(instance))
        return exitto(current.location, instance);
    else
        return exitto(current.location, where(instance, transitivity));
}


/*======================================================================*/
bool isNear(int instance, int other, ATrans trans)
{
    Aint l1, l2;

    verifyInstance(instance, "NEAR");

    if (isALocation(instance))
        l1 = instance;
    else
        l1 = where(instance, trans);
    if (isALocation(other))
        l2 = other;
    else
        l2 = where(other, trans);
    return exitto(l2, l1);
}


/*======================================================================*/
/* Look in a container to see if the instance is in it. */
bool isIn(int instance, int container, ATrans trans)
{
    int loc;

    if (!isAContainer(container))
        syserr("IN in a non-container.");

    if (trans == DIRECT)
        return admin[instance].location == container;
    else {
        loc = admin[instance].location;
        if (trans == INDIRECT && loc != 0 && !isA(loc, LOCATION))
            loc = admin[loc].location;
        while (loc != 0 && !isA(loc, LOCATION))
            if (loc == container)
                return TRUE;
            else
                loc = admin[loc].location;
        return FALSE;
    }
}



/*======================================================================*/
/* Look see if an instance is AT another. */
bool isAt(int instance, int other, ATrans trans)
{
    if (instance == 0 || other == 0) return FALSE;

    if (isALocation(instance)) {
        /* Nested locations */
        /* TODO - What if the other is not a location? */
        int current = admin[instance].location;
        switch (trans) {
        case DIRECT:
            return admin[instance].location == other;
        case INDIRECT:
            if (current == other)
                return FALSE;
            current = admin[current].location;
        case TRANSITIVE:
            while (current != 0) {
                if (current == other)
                    return TRUE;
                else
                    current = admin[current].location;
            }
            return FALSE;
        }
        syserr("Unexpected value in switch in isAt() for location");
        return FALSE;
    } else if (isALocation(other)) {
        /* Instance is not a location but other is */
        switch (trans) {
        case DIRECT:
            return admin[instance].location == other;
        case INDIRECT: {
            if (admin[instance].location == other)
                return FALSE;   /* Directly, so not Indirectly */
            /* Fall through to transitive handling of the location */
        }
        case TRANSITIVE: {
            int location = locationOf(instance);
            int current = other;
            while (current != 0) {
                if (current == location)
                    return TRUE;
                else
                    current = admin[current].location;
            }
            return FALSE;
        }
        }
        syserr("Unexpected value in switch in isAt() for non-location");
        return FALSE;
    } else {
        /* Other is also not a location */
        switch (trans) {
        case DIRECT:
            return positionOf(instance) == admin[other].location;
        case INDIRECT: {
            int location = locationOf(instance);
            int current = other;
            if (location == current)
                return FALSE;
            else
                current = admin[current].location;
            while (current != 0) {
                if (current == location)
                    return TRUE;
                else
                    current = admin[current].location;
            }
            return FALSE;
        }
        case TRANSITIVE: {
            int location = locationOf(other);
            int current = locationOf(instance);
            bool ok = FALSE;
            while (current != 0 && !ok) {
                if (current == location)
                    ok = TRUE;
                else
                    current = admin[current].location;
            }
            return ok;
        }
        }
        syserr("Unexpected value in switch in isAt() for non-location");
        return FALSE;
    }
}


/*======================================================================*/
/* Return the *location* of an instance, transitively, i.e. the first
   location instance found when traversing the containment/position
   links. If that didn't turn up a location see if it was in a
   container that is somewhere, or a THING that is nowhere. It might
   also be an ENTITY which is always everywhere so take that to mean
   where the hero is. */
int locationOf(int instance)
{
    int position;
    int container = 0;

    verifyInstance(instance, "get LOCATION of");

    position = admin[instance].location;
    while (position != 0 && !isALocation(position)) {
        container = position;   /* Remember innermost container */
        position = admin[position].location;
    }
    if (position > NOWHERE) /* It was a location so return that */
        return position;
    else {
        /* If we did not find a location then it might be in a container */
        if (container != 0)
            instance = container;
        /* If the instance or the container it was in is a THING then its nowhere. */
        if (isA(instance, THING))
            return NOWHERE;     /* #nowhere */
        else if (isALocation(instance))
            return NO_LOCATION; /* No location */
        else
            return locationOf(HERO);
    }
}


/*======================================================================*/
/* Return the current position of an instance, directly or not */
/* TODO: this will be a possible duplicate of where() */
int positionOf(int instance)
{
    return admin[instance].location;
}


/*======================================================================*/
/* Return the current position of an instance, directly or not */
int where(int instance, ATrans trans)
{
    verifyInstance(instance, "WHERE");

    if (isALocation(instance))
        return 0;
    else if (trans == DIRECT)
        return admin[instance].location;
    else
        return locationOf(instance);
}


/*----------------------------------------------------------------------*/
static bool executeInheritedMentioned(int class) {
    if (class == 0) return FALSE;

    if (classes[class].mentioned) {
        interpret(classes[class].mentioned);
        return TRUE;
    } else
        return executeInheritedMentioned(classes[class].parent);
}


/*----------------------------------------------------------------------*/
static bool mention(int instance) {
    if (instances[instance].mentioned) {
        interpret(instances[instance].mentioned);
        return TRUE;
    } else
        return executeInheritedMentioned(instances[instance].parent);
}


/*======================================================================*/
void sayInstance(int instance)
{
#ifdef SAY_INSTANCE_WITH_PLAYER_WORDS_IF_PARAMETER
    int p, i;

    /* Find the id in the parameters... */
    if (params != NULL)
        for (p = 0; params[p].code != EOF; p++)
            if (params[p].code == instance) {
                /* Found it so.. */
                if (params[p].firstWord == EOF) /* Any words he used? */
                    break;      /* No... */
                else {              /* Yes, so use them... */
                    char *capitalized;
                    /* Assuming the noun is the last word we can simply output the adjectives... */
                    for (i = params[p].firstWord; i <= params[p].lastWord-1; i++)
                        output((char *)pointerTo(dict[wrds[i]].wrd));
                    /* ... and then the noun, capitalized if necessary */
                    if (header->capitalizeNouns) {
                        capitalized = strdup((char *)pointerTo(dict[wrds[params[p].lastWord]].wrd));
                        capitalized[0] = IsoToUpperCase(capitalized[0]);
                        output(capitalized);
                        deallocate(capitalized);
                    } else
                        output((char *)pointerTo(dict[wrds[params[p].lastWord]].wrd));
                }
                return;
            }
#endif
    if (!mention(instance))
        interpret(instances[instance].name);
}


/*======================================================================*/
void sayInteger(int value)
{
    char buf[25];

    if (isHere(HERO, FALSE)) {
        sprintf(buf, "%d", value);
        output(buf);
    }
}


/*======================================================================*/
void sayString(char *string)
{
    if (isHere(HERO, FALSE))
        output(string);
    deallocate(string);
}


/*----------------------------------------------------------------------*/
static void sayLiteral(int literal)
{
    char *str;

    if (isANumeric(literal))
        sayInteger(literals[literal-header->instanceMax].value);
    else {
        str = (char *)strdup((char *)fromAptr(literals[literal-header->instanceMax].value));
        sayString(str);
    }
}


/*----------------------------------------------------------------------*/
static char *wordWithCode(int classBit, int code) {
    int w;
    char str[50];

    for (w = 0; w < dictionarySize; w++)
        if (dictionary[w].code == code && ((classBit&dictionary[w].classBits) != 0))
            return pointerTo(dictionary[w].string);
    sprintf(str, "Could not find word of class %d with code %d.", classBit, code);
    syserr(str);
    return NULL;
}


/*----------------------------------------------------------------------*/
static bool sayInheritedDefiniteForm(int class) {
    if (class == 0) {
        syserr("No default definite article");
        return FALSE;
    } else {
        if (classes[class].definite.address) {
            interpret(classes[class].definite.address);
            return classes[class].definite.isForm;
        } else
            return sayInheritedDefiniteForm(classes[class].parent);
    }
}


/*----------------------------------------------------------------------*/
static void sayDefinite(int instance) {
    if (instances[instance].definite.address) {
        interpret(instances[instance].definite.address);
        if (!instances[instance].definite.isForm)
            sayInstance(instance);
    } else
        if (!sayInheritedDefiniteForm(instances[instance].parent))
            sayInstance(instance);
}


/*----------------------------------------------------------------------*/
static bool sayInheritedIndefiniteForm(int class) {
    if (class == 0) {
        syserr("No default indefinite article");
        return FALSE;
    } else {
        if (classes[class].indefinite.address) {
            interpret(classes[class].indefinite.address);
            return classes[class].indefinite.isForm;
        } else
            return sayInheritedIndefiniteForm(classes[class].parent);
    }
}


/*----------------------------------------------------------------------*/
static void sayIndefinite(int instance) {
    if (instances[instance].indefinite.address) {
        interpret(instances[instance].indefinite.address);
        if (!instances[instance].indefinite.isForm)
            sayInstance(instance);
    } else
        if (!sayInheritedIndefiniteForm(instances[instance].parent))
            sayInstance(instance);
}


/*----------------------------------------------------------------------*/
static bool sayInheritedNegativeForm(int class) {
    if (class == 0) {
        syserr("No default negative form");
        return FALSE;
    } else {
        if (classes[class].negative.address) {
            interpret(classes[class].negative.address);
            return classes[class].negative.isForm;
        } else
            return sayInheritedNegativeForm(classes[class].parent);
    }
}


/*----------------------------------------------------------------------*/
static void sayNegative(int instance) {
    if (instances[instance].negative.address) {
        interpret(instances[instance].negative.address);
        if (!instances[instance].negative.isForm)
            sayInstance(instance);
    } else
        if (!sayInheritedNegativeForm(instances[instance].parent))
            sayInstance(instance);
}


/*----------------------------------------------------------------------*/
static void sayInheritedPronoun(int instance) {
    if (instance == 0)
        syserr("No default pronoun");
    else {
        if (classes[instance].pronoun != 0)
            output(wordWithCode(PRONOUN_BIT, classes[instance].pronoun));
        else
            sayInheritedPronoun(classes[instance].parent);
    }
}


/*----------------------------------------------------------------------*/
static void sayPronoun(int instance) {
    if (instances[instance].pronoun != 0)
        output(wordWithCode(PRONOUN_BIT, instances[instance].pronoun));
    else
        sayInheritedPronoun(instances[instance].parent);
}


/*----------------------------------------------------------------------*/
static void sayArticleOrForm(int id, SayForm form)
{
    if (!isLiteral(id))
        switch (form) {
        case SAY_DEFINITE:
            sayDefinite(id);
            break;
        case SAY_INDEFINITE:
            sayIndefinite(id);
            break;
        case SAY_NEGATIVE:
            sayNegative(id);
            break;
        case SAY_PRONOUN:
            sayPronoun(id);
            break;
        case SAY_SIMPLE:
            say(id);
            break;
        default:
            syserr("Unexpected form in 'sayArticleOrForm()'");
        }
    else
        say(id);
}


/*======================================================================*/
void say(int instance)
{
    Aword previousInstance = current.instance;
    current.instance = instance;

    if (isHere(HERO, FALSE)) {
        if (isLiteral(instance))
            sayLiteral(instance);
        else {
            verifyInstance(instance, "SAY");
            sayInstance(instance);
        }
    }
    current.instance = previousInstance;
}


/*======================================================================*/
void sayForm(int instance, SayForm form)
{
    Aword previousInstance = current.instance;
    current.instance = instance;

    sayArticleOrForm(instance, form);

    current.instance = previousInstance;
}


/*======================================================================*/
bool isDescribable(int instance) {
    return isAObject(instance) || isAActor(instance);
}


/*----------------------------------------------------------------------*/
static bool inheritsDescriptionFrom(int class)
{
    if (classes[class].description != 0)
        return TRUE;
    else if (classes[class].parent != 0)
        return inheritsDescriptionFrom(classes[class].parent);
    else
        return FALSE;
}


/*======================================================================*/
bool hasDescription(int instance)
{
    if (instances[instance].description != 0)
        return TRUE;
    else if (instances[instance].parent != 0)
        return inheritsDescriptionFrom(instances[instance].parent);
    else
        return FALSE;
}


/*----------------------------------------------------------------------*/
static void describeClass(int instance)
{
    if (classes[instance].description != 0) {
        /* This class has a description, run it */
        interpret(classes[instance].description);
    } else {
        /* Search up the inheritance tree, if any, to find a description */
        if (classes[instance].parent != 0)
            describeClass(classes[instance].parent);
    }
}


/*======================================================================*/
void describeAnything(int instance)
{
    if (instances[instance].description != 0) {
        /* This instance has its own description, run it */
        interpret(instances[instance].description);
    } else {
        /* Search up the inheritance tree to find a description */
        if (instances[instance].parent != 0)
            describeClass(instances[instance].parent);
    }
    admin[instance].alreadyDescribed = TRUE;
}


/*----------------------------------------------------------------------*/
static void describeObject(int object)
{
    if (hasDescription(object))
        describeAnything(object);
    else {
        printMessageWithInstanceParameter(M_SEE_START, object);
        printMessage(M_SEE_END);
        if (instances[object].container != 0)
            describeContainer(object);
    }
    admin[object].alreadyDescribed = TRUE;
}


/*----------------------------------------------------------------------*/
static bool inheritedDescriptionCheck(int class)
{
    if (class == 0) return TRUE;
    if (!inheritedDescriptionCheck(classes[class].parent)) return FALSE;
    if (classes[class].descriptionChecks == 0) return TRUE;
    return !checksFailed(classes[class].descriptionChecks, TRUE);
}

/*----------------------------------------------------------------------*/
static bool descriptionCheck(int instance)
{
    int previousInstance = current.instance;
    bool r;

    current.instance = instance;
    if (inheritedDescriptionCheck(instances[instance].parent)) {
        if (instances[instance].checks == 0)
            r = TRUE;
        else
            r = !checksFailed(instances[instance].checks, TRUE);
    } else
        r = FALSE;
    current.instance = previousInstance;
    return r;
}


/*======================================================================*/
void describeInstances(void)
{
    int i;
    int lastInstanceFound = 0;
    int found = 0;

    /* First describe every object here with its own description */
    for (i = 1; i <= header->instanceMax; i++)
        if (admin[i].location == current.location && isAObject(i) &&
                !admin[i].alreadyDescribed && hasDescription(i))
            describe(i);

    /* Then list all things without a description */
    for (i = 1; i <= header->instanceMax; i++)
        if (admin[i].location == current.location
                && !admin[i].alreadyDescribed
                && isAObject(i)
                && descriptionCheck(i)) {
            if (found == 0)
                printMessageWithInstanceParameter(M_SEE_START, i);
            else if (found > 1)
                printMessageWithInstanceParameter(M_SEE_COMMA, lastInstanceFound);
            admin[i].alreadyDescribed = TRUE;

            // TODO : isOpaque()
            if (instances[i].container && containerSize(i, DIRECT) > 0 && !getInstanceAttribute(i, OPAQUEATTRIBUTE)) {
                if (found > 0)
                    printMessageWithInstanceParameter(M_SEE_AND, i);
                printMessage(M_SEE_END);
                describeContainer(i);
                found = 0;
                continue;       /* Actually start another list. */
            }
            found++;
            lastInstanceFound = i;
        }

    if (found > 0) {
        if (found > 1) {
            printMessageWithInstanceParameter(M_SEE_AND, lastInstanceFound);
        }
        printMessage(M_SEE_END);
    }

    /* Finally all actors with a separate description */
    for (i = 1; i <= header->instanceMax; i++)
        if (admin[i].location == current.location && i != HERO && isAActor(i)
        && !admin[i].alreadyDescribed)
            describe(i);

    /* Clear the describe flag for all instances */
    for (i = 1; i <= header->instanceMax; i++)
        admin[i].alreadyDescribed = FALSE;
}


/*======================================================================*/
bool describe(int instance)
{
    bool descriptionOk;
    int previousInstance = current.instance;

    current.instance = instance;
    verifyInstance(instance, "DESCRIBE");
    if (descriptionCheck(instance)) {
        descriptionOk = TRUE;
        if (isAObject(instance)) {
            describeObject(instance);
        } else if (isAActor(instance)) {
            describeActor(instance);
        } else
            describeAnything(instance);
    } else
        descriptionOk = FALSE;
    current.instance = previousInstance;
    return descriptionOk;
}


/*----------------------------------------------------------------------*/
static void locateIntoContainer(Aword theInstance, Aword theContainer) {
    if (!isA(theInstance, containers[instances[theContainer].container].class))
        printMessageUsing2InstanceParameters(M_CANNOTCONTAIN, theContainer, theInstance);
    else if (passesContainerLimits(theContainer, theInstance))
        admin[theInstance].location = theContainer;
    else
        abortPlayerCommand();
}


/*----------------------------------------------------------------------*/
static void locateLocation(Aword loc, Aword whr)
{
    Aint l = whr;

    /* Ensure this does not create a recursive location chain */
    while (l != 0) {
        if (admin[l].location == loc)
            apperr("Locating a location that would create a recursive loop of locations containing each other.");
        else
            l = admin[l].location;
    }
    admin[loc].location = whr;
}


/*----------------------------------------------------------------------*/
static void locateObject(Aword obj, Aword whr)
{
    if (isAContainer(whr)) { /* Into a container */
        locateIntoContainer(obj, whr);
    } else {
        admin[obj].location = whr;
        /* Make sure the location is described since it's changed */
        admin[whr].visitsCount = 0;
    }
}


/*----------------------------------------------------------------------*/
static void traceEnteredClass(Aint theClass, bool empty) {
    printf("\n<ENTERED in class ");
    printf("%s", idOfClass(theClass));
    printf("[%d]%s>\n", theClass, empty?" is empty":":");
}


/*----------------------------------------------------------------------*/
static void traceEnteredInstance(Aint instance, bool empty) {
    printf("\n<ENTERED in instance ");
    traceSay(instance);
    printf("[%d]%s>\n", instance, empty?" is empty":"");
}


/*----------------------------------------------------------------------*/
static void executeInheritedEntered(Aint theClass) {
    if (theClass == 0) return;
    executeInheritedEntered(classes[theClass].parent);
    if (traceSectionOption)
        traceEnteredClass(theClass, classes[theClass].entered == 0);
    if (classes[theClass].entered) {
        interpret(classes[theClass].entered);
    }
}


/*----------------------------------------------------------------------*/
static void executeEntered(Aint instance) {
    int currentInstance = current.instance;
    current.instance = instance;
    if (admin[instance].location != 0)
        executeEntered(admin[instance].location);
    executeInheritedEntered(instances[instance].parent);
    if (traceSectionOption)
        traceEnteredInstance(instance, instances[instance].entered == 0);
    if (instances[instance].entered != 0) {
        interpret(instances[instance].entered);
    }
    current.instance = currentInstance;
}


/*----------------------------------------------------------------------*/
static int getVisits(int location) {
    return getInstanceAttribute(location, VISITSATTRIBUTE);
}


/*----------------------------------------------------------------------*/
static void incrementVisits(int location) {
    setInstanceAttribute(location, VISITSATTRIBUTE, getVisits(location)+1);
    if (admin[location].location != 0)
        /* Nested location, so increment that too */
        incrementVisits(admin[location].location);
}


/*----------------------------------------------------------------------*/
static void revisited(void) {
    if (anyOutput)
        para();
    say(where(HERO, DIRECT));
    printMessage(M_AGAIN);
    newline();
    describeInstances();
}


/*----------------------------------------------------------------------*/
static bool shouldBeDescribed(void) {
    if (!isPreBeta5(header->version))
        return getVisits(admin[HERO].location) % (current.visits+1) == 0
            || admin[admin[HERO].location].visitsCount == 0;
    else
        return admin[admin[HERO].location].visitsCount % (current.visits+1) == 0;
}


/*----------------------------------------------------------------------*/
static void locateActor(Aint movingActor, Aint whr)
{
    Aint previousCurrentLocation = current.location;
    Aint previousActorLocation = admin[movingActor].location;
    Aint previousActor = current.actor;
    Aint previousInstance = current.instance;

    /* Before leaving, remember that we visited the location */
    if (!isPreBeta5(header->version))
        if (movingActor == HERO)
            incrementVisits(where(HERO, DIRECT));

    /* TODO Actors locating into containers is dubious, anyway as it
       is now it allows the hero to be located into a container. And what
       happens with current location if so... */
    if (isAContainer(whr))
        locateIntoContainer(movingActor, whr);
    else {
        current.location = whr;
        admin[movingActor].location = whr;
    }

    /* Now we have moved, so show what is needed... */
    current.instance = current.location;

    /* Execute possible entered */
    current.actor = movingActor;
    if (previousActorLocation != current.location) {
        executeEntered(current.location);
    }
    current.instance = previousInstance;
    current.actor = previousActor;

    if (movingActor == HERO) {
        if (shouldBeDescribed())
            look();
        else
            revisited();
        admin[where(HERO, DIRECT)].visitsCount++;
    } else
        /* Ensure that the location will be described to the hero next time */
        admin[whr].visitsCount = 0;

    if (current.actor != movingActor)
        current.location = previousCurrentLocation;

    current.instance = previousInstance;
}


/*----------------------------------------------------------------------*/
static void traceExtract(int instance, int containerId, char *what) {
    if (traceSectionOption) {
        printf("\n<EXTRACT from ");
        traceSay(instance);
        printf("[%d, container %d], %s:>\n", instance, containerId, what);
    }
}


/*----------------------------------------------------------------------*/
static void containmentLoopError(int instance, int whr) {
    ParameterArray parameters = newParameterArray();
    if (isPreBeta4(header->version))
        output("That would be to put something inside itself.");
    else if (whr == instance) {
        addParameterForInstance(parameters, instance);
        printMessageWithParameters(M_CONTAINMENT_LOOP, parameters);
    } else {
        addParameterForInstance(parameters, instance);
        addParameterForInstance(parameters, whr);
        printMessageWithParameters(M_CONTAINMENT_LOOP2, parameters);
    }
    free(parameters);
    error(NO_MSG);
}


/*----------------------------------------------------------------------*/
static void runExtractStatements(int instance, int containerId) {
    ContainerEntry *theContainer = &containers[containerId];

    if (theContainer->extractStatements != 0) {
        traceExtract(instance, containerId, "Executing");
        interpret(theContainer->extractStatements);
    }
}


/*----------------------------------------------------------------------*/
static bool runExtractChecks(int instance, int containerId) {
    ContainerEntry *theContainer = &containers[containerId];

    if (theContainer->extractChecks != 0) {
        traceExtract(instance, containerId, "Checking");
        if (checksFailed(theContainer->extractChecks, EXECUTE_CHECK_BODY_ON_FAIL)) {
            fail = TRUE;
            return FALSE;       /* Failed! */
        }
    }
    return TRUE;                /* Passed! */
}


/*======================================================================*/
void locate(int instance, int whr)
{
    int containerId;
    int previousInstance = current.instance;

    verifyInstance(instance, "LOCATE");
    verifyInstance(whr, "LOCATE AT");

    /* Will this create a containment loop? */
    if (whr == instance || (isAContainer(instance) && isIn(whr, instance, TRANSITIVE)))
        containmentLoopError(instance, whr);

    /* First check if the instance is in a container, if so run extract checks */
    if (isAContainer(admin[instance].location)) {    /* In something? */
        int loc = admin[instance].location;

        /* Run all nested extraction checks */
        while (isAContainer(loc)) {
            current.instance = loc;
            containerId = instances[loc].container;

            if (!runExtractChecks(instance, containerId)) {
                current.instance = previousInstance;
                return;
            }
            runExtractStatements(instance, containerId);
            loc = admin[loc].location;
        }
        current.instance = previousInstance;
    }

    if (isAActor(instance))
        locateActor(instance, whr);
    else if (isALocation(instance))
        locateLocation(instance, whr);
    else
        locateObject(instance, whr);

    gameStateChanged = TRUE;
}
