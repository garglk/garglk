#ifndef _ATYPES_H_
#define _ATYPES_H_
/*----------------------------------------------------------------------*\

  types.h

  Header file for the Alan interpreter module.

\*----------------------------------------------------------------------*/


#include "sysdep.h"
#include "acode.h"


/* MEMORY DEBUGGING? */
#include "smartall.h"


/* PREPROCESSOR */
#define FORWARD
#define NEW(type) ((type *)allocate(sizeof(type)))

/* CONSTANTS */

#ifndef __mac__
#ifndef TRUE
#define TRUE (0==0)
#endif
#ifndef FALSE
#define FALSE (!TRUE)
#endif
#endif

#define LITMIN (header->locmax+1)
#define LITMAX (header->locmax+1+litCount)

#define HERO (header->theHero)
#define ENTITY (header->entityClassId)
#define OBJECT (header->objectClassId)
#define LOCATION (header->locationClassId)
#define THING (header->thingClassId)
#define ACTOR (header->actorClassId)

#define MAXPARAMS (header->maxParameters)
#define MAXENTITY (header->instanceMax)

#define pointerTo(x) ((void *)&memory[x])
#define addressOf(x) ((((long)x)-((long)memory))/sizeof(Aword))


/* TYPES */

typedef int Bool;		/* Boolean values within interpreter */

/* The various tables */
typedef struct VerbEntry {	/* VERB TABLE */
  Aword code;			/* Code for the verb */
  Aaddr alts;			/* Address to alternatives */
} VerbEntry;

typedef struct LimEntry {	/* LIMIT Type */
  Aword atr;			/* Attribute that limits */
  Aword val;			/* And the limiting value */
  Aaddr stms;			/* Statements if fail */
} LimitEntry;

#endif
