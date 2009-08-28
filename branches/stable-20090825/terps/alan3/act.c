/*----------------------------------------------------------------------*\

  act.c

  Action routines

\*----------------------------------------------------------------------*/

#include "sysdep.h"
#include "act.h"

/* Import */

#ifdef HAVE_GLK
#define MAP_STDIO_TO_GLK
#include "glkio.h"
#endif

#include "types.h"
#include "main.h"
#include "inter.h"
#include "exe.h"
#include "stack.h"
#include "parse.h"
#include "debug.h"
#include "syserr.h"


/*----------------------------------------------------------------------*/
static int count(int cnt)	/* IN - the container to count in */
{
  int i, j = 0;
  
  for (i = 1; i <= header->instanceMax; i++)
    if (in(i, cnt, TRUE))
      /* Then it's in this container also */
      j++;
  return(j);
}



/*----------------------------------------------------------------------
  sumatr()

  Sum the values of one attribute in a container. Possibly recursively.

  */
static int sumatr(
     Aword atr,			/* IN - the attribute to sum over */
     Aword cnt			/* IN - the container to sum */
) {
  int i;
  int sum = 0;

  for (i = 1; i <= header->instanceMax; i++)
    if (in(i, cnt, TRUE)) {		/* Then it's directly in this cont */
      if (instance[i].container != 0)	/* This is also a container! */
	sum = sum + sumatr(atr, i);
      sum = sum + attributeOf(i, atr);
    }
  return(sum);
}


/*======================================================================
  checklim()

  Checks if a limit for a container is exceeded.

  */
Bool checklim(
     Aword cnt,			/* IN - Container code */
     Aword obj			/* IN - The object to add */
) {
  LimEntry *lim;
  Aword props;

  if (!isContainer(cnt))
    syserr("Checking limits for a non-container.");

  /* Find the container properties */
  props = instance[cnt].container;

  if (container[props].limits != 0) { /* Any limits at all? */
    for (lim = (LimEntry *) pointerTo(container[props].limits); !endOfTable(lim); lim++)
      if (lim->atr == 1-I_COUNT) {
	if (count(cnt) >= lim->val) {
	  interpret(lim->stms);
	  return(TRUE);		/* Limit check failed */
	}
      } else {
	if (sumatr(lim->atr, cnt) + attributeOf(obj, lim->atr) > lim->val) {
	  interpret(lim->stms);
	  return(TRUE);
	}
      }
  }
  return(FALSE);
}



/*======================================================================
  trycheck()

  Tries a check, returns TRUE if it passed, FALSE else.

  */
Bool trycheck(Aaddr adr,	/* IN - ACODE address to check table */
		 Bool execute /* IN - Act if it fails ? */
)
{
  ChkEntry *chk;

  chk = (ChkEntry *) pointerTo(adr);
  if (chk->exp == 0) {
    if (execute)
      interpret(chk->stms);
    return(FALSE);
  } else {
    while (!endOfTable(chk)) {
      interpret(chk->exp);
      if (!(Abool)pop()) {
	if (execute)
	  interpret(chk->stms);
	return(FALSE);
      }
      chk++;
    }
    return(TRUE);
  }
}


/*======================================================================*/
void go(int dir)
{
  ExitEntry *theExit;
  Bool ok;
  Aword oldloc;

  theExit = (ExitEntry *) pointerTo(instance[current.location].exits);
  if (instance[current.location].exits != 0)
    while (!endOfTable(theExit)) {
      if (theExit->code == dir) {
	ok = TRUE;
	if (theExit->checks != 0) {
	  if (sectionTraceOption) {
	    printf("\n<EXIT %d(%s) from ", dir,
		   (char *)pointerTo(dictionary[playerWords[wordIndex-1].code].string));
	    traceSay(current.location);
	    printf("(%d), Checking:>\n", current.location);
	  }
	  ok = trycheck(theExit->checks, EXECUTE);
	}
	if (ok) {
	  oldloc = current.location;
	  if (theExit->action != 0) {
	    if (sectionTraceOption) {
	      printf("\n<EXIT %s(%d) from ", 
		     (char *)pointerTo(dictionary[playerWords[wordIndex-1].code].string), dir);
	      traceSay(current.location);
	      printf("(%d), Executing:>\n", current.location);
	    }	    
	    interpret(theExit->action);
	  }
	  /* Still at the same place? */
	  if (where(HERO, FALSE) == oldloc) {
	    if (sectionTraceOption) {
	      printf("\n<EXIT %s(%d) from ",
		     (char *)pointerTo(dictionary[playerWords[wordIndex-1].code].string), dir);
	      traceSay(current.location);
	      printf("(%d), Moving:>\n", current.location);
	    }
	    locate(HERO, theExit->target);
	  }
	}
	return;
      }
      theExit++;
    }
  error(M_NO_WAY);
}


/*----------------------------------------------------------------------*/
static AltEntry *findAlternativeInVerbList(Aaddr verbListAddress,
					   Aint parameter)
{
  AltEntry *alt;
  VerbEntry *verb;

  if (verbListAddress == 0) return NULL;

  for (verb = (VerbEntry *) pointerTo(verbListAddress); !endOfTable(verb); verb++)
    if (verb->code == current.verb) {
      for (alt = (AltEntry *) pointerTo(verb->alts); !endOfTable(alt); alt++) {
	if (alt->param == parameter || alt->param == 0)
	  return alt;
      }
      return NULL;
    }
  return NULL;
}



/*----------------------------------------------------------------------*/
static AltEntry *findAlternativeInInstance(
     Aint inInstance,		/* IN - Which instance to check (0=global) */
     Aint parameter		/* IN - Which parameter to match */
)
{
  if (inInstance == 0)
    return findAlternativeInVerbList(header->verbTableAddress, parameter);
  else
    return findAlternativeInVerbList(instance[inInstance].verbs, parameter);
}


/*----------------------------------------------------------------------*/
static AltEntry *findAlternativeInClass(
     Aint inClass,		/* IN - Which class to check */
     Aint parameter		/* IN - Which parameter to match */
)
{
  return findAlternativeInVerbList(class[inClass].verbs, parameter);
}


/*----------------------------------------------------------------------
  An info node about the Alternatives found and possibly executed
  ----------------------------------------------------------------------*/
typedef struct AltInfo {
  Bool end;
  AltEntry *alt;
  Bool done;
  int level;			/* 0 - Global, 1 - location, 2.. - parameter */
  Aint instance;
  int parameter;
  int class;
} AltInfo;


/*----------------------------------------------------------------------*/
static void primeAltInfo(AltInfo *altInfo, int level, int parameter, int instance, int class)
{
  altInfo->level = level;
  altInfo->parameter = parameter;
  altInfo->instance = instance;
  altInfo->class = class;
  altInfo->done = FALSE;
  altInfo->end = FALSE;

  altInfo++;
  altInfo->end = TRUE;

}


/*----------------------------------------------------------------------*/
static int alternativesFoundInParents(AltInfo altInfo[],
				      Aint theClass,
				      Aint theInstance,
				      int level,
				      Aint parameterNumber)
{
  int found;

  if (theClass == 0) return 0;

  found = alternativesFoundInParents(altInfo, class[theClass].parent,
				     theInstance, level, parameterNumber);
  altInfo[found].alt = findAlternativeInClass(theClass, parameterNumber);
  if (altInfo[found].alt != NULL) {
    primeAltInfo(&altInfo[found], level, parameterNumber, theInstance, theClass);
    return found + 1;
  } else
    return found;
}  


/*----------------------------------------------------------------------*/
static void findAllAlternatives(AltInfo alt[]) {
  int paramIndex;
  int altIndex = 0;
  int parent;

  alt[0].end = TRUE;
  alt[altIndex].alt = findAlternativeInInstance(0, 0);
  if (alt[altIndex].alt != NULL) {
    primeAltInfo(&alt[altIndex], 0, 0, parameters[0].instance, 0);
    altIndex++;
  }

  altIndex += alternativesFoundInParents(&alt[altIndex], instance[current.location].parent, current.location, 1, -1);
  alt[altIndex].alt = findAlternativeInInstance(current.location, -1);
  if (alt[altIndex].alt != NULL) {
    primeAltInfo(&alt[altIndex], 1, -1, current.location, 0);
    altIndex++;
  }

  for (paramIndex = 0; parameters[paramIndex].instance != EOF; paramIndex++) {
    if (isLiteral(parameters[paramIndex].instance))
      parent = literal[paramIndex+1].class;
    else
      parent = instance[parameters[paramIndex].instance].parent;
    altIndex += alternativesFoundInParents(&alt[altIndex], parent,
					   parameters[paramIndex].instance, 2, paramIndex+1);
    if (!isLiteral(parameters[paramIndex].instance)) {
      alt[altIndex].alt = findAlternativeInInstance(parameters[paramIndex].instance, paramIndex+1);
      if (alt[altIndex].alt != NULL) {
	primeAltInfo(&alt[altIndex], 2, paramIndex+1, parameters[paramIndex].instance, 0);
	altIndex++;
      }
    }
  }
}


/*----------------------------------------------------------------------*/
static void traceCheck(AltInfo alt)
{
  printf("\n<VERB %d, ", current.verb);
  switch (alt.level) {
  case 0: printf("GLOBAL"); break;
  case 1: printf("in LOCATION"); break;
  default:
    printf("in parameter #%d (", alt.parameter);
    traceSay(parameters[alt.parameter-1].instance);
    printf(")");
    if (alt.class != 0)
      printf(", inherited from class %s(%d)", (char *)pointerTo(class[alt.class].id), alt.class);
    break;
  }
  printf(", CHECK:>\n");
}


/*----------------------------------------------------------------------*/
static Bool executeCheckOK(AltInfo alt, Bool execute)
{
  if (alt.alt != NULL && alt.alt->checks != 0) {
    if (sectionTraceOption && execute)
      traceCheck(alt);
    if (!trycheck(alt.alt->checks, execute)) return FALSE;
    if (fail) return FALSE;
  }
  return TRUE;
}


/*----------------------------------------------------------------------*/
static Bool performChecksOK(AltInfo altInfo[], Bool execute)
{
  int altIndex;

  for (altIndex = 0; !altInfo[altIndex].end; altIndex++) {
    current.instance = altInfo[altIndex].instance;
    if (!executeCheckOK(altInfo[altIndex], execute))
      return FALSE;
  }
  return TRUE;
}


/*----------------------------------------------------------------------*/
static Bool anythingToExecute(AltInfo altInfo[])
{
  int altIndex;

  /* Check for anything to execute... */
  for (altIndex = 0; !altInfo[altIndex].end; altIndex++)
    if (altInfo[altIndex].alt != NULL && altInfo[altIndex].alt->action != 0)
      return TRUE;
  return FALSE;
}



/*======================================================================*/
Bool possible(void)
{
  AltInfo alt[MAXPARAMS+2];

  findAllAlternatives(alt);

  if (!performChecksOK(alt, DONT_EXECUTE))
    return FALSE;

  return anythingToExecute(alt);
}


/*----------------------------------------------------------------------*/
static void traceExecution(AltInfo *alt)
{
  if (sectionTraceOption) {
    printf("\n<VERB %d", current.verb);
    if (alt->level == 0)
      printf(", GLOBAL");
    else if (alt->level == 1)
      printf(", in LOCATION");
    else
      printf(", in parameter #%d", alt->parameter);
    if (alt->class != 0)
      printf(", inherited from class %s(%d)", (char *)pointerTo(class[alt->class].id), alt->class);
    printf(", DOES");
    switch (alt->alt->qual) {
    case Q_BEFORE: printf(" (BEFORE)"); break;
    case Q_ONLY: printf(" (ONLY)"); break;
    case Q_AFTER: printf(" (AFTER)"); break;
    case Q_DEFAULT: break;
    }
    printf(":>\n");  }
}


/*----------------------------------------------------------------------*/
static Bool executeOK(AltInfo *altInfo)
{
  fail = FALSE;
  if (!altInfo->done && altInfo->alt->action != 0) {
    traceExecution(altInfo);
    current.instance = altInfo->instance;
    interpret(altInfo->alt->action);
  }
  altInfo->done = TRUE;
  return !fail;
}

/*----------------------------------------------------------------------*/
static int lastAltInfo(AltInfo altInfo[])
{
  int altIndex;

  for (altIndex = -1; !altInfo[altIndex+1].end; altIndex++)
    /* Step to last alternative */;
  return(altIndex);
}



/*----------------------------------------------------------------------*/
static void executeCommand(void)
{
  AltInfo altInfo[MAXPARAMS+2];
  int altIndex;

  fail = FALSE;

  findAllAlternatives(altInfo);

  if (!performChecksOK(altInfo, EXECUTE)) return;

  /* Check for anything to execute... */
  if (!anythingToExecute(altInfo))
    error(M_CANT0);

  /* Now perform actions! First try any BEFORE or ONLY from inside out */
  for (altIndex = lastAltInfo(altInfo); altIndex >= 0; altIndex--) {
    if (altInfo[altIndex].alt != NULL)
      if (altInfo[altIndex].alt->qual == (Aword)Q_BEFORE || altInfo[altIndex].alt->qual == (Aword)Q_ONLY) {
	if (!executeOK(&altInfo[altIndex]))
	  return;
	if (altInfo[altIndex].alt->qual == (Aword)Q_ONLY)
	  return;
      }
  }
  
  /* Then execute any not declared as AFTER, i.e. the default */
  for (altIndex = 0; !altInfo[altIndex].end; altIndex++) {
    if (altInfo[altIndex].alt != 0)
      if (altInfo[altIndex].alt->qual != (Aword)Q_AFTER)
	if (!executeOK(&altInfo[altIndex]))
	  return;
  }

  /* Finally, the ones declared as AFTER */
  for (altIndex = lastAltInfo(altInfo); altIndex >= 0; altIndex--) {
    if (altInfo[altIndex].alt != 0)
      if (!executeOK(&altInfo[altIndex]))
	return;
  }
}



/*======================================================================

  action()

  Execute all activities commanded. Handles possible multiple actions
  such as THEM or lists of objects.

  */
#ifdef _PROTOTYPES_
void action(
     ParameterList plst		/* IN - Plural parameter list */
)
#else
void action(plst)
     ParameterList plst;
#endif
{
  int i, mpos;
  char marker[10];

  if (plural) {
    /*
       The code == 0 means this is a multiple position. We must loop
       over this position (and replace it by each present in the plst)
     */
    for (mpos = 0; parameters[mpos].instance != 0; mpos++); /* Find multiple position */
    sprintf(marker, "($%d)", mpos+1); /* Prepare a printout with $1/2/3 */
    for (i = 0; plst[i].instance != EOF; i++) {
      parameters[mpos] = plst[i];
      output(marker);
      executeCommand();
      if (plst[i+1].instance != EOF)
        para();
    }
    parameters[mpos].instance = 0;
  } else
    executeCommand();
}
