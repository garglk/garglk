#ifndef PARAMS_H
#define PARAMS_H
/*----------------------------------------------------------------------*\

  params.h

  Various utility functions for handling parameters

  \*----------------------------------------------------------------------*/

/* IMPORTS */
#include "types.h"
#include "acode.h"


/* TYPES */
typedef struct ParamEntry {     /* PARAMETER */
    Aid instance;				/* Instance code for the parameter (0=multiple) */
    bool isLiteral;
    bool isPronoun;
    bool isThem;
    bool useWords;              /* Indicate to use words instead of instance code when saying */
    int firstWord;              /* Index to first word used by player */
    int lastWord;               /* d:o to last */
    struct ParamEntry *candidates; /* Array of instances possibly matching this parameter depending on player input */
} Parameter;


/* DATA */
extern Parameter *globalParameters;


/* FUNCTIONS */
extern bool exists(Parameter *parameters);
extern void clearParameter(Parameter *parameter, Parameter *candidates);
extern void setParameters(Parameter parameters[]);
extern Parameter *getParameters(void);
extern Parameter *getParameter(int parameterIndex);
extern Parameter *ensureParameterArrayAllocated(Parameter *currentList);
extern Parameter *allocateParameterArray(int n);
extern Parameter *findEndOfParameterArray(Parameter *parameters);
extern int findMultiplePosition(Parameter parameters[]);
extern void compressParameterArray(Parameter *a);
extern int lengthOfParameterArray(Parameter *a);
extern bool equalParameterArrays(Parameter parameters1[], Parameter parameters2[]);
extern bool inParameterArray(Parameter *l, Aword e);
extern void copyParameter(Parameter *theCopy, Parameter *theOriginal);
extern void addParameter(Parameter theParameterArray[], Parameter *theParameter);
extern void copyParameterArray(Parameter *to, Parameter *from);
extern void clearParameterArray(Parameter *list);
extern void subtractParameterArrays(Parameter *a, Parameter *b);
extern void mergeParameterArrays(Parameter *a, Parameter *b);
extern void intersectParameterArrays(Parameter *a, Parameter *b);
extern void copyReferencesToParameterArray(Aint *references, Parameter *parameters);
extern void addParameterForInstance(Parameter *parameter, int instance);
extern void addParameterForInteger(Parameter *parameters, int value);
extern void addParameterForString(Parameter *parameters, char *value);
extern void printParameterArray(Parameter parameters[]);
#endif
