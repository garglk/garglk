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
typedef struct Parameter {        /* PARAMETER */
    Aid instance;                  /* Instance code for the parameter (0=multiple) */
    bool isLiteral;
    bool isPronoun;
    bool isThem;
    bool useWords;                 /* Indicate to use words instead of instance code when saying */
    int firstWord;                 /* Index to first word used by player */
    int lastWord;                  /* d:o to last */
    struct Parameter *candidates; /* Array of instances possibly matching this parameter depending on player input */
} Parameter;

typedef Parameter *ParameterArray;


/* DATA */
extern Parameter *globalParameters;


/* FUNCTIONS */
/* Single Parameter: */
extern Parameter *newParameter(int instanceId);
extern void clearParameter(Parameter *parameter);
extern void copyParameter(Parameter *theCopy, Parameter *theOriginal);

/* ParameterArray: */
extern ParameterArray newParameterArray(void);
extern ParameterArray ensureParameterArrayAllocated(ParameterArray currentArray);
extern void freeParameterArray(Parameter *array);

extern bool parameterArrayIsEmpty(ParameterArray parameters);
extern void addParameterToParameterArray(ParameterArray theArray, Parameter *theParameter);
extern void addParameterForInstance(ParameterArray parameters, int instance);
extern void addParameterForInteger(ParameterArray parameters, int value);
extern void addParameterForString(ParameterArray parameters, char *value);
extern Parameter *findEndOfParameterArray(ParameterArray parameters);
extern void compressParameterArray(ParameterArray a);
extern int lengthOfParameterArray(ParameterArray a);
extern bool equalParameterArrays(ParameterArray parameters1, ParameterArray parameters2);
extern bool inParameterArray(ParameterArray l, Aword e);
extern void copyParameterArray(ParameterArray to, ParameterArray from);
extern void clearParameterArray(ParameterArray list);
extern void subtractParameterArrays(ParameterArray a, ParameterArray b);
extern void mergeParameterArrays(ParameterArray a, ParameterArray b);
extern void intersectParameterArrays(ParameterArray a, ParameterArray b);
extern void copyReferencesToParameterArray(Aint *references, ParameterArray parameters);
extern void printParameterArray(ParameterArray parameters);

extern int findMultiplePosition(ParameterArray parameters);

/* Global Parameters: */
extern void setGlobalParameters(ParameterArray parameters);
extern ParameterArray getGlobalParameters(void);
extern ParameterArray getGlobalParameter(int parameterIndex);

#endif
