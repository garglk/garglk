#ifndef PARAMETERPOSITION_H_
#define PARAMETERPOSITION_H_
/*----------------------------------------------------------------------*\

  ParameterPosition

\*----------------------------------------------------------------------*/

/* Imports: */
#include "acode.h"
#include "types.h"
#include "params.h"


/* Constants: */


/* Types: */
typedef struct ParameterPosition {
    bool endOfList;
    bool explicitMultiple;
    bool all;
    bool them;
    bool checked;
    Aword flags;
    Parameter *parameters;
    Parameter *exceptions;
} ParameterPosition;


/* Data: */


/* Functions: */
extern void uncheckAllParameterPositions(ParameterPosition parameterPositions[]);
extern void copyParameterPositions(ParameterPosition originalParameterPositions[], ParameterPosition parameterPositions[]);
extern bool equalParameterPositions(ParameterPosition parameterPositions1[], ParameterPosition parameterPositions2[]);
extern int findMultipleParameterPosition(ParameterPosition parameterPositions[]);
extern void markExplicitMultiple(ParameterPosition parameterPositions[], Parameter parameters[]);
extern void convertPositionsToParameters(ParameterPosition parameterPositions[], Parameter parameters[]);

#endif /* PARAMETERPOSITION_H_ */
