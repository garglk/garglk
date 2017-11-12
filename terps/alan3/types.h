#ifndef _ATYPES_H_
#define _ATYPES_H_
/*----------------------------------------------------------------------*\

  types.h

  Header file for the Alan interpreter module.

\*----------------------------------------------------------------------*/


#include "sysdep.h"
#include "acode.h"
#include "memory.h"


/* PREPROCESSOR */
#define FORWARD
#define NEW(type) ((type *)allocate(sizeof(type)))


/* CONSTANTS */

#define HERO (header->theHero)
#define ENTITY (header->entityClassId)
#define OBJECT (header->objectClassId)
#define LOCATION (header->locationClassId)
#define THING (header->thingClassId)
#define ACTOR (header->actorClassId)

#define MAXPARAMS (header->maxParameters)
#define MAXINSTANCE (header->instanceMax)

#define pointerTo(x) ((void *)&memory[x])
#define addressOf(x) ((((long)x)-((long)memory))/sizeof(Aword))
#define stringAt(x) ((char *)pointerTo(x))

#define ASIZE(x) (sizeof(x)/sizeof(Aword))

/* TYPES */

#include <stdbool.h>

/* The various tables */
typedef struct VerbEntry {	/* VERB TABLE */
  Aint code;			/* Code for the verb */
  Aaddr alts;			/* Address to alternatives */
} VerbEntry;

typedef struct LimEntry {	/* LIMIT Type */
  Aword atr;			/* Attribute that limits */
  Aword val;			/* And the limiting value */
  Aaddr stms;			/* Statements if fail */
} LimitEntry;


/* Functions: */
extern Aaddr addressAfterTable(Aaddr adr, int size);

#endif
