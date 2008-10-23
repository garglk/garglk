#ifndef _ATYPES_H_
#define _ATYPES_H_
/*----------------------------------------------------------------------*\

  types.h

  Header file for the Alan interpreter module.

\*----------------------------------------------------------------------*/


#include "sysdep.h"
#include "acode.h"

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
#define OBJECT (header->objectClassId)
#define LOCATION (header->locationClassId)
#define THING (header->thingClassId)
#define ACTOR (header->actorClassId)


#define pointerTo(x) ((void *)&memory[x])
#define addressOf(x) ((((long)x)-((long)memory))/sizeof(Aword))


/* TYPES */

typedef int Bool;		/* Boolean values within interpreter */

/* Amachine variables */
typedef struct CurVars {
  int
    verb,
    location,
    actor,
    instance,
    tick,
    score,
    visits,
    sourceLine,
    sourceFile;
} CurVars;

typedef struct AdminEntry {	/* Administrative data about instances */
  Aint location;
  AttributeEntry *attributes;
  Abool alreadyDescribed;
  Aint visitsCount;
  Aint script;
  Aint step;
  Aint waitCount;
} AdminEntry;


/* The various tables */
typedef struct ActEntry {	/* ACTOR TABLE */
  Aword loc;			/* Location */
  Abool describe;		/* Description flag */
  Aaddr nam;			/* Address to name printing code */
  Aaddr atrs;			/* Address to attribute list */
  Aword cont;			/* Code for the container props if any */
  Aword script;			/* Which script is he using */
  Aaddr scradr;			/* Address to script table */
  Aword step;
  Aword count;
  Aaddr vrbs;
  Aaddr dscr;			/* Address of description code */
} ActEntry;

typedef struct ChkEntry {	/* CHECK TABLE */
  Aaddr exp;			/* ACODE address to expression code */
  Aaddr stms;			/* ACODE address to statement code */
} ChkEntry;

typedef struct VerbEntry {	/* VERB TABLE */
  Aword code;			/* Code for the verb */
  Aaddr alts;			/* Address to alternatives */
} VerbEntry;

typedef struct LimEntry {	/* LIMIT Type */
  Aword atr;			/* Attribute that limits */
  Aword val;			/* And the limiting value */
  Aaddr stms;			/* Statements if fail */
} LimEntry;

typedef struct RulEntry {	/* RULE TABLE */
  Abool run;			/* Is rule already run? */
  Aaddr exp;			/* Address to expression code */
  Aaddr stms;			/* Address to run */
} RulEntry;

typedef struct EventQueueEntry { /* EVENT QUEUE ENTRIES */
  int time;
  int event;
  int where;
} EventQueueEntry;

typedef struct MessageEntry {	/* MESSAGE TABLE */
  Aaddr stms;			/* Address to statements*/
} MessageEntry;

typedef struct ParamEntry {	/* PARAMETER */
  Aword instance;		/* Instance code for the parameter
				   (0=multiple) */
  Abool useWords;		/* Indicate to use words instead of
				   instance code when saying */
  Aword firstWord;		/* Index to first word used by
				   player */
  Aword lastWord;		/* d:o to last */
} ParamEntry;

typedef enum LiteralType {
  NO_LITERAL, NUMERIC_LITERAL, STRING_LITERAL
} LiteralType;

typedef struct LiteralEntry {	/* LITERAL */
  Aint class;			/* Class id of the literal type */
  LiteralType type;
  Aword value;
} LiteralEntry;

typedef struct Breakpoint {	/* BREAKPOINT */
  int line;
  int file;
} Breakpoint;

#define MAXPARAMS 9
#define MAXENTITY (header->instanceMax)

#endif
