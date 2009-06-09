/*----------------------------------------------------------------------*\

  inter.c

  Interpreter unit for Alan interpreter Arun

\*----------------------------------------------------------------------*/


#include <stdio.h>

#include "types.h"
#include "main.h"
#include "parse.h"
#include "exe.h"
#include "stack.h"
#include "syserr.h"
#include "sysdep.h"
#include "debug.h"
#include "set.h"

#include "inter.h"

#ifdef HAVE_GLK
#define MAP_STDIO_TO_GLK
#include "glkio.h"
#endif


Bool stopAtNextLine = FALSE;


/* PRIVATE DATA */

static int pc;


/*----------------------------------------------------------------------*/
static void traceSkip() {
  printf("\n    : \t\t\t\t\t\t\t");
}


/*----------------------------------------------------------------------*/
static void interpretIf(Aword v)
{
  int lev = 1;
  Aword i;

  if (!v) {
    /* Skip to next ELSE or ENDIF on same level */
    if (singleStepOption) traceSkip();
    while (TRUE) {
      i = memory[pc++];
      if (I_CLASS(i) == (Aword)C_STMOP)
	switch (I_OP(i)) {
	case I_ELSE:
	  if (lev == 1) {
	    if (singleStepOption)
	      printf("\n%4x: ELSE\t\t\t\t\t\t", pc);
	    return;
	  }
	  break;
	case I_IF:
	  lev++;
	  break;
	case I_ENDIF:
	  lev--;
	  if (lev == 0) {
	    if (singleStepOption)
	      printf("\n%4x: ENDIF\t\t\t\t\t\t", pc);
	    return;
	  }
	  break;
	}
    }
  }
}


/*----------------------------------------------------------------------*/
static void interpretElse(void)
{
  int lev = 1;
  Aword i;

  if (singleStepOption) traceSkip();
  while (TRUE) {
    /* Skip to ENDIF on the same level */
    i = memory[pc++];
    if (I_CLASS(i) == (Aword)C_STMOP)
      switch (I_OP(i)) {
      case I_ENDIF:
	lev--;
	if (lev == 0) return;
	break;
      case I_IF:
	lev++;
	break;
      }
  }
}


/*----------------------------------------------------------------------*/
static void goToLOOPEND(void) {
  int level = 1;
  int i;

  if (singleStepOption) traceSkip();
  while (TRUE) {
    /* Skip past LOOPEND on the same level */
    i = memory[pc];
    if (I_CLASS(i) == (Aword)C_STMOP)
      switch (I_OP(i)) {
      case I_LOOPEND:
	level--;
	if (level == 0)
	  return;
	break;
      case I_LOOP:
	level++;
	break;
      }
    pc++;
  }
}


/*----------------------------------------------------------------------*/
static void goToLOOP(void) {
  int level = 1;
  int i;

  if (singleStepOption) traceSkip();
  pc--;				/* Ignore the instruction we're on */
  while (TRUE) {
    /* Skip back past LOOP on the same level */
    i = memory[--pc];
    if (I_CLASS(i) == (Aword)C_STMOP)
      switch (I_OP(i)) {
      case I_LOOPEND:
	level++;
	break;
      case I_LOOP:
	level--;
	if (level == 0) {
	  return;
	}
	break;
      }
  }
}


/*----------------------------------------------------------------------*/
static void nextLoop(void)
{
  goToLOOPEND();
}  


/*----------------------------------------------------------------------*/
static void endLoop(Aint index, Aint limit)
{
  if (index < limit) {
    index++;
    push(limit);
    push(index);
    goToLOOP();
    if (singleStepOption)
      printf("\n%4x: LOOP\t\t\t\t\t\t", pc);
    pc++;
  }
}


/*----------------------------------------------------------------------*/
static void stackDup(void)
{
  push(top());
}


/*----------------------------------------------------------------------*/
static void depexec(Aword v)
{
  int lev = 1;
  Aword i;
  char *instructionString = "DEPELSE";

  if (!v) {
    /* The expression was not true, skip to next CASE on the same
       level which could be a DEPCASE or DEPELSE */
    if (singleStepOption) printf("\n    : ");
    while (TRUE) {
      i = memory[pc++];
      if (I_CLASS(i) == (Aword)C_STMOP)
	switch (I_OP(i)) {
	case I_DEPEND:
	  lev++;
	  break;
	case I_ENDDEP:
	  if (lev == 1) {
	    pc--;
	    if (singleStepOption)
	      printf("\n%4x: ENDDEP", pc);
	    return;
	  } else
	    lev--;
	  break;
	case I_DEPCASE:
	  instructionString = "DEPCASE";
	case I_DEPELSE:
	  if (lev == 1) {
	    if (singleStepOption)
	      printf("\n%4x: %s", pc, instructionString);
	    return;
	  }
	  break;
	}
    }
  }
}


/*----------------------------------------------------------------------*/
static void depcase(void)
{
  int lev = 1;
  Aword i;

  /* 
     We have just executed a DEPCASE/DEPELSE clause as a result of a
     DEPCASE catching so skip to end of DEPENDING block (next DEPEND
     on same level) then return.
  */

  if (singleStepOption) printf("\n    : ");
  while (TRUE) {
    i = memory[pc++];
    if (I_CLASS(i) == (Aword)C_STMOP)
      switch (I_OP(i)) {
      case I_DEPEND:
	lev++;
	break;
      case I_ENDDEP:
	lev--;
	if (lev == 0) {
	  pc--;
	  return;
	}
	break;
      }
  }
}


/*----------------------------------------------------------------------*/
static char *booleanValue(Abool bool) {
  if (bool) return "   TRUE";
  else return "  FALSE";
}

/*----------------------------------------------------------------------*/
static char *stringValue(Aptr adress) {
  static char string[100];

  sprintf(string, "0x%lx (\"%s\")\t\t", (unsigned long) adress, (char *)adress);
  return string;
}

/*----------------------------------------------------------------------*/
static char *pointerValue(Aptr adress) {
  static char string[100];

  sprintf(string, "@%6lx", (unsigned long) adress);
  return string;
}

/*----------------------------------------------------------------------*/
static void traceStringTopValue() {
  if (singleStepOption)
    printf("\t=%s", stringValue(top()));
}

/*----------------------------------------------------------------------*/
static void traceBooleanTopValue() {
  if (singleStepOption) {
    if (top()) printf("\t=TRUE\t");
    else printf("\t=FALSE\t");
  }
}

/*----------------------------------------------------------------------*/
static void traceIntegerTopValue() {
  if (singleStepOption)
    printf("\t=%ld\t", top());
}

/*----------------------------------------------------------------------*/
static void tracePointerTopValue() {
  if (singleStepOption)
    printf("\t=%s\t", pointerValue(top()));
}

/*----------------------------------------------------------------------*/
static void traceInstanceTopValue() {
  if (singleStepOption) {
    printf("\t=%ld ('", top());
    traceSay(top());
    printf("')");
    if (traceStackOption)
      printf("\n\t\t\t\t\t\t\t");
  }
}

/*----------------------------------------------------------------------*/
static char *directlyFlag(Abool bool) {
  if (bool) return "Direct";
  else return " Trans";
}

/*----------------------------------------------------------------------*/
static char *printForm(SayForm form) {
  switch (form) {
  case SAY_SIMPLE: return "-";
  case SAY_INDEFINITE: return "An";
  case SAY_DEFINITE: return "The";
  case SAY_NEGATIVE: return "No";
  case SAY_PRONOUN: return "It";
  }
  return "**Unknown!!***";
}


static Aaddr invocation[1000];
int depth = 0;

/*----------------------------------------------------------------------*/
static void checkForRecursion(Aaddr adr) {
  int i;

  for (i = 0; i < depth; i++)
    if (invocation[i] == adr)
      syserr("Interpreter recursion.");
  invocation[depth++] = adr;
  if (depth > 1000)
    syserr("Interpreter call stack too deep.");
}


static Bool skipStackDump = FALSE; /* Need to be able to skip it for LINE */

/*======================================================================*/
void interpret(Aaddr adr)
{
  Aaddr oldpc;
  Aword i;

  /* Sanity checks: */
  if (adr == 0) syserr("Interpreting at address 0.");
  checkForRecursion(adr);
  
  if (singleStepOption)
    printf("\n++++++++++++++++++++++++++++++++++++++++++++++++++");
  
  oldpc = pc;
  pc = adr;
  while(TRUE) {
    if (pc > memTop)
      syserr("Interpreting outside program.");

    i = memory[pc++];

    switch (I_CLASS(i)) {
    case C_CONST:
      if (tracePushOption) printf("\n%4x: PUSH  \t%7ld\t\t\t\t\t", pc-1, I_OP(i));
      push(I_OP(i));
      if (tracePushOption && traceStackOption)
	dumpStack();
      break;
    case C_CURVAR:
      if (singleStepOption) printf("\n%4x: ", pc-1);
      switch (I_OP(i)) {
      case V_PARAM:
	if (singleStepOption) printf("PARAM \t%7ld\t\t\t\t=%ld\t", top(),
				     parameters[top()-1].instance);
	push(parameters[pop()-1].instance);
	break;
      case V_CURLOC:
	if (singleStepOption) printf("CURLOC \t\t\t\t\t=%d\t", current.location);
	push(current.location);
	break;
      case V_CURACT:
	if (singleStepOption) printf("CURACT \t\t\t\t\t=%d\t", current.actor);
	push(current.actor);
	break;
      case V_CURVRB:
	if (singleStepOption) printf("CURVRB \t\t\t\t\t=%d\t", current.verb);
	push(current.verb);
	break;
      case V_CURRENT_INSTANCE:
	if (singleStepOption) printf("CURINS \t\t\t\t\t=%d\t", current.instance);
	push(current.instance);
	break;
      case V_SCORE:
	if (singleStepOption) printf("CURSCORE \t\t\t\t\t=%d\t", current.score);
	push(current.score);
	break;
      case V_MAX_INSTANCE:
	if (singleStepOption) printf("MAXINSTANCE \t\t\t\t=%d\t", (int)header->instanceMax);
	push(header->instanceMax);
	break;
      default:
	syserr("Unknown CURVAR instruction.");
	break;
      }
      if (traceStackOption)
	dumpStack();
      break;

    case C_STMOP:
      if (singleStepOption) printf("\n%4x: ", pc-1);
      switch (I_OP(i)) {

      case I_DUP:
	if (singleStepOption)
	  printf("DUP\t\t\t\t\t\t");
	stackDup();
	break;

      case I_POP: {
	Aptr top = pop();
	if (singleStepOption)
	  printf("POP\t%7ld", top);
	break;
      }

      case I_LINE: {
	Aint line = pop();
	Aint file = pop();
	if (singleStepOption)
	  printf("LINE\t%7ld, %7ld\t\t\t", file, line);
	if (traceStackOption)
	  dumpStack();
	skipStackDump = TRUE;
	if (line != 0) {
	  Bool atNext = stopAtNextLine && line != current.sourceLine;
	  Bool atBreakpoint =  breakpointIndex(file, line) != -1;
	  if (traceSourceOption
	      && (line != current.sourceLine || file != current.sourceFile)) {
	    if (col != 1 || singleStepOption)
	      newline();
	    showSourceLine(file, line);
	    if (!singleStepOption)
	      newline();
	  }
	  current.sourceLine = line;
	  current.sourceFile = file;
	  if (atNext || atBreakpoint) {
	    stopAtNextLine = FALSE;
	    debug(TRUE, line, file);
	  }
	}
	break;
      }

      case I_PRINT: {
	Aint fpos = pop();
	Aint len = pop();
	if (singleStepOption) {
	  printf("PRINT \t%7ld, %7ld\t\"", fpos, len);
	  col = 41;		/* To break lines better! */
	}
	print(fpos, len);
	if (singleStepOption) {
	  printf("\"");
	  if (traceStackOption)
	    printf("\n\t\t\t\t\t\t\t");
	}
	break;
      }

      case I_STYLE: {
	Aword style = pop();
	if (singleStepOption) {
	  printf("STYLE \t%7ld\t\t\"", style);
	}
	setStyle(style);
	break;
      }

      case I_SYSTEM: {
	Aint fpos = pop();
	Aint len = pop();
	if (singleStepOption) {
	  printf("SYSTEM \t%7ld, %7ld\t\"", fpos, len);
	  col = 34;		/* To format it better! */
	}
	sys(fpos, len);
	if (singleStepOption)
	  printf("\"\t\t\t\t\t\t");
	break;
      }

      case I_GETSTR: {
	Aint fpos = pop();
	Aint len = pop();
	if (singleStepOption)
	  printf("GETSTR\t%7ld, %7ld", fpos, len);
	push((Aptr)getStringFromFile(fpos, len));
	traceStringTopValue();
	break;
      }

      case I_QUIT: {
	if (singleStepOption)
	  printf("QUIT\t\t\t\t\t\t");
	quitGame();
	break;
      }
      case I_LOOK: {
	if (singleStepOption)
	  printf("LOOK\t\t\t\t\t\t");
	look();
	break;
      }
      case I_SAVE: {
	if (singleStepOption)
	  printf("SAVE\t\t\t\t\t\t");
	save();
	break;
      }
      case I_RESTORE: {
	if (singleStepOption)
	  printf("RESTORE\t\t\t\t\t\t");
	restore();
	break;
      }
      case I_RESTART: {
	if (singleStepOption)
	  printf("RESTART\t\t\t\t\t\t");
	restartGame();
	break;
      }

      case I_SCORE: {
	Aword sc = pop();
	if (singleStepOption)
	  printf("SCORE \t%7ld\t\t=%ld\t\t\t", sc, scores[sc-1]);
	score(sc);
	break;
      }
      case I_VISITS: {
	Aword v = pop();
	if (singleStepOption)
	  printf("VISITS \t%7ld\t\t\t\t\t", v);
	visits(v);
	break;
      }

      case I_LIST: {
	Aword cnt = pop();
	if (singleStepOption)
	  printf("LIST \t%7ld\t\t\t\t\t", cnt);
	list(cnt);
	break;
      }
      case I_EMPTY: {
	Aint cnt = pop();
	Aint whr = pop();
	if (singleStepOption)
	  printf("EMPTY \t%7ld, %7ld\t\t\t\t", cnt, whr);
	empty(cnt, whr);
	break;
      }
      case I_SCHEDULE: {
	Aint event = pop();
	Aint where = pop();
	Aint after = pop();
	if (singleStepOption)
	  printf("SCHEDULE \t%7ld, %7ld, %7ld\t\t\t\t", event, where, after);
	schedule(event, where, after);
	break;
      }
      case I_CANCEL: {
	Aword event = pop();
	if (singleStepOption)
	  printf("CANCEL \t%7ld\t\t\t\t", event);
	cancelEvent(event);
	break;
      }
      case I_MAKE: {
	Aint atr = pop();
	Aint id = pop();
	Abool val = pop();
	if (singleStepOption)
	  printf("MAKE \t%7ld, %7ld, %s\t\t\t", id, atr, booleanValue(val));
	setValue(id, atr, val);
	break;
      }
      case I_SET: {
	Aint atr = pop();
	Aint id = pop();
	Aptr val = pop();
	if (singleStepOption) {
	  printf("SET \t%7ld, %7ld, %7ld\t\t\t\t", id, atr, val);
	}
	setValue(id, atr, val);
	break;
      }
      case I_SETSTR: {
	Aint atr = pop();
	Aint id = pop();
	Aptr str = pop();
	if (singleStepOption) {
	  printf("SETSTR\t%7ld, %7ld, %s\t\t\t\t", id, atr, stringValue(str));
	}
	setStringAttribute(id, atr, (char *)str);
	break;
      }
      case I_SETSET: {
	Aint atr = pop();
	Aint id = pop();
	Aptr set = pop();
	if (singleStepOption) {
	  printf("SETSET\t%7ld, %7ld, %7s\t\t", id, atr, pointerValue(set));
	}
	setSetAttribute(id, atr, set);
	break;
      }
      case I_NEWSET: {
	Set *set = newSet(0);
	if (singleStepOption) {
	  printf("NEWSET\t\t\t");
	}
	push((Aptr)set);
	tracePointerTopValue();
	break;
      }
      case I_UNION: {
	Aptr set2 = pop();
	Aptr set1 = pop();
	if (singleStepOption) {
	  printf("UNION\t%7ld, %7ld\t\t\t\t", set1, set2);
	}
	push((Aptr)setUnion((Set *)set1, (Set *)set2));
	tracePointerTopValue();
	freeSet((Set *)set1);
	freeSet((Set *)set2);
	break;
      }
      case I_INCR: {
	Aint step = pop();
	if (singleStepOption) {
	  printf("INCR\t%7ld", step);
	}
	push(pop() + step);
	traceIntegerTopValue();
	break;
      }
      case I_DECR: {
	Aint step = pop();
	if (singleStepOption) {
	  printf("DECR\t%7ld\t\t\t\t\t", step);
	}
	push(pop() - step);
	traceIntegerTopValue();
	break;
      }
      case I_INCLUDE: {
	Aword member = pop();
	if (singleStepOption) {
	  printf("INCLUDE\t%7ld\t\t\t\t\t", member);
	}
	addToSet((Set *)top(), member);
	break;
      }
      case I_EXCLUDE: {
	Aword member = pop();
	if (singleStepOption) {
	  printf("EXCLUDE\t%7ld", member);
	}
	removeFromSet((Set *)top(), member);
	break;
      }
      case I_SETSIZE: {
	Set *set = (Set *)pop();
	if (singleStepOption)
	  printf("SETSIZE\t%7ld\t\t", (Aptr)set);
	push(setSize(set));
	if (singleStepOption)
	  traceIntegerTopValue();
	break;
      }
      case I_SETMEMB: {
	Set *set = (Set *)pop();
	Aint index = pop();
	if (singleStepOption)
	  printf("SETMEMB\t%7ld, %7ld", (Aptr)set, index);
	push(getSetMember(set, index));
	if (singleStepOption)
	  traceIntegerTopValue();
	break;
      }
      case I_CONTSIZE: {
	Abool directly = pop();
	Aint container = pop();
	if (singleStepOption)
	  printf("CONTSIZE\t%7ld, %7s\t", container, directlyFlag(directly));
	push(containerSize(container, directly));
	if (singleStepOption)
	  traceIntegerTopValue();
	break;
      }
      case I_CONTMEMB: {
	Abool directly = pop();
	Aint container = pop();
	Aint index = pop();
	if (singleStepOption)
	  printf("CONTMEMB\t%7ld, %7ld, %7s", container, index, directlyFlag(directly));
	push(getContainerMember(container, index, directly));
	if (singleStepOption)
	  traceIntegerTopValue();
	break;
      }
      case I_ATTRIBUTE: {
	Aint atr = pop();
	Aint id = pop();
	if (singleStepOption)
	  printf("ATTRIBUTE %7ld, %7ld\t", id, atr);
	push(attributeOf(id, atr));
	traceIntegerTopValue();
	break;
      }
      case I_ATTRSTR: {
	Aint atr = pop();
	Aint id = pop();
	if (singleStepOption)
	  printf("STRATTR \t%7ld, %7ld\t", id, atr);
	push(getStringAttribute(id, atr));
	traceStringTopValue();
	break;
      }
      case I_ATTRSET: {
	Aint atr = pop();
	Aint id = pop();
	if (singleStepOption)
	  printf("ATTRSET \t%7ld, %7ld", id, atr);
	push(getSetAttribute(id, atr));
	tracePointerTopValue();
	break;
      }
      case I_SHOW: {
	Aint image = pop();
	Aint align = pop();
	if (singleStepOption)
	  printf("SHOW \t%7ld, %7ld\t\t\t\t", image, align);
	showImage(image, align);
	break;
      }
      case I_PLAY: {
	Aint sound = pop();
	if (singleStepOption)
	  printf("PLAY \t%7ld\t\t\t\t", sound);
	playSound(sound);
	break;
      }
      case I_LOCATE: {
	Aint id = pop();
	Aint whr = pop();
	if (singleStepOption)
	  printf("LOCATE \t%7ld, %7ld\t\t\t", id, whr);
	locate(id, whr);
	break;
      }
      case I_WHERE: {
	Abool directly = pop();
	Aword id = pop();
	if (singleStepOption)
	  printf("WHERE \t%7ld, %7s", id, directlyFlag(directly));
	push(where(id, directly));
	traceInstanceTopValue();
	break;
      }
      case I_LOCATION: {
	Aword id = pop();
	if (singleStepOption)
	  printf("LOCATION \t%7ld\t\t", id);
	push(location(id));
	traceInstanceTopValue();
	break;
      }
      case I_HERE: {
	Abool directly = pop();
	Aint id = pop();
	if (singleStepOption)
	  printf("HERE \t%7ld, %s\t\t\t", id, directlyFlag(directly));
	push(isHere(id, directly));
	traceBooleanTopValue();
	break;
      }
      case I_NEARBY: {
	Abool directly = pop();
	Aint id = pop();
	if (singleStepOption)
	  printf("NEARBY \t%7ld, %s\t\t\t", id, directlyFlag(directly));
	push(isNearby(id, directly));
	traceBooleanTopValue();
	break;
      }
      case I_NEAR: {
	Abool directly = pop();
	Aint other = pop();
	Aint id = pop();
	if (singleStepOption)
	  printf("NEAR \t%7ld, %7ld, %s\t\t\t", id, other, directlyFlag(directly));
	push(isNear(id, other, directly));
	traceBooleanTopValue();
	break;
      }
      case I_AT: {
	Abool directly = pop();
	Aint other = pop();
	Aint instance = pop();
	if (singleStepOption)
	  printf("AT \t%7ld, %7ld, %s", instance, other, directlyFlag(directly));
	push(at(instance, other, directly));
	traceBooleanTopValue();
	break;
      }
      case I_IN: {
	Abool directly = pop();
	Aint cnt = pop();
	Aint obj = pop();
	if (singleStepOption)
	  printf("IN \t%7ld, %7ld, %s", obj, cnt, directlyFlag(directly));
	push(in(obj, cnt, directly));
	traceBooleanTopValue();
	break;
      }
      case I_INSET: {
	Aptr set = pop();
	Aword element = pop();
	if (singleStepOption)
	  printf("INSET \t%7ld, %7ld", element, set);
	push(inSet((Set*)set, element));
	traceBooleanTopValue();
	break;
      }
      case I_USE: {
	Aint act = pop();
	Aint scr = pop();
	if (singleStepOption)
	  printf("USE \t%7ld, %7ld\t\t\t\t", act, scr);
	use(act, scr);
	break;
      }
      case I_STOP: {
	Aword actor = pop();
	if (singleStepOption)
	  printf("STOP \t%7ld\t\t\t\t\t", actor);
	stop(actor);
	break;
      }
      case I_DESCRIBE: {
	Aword id = pop();
	if (singleStepOption) {
	  printf("DESCRIBE \t%7ld\t\t\t", id);
	  col = 41;		/* To format it better! */
	}
	describe(id);
	if (singleStepOption)
	  printf("\n\t\t\t\t\t\t");
	break;
      }
      case I_SAY: {
	Aint form = pop();
	Aint id = pop();
	if (singleStepOption)
	  printf("SAY\t%7s, %7ld\t\t\t", printForm(form), id);
	if (form == SAY_SIMPLE)
	  say(id);
	else
	  sayForm(id, form);
	if (singleStepOption)
	  printf("\t\t\t\t\t\t\t");
	break;
      }
      case I_SAYINT: {
	Aword val = pop();
	if (singleStepOption)
	  printf("SAYINT\t%7ld\t\t\t\"", val);
	sayInteger(val);
	if (singleStepOption)
	  printf("\"\n\t\t\t\t\t\t\t");
	break;
      }
      case I_SAYSTR: {
	Aptr adr = pop();
	if (singleStepOption)
	  printf("SAYSTR\t%7ld\t\ty\t", adr);
	sayString((char *)adr);
	if (singleStepOption)
	  printf("\n\t\t\t\t\t\t");
	break;
      }
      case I_IF: {
	Aword v = pop();
	if (singleStepOption)
	  printf("IF \t%s\t\t\t\t\t", booleanValue(v));
	interpretIf(v);
	break;
      }
      case I_ELSE: {
	if (singleStepOption)
	  printf("ELSE\t\t\t\t\t\t");
	interpretElse();
	break;
      }
      case I_ENDIF: {
	if (singleStepOption)
	  printf("ENDIF\t\t\t\t\t\t");
	break;
      }
      case I_AND: {
	Aword rh = pop();
	Aword lh = pop();
	if (singleStepOption)
	  printf("AND \t%s, %s", booleanValue(lh), booleanValue(rh));
	push(lh && rh);
	traceBooleanTopValue();
	break;
      }
      case I_OR: {
	Aword rh = pop();
	Aword lh = pop();
	if (singleStepOption)
	  printf("OR \t%s, %s", booleanValue(lh), booleanValue(rh));
	push(lh || rh);
	traceBooleanTopValue();
	break;
      }
      case I_NE: {
	Aword rh = pop();
	Aword lh = pop();
	if (singleStepOption)
	  printf("NE \t%7ld, %7ld", lh, rh);
	push(lh != rh);
	traceBooleanTopValue();
	break;
      }
      case I_EQ: {
	Aword rh = pop();
	Aword lh = pop();
	if (singleStepOption)
	  printf("EQ \t%7ld, %7ld", lh, rh);
	push(lh == rh);
	traceBooleanTopValue();
	break;
      }
      case I_STREQ: {
	Aptr rh = pop();
	Aptr lh = pop();
	if (singleStepOption)
	  printf("STREQ \t%7ld, %7ld", lh, rh);
	push(streq((char *)lh, (char *)rh));
	traceBooleanTopValue();
	break;
      }
      case I_STREXACT: {
	Aptr rh = pop();
	Aptr lh = pop();
	if (singleStepOption)
	  printf("STREXACT \t%7ld, %7ld", lh, rh);
	push(strcmp((char *)lh, (char *)rh) == 0);
	traceBooleanTopValue();
	free((void *)lh);
	free((void *)rh);
	break;
      }
      case I_LE: {
	Aint rh = pop();
	Aint lh = pop();
	if (singleStepOption)
	  printf("LE \t%7ld, %7ld", lh, rh);
	push(lh <= rh);
	traceBooleanTopValue();
	break;
      }
      case I_GE: {
	Aint rh = pop();
	Aint lh = pop();
	if (singleStepOption)
	  printf("GE \t%7ld, %7ld", lh, rh);
	push(lh >= rh);
	traceBooleanTopValue();
	break;
      }
      case I_LT: {
	Aint rh = pop();
	Aint lh = pop();
	if (singleStepOption)
	  printf("LT \t%7ld, %7ld", lh, rh);
	push(lh < rh);
	traceBooleanTopValue();
	break;
      }
      case I_GT: {
	Aint rh = pop();
	Aint lh = pop();
	if (singleStepOption)
	  printf("GT \t%7ld, %7ld", lh, rh);
	push(lh > rh);
	traceBooleanTopValue();
	break;
      }
      case I_PLUS: {
	Aint rh = pop();
	Aint lh = pop();
	if (singleStepOption)
	  printf("PLUS \t%7ld, %7ld", lh, rh);
	push(lh + rh);
	traceIntegerTopValue();
	break;
      }
      case I_MINUS: {
	Aint rh = pop();
	Aint lh = pop();
	if (singleStepOption)
	  printf("MINUS \t%7ld, %7ld", lh, rh);
	push(lh - rh);
	traceIntegerTopValue();
	break;
      }
      case I_MULT: {
	Aint rh = pop();
	Aint lh = pop();
	if (singleStepOption)
	  printf("MULT \t%7ld, %7ld", lh, rh);
	push(lh * rh);
	traceIntegerTopValue();
	break;
      }
      case I_DIV: {
	Aint rh = pop();
	Aint lh = pop();
	if (singleStepOption)
	  printf("DIV \t%7ld, %7ld", lh, rh);
	push(lh / rh);
	traceIntegerTopValue();
	break;
      }
      case I_NOT: {
	Aword val = pop();
	if (singleStepOption)
	  printf("NOT \t%s\t\t\t", booleanValue(val));
	push(!val);
	traceBooleanTopValue();
	break;
      }
      case I_RND: {
	Aint from = pop();
	Aint to = pop();
	if (singleStepOption)
	  printf("RANDOM \t%7ld, %7ld", from, to);
	push(randomInteger(from, to));
	traceIntegerTopValue();
	break;
      }
      case I_BTW: {
	Aint high = pop();
	Aint low = pop();
	Aint val = pop();
	if (singleStepOption)
	  printf("BETWEEN \t%7ld, %7ld, %7ld", val, low, high);
	push(btw(val, low, high));
	traceIntegerTopValue();
	break;
      }

	/*------------------------------------------------------------*\
	  String functions
	\*------------------------------------------------------------*/
      case I_CONCAT: {
	Aptr s2 = pop();
	Aptr s1 = pop();
	if (singleStepOption)
	  printf("CONCAT \t%7ld, %7ld", s1, s2);
	push(concat(s1, s2));
	traceStringTopValue();
	break;
      }

      case I_CONTAINS: {
	Aptr substring = pop();
	Aptr string = pop();
	if (singleStepOption)
	  printf("CONTAINS \t%7ld, %7ld", string, substring);
	push(contains(string, substring));
	traceIntegerTopValue();
	break;
      }

      case I_STRIP: {
	Aint atr = pop();
	Aint id = pop();
	Aint words = pop();
	Aint count = pop();
	Aint first = pop();
	if (singleStepOption)
	  printf("STRIP \t%7ld, %7ld, %7ld, %7ld, %7ld", first, count, words, id, atr);
	push(strip(first, count, words, id, atr));
	traceStringTopValue();
	break;
      }

	/*------------------------------------------------------------
	  Aggregation
	  ------------------------------------------------------------*/
      case I_MIN:
      case I_SUM:
      case I_MAX: {
	Aint attribute = pop();
	Aint loopIndex = pop();
	Aint limit = pop();
	Aint aggregate = pop();
	switch (I_OP(i)) {
	case I_MAX:
	  if (singleStepOption)
	    printf("MAX \t%7ld\t\t\t", attribute);
	  if (aggregate < attribute)
	    push(attribute); 
	  else
	    push(aggregate);
	  break;
	case I_MIN:
	  if (singleStepOption)
	    printf("MIN \t%7ld\t\t\t", attribute);
	  if (aggregate > attribute)
	    push(attribute);
	  else
	    push(aggregate);
	  break;
	case I_SUM:
	  if (singleStepOption)
	    printf("SUM \t%7ld\t\t\t", attribute);
	  push(aggregate + attribute);
	  break;
	}
	traceIntegerTopValue();
	push(limit);
	push(loopIndex);
	break;
      }
      case I_COUNT: {
	Aint loopIndex = pop();
	Aint limit = pop();
	if (singleStepOption)
	  printf("COUNT\t\t\t");
	push(pop() + 1);
	traceIntegerTopValue();
	push(limit);
	push(loopIndex);
	break;
      }

	/*------------------------------------------------------------*\
	  Depending On
	\*------------------------------------------------------------*/
      case I_DEPEND:
	if (singleStepOption)
	  printf("DEPEND\t\t\t\t\t\t");
	break;

      case I_DEPCASE:
	if (singleStepOption)
	  printf("DEPCASE\t\t\t\t\t\t");
	depcase();
	break;

      case I_DEPEXEC: {
	Aword v = pop();
	if (singleStepOption) {
	  printf("DEPEXEC \t\t\t");
	  if (v) printf(" TRUE"); else printf("FALSE");
	  printf("\t\t\t\t\t");
	}
	depexec(v);
	break;
      }
	
      case I_DEPELSE:
	if (singleStepOption)
	  printf("DEPELSE\t\t\t\t\t\t");
	depcase();
	break;

      case I_ENDDEP:
	if (singleStepOption)
	  printf("ENDDEP\t\t\t\t\t\t");
	pop();
	break;

      case I_ISA: {
	Aint rh = pop();
	Aint lh = pop();
	if (singleStepOption)
	  printf("ISA \t%7ld, %7ld\t", lh, rh);
	push(isA(lh, rh));
	traceBooleanTopValue();
	break;
      }

      case I_FRAME: {
	Aint size = pop();
	if (singleStepOption)
	  printf("FRAME \t%7ld\t\t\t\t\t", size);
	newFrame(size);
	break;
      }

      case I_GETLOCAL: {
	Aint framesBelow = pop();
	Aint variableNumber = pop();
	if (singleStepOption)
	  printf("GETLOCAL \t%7ld, %7ld\t", framesBelow, variableNumber);
	push(getLocal(framesBelow, variableNumber));
	traceIntegerTopValue();
	break;
      }

      case I_SETLOCAL: {
	Aint framesBelow = pop();
	Aint variableNumber = pop();
	Aint value = pop();
	if (singleStepOption)
	  printf("SETLOCAL \t%7ld, %7ld, %7ld\t\t", framesBelow, variableNumber, value);
	setLocal(framesBelow, variableNumber, value);
	break;
      }

      case I_ENDFRAME: {
	if (singleStepOption)
	  printf("ENDFRAME\t\t\t\t\t\t");
	endFrame();
	break;
      }

      case I_LOOP: {
	Aint index = pop();
	Aint limit = pop();
	if (singleStepOption)
	  printf("LOOP \t\t\t\t\t\t");
	push(limit);
	push(index);
	if (index > limit)
	  goToLOOPEND();
	break;
      }

      case I_LOOPNEXT: {
	if (singleStepOption)
	  printf("LOOPNEXT\t\t\t\t\t\t");
	nextLoop();
	break;
      }

      case I_LOOPEND: {
	Aint index = pop();
	Aint limit = pop();
	if (singleStepOption)
	  printf("LOOPEND\t\t\t\t\t\t");
	endLoop(index, limit);
	break;
      }

      case I_RETURN:
	if (singleStepOption)
	  printf("RETURN\n--------------------------------------------------\n");
	pc = oldpc;
	goto exitInterpreter;

      default:
	syserr("Unknown STMOP instruction.");
	break;
      }
      if (fail) {
	pc = oldpc;
	goto exitInterpreter;
      }
      if (traceStackOption) {
	if (!skipStackDump)
	  dumpStack();
	skipStackDump = FALSE;
      }      
      break;

    default:
      syserr("Unknown instruction class.");
      break;
    }
  }
 exitInterpreter:
  depth--;
}
