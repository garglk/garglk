#include "ParameterPosition.h"

/* Imports */
#include "memory.h"
#include "lists.h"


/* Public Data */


/* Methods */


/*======================================================================*/
void uncheckAllParameterPositions(ParameterPosition parameterPositions[]) {
    int position;
    for (position = 0; position < MAXPARAMS; position++) {
        parameterPositions[position].checked = FALSE;
    }
}


/*======================================================================*/
void copyParameterPositions(ParameterPosition originalParameterPositions[], ParameterPosition parameterPositions[]) {
    int i;
    for (i = 0; !originalParameterPositions[i].endOfList; i++)
        parameterPositions[i] = originalParameterPositions[i];
    parameterPositions[i].endOfList = TRUE;
}


/*======================================================================*/
bool equalParameterPositions(ParameterPosition parameterPositions1[], ParameterPosition parameterPositions2[]) {
    int i;
    for (i = 0; !parameterPositions1[i].endOfList; i++) {
        if (parameterPositions2[i].endOfList)
            return FALSE;
        if (!equalParameterArrays(parameterPositions1[i].parameters, parameterPositions2[i].parameters))
            return FALSE;
    }
    return parameterPositions2[i].endOfList;
}


/*======================================================================*/
int findMultipleParameterPosition(ParameterPosition parameterPositions[]) {
    Aint parameterNumber;
    for (parameterNumber = 0; !parameterPositions[parameterNumber].endOfList; parameterNumber++)
        if (parameterPositions[parameterNumber].explicitMultiple)
            return parameterNumber;
    return -1;
}


/*======================================================================*/
void markExplicitMultiple(ParameterPosition parameterPositions[], Parameter parameters[]) {
    int parameterCount;
    for (parameterCount = 0; !parameterPositions[parameterCount].endOfList; parameterCount++)
        if (parameterPositions[parameterCount].explicitMultiple)
            parameters[parameterCount].instance = 0;
}

/*======================================================================*/
void convertPositionsToParameters(ParameterPosition parameterPositions[], Parameter parameters[]) {
    int parameterCount;
    for (parameterCount = 0; !parameterPositions[parameterCount].endOfList; parameterCount++)
	parameters[parameterCount] = parameterPositions[parameterCount].parameters[0];
    setEndOfArray(&parameters[parameterCount]);
}
