/*----------------------------------------------------------------------*\

inter.c

Interpreter unit for Alan interpreter Arun

\*----------------------------------------------------------------------*/
#include "inter.h"


#include <stdio.h>

#include "current.h"
#include "exe.h"
#include "syserr.h"
#include "debug.h"
#include "options.h"
#include "save.h"
#include "memory.h"
#include "output.h"
#include "score.h"
#include "params.h"
#include "instance.h"
#include "Container.h"
#include "Location.h"

#ifdef HAVE_GLK
#define MAP_STDIO_TO_GLK
#include "glkio.h"
#endif


bool stopAtNextLine = FALSE;
bool fail = FALSE;


/* PRIVATE DATA */

static int pc;
static Stack stack = NULL;

static void (*interpreterMock)(Aaddr adr) = NULL;


/*======================================================================*/
void setInterpreterMock(void (*mock)(Aaddr adr)) {
    interpreterMock = mock;
}


/*======================================================================*/
void setInterpreterStack(Stack theStack)
{
    stack = theStack;
}


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
static void jumpBackToStartOfMatchingLOOP(void) {
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
        push(stack, limit);
        push(stack, index);
        jumpBackToStartOfMatchingLOOP();
        if (singleStepOption)
            printf("\n%4x: LOOP\t\t\t\t\t\t", pc);
        pc++;
    }
}


/*----------------------------------------------------------------------*/
static void stackDup(void)
{
    push(stack, top(stack));
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
static char *booleanValue(Abool value) {
    if (value) return "   TRUE";
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

    sprintf(string, "@%6lx",(unsigned long) adress);
    return string;
}

/*----------------------------------------------------------------------*/
static void traceStringTopValue() {
    if (singleStepOption)
        printf("\t=%s", stringValue(top(stack)));
}

/*----------------------------------------------------------------------*/
static void tracebooleanTopValue() {
    if (singleStepOption) {
        if (top(stack)) printf("\t=TRUE\t");
        else printf("\t=FALSE\t");
    }
}

/*----------------------------------------------------------------------*/
static void traceIntegerTopValue() {
    if (singleStepOption)
        printf("\t=%ld\t", (long)top(stack));
}

/*----------------------------------------------------------------------*/
static void tracePointerTopValue() {
    if (singleStepOption)
        printf("\t=%s\t", pointerValue(top(stack)));
}

/*----------------------------------------------------------------------*/
static void traceInstanceTopValue() {
    if (singleStepOption) {
        printf("\t=%ld ('", (long)top(stack));
        traceSay(top(stack));
        printf("')");
        if (traceStackOption)
            printf("\n\t\t\t\t\t\t\t");
    }
}

/*----------------------------------------------------------------------*/
static char *directlyFlag(Abool value) {
    if (value) return "Direct";
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
int recursionDepth = 0;

/*----------------------------------------------------------------------*/
static void checkForRecursion(Aaddr adr) {
    int i;

    for (i = 0; i < recursionDepth; i++)
        if (invocation[i] == adr)
            apperr("Interpreter recursion.");
    invocation[recursionDepth++] = adr;
    if (recursionDepth > 1000)
        syserr("Interpreter call stack too deep.");
}


static bool skipStackDump = FALSE; /* Need to be able to skip it for LINE */

/*======================================================================*/
void interpret(Aaddr adr)
{
    Aaddr oldpc;
    Aword i;

    /* Check for mock implementation */
    if (interpreterMock != NULL) {
        interpreterMock(adr);
        return;
    }

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
            if (tracePushOption) printf("\n%4x: PUSH  \t%7ld\t\t\t\t\t", pc-1, (long)I_OP(i));
            push(stack, I_OP(i));
            if (tracePushOption && traceStackOption)
                dumpStack(stack);
            break;
        case C_CURVAR:
            if (singleStepOption) printf("\n%4x: ", pc-1);
            switch (I_OP(i)) {
            case V_PARAM:
                if (singleStepOption) printf("PARAM \t%7ld\t\t\t\t=%ld\t", (long)top(stack),
                                             (long)globalParameters[top(stack)-1].instance);
                push(stack, globalParameters[pop(stack)-1].instance);
                break;
            case V_CURLOC:
                if (singleStepOption) printf("CURLOC \t\t\t\t\t=%d\t", current.location);
                push(stack, current.location);
                break;
            case V_CURACT:
                if (singleStepOption) printf("CURACT \t\t\t\t\t=%d\t", current.actor);
                push(stack, current.actor);
                break;
            case V_CURVRB:
                if (singleStepOption) printf("CURVRB \t\t\t\t\t=%d\t", current.verb);
                push(stack, current.verb);
                break;
            case V_CURRENT_INSTANCE:
                if (singleStepOption) printf("CURINS \t\t\t\t\t=%d\t", current.instance);
                push(stack, current.instance);
                break;
            case V_SCORE:
                if (singleStepOption) printf("CURSCORE \t\t\t\t\t=%d\t", current.score);
                push(stack, current.score);
                break;
            case V_MAX_INSTANCE:
                if (singleStepOption) printf("MAXINSTANCE \t\t\t\t=%d\t", (int)header->instanceMax);
                push(stack, header->instanceMax);
                break;
            default:
                syserr("Unknown CURVAR instruction.");
                break;
            }
            if (traceStackOption)
                dumpStack(stack);
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
                Aptr top = pop(stack);
                if (singleStepOption)
                    printf("POP\t%7ld", (long)top);
                break;
            }

            case I_LINE: {
                Aint line = pop(stack);
                Aint file = pop(stack);
                if (singleStepOption)
                    printf("LINE\t%7ld, %7ld\t\t\t", (long)file, (long)line);
                if (traceStackOption)
                    dumpStack(stack);
                skipStackDump = TRUE;
                if (line != 0) {
                    bool atNext = stopAtNextLine && line != current.sourceLine;
                    bool atBreakpoint =  breakpointIndex(file, line) != -1;
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
                Aint fpos = pop(stack);
                Aint len = pop(stack);
                if (singleStepOption) {
                    printf("PRINT \t%7ld, %7ld\t\"", (long)fpos, (long)len);
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
                Aint style = pop(stack);
                if (singleStepOption) {
                    printf("STYLE \t%7ld\t\t\"", (long)style);
                }
                setStyle(style);
                break;
            }

            case I_SYSTEM: {
                Aint fpos = pop(stack);
                Aint len = pop(stack);
                if (singleStepOption) {
                    printf("SYSTEM \t%7ld, %7ld\t\"", (long)fpos, (long)len);
                    col = 34;		/* To format it better! */
                }
                sys(fpos, len);
                if (singleStepOption)
                    printf("\"\t\t\t\t\t\t");
                break;
            }

            case I_GETSTR: {
                Aint fpos = pop(stack);
                Aint len = pop(stack);
                if (singleStepOption)
                    printf("GETSTR\t%7ld, %7ld", (long)fpos, (long)len);
                push(stack, (Aptr)getStringFromFile(fpos, len));
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
                Aint sc = pop(stack);
                if (singleStepOption)
                    printf("SCORE \t%7ld\t\t=%ld\t\t\t", (long)sc, (long)scores[sc-1]);
                score(sc);
                break;
            }
            case I_VISITS: {
                Aint v = pop(stack);
                if (singleStepOption)
                    printf("VISITS \t%7ld\t\t\t\t\t", (long)v);
                visits(v);
                break;
            }

            case I_LIST: {
                Aint cnt = pop(stack);
                if (singleStepOption)
                    printf("LIST \t%7ld\t\t\t\t\t", (long)cnt);
                list(cnt);
                break;
            }
            case I_EMPTY: {
                Aint cnt = pop(stack);
                Aint whr = pop(stack);
                if (singleStepOption)
                    printf("EMPTY \t%7ld, %7ld\t\t\t\t", (long)cnt, (long)whr);
                empty(cnt, whr);
                break;
            }
            case I_SCHEDULE: {
                Aint event = pop(stack);
                Aint where = pop(stack);
                Aint after = pop(stack);
                if (singleStepOption)
                    printf("SCHEDULE \t%7ld, %7ld, %7ld\t\t\t\t", (long)event, (long)where, (long)after);
                schedule(event, where, after);
                break;
            }
            case I_CANCEL: {
                Aint event = pop(stack);
                if (singleStepOption)
                    printf("CANCEL \t%7ld\t\t\t\t", (long)event);
                cancelEvent(event);
                break;
            }
            case I_MAKE: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                Abool val = pop(stack);
                if (singleStepOption)
                    printf("MAKE \t%7ld, %7ld, %s\t\t\t", (long)id, (long)atr, booleanValue(val));
                setInstanceAttribute(id, atr, val);
                break;
            }
            case I_SET: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                Aptr val = pop(stack);
                if (singleStepOption) {
                    printf("SET \t%7ld, %7ld, %7ld\t\t\t\t", (long)id, (long)atr, (long)val);
                }
                setInstanceAttribute(id, atr, val);
                break;
            }
            case I_SETSTR: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                Aptr str = pop(stack);
                if (singleStepOption) {
                    printf("SETSTR\t%7ld, %7ld, %s\t\t\t\t", (long)id, (long)atr, stringValue(str));
                }
                setInstanceStringAttribute(id, atr, (char *)str);
                break;
            }
            case I_SETSET: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                Aptr set = pop(stack);
                if (singleStepOption) {
                    printf("SETSET\t%7ld, %7ld, %7s\t\t", (long)id, (long)atr, pointerValue(set));
                }
                setInstanceSetAttribute(id, atr, set);
                break;
            }
            case I_NEWSET: {
                Set *set = newSet(0);
                if (singleStepOption) {
                    printf("NEWSET\t\t\t");
                }
                push(stack, (Aptr)set);
                tracePointerTopValue();
                break;
            }
            case I_UNION: {
                Aptr set2 = pop(stack);
                Aptr set1 = pop(stack);
                if (singleStepOption) {
                    printf("UNION\t%7ld, %7ld\t\t\t\t", (long)set1, (long)set2);
                }
                push(stack, (Aptr)setUnion((Set *)set1, (Set *)set2));
                tracePointerTopValue();
                freeSet((Set *)set1);
                freeSet((Set *)set2);
                break;
            }
            case I_INCR: {
                Aint step = pop(stack);
                if (singleStepOption) {
                    printf("INCR\t%7ld", (long)step);
                }
                push(stack, pop(stack) + step);
                traceIntegerTopValue();
                break;
            }
            case I_DECR: {
                Aint step = pop(stack);
                if (singleStepOption) {
                    printf("DECR\t%7ld\t\t\t\t\t", (long)step);
                }
                push(stack, pop(stack) - step);
                traceIntegerTopValue();
                break;
            }
            case I_INCLUDE: {
                Aint member = pop(stack);
                if (singleStepOption) {
                    printf("INCLUDE\t%7ld\t\t\t\t\t", (long)member);
                }
                addToSet((Set *)top(stack), member);
                break;
            }
            case I_EXCLUDE: {
                Aint member = pop(stack);
                if (singleStepOption) {
                    printf("EXCLUDE\t%7ld", (long)member);
                }
                removeFromSet((Set *)top(stack), member);
                break;
            }
            case I_SETSIZE: {
                Set *set = (Set *)pop(stack);
                if (singleStepOption)
                    printf("SETSIZE\t%7ld\t\t", (long)set);
                push(stack, setSize(set));
                if (singleStepOption)
                    traceIntegerTopValue();
                break;
            }
            case I_SETMEMB: {
                Set *set = (Set *)pop(stack);
                Aint index = pop(stack);
                if (singleStepOption)
                    printf("SETMEMB\t%7ld, %7ld", (long)set, (long)index);
                push(stack, getSetMember(set, index));
                if (singleStepOption)
                    traceIntegerTopValue();
                break;
            }
            case I_CONTSIZE: {
                Abool directly = pop(stack);
                Aint container = pop(stack);
                if (singleStepOption)
                    printf("CONTSIZE\t%7ld, %7s\t", (long)container, directlyFlag(directly));
                push(stack, containerSize(container, directly));
                if (singleStepOption)
                    traceIntegerTopValue();
                break;
            }
            case I_CONTMEMB: {
                Abool directly = pop(stack);
                Aint container = pop(stack);
                Aint index = pop(stack);
                if (singleStepOption)
                    printf("CONTMEMB\t%7ld, %7ld, %7s", (long)container, (long)index, directlyFlag(directly));
                push(stack, getContainerMember(container, index, directly));
                if (singleStepOption)
                    traceIntegerTopValue();
                break;
            }
            case I_ATTRIBUTE: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                if (singleStepOption)
                    printf("ATTRIBUTE %7ld, %7ld\t", (long)id, (long)atr);
                push(stack, getInstanceAttribute(id, atr));
                traceIntegerTopValue();
                break;
            }
            case I_ATTRSTR: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                if (singleStepOption)
                    printf("STRATTR \t%7ld, %7ld\t", (long)id, (long)atr);
                push(stack, (Aptr)getInstanceStringAttribute(id, atr));
                traceStringTopValue();
                break;
            }
            case I_ATTRSET: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                if (singleStepOption)
                    printf("ATTRSET \t%7ld, %7ld", (long)id, (long)atr);
                push(stack, (Aptr)getInstanceSetAttribute(id, atr));
                tracePointerTopValue();
                break;
            }
            case I_SHOW: {
                Aint image = pop(stack);
                Aint align = pop(stack);
                if (singleStepOption)
                    printf("SHOW \t%7ld, %7ld\t\t\t\t", (long)image, (long)align);
                showImage(image, align);
                break;
            }
            case I_PLAY: {
                Aint sound = pop(stack);
                if (singleStepOption)
                    printf("PLAY \t%7ld\t\t\t\t", (long)sound);
                playSound(sound);
                break;
            }
            case I_LOCATE: {
                Aid id = pop(stack);
                Aint whr = pop(stack);
                if (singleStepOption)
                    printf("LOCATE \t%7ld, %7ld\t\t\t", (long)id, (long)whr);
                locate(id, whr);
                break;
            }
            case I_WHERE: {
                Abool directly = pop(stack);
                Aid id = pop(stack);
                if (singleStepOption)
                    printf("WHERE \t%7ld, %7s", (long)id, directlyFlag(directly));
                push(stack, where(id, directly));
                traceInstanceTopValue();
                break;
            }
            case I_LOCATION: {
                Aid id = pop(stack);
                if (singleStepOption)
                    printf("LOCATION \t%7ld\t\t", (long)id);
                push(stack, locationOf(id));
                traceInstanceTopValue();
                break;
            }
            case I_HERE: {
                Abool directly = pop(stack);
                Aid id = pop(stack);
                if (singleStepOption)
                    printf("HERE \t%7ld, %s\t\t\t", (long)id, directlyFlag(directly));
                push(stack, isHere(id, directly));
                tracebooleanTopValue();
                break;
            }
            case I_NEARBY: {
                Abool directly = pop(stack);
                Aid id = pop(stack);
                if (singleStepOption)
                    printf("NEARBY \t%7ld, %s\t\t\t", (long)id, directlyFlag(directly));
                push(stack, isNearby(id, directly));
                tracebooleanTopValue();
                break;
            }
            case I_NEAR: {
                Abool directly = pop(stack);
                Aint other = pop(stack);
                Aid id = pop(stack);
                if (singleStepOption)
                    printf("NEAR \t%7ld, %7ld, %s\t\t\t", (long)id, (long)other, directlyFlag(directly));
                push(stack, isNear(id, other, directly));
                tracebooleanTopValue();
                break;
            }
            case I_AT: {
                Abool directly = pop(stack);
                Aint other = pop(stack);
                Aint instance = pop(stack);
                if (singleStepOption)
                    printf("AT \t%7ld, %7ld, %s", (long)instance, (long)other, directlyFlag(directly));
                push(stack, at(instance, other, directly));
                tracebooleanTopValue();
                break;
            }
            case I_IN: {
                Abool directly = pop(stack);
                Aint cnt = pop(stack);
                Aint obj = pop(stack);
                if (singleStepOption)
                    printf("IN \t%7ld, %7ld, %s", (long)obj, (long)cnt, directlyFlag(directly));
                push(stack, in(obj, cnt, directly));
                tracebooleanTopValue();
                break;
            }
            case I_INSET: {
                Aptr set = pop(stack);
                Aword element = pop(stack);
                if (singleStepOption)
                    printf("INSET \t%7ld, %7ld", (long)element, (long)set);
                push(stack, inSet((Set*)set, element));
                tracebooleanTopValue();
                break;
            }
            case I_USE: {
                Aid act = pop(stack);
                Aint scr = pop(stack);
                if (singleStepOption)
                    printf("USE \t%7ld, %7ld\t\t\t\t", (long)act, (long)scr);
                use(act, scr);
                break;
            }
            case I_STOP: {
                Aid actor = pop(stack);
                if (singleStepOption)
                    printf("STOP \t%7ld\t\t\t\t\t", (long)actor);
                stop(actor);
                break;
            }
            case I_DESCRIBE: {
                Aid id = pop(stack);
                if (singleStepOption) {
                    printf("DESCRIBE \t%7ld\t\t\t", (long)id);
                    col = 41;		/* To format it better! */
                }
                describe(id);
                if (singleStepOption)
                    printf("\n\t\t\t\t\t\t");
                break;
            }
            case I_SAY: {
                Aint form = pop(stack);
                Aid id = pop(stack);
                if (singleStepOption)
                    printf("SAY\t%7s, %7ld\t\t\t", printForm(form), (long)id);
                if (form == SAY_SIMPLE)
                    say(id);
                else
                    sayForm(id, form);
                if (singleStepOption)
                    printf("\t\t\t\t\t\t\t");
                break;
            }
            case I_SAYINT: {
                Aword val = pop(stack);
                if (singleStepOption)
                    printf("SAYINT\t%7ld\t\t\t\"", (long)val);
                sayInteger(val);
                if (singleStepOption)
                    printf("\"\n\t\t\t\t\t\t\t");
                break;
            }
            case I_SAYSTR: {
                Aptr adr = pop(stack);
                if (singleStepOption)
                    printf("SAYSTR\t%7ld\t\ty\t", (long)adr);
                sayString((char *)adr);
                if (singleStepOption)
                    printf("\n\t\t\t\t\t\t");
                break;
            }
            case I_IF: {
                Aword v = pop(stack);
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
                Aword rh = pop(stack);
                Aword lh = pop(stack);
                if (singleStepOption)
                    printf("AND \t%s, %s", booleanValue(lh), booleanValue(rh));
                push(stack, lh && rh);
                tracebooleanTopValue();
                break;
            }
            case I_OR: {
                Aword rh = pop(stack);
                Aword lh = pop(stack);
                if (singleStepOption)
                    printf("OR \t%s, %s", booleanValue(lh), booleanValue(rh));
                push(stack, lh || rh);
                tracebooleanTopValue();
                break;
            }
            case I_NE: {
                Aword rh = pop(stack);
                Aword lh = pop(stack);
                if (singleStepOption)
                    printf("NE \t%7ld, %7ld", (long)lh, (long)rh);
                push(stack, lh != rh);
                tracebooleanTopValue();
                break;
            }
            case I_EQ: {
                Aword rh = pop(stack);
                Aword lh = pop(stack);
                if (singleStepOption)
                    printf("EQ \t%7ld, %7ld", (long)lh, (long)rh);
                push(stack, lh == rh);
                tracebooleanTopValue();
                break;
            }
            case I_STREQ: {
                Aptr rh = pop(stack);
                Aptr lh = pop(stack);
                if (singleStepOption)
                    printf("STREQ \t%7ld, %7ld", (long)lh, (long)rh);
                push(stack, streq((char *)lh, (char *)rh));
                tracebooleanTopValue();
                break;
            }
            case I_STREXACT: {
                Aptr rh = pop(stack);
                Aptr lh = pop(stack);
                if (singleStepOption)
                    printf("STREXACT \t%7ld, %7ld", (long)lh, (long)rh);
                push(stack, strcmp((char *)lh, (char *)rh) == 0);
                tracebooleanTopValue();
                free((void *)lh);
                free((void *)rh);
                break;
            }
            case I_LE: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                if (singleStepOption)
                    printf("LE \t%7ld, %7ld", (long)lh, (long)rh);
                push(stack, lh <= rh);
                tracebooleanTopValue();
                break;
            }
            case I_GE: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                if (singleStepOption)
                    printf("GE \t%7ld, %7ld", (long)lh, (long)rh);
                push(stack, lh >= rh);
                tracebooleanTopValue();
                break;
            }
            case I_LT: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                if (singleStepOption)
                    printf("LT \t%7ld, %7ld", (long)lh, (long)rh);
                push(stack, lh < rh);
                tracebooleanTopValue();
                break;
            }
            case I_GT: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                if (singleStepOption)
                    printf("GT \t%7ld, %7ld", (long)lh, (long)rh);
                push(stack, lh > rh);
                tracebooleanTopValue();
                break;
            }
            case I_PLUS: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                if (singleStepOption)
                    printf("PLUS \t%7ld, %7ld", (long)lh, (long)rh);
                push(stack, lh + rh);
                traceIntegerTopValue();
                break;
            }
            case I_MINUS: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                if (singleStepOption)
                    printf("MINUS \t%7ld, %7ld", (long)lh, (long)rh);
                push(stack, lh - rh);
                traceIntegerTopValue();
                break;
            }
            case I_MULT: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                if (singleStepOption)
                    printf("MULT \t%7ld, %7ld", (long)lh, (long)rh);
                push(stack, lh * rh);
                traceIntegerTopValue();
                break;
            }
            case I_DIV: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                if (singleStepOption)
                    printf("DIV \t%7ld, %7ld", (long)lh, (long)rh);
                push(stack, lh / rh);
                traceIntegerTopValue();
                break;
            }
            case I_NOT: {
                Aword val = pop(stack);
                if (singleStepOption)
                    printf("NOT \t%s\t\t\t", booleanValue(val));
                push(stack, !val);
                tracebooleanTopValue();
                break;
            }
            case I_RND: {
                Aint from = pop(stack);
                Aint to = pop(stack);
                if (singleStepOption)
                    printf("RANDOM \t%7ld, %7ld", (long)from, (long)to);
                push(stack, randomInteger(from, to));
                traceIntegerTopValue();
                break;
            }
            case I_BTW: {
                Aint high = pop(stack);
                Aint low = pop(stack);
                Aint val = pop(stack);
                if (singleStepOption)
                    printf("BETWEEN \t%7ld, %7ld, %7ld", (long)val, (long)low, (long)high);
                push(stack, btw(val, low, high));
                traceIntegerTopValue();
                break;
            }

                /*------------------------------------------------------------* \
                  String functions
                \*------------------------------------------------------------*/
            case I_CONCAT: {
                Aptr s2 = pop(stack);
                Aptr s1 = pop(stack);
                if (singleStepOption)
                    printf("CONCAT \t%s, %s", pointerValue(s1), pointerValue(s2));
                push(stack, concat(s1, s2));
                traceStringTopValue();
                break;
            }

            case I_CONTAINS: {
                Aptr substring = pop(stack);
                Aptr string = pop(stack);
                if (singleStepOption)
                    printf("CONTAINS \t%s, %s", pointerValue(string), pointerValue(substring));
                push(stack, contains(string, substring));
                traceIntegerTopValue();
                break;
            }

            case I_STRIP: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                Aint words = pop(stack);
                Aint count = pop(stack);
                Aint first = pop(stack);
                if (singleStepOption)
                    printf("STRIP \t%7ld, %7ld, %7ld, %7ld, %7ld", (long)first, (long)count, (long)words, (long)id, (long)atr);
                push(stack, strip(first, count, words, id, atr));
                traceStringTopValue();
                break;
            }

                /*------------------------------------------------------------
                  Aggregation
                  ------------------------------------------------------------*/
            case I_MIN:
            case I_SUM:
            case I_MAX: {
                Aint attribute = pop(stack);
                Aint loopIndex = pop(stack);
                Aint limit = pop(stack);
                Aint aggregate = pop(stack);
                switch (I_OP(i)) {
                case I_MAX:
                    if (singleStepOption)
                        printf("MAX \t%7ld\t\t\t", (long)attribute);
                    if (aggregate < attribute)
                        push(stack, attribute);
                    else
                        push(stack, aggregate);
                    break;
                case I_MIN:
                    if (singleStepOption)
                        printf("MIN \t%7ld\t\t\t", (long)attribute);
                    if (aggregate > attribute)
                        push(stack, attribute);
                    else
                        push(stack, aggregate);
                    break;
                case I_SUM:
                    if (singleStepOption)
                        printf("SUM \t%7ld\t\t\t", (long)attribute);
                    push(stack, aggregate + attribute);
                    break;
                }
                traceIntegerTopValue();
                push(stack, limit);
                push(stack, loopIndex);
                break;
            }
            case I_COUNT: {
                Aint loopIndex = pop(stack);
                Aint limit = pop(stack);
                if (singleStepOption)
                    printf("COUNT\t\t\t");
                push(stack, pop(stack) + 1);
                traceIntegerTopValue();
                push(stack, limit);
                push(stack, loopIndex);
                break;
            }
            case I_TRANSCRIPT: {
                Aint on_or_off = pop(stack);
                if (singleStepOption)
                    printf("TRANSCRIPT\t\t\t");
                if (on_or_off)
                    startTranscript();
                else
                    stopTranscript();
                break;
            }

                /*------------------------------------------------------------
                  Depending On
                  ------------------------------------------------------------*/
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
                Aword v = pop(stack);
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
                pop(stack);
                break;

            case I_ISA: {
                Aid rh = pop(stack);
                Aid lh = pop(stack);
                if (singleStepOption)
                    printf("ISA \t%7ld, %7ld\t", (long)lh, (long)rh);
                push(stack, isA(lh, rh));
                tracebooleanTopValue();
                break;
            }

            case I_FRAME: {
                Aint size = pop(stack);
                if (singleStepOption)
                    printf("FRAME \t%7ld\t\t\t\t\t", (long)size);
                newFrame(stack, size);
                break;
            }

            case I_GETLOCAL: {
                Aint framesBelow = pop(stack);
                Aint variableNumber = pop(stack);
                if (singleStepOption)
                    printf("GETLOCAL \t%7ld, %7ld\t", (long)framesBelow, (long)variableNumber);
                push(stack, getLocal(stack, framesBelow, variableNumber));
                traceIntegerTopValue();
                break;
            }

            case I_SETLOCAL: {
                Aint framesBelow = pop(stack);
                Aint variableNumber = pop(stack);
                Aint value = pop(stack);
                if (singleStepOption)
                    printf("SETLOCAL \t%7ld, %7ld, %7ld\t\t", (long)framesBelow, (long)variableNumber, (long)value);
                setLocal(stack, framesBelow, variableNumber, value);
                break;
            }

            case I_ENDFRAME: {
                if (singleStepOption)
                    printf("ENDFRAME\t\t\t\t\t\t");
                endFrame(stack);
                break;
            }

            case I_LOOP: {
                Aint index = pop(stack);
                Aint limit = pop(stack);
                if (singleStepOption)
                    printf("LOOP \t\t\t\t\t\t");
                push(stack, limit);
                push(stack, index);
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
                Aint index = pop(stack);
                Aint limit = pop(stack);
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
                    dumpStack(stack);
                skipStackDump = FALSE;
            }
            break;

        default:
            syserr("Unknown instruction class.");
            break;
        }
    }
 exitInterpreter:
    recursionDepth--;
}

/*======================================================================*/
Aword evaluate(Aaddr adr) {
    interpret(adr);
    return pop(stack);
}
