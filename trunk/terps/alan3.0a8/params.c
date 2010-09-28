/*----------------------------------------------------------------------

  params.c

  Various utility functions for handling parameters

  ----------------------------------------------------------------------*/
#include "params.h"

/* IMPORTS */
#include "lists.h"
#include "memory.h"
#include "literal.h"
#include "parameterPosition.h"
#include "syserr.h"

/* PUBLIC DATA */
Parameter *globalParameters = NULL;



/*======================================================================*/
Bool exists(Parameter *parameters) {
    return parameters != NULL && lengthOfParameterArray(parameters) > 0;
}


/*======================================================================*/
void clearParameter(Parameter *parameter, Parameter *candidates) {
    memset(parameter, 0, sizeof(Parameter));
    parameter->candidates = candidates;
    if (parameter->candidates != NULL)
	setEndOfArray(&parameter->candidates[0]);
}


/*======================================================================*/
void setParameters(Parameter *newParameters) {
    if (globalParameters == NULL)
        globalParameters = allocateParameterArray();
    copyParameterArray(globalParameters, newParameters);
}


/*======================================================================*/
Parameter *getParameters(void) {
    if (globalParameters == NULL)
        globalParameters = allocateParameterArray();
    return globalParameters;
}


/*======================================================================*/
Parameter *getParameter(int parameterIndex) {
    return &globalParameters[parameterIndex];
}


/*======================================================================*/
Parameter *ensureParameterArrayAllocated(Parameter *currentArray) {
    if (currentArray == NULL)
        currentArray = allocateParameterArray();
    else
	clearParameterArray(currentArray);
    return currentArray;
}


/*======================================================================*/
Parameter *allocateParameterArray() {
    Parameter *newArray = allocate(sizeof(Parameter)*(MAXENTITY+1));
    setEndOfArray(newArray);
    return newArray;
}


/*======================================================================*/
Parameter *findEndOfParameterArray(Parameter *parameters) {
    Parameter *parameter;
    for (parameter = parameters; !isEndOfArray(parameter); parameter++);
    return parameter;
}


/*======================================================================*/
/* A parameter position with code == 0 means this is a multiple position.
 * We must loop over this position (and replace it by each present in the
 * matched list)
 */
int findMultiplePosition(Parameter parameters[]) {
    // TODO: this should look at the isAll and isExplicitMultiple flags instead
    int multiplePosition;
    for (multiplePosition = 0; !isEndOfArray(&parameters[multiplePosition]); multiplePosition++)
	if (parameters[multiplePosition].instance == 0)
	    return multiplePosition;
    return -1;
}


/*======================================================================*/
void compressParameterArray(Parameter theArray[])
{
    int i, j;

    for (i = 0, j = 0; theArray[j].instance != EOF; j++)
	if (theArray[j].instance != 0)
	    theArray[i++] = theArray[j];
    setEndOfArray(&theArray[i]);
}


/*======================================================================*/
int lengthOfParameterArray(Parameter theArray[])
{
    int i = 0;

    if (theArray == NULL) return 0;

    while (!isEndOfArray(&theArray[i]))
        i++;
    return i;
}


/*======================================================================*/
Bool equalParameterArrays(Parameter parameters1[], Parameter parameters2[])
{
    int i;

    if ((parameters1 == NULL) != (parameters2 == NULL))
        return FALSE;
    if (parameters1 == NULL) // Because then parameter2 is also NULL
        return TRUE;
    for (i = 0; i < lengthOfParameterArray(parameters1); i++) {
        if (isEndOfArray(&parameters2[i])) return FALSE;
        if (parameters1[i].instance != parameters2[i].instance) return FALSE;
    }
    return isEndOfArray(&parameters2[i]);
}


/*======================================================================*/
Bool inParameterArray(Parameter theArray[], Aword theCode)
{
    int i;

    for (i = 0; !isEndOfArray(&theArray[i]) && theArray[i].instance != theCode; i++);
    return (theArray[i].instance == theCode);
}


/*======================================================================*/
void copyParameter(Parameter *theCopy, Parameter theOriginal) {
    Parameter *theCopyCandidates = theCopy->candidates;
    *theCopy = theOriginal;
    if (lengthOfParameterArray(theCopyCandidates) < lengthOfParameterArray(theOriginal.candidates)) {
	theCopy->candidates = allocateParameterArray();
    }
    copyParameterArray(theCopy->candidates, theOriginal.candidates);
}


/*======================================================================*/
void copyParameterArray(Parameter to[], Parameter from[])
{
    int i;

    if (to == NULL && from == NULL) return;

    if (to == NULL) syserr("Copying to null parameter array");

    for (i = 0; !isEndOfArray(&from[i]); i++) {
	to[i] = from[i];
	copyParameter(&to[i], from[i]);
    }
    setEndOfArray(&to[i]);
}


/*======================================================================*/
void subtractParameterArrays(Parameter theArray[], Parameter remove[])
{
    int i;
    
    if (remove == NULL) return;

    for (i = 0; !isEndOfArray(&theArray[i]); i++)
        if (inParameterArray(remove, theArray[i].instance))
            theArray[i].instance = 0;		/* Mark empty */
    compressParameterArray(theArray);
}


/*======================================================================*/
void mergeParameterArrays(Parameter one[], Parameter other[])
{
    int i,last;

    for (last = 0; !isEndOfArray(&one[last]); last++); /* Find end of array */
    for (i = 0; !isEndOfArray(&other[i]); i++)
	if (!inParameterArray(one, other[i].instance)) {
	    one[last++] = other[i];
	    setEndOfArray(&one[last]);
	}
}


/*======================================================================*/
void clearParameterArray(Parameter theArray[]) {
    clearParameter(&theArray[0], theArray[0].candidates);
    setEndOfArray(theArray);
}


/*======================================================================*/
void intersectParameterArrays(Parameter one[], Parameter other[])
{
    int i, last = 0;

    for (i = 0; !isEndOfArray(&one[i]); i++)
	if (inParameterArray(other, one[i].instance))
	    one[last++] = one[i];
    setEndOfArray(&one[last]);
}


/*======================================================================*/
void copyReferencesToParameterArray(Aint references[], Parameter parameterArray[])
{
    int i;

    for (i = 0; !isEndOfArray(&references[i]); i++) {
        parameterArray[i].instance = references[i];
        parameterArray[i].firstWord = EOF; /* Ensure that there is no word that can be used */
    }
    setEndOfArray(&parameterArray[i]);
}


/*======================================================================*/
void addParameterForInstance(Parameter *parameters, int instance) {
    Parameter *parameter = findEndOfParameterArray(parameters);

    parameter->instance = instance;
    parameter->useWords = FALSE;

    setEndOfArray(parameter+1);
}


/*======================================================================*/
void addParameterForInteger(Parameter *parameters, int value) {
    Parameter *parameter = findEndOfParameterArray(parameters);

    createIntegerLiteral(value);
    parameter->instance = instanceFromLiteral(litCount);
    parameter->useWords = FALSE;

    setEndOfArray(parameter+1);
}

/*======================================================================*/
void addParameterForString(Parameter *parameters, char *value) {
    Parameter *parameter = findEndOfParameterArray(parameters);

    createStringLiteral(value);
    parameter->instance = instanceFromLiteral(litCount);
    parameter->useWords = FALSE;

    setEndOfArray(parameter+1);
}

/*======================================================================*/
void printParameterArray(Parameter parameters[]) {
    int i;
    printf("[");
    for (i = 0; !isEndOfArray(&parameters[i]); i++) {
	printf("%d ", (int)parameters[i].instance);
    }
    printf("]\n");
}

