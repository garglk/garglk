#ifndef _MAIN_H_
#define _MAIN_H_
/*----------------------------------------------------------------------*\

  MAIN.H

  Header file for main unit of ARUN Alan System interpreter

\*----------------------------------------------------------------------*/

#include "types.h"
#include "acode.h"
#include <setjmp.h>


/* DATA */

extern int memTop;		/* Top of memory */

extern int conjWord;		/* First conjunction in dictionary */

/* The Amachine memory */
extern Aword *memory;
extern ACodeHeader *header;

/* Event queue */
extern int eventQueueSize;
extern EventQueueEntry *eventQueue;
extern Aint eventQueueTop;		/* Event queue top pointer */

/* Amachine variables */
extern CurVars current;

/* Amachine data structures - Dynamic */
extern InstanceEntry *instance; /* Instance table pointer */
extern AdminEntry *admin;	/* Administrative data about instances */
extern AttributeEntry *attributes; /* Dynamic attribute values */
extern Aword *scores;		/* Score table pointer */

/* Amachine data structures - Static */
extern DictionaryEntry *dictionary; /* Dictionary pointer */
extern ClassEntry *class;	/* Class table pointer */
extern ContainerEntry *container; /* Container table pointer */

extern VerbEntry *vrbs;		/* Verb table pointer */
extern SyntaxEntry *stxs;	/* Syntax table pointer */
extern RulEntry *ruls;		/* Rule table pointer */
extern EventEntry *events;	/* Event table pointer */
extern MessageEntry *msgs;	/* Message table pointer */
extern Aword *freq;		/* Cumulated frequencies */

extern int dictsize;		/* Number of entries in dictionary */

/* The text and message file */
extern FILE *textFile;
#ifdef HAVE_GLK
#include "glk.h"
strid_t logFile;
#else
FILE *logFile;
#endif

/* File names */
extern char *adventureName;	/* The name of the game */
extern char *adventureFileName;

/* Screen formatting info */
extern int col, lin;
extern int pageLength, pageWidth;

/* Long jump buffer for restart, errors and undo */
extern jmp_buf restartLabel;

extern jmp_buf returnLabel;
#define NO_JUMP_RETURN 0
#define ERROR_RETURN 1
#define UNDO_RETURN 2

extern jmp_buf forfeitLabel;

extern Bool verbose;
extern Bool ignoreErrorOption;
extern Bool debugOption;
extern Bool sectionTraceOption;
extern Bool tracePushOption;
extern Bool traceStackOption;
extern Bool traceSourceOption;
extern Bool singleStepOption;
extern Bool transcriptOption;
extern Bool logOption;
extern Bool statusLineOption;
extern Bool regressionTestOption;
extern Bool fail;
extern Bool anyOutput;
extern Bool needSpace;
extern Bool capitalize;

#define endOfTable(x) eot((Aword *) (x))


/* Functions: */
#ifndef DMALLOC
extern void *allocate(unsigned long len);
#else
#define allocate(s) calloc(s, 1)
#endif
extern void *duplicate(void *original, unsigned long len);
extern void terminate(int code);
extern void usage(void);
extern void error(MsgKind msg);
extern void statusline(void);
extern void output(char string[]);
extern void printMessage(MsgKind msg);
extern void para(void);
extern void newline(void);
extern void printAndLog(char string[]);

extern Bool eot(Aword *adr);
extern Bool isObject(Aword x);
extern Bool isContainer(Aword x);
extern Bool isActor(Aword x);
extern Bool isLocation(Aword x);
extern Bool isLiteral(Aword x);
extern Bool isNumeric(Aword x);
extern Bool isString(Aword x);

extern Bool isVerb(int word);
extern Bool isConj(int word);
extern Bool isBut(int word);
extern Bool isThem(int word);
extern Bool isIt(int word);
extern Bool isNoun(int word);
extern Bool isAdjective(int word);
extern Bool isPreposition(int word);
extern Bool isAll(int word);
extern Bool isDir(int word);
extern Bool isNoise(int word);
extern Bool isPronoun(int word);
extern Bool isLiteralWord(int word);


/* Run the game! */
extern void run(void);

#endif
