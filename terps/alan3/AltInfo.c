#include "AltInfo.h"

/*----------------------------------------------------------------------*\
 
An info node about the Verb Alternatives found and possibly executed
 
\*----------------------------------------------------------------------*/

#include "types.h"

#include "checkentry.h"
#include "debug.h"
#include "inter.h"
#include "lists.h"
#include "instance.h"
#include "options.h"
#include "memory.h"
#include "current.h"
#include "class.h"
#include "params.h"
#include "literal.h"
#include "syntax.h"


/* Types */
typedef AltInfo *AltInfoFinder(int verb, Parameter parameters[]);


/*======================================================================*/
void primeAltInfo(AltInfo *altInfo, int level, int parameter, int instance, int class)
{
    altInfo->level = level;
    altInfo->parameter = parameter;
    altInfo->instance = instance;
    altInfo->class = class;
    altInfo->done = FALSE;
    altInfo->end = FALSE;
}


/*----------------------------------------------------------------------*/
static void traceInstanceAndItsClass(AltInfo *alt)
{
    traceSay(globalParameters[alt->parameter-1].instance);
    printf("[%d]", globalParameters[alt->parameter-1].instance);
    if (alt->class != NO_CLASS)
        printf(", inherited from %s[%d]", idOfClass(alt->class), alt->class);
}


/*----------------------------------------------------------------------*/
static void traceAltInfo(AltInfo *alt) {
    switch (alt->level) {
    case GLOBAL_LEVEL:
        printf("GLOBAL");
        break;
    case LOCATION_LEVEL:
        printf("in (location) ");
        traceInstanceAndItsClass(alt);
        break;
    case PARAMETER_LEVEL: {
		char *parameterName = parameterNameInSyntax(alt->parameter);
		if (parameterName != NULL)
			printf("in parameter %s(#%d)=", parameterName, alt->parameter);
		else
			printf("in parameter #%d=", alt->parameter);
        traceInstanceAndItsClass(alt);
        break;
	}
    }
}


/*----------------------------------------------------------------------*/
static void traceVerbCheck(AltInfo *alt, bool execute)
{
    if (sectionTraceOption && execute) {
        printf("\n<VERB %d, ", current.verb);
        traceAltInfo(alt);
        printf(", CHECK:>\n");
    }
}


/*======================================================================*/
bool checkFailed(AltInfo *altInfo, bool execute)
{
    if (altInfo->alt != NULL && altInfo->alt->checks != 0) {
        traceVerbCheck(altInfo, execute);
        // TODO Why does this not generate a regression error with !
        // Need a new regression case?
        if (checksFailed(altInfo->alt->checks, execute)) return TRUE;
        if (fail) return TRUE;
    }
    return FALSE;
}


/*----------------------------------------------------------------------*/
static void traceVerbExecution(AltInfo *alt)
{
    if (sectionTraceOption) {
        printf("\n<VERB %d, ", current.verb);
        traceAltInfo(alt);
        printf(", DOES");
        switch (alt->alt->qual) {
        case Q_BEFORE: printf(" (BEFORE)");
            break;
        case Q_ONLY: printf(" (ONLY)");
            break;
        case Q_AFTER: printf(" (AFTER)");
            break;
        case Q_DEFAULT:
            break;
        }
        printf(":>\n");
    }
}


/*======================================================================*/
bool executedOk(AltInfo *altInfo)
{
    fail = FALSE;
    if (!altInfo->done && altInfo->alt->action != 0) {
        traceVerbExecution(altInfo);
        current.instance = altInfo->instance;
        interpret(altInfo->alt->action);
    }
    altInfo->done = TRUE;
    return !fail;
}


/*======================================================================*/
bool canBeExecuted(AltInfo *altInfo) {
    return altInfo->alt != NULL && altInfo->alt->action != 0;
}


/*======================================================================*/
AltInfo *duplicateAltInfoArray(AltInfo original[]) {
    int size;
    AltInfo *duplicate;
	
    for (size = 0; original[size].end != TRUE; size++)
        ;
    size++;
    duplicate = allocate(size*sizeof(AltInfo));
    memcpy(duplicate, original, size*sizeof(AltInfo));
    return duplicate;
}


/*======================================================================*/
int lastAltInfoIndex(AltInfo altInfo[])
{
    int altIndex;
	
    /* Loop to last alternative */
    for (altIndex = -1; !altInfo[altIndex+1].end; altIndex++)
        ;
    return(altIndex);
}


/*----------------------------------------------------------------------*/
static AltInfo *nextFreeAltInfo(AltInfoArray altInfos) {
    return &altInfos[lastAltInfoIndex(altInfos)+1];
}


/*----------------------------------------------------------------------*/
static void addAlternative(AltInfoArray altInfos, int verb, int level, Aint parameterNumber, Aint theClass, Aid theInstance, AltEntryFinder finder) {
    AltInfo *altInfoP = nextFreeAltInfo(altInfos);
	
    altInfoP->alt = (*finder)(verb, parameterNumber, theInstance, theClass);
    if (altInfoP->alt != NULL) {
        primeAltInfo(altInfoP, level, parameterNumber, theInstance, theClass);
        altInfoP[1].end = TRUE;
    }
}


/*----------------------------------------------------------------------*/
static void addGlobalAlternatives(AltInfoArray altInfos, int verb, AltEntryFinder finder ) {
    addAlternative(altInfos, verb, GLOBAL_LEVEL, NO_PARAMETER, NO_CLASS, NO_INSTANCE, finder);
}


/*----------------------------------------------------------------------*/
static void addAlternativesFromParents(AltInfoArray altInfos, int verb, int level, Aint parameterNumber, Aint theClass, Aid theInstance, AltEntryFinder finder){
    if (classes[theClass].parent != 0)
        addAlternativesFromParents(altInfos, verb, level,
                                   parameterNumber,
                                   classes[theClass].parent,
                                   theInstance,
                                   finder);
	
    addAlternative(altInfos, verb, level, parameterNumber, theClass, theInstance, finder);
}


/*----------------------------------------------------------------------*/
static void addAlternativesFromLocation(AltInfoArray altInfos, int verb, Aid location, AltEntryFinder finder) {
    if (admin[location].location != 0)
        addAlternativesFromLocation(altInfos, verb, admin[location].location, finder);
	
    addAlternativesFromParents(altInfos, verb,
                               LOCATION_LEVEL,
                               NO_PARAMETER,
                               instances[location].parent,
                               location,
                               finder);
	
    addAlternative(altInfos, verb, LOCATION_LEVEL, NO_PARAMETER, NO_CLASS, location, finder);
}


/*----------------------------------------------------------------------*/
static void addAlternativesFromParameter(AltInfoArray altInfos, int verb, Parameter parameters[], int parameterNumber, AltEntryFinder finder) {
    Aid parent;
    Aid theInstance = parameters[parameterNumber-1].instance;
	
    if (isLiteral(theInstance))
        parent = literals[literalFromInstance(theInstance)].class;
    else
        parent = instances[theInstance].parent;
    addAlternativesFromParents(altInfos, verb, PARAMETER_LEVEL, parameterNumber, parent, theInstance, finder);
	
    if (!isLiteral(theInstance))
        addAlternative(altInfos, verb, PARAMETER_LEVEL, parameterNumber, NO_CLASS, theInstance, finder);
}


/*======================================================================*/
bool anyCheckFailed(AltInfoArray altInfo, bool execute)
{
    int altIndex;
    
    if (altInfo != NULL)
	for (altIndex = 0; !altInfo[altIndex].end; altIndex++) {
	    current.instance = altInfo[altIndex].instance;
	    if (checkFailed(&altInfo[altIndex], execute))
		return TRUE;
	}
    return FALSE;
}


/*======================================================================*/
bool anythingToExecute(AltInfo altInfo[])
{
    int altIndex;
	
    /* Check for anything to execute... */
    if (altInfo != NULL)
	for (altIndex = 0; !altInfo[altIndex].end; altIndex++)
	    if (canBeExecuted(&altInfo[altIndex]))
		return TRUE;
    return FALSE;
}


/*----------------------------------------------------------------------*/
static AltEntry *findAlternative(Aaddr verbTableAddress, int verbCode, int parameterNumber)
{
    AltEntry *alt;
    VerbEntry *verbEntry;
	
    if (verbTableAddress == 0) return NULL;
	
    for (verbEntry = (VerbEntry *) pointerTo(verbTableAddress); !isEndOfArray(verbEntry); verbEntry++)
        if (verbEntry->code == verbCode) {
            for (alt = (AltEntry *) pointerTo(verbEntry->alts); !isEndOfArray(alt); alt++) {
                if (alt->param == parameterNumber || alt->param == 0)
                    return alt;
            }
            return NULL;
        }
    return NULL;
}


/*----------------------------------------------------------------------*/
static AltEntry *alternativeFinder(int verb, int parameterNumber, int theInstance, int theClass)
{
    if (theClass != NO_CLASS)
        return findAlternative(classes[theClass].verbs, verb, parameterNumber);
    else if (theInstance != NO_INSTANCE)
        return findAlternative(instances[theInstance].verbs, verb, parameterNumber);
    else
        return findAlternative(header->verbTableAddress, verb, parameterNumber);
}


/*======================================================================*/
AltInfo *findAllAlternatives(int verb, Parameter parameters[]) {
    int parameterNumber;
    AltInfo altInfos[1000];
    altInfos[0].end = TRUE;
    
    addGlobalAlternatives(altInfos, verb, &alternativeFinder);
	
    addAlternativesFromLocation(altInfos, verb, current.location, &alternativeFinder);

    for (parameterNumber = 1; !isEndOfArray(&parameters[parameterNumber-1]); parameterNumber++) {
        addAlternativesFromParameter(altInfos, verb, parameters, parameterNumber, &alternativeFinder);
    }
    return duplicateAltInfoArray(altInfos);
}


/*----------------------------------------------------------------------*/
static bool possibleWithFinder(int verb, Parameter parameters[], AltInfoFinder *finder) {
    bool anything;
    AltInfo *allAlternatives;

    allAlternatives = finder(verb, parameters);

    // TODO Need to do this since anyCheckFailed() call execute() which assumes the global parameters
    setParameters(parameters);
    if (anyCheckFailed(allAlternatives, DONT_EXECUTE_CHECK_BODY_ON_FAIL))
        anything = FALSE;
    else
	anything = anythingToExecute(allAlternatives);

    if (allAlternatives != NULL)
      free(allAlternatives);

    return(anything);
}



/*======================================================================*/
bool possible(int verb, Parameter inParameters[], ParameterPosition parameterPositions[])
{
    // This is a wrapper for possibleWithFinder() which is used in unit tests
    // possible() should be used "for real".

    return possibleWithFinder(verb, inParameters, findAllAlternatives);
}
