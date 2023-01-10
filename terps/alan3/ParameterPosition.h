#ifndef PARAMETERPOSITION_H_
#define PARAMETERPOSITION_H_
/*----------------------------------------------------------------------*\

  ParameterPosition

  Represents on position in the player input holding a parameter. That
  position is filled with some words from the player, those words must
  be disambiguated to one or more instances. There are three cases:

  1) words presuming it would be a single instance (it
  might actually not be) "the chair"

  2) words indicating explicit mentioning of multiple instances, "the
  book and the chair"

  3) implicit multiple using "all" or "everything except the blue
  ball"

  For all those cases the position must be able to deliver the words,
  possible explicit or implicit multiple, and the resulting set of
  instances.

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
extern void deallocateParameterPositions(ParameterPosition *parameterPositions);
extern void uncheckAllParameterPositions(ParameterPosition parameterPositions[]);
extern void copyParameterPositions(ParameterPosition originalParameterPositions[], ParameterPosition parameterPositions[]);
extern bool equalParameterPositions(ParameterPosition parameterPositions1[], ParameterPosition parameterPositions2[]);
extern int findMultipleParameterPosition(ParameterPosition parameterPositions[]);
extern void markExplicitMultiple(ParameterPosition parameterPositions[], Parameter parameters[]);
extern void convertPositionsToParameters(ParameterPosition parameterPositions[], Parameter parameters[]);

#endif /* PARAMETERPOSITION_H_ */
