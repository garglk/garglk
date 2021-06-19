/*----------------------------------------------------------------------*\

inter.c

Interpreter unit for Alan interpreter Arun

\*----------------------------------------------------------------------*/
#include "inter.h"


#include <stdio.h>
#include <stdarg.h>

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
#include "compatibility.h"

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

/* Indicates if the result "field" has been printed, so that we know
 * if we need to advance over it */
static bool resultTraced = false;

/* Format for all trace lines:
   - address
   - instruction name
   - 3 parameters
   - result field
   - stack dump

   All traceInstruction-functions are responsible to move to beginning
   of result field. The single traceResult() function will print there,
   and outputs (like print and system) will move over it if necessary, as
   will the stack dump function.
 */
static char *format = "%-11s %7s %7s %7s %7s";
/*----------------------------------------------------------------------*/
static void traceInstruction0(char *name) {
    if (!traceInstructionOption)
        return;

    printf(format, name, "", "", "", "");

}

/*----------------------------------------------------------------------*/
static void traceInstruction1(char *name, Aword p1) {
    char buf1[100];

    if (!traceInstructionOption)
        return;

    sprintf(buf1, "%6d ", p1);

    printf(format, name, buf1, "", "", "");
}

/*----------------------------------------------------------------------*/
static void traceInstruction1s(char *name, char *p1) {
    if (!traceInstructionOption)
        return;

    printf(format, name, p1, "", "", "");
}

/*----------------------------------------------------------------------*/
static void traceInstruction1R(char *name, Aword p1, Aword result) {
    char buf1[100];

    if (!traceInstructionOption)
        return;

    sprintf(buf1, "%6d ", p1);

    printf(format, name, buf1, "", "", "");
    printf("=%-7d ", result);
    resultTraced = true;
}

/*----------------------------------------------------------------------*/
static void traceInstruction2(char *name, Aword p1, Aword p2) {
    char buf1[100];
    char buf2[100];

    if (!traceInstructionOption)
        return;

    sprintf(buf1, "%6d,", p1);
    sprintf(buf2, "%6d ", p2);

    printf(format, name, buf1, buf2, "", "");
}


/*----------------------------------------------------------------------*/
static void traceInstruction2s(char *name, char *p1, char *p2) {
    if (!traceInstructionOption)
        return;
    printf(format, name, p1, p2, "", "");
}


/*----------------------------------------------------------------------*/
static void traceInstruction2ds(char *name, Aword p1, char *p2) {
    char buf1[100];

    if (!traceInstructionOption)
        return;

    sprintf(buf1, "%6d ", p1);
    printf(format, name, buf1, p2, "", "");
}

/*----------------------------------------------------------------------*/
static void traceInstruction2sd(char *name, char *p1, Aword p2) {
    char buf2[100];

    if (!traceInstructionOption)
        return;

    sprintf(buf2, "%6d ", p2);
    printf(format, name, p1, buf2, "", "");
}

/*----------------------------------------------------------------------*/
static void traceInstruction3(char *name, Aword p1, Aword p2, Aword p3) {
    char buf1[100];
    char buf2[100];
    char buf3[100];

    if (!traceInstructionOption)
        return;

    sprintf(buf1, "%6d,", p1);
    sprintf(buf2, "%6d,", p2);
    sprintf(buf3, "%6d ", p3);

    printf(format, name, buf1, buf2, buf3, "");
}


/*----------------------------------------------------------------------*/
static void traceInstruction3dds(char *name, Aword p1, Aword p2, char *p3) {
    char buf1[100];
    char buf2[100];

    if (!traceInstructionOption)
        return;

    sprintf(buf1, "%6d,", p1);
    sprintf(buf2, "%6d,", p2);

    printf(format, name, buf1, buf2, p3, "");
}


/*----------------------------------------------------------------------*/
static void traceInstruction5(char *name, Aword p1, Aword p2, Aword p3, Aword p4, Aword p5) {
    char buf1[100];
    char buf2[100];
    char buf3[100];
    char buf4[100];
    char buf5[100];

    if (!traceInstructionOption)
        return;

    sprintf(buf1, "%6d,", p1);
    sprintf(buf2, "%6d,", p2);
    sprintf(buf3, "%6d,", p3);
    sprintf(buf4, "%6d,", p4);
    sprintf(buf5, "%6d ", p5);

    printf(format, name, buf1, buf2, buf3, buf4, buf5);
}


/*----------------------------------------------------------------------*/
static void moveToStackTraceField() {
    if (!resultTraced) {
        printf("%11s", "");       /* Move past result field */
        resultTraced = true;
    }
}

/*----------------------------------------------------------------------*/
static void traceStack(Stack theStack) {
    moveToStackTraceField();
    dumpStack(theStack);
}


/*----------------------------------------------------------------------*/
static void traceOutput(char *leading) {
    moveToStackTraceField();
    printf("%s", leading);
}

/*----------------------------------------------------------------------*/
static void traceEndOfOutput(char *terminator) {
    printf("%s", terminator);
    if (traceStackOption) {
        printf("\n      ");
        printf(format, "", "", "", "", "");
        resultTraced = false;
        moveToStackTraceField();
    }
}

/*----------------------------------------------------------------------*/
static void tracePC(int pc) {
    if (traceInstructionOption)
        printf("\n%4x: ", pc);
    resultTraced = false;
}

static bool skipStackDump = FALSE; /* Need to be able to skip it for some outputs */

/*----------------------------------------------------------------------*/
static void traceSkip() {
    if (traceInstructionOption) {
        printf("\n    :");
        skipStackDump = true;
    }
}

/*----------------------------------------------------------------------*/
static void interpretIf(Aword v)
{
    int lev = 1;
    Aword i;

    if (!v) {
        /* Skip to next ELSE or ENDIF on same level */
        traceSkip();
        while (TRUE) {
            i = memory[pc++];
            if (I_CLASS(i) == (Aword)C_STMOP)
                switch (I_OP(i)) {
                case I_ELSE:
                    if (lev == 1) {
                        tracePC(pc);
                        traceInstruction0("ELSE");
                        return;
                    }
                    break;
                case I_IF:
                    lev++;
                    break;
                case I_ENDIF:
                    lev--;
                    if (lev == 0) {
                        tracePC(pc);
                        traceInstruction0("ENDIF");
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

    traceSkip();
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

    traceSkip();
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

    traceSkip();
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
        tracePC(pc);
        traceInstruction0("LOOP");
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
        if (traceInstructionOption) printf("\n    : ");
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
                        if (traceInstructionOption)
                            printf("\n%4x: ENDDEP", pc);
                        return;
                    } else
                        lev--;
                    break;
                case I_DEPCASE:
                    instructionString = "DEPCASE";
                case I_DEPELSE:
                    if (lev == 1) {
                        if (traceInstructionOption)
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

    if (traceInstructionOption) printf("\n    : ");
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
    if (value) return "TRUE";
    else return "FALSE";
}

/*----------------------------------------------------------------------*/
static char *stringValue(Aptr address) {
    static char string[1000];

    sprintf(string, "0x%lx (\"%s\")\t\t", (unsigned long) address, (char *)fromAptr(address));
    return string;
}

/*----------------------------------------------------------------------*/
static char *pointerValue(Aptr address) {
    static char string[100];

    sprintf(string, "@%lx ",(unsigned long) address);
    return string;
}

/*----------------------------------------------------------------------*/
static void traceStringTopValue() {
    if (traceInstructionOption) {
        printf("=%10s", stringValue(top(stack)));
        resultTraced = true;
    }
}

/*----------------------------------------------------------------------*/
static void traceBooleanTopValue() {
    if (traceInstructionOption) {
        printf("=%-10s", top(stack)?"TRUE":"FALSE");
        resultTraced = true;
    }
}

/*----------------------------------------------------------------------*/
static void traceIntegerTopValue() {
    if (traceInstructionOption) {
        printf("=%-10d", top(stack));
        resultTraced = true;
    }
}

/*----------------------------------------------------------------------*/
static void tracePointerTopValue() {
    if (traceInstructionOption) {
        printf("=%-10s", pointerValue(top(stack)));
        resultTraced = true;
    }
}

/*----------------------------------------------------------------------*/
static void traceInstanceTopValue() {
    if (traceInstructionOption) {
        printf("=%ld ('", (long)top(stack));
        traceSay(top(stack));
        printf("')");
        resultTraced = true;
        if (traceStackOption) {
            /* TODO Move to stack field on next line */
            printf("\n      ");
            printf(format, "", "", "", "");
            printf("%10s", "");
        }
    }
}


/*----------------------------------------------------------------------*/
static char *transitivityFlag(ATrans value) {
    switch (value) {
    case TRANSITIVE:
        return "Trans.";
    case DIRECT:
        return "Direct";
    case INDIRECT:
        return "Indir.";
    }
    syserr("Unexpected transitivity");
    return "ERROR";
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


/*----------------------------------------------------------------------*/
static bool stillOnSameLine(Aint line, Aint file) {
    return line != current.sourceLine || file != current.sourceFile;
}

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

    if (traceInstructionOption)
        printf("\n++++++++++++++++++++++++++++++++++++++++++++++++++");

    oldpc = pc;
    pc = adr;
    while(TRUE) {
        if (pc > memTop)
            syserr("Interpreting outside program.");

        i = memory[pc++];

        switch (I_CLASS(i)) {
        case C_CONST:
            if (tracePushOption) {
                tracePC(pc-1);
                traceInstruction1("PUSH", I_OP(i));
            }
            push(stack, I_OP(i));
            if (tracePushOption && traceStackOption)
                traceStack(stack);
            break;
        case C_CURVAR:
            tracePC(pc-1);
            switch (I_OP(i)) {
            case V_PARAM:
                traceInstruction1("PARAM", top(stack));
                push(stack, globalParameters[pop(stack)-1].instance);
                traceIntegerTopValue();
                break;
            case V_CURLOC:
                traceInstruction0("CURLOC");
                push(stack, current.location);
                traceIntegerTopValue();
                break;
            case V_CURACT:
                traceInstruction0("CURACT");
                push(stack, current.actor);
                traceIntegerTopValue();
                break;
            case V_CURVRB:
                traceInstruction0("CURVRB");
                push(stack, current.verb);
                traceIntegerTopValue();
                break;
            case V_CURRENT_INSTANCE:
                traceInstruction0("CURINS");
                push(stack, current.instance);
                traceIntegerTopValue();
                break;
            case V_SCORE:
                traceInstruction0("CURSCORE");
                push(stack, current.score);
                traceIntegerTopValue();
                break;
            case V_MAX_INSTANCE: {
                int instanceMax = isPreBeta3(header->version)?header->instanceMax:header->instanceMax-1;
                traceInstruction0("MAXINSTANCE");
                push(stack, instanceMax);
                traceIntegerTopValue();
                break;
            }
            default:
                syserr("Unknown CURVAR instruction.");
                break;
            }
            if (traceStackOption)
                traceStack(stack);
            break;

        case C_STMOP:
            tracePC(pc-1);
            switch (I_OP(i)) {

            case I_DUP:
                traceInstruction0("DUP");
                stackDup();
                break;

            case I_DUPSTR:
                traceInstruction0("DUPSTR");
                push(stack, toAptr(strdup((char*)fromAptr(top(stack)))));
                break;

            case I_POP: {
                Aptr top = pop(stack);
                traceInstruction1("POP", top);
                break;
            }

            case I_LINE: {
                Aint line = pop(stack);
                Aint file = pop(stack);
                traceInstruction2("LINE", file, line);
                if (traceStackOption)
                    traceStack(stack);
                skipStackDump = TRUE;
                if (line != 0) {
                    bool atNext = stopAtNextLine && line != current.sourceLine;
                    bool atBreakpoint =  breakpointIndex(file, line) != -1;
                    if (traceSourceOption && stillOnSameLine(line, file)) {
                        if (col != 1 || traceInstructionOption)
                            printf("\n");
                        showSourceLine(file, line);
                        if (!traceInstructionOption)
                            printf("\n");
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
                if (traceInstructionOption) {
                    traceInstruction2("PRINT", fpos, len);
                    col = 41;		/* To break lines better! */
                    traceOutput("\"");
                }
                print(fpos, len);
                if (traceInstructionOption) {
                    traceEndOfOutput("\"");
                }
                break;
            }

            case I_STYLE: {
                Aint style = pop(stack);
                traceInstruction1("STYLE", style);
                setStyle(style);
                break;
            }

            case I_SYSTEM: {
                Aint fpos = pop(stack);
                Aint len = pop(stack);
                if (traceInstructionOption) {
                    traceInstruction2("SYSTEM", fpos, len);
                    col = 34;		/* To format possible output better! */
                    traceOutput("\"");
                }
                sys(fpos, len);
                if (traceInstructionOption)
                    printf("\"\t\t\t\t\t\t");
                break;
            }

            case I_GETSTR: {
                Aint fpos = pop(stack);
                Aint len = pop(stack);
                traceInstruction2("GETSTR", fpos, len);
                push(stack, toAptr(getStringFromFile(fpos, len)));
                traceStringTopValue();
                break;
            }

            case I_QUIT: {
                traceInstruction0("QUIT");
                quitGame();
                break;
            }
            case I_LOOK: {
                traceInstruction0("LOOK");
                look();
                break;
            }
            case I_SAVE: {
                traceInstruction0("SAVE");
                save();
                break;
            }
            case I_RESTORE: {
                traceInstruction0("RESTORE");
                restore();
                break;
            }
            case I_RESTART: {
                traceInstruction0("RESTART");
                restartGame();
                break;
            }

            case I_SCORE: {
                Aint sc = pop(stack);
                traceInstruction1R("SCORE", sc, scores[sc-1]);
                score(sc);
                break;
            }
            case I_VISITS: {
                Aint v = pop(stack);
                traceInstruction1("VISITS", v);
                visits(v);
                break;
            }

            case I_LIST: {
                Aint cnt = pop(stack);
                traceInstruction1("LIST", cnt);
                list(cnt);
                break;
            }
            case I_EMPTY: {
                Aint cnt = pop(stack);
                Aint whr = pop(stack);
                traceInstruction2("EMPTY", cnt, whr);
                empty(cnt, whr);
                break;
            }
            case I_SCHEDULE: {
                Aint event = pop(stack);
                Aint where = pop(stack);
                Aint after = pop(stack);
                traceInstruction3("SCHEDULE", event, where, after);
                schedule(event, where, after);
                break;
            }
            case I_CANCEL: {
                Aint event = pop(stack);
                traceInstruction1("CANCEL", event);
                cancelEvent(event);
                break;
            }
            case I_MAKE: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                Abool val = pop(stack);
                traceInstruction3dds("MAKE", id, atr, booleanValue(val));
                setInstanceAttribute(id, atr, val);
                break;
            }
            case I_SET: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                Aptr val = pop(stack);
                traceInstruction3("SET", id, atr, val);
                setInstanceAttribute(id, atr, val);
                break;
            }
            case I_SETSTR: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                Aptr str = pop(stack);
                traceInstruction3dds("SETSTR", id, atr, stringValue(str));
                setInstanceStringAttribute(id, atr, fromAptr(str));
                break;
            }
            case I_SETSET: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                Aptr set = pop(stack);
                traceInstruction3dds("SETSET", id, atr, pointerValue(set));
                setInstanceSetAttribute(id, atr, set);
                break;
            }
            case I_NEWSET: {
                Set *set = newSet(0);
                traceInstruction0("NEWSET");
                push(stack, toAptr(set));
                tracePointerTopValue();
                break;
            }
            case I_UNION: {
                Aptr set2 = pop(stack);
                Aptr set1 = pop(stack);
                traceInstruction2("UNION", set1, set2);
                push(stack, toAptr(setUnion((Set *)fromAptr(set1), (Set *)fromAptr(set2))));
                tracePointerTopValue();
                freeSet((Set *)fromAptr(set1));
                freeSet((Set *)fromAptr(set2));
                break;
            }
            case I_INCR: {
                Aint step = pop(stack);
                traceInstruction1("INCR", step);
                push(stack, pop(stack) + step);
                traceIntegerTopValue();
                break;
            }
            case I_DECR: {
                Aint step = pop(stack);
                traceInstruction1("DECR", step);
                push(stack, pop(stack) - step);
                traceIntegerTopValue();
                break;
            }
            case I_INCLUDE: {
                Aint member = pop(stack);
                traceInstruction1("INCLUDE", member);
                addToSet((Set *)fromAptr(top(stack)), member);
                break;
            }
            case I_EXCLUDE: {
                Aint member = pop(stack);
                traceInstruction1("EXCLUDE", member);
                removeFromSet((Set *)fromAptr(top(stack)), member);
                break;
            }
            case I_SETSIZE: {
                Aptr ptr = pop(stack);
                Set *set = (Set *)fromAptr(ptr);
                traceInstruction1s("SETSIZE", pointerValue(ptr));
                push(stack, setSize(set));
                traceIntegerTopValue();
                break;
            }
            case I_SETMEMB: {
                Aptr ptr = pop(stack);
                Set *set = (Set *)fromAptr(ptr);
                Aint index = pop(stack);
                traceInstruction2sd("SETMEMB", pointerValue(ptr), index);
                push(stack, getSetMember(set, index));
                traceIntegerTopValue();
                break;
            }
            case I_CONTSIZE: {
                Abool transitivity = pop(stack);
                Aint container = pop(stack);
                traceInstruction2ds("CONTSIZE", container, transitivityFlag(transitivity));
                push(stack, containerSize(container, transitivity));
                if (traceInstructionOption)
                    traceIntegerTopValue();
                break;
            }
            case I_CONTMEMB: {
                Abool transitivity = pop(stack);
                Aint container = pop(stack);
                Aint index = pop(stack);
                traceInstruction3dds("CONTMEMB", container, index, transitivityFlag(transitivity));
                push(stack, getContainerMember(container, index, transitivity));
                traceIntegerTopValue();
                break;
            }
            case I_ATTRIBUTE: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                traceInstruction2("ATTRIBUTE", id, atr);
                push(stack, getInstanceAttribute(id, atr));
                traceIntegerTopValue();
                break;
            }
            case I_ATTRSTR: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                traceInstruction2("ATTRSTR", id, atr);
                push(stack, toAptr(getInstanceStringAttribute(id, atr)));
                traceStringTopValue();
                break;
            }
            case I_ATTRSET: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                traceInstruction2("ATTRSET", id, atr);
                push(stack, toAptr(getInstanceSetAttribute(id, atr)));
                tracePointerTopValue();
                break;
            }
            case I_SHOW: {
                Aint image = pop(stack);
                Aint align = pop(stack);
                traceInstruction2("SHOW", image, align);
                showImage(image, align);
                break;
            }
            case I_PLAY: {
                Aint sound = pop(stack);
                traceInstruction1("PLAY", sound);
                playSound(sound);
                break;
            }
            case I_LOCATE: {
                Aid id = pop(stack);
                Aint whr = pop(stack);
                traceInstruction2("LOCATE", id, whr);
                locate(id, whr);
                break;
            }
            case I_WHERE: {
                Abool transitivity = pop(stack);
                Aid id = pop(stack);
                traceInstruction2ds("WHERE", id, transitivityFlag(transitivity));
                push(stack, where(id, transitivity));
                traceInstanceTopValue();
                break;
            }
            case I_LOCATION: {
                Aid id = pop(stack);
                traceInstruction1("LOCATION", id);
                push(stack, locationOf(id));
                traceInstanceTopValue();
                break;
            }
            case I_HERE: {
                Abool transitivity = pop(stack);
                Aid id = pop(stack);
                traceInstruction2ds("HERE", id, transitivityFlag(transitivity));
                push(stack, isHere(id, transitivity));
                traceBooleanTopValue();
                break;
            }
            case I_NEARBY: {
                Abool transitivity = pop(stack);
                Aid id = pop(stack);
                traceInstruction2ds("NEARBY", id, transitivityFlag(transitivity));
                push(stack, isNearby(id, transitivity));
                traceBooleanTopValue();
                break;
            }
            case I_NEAR: {
                Abool transitivity = pop(stack);
                Aint other = pop(stack);
                Aid id = pop(stack);
                traceInstruction3dds("NEAR", id, other, transitivityFlag(transitivity));
                push(stack, isNear(id, other, transitivity));
                traceBooleanTopValue();
                break;
            }
            case I_AT: {
                Abool transitivity = pop(stack);
                Aint other = pop(stack);
                Aint instance = pop(stack);
                traceInstruction3dds("AT", instance, other, transitivityFlag(transitivity));
                push(stack, isAt(instance, other, transitivity));
                traceBooleanTopValue();
                break;
            }
            case I_IN: {
                Abool transitivity = pop(stack);
                Aint cnt = pop(stack);
                Aint obj = pop(stack);
                traceInstruction3dds("IN", obj, cnt, transitivityFlag(transitivity));
                push(stack, isIn(obj, cnt, transitivity));
                traceBooleanTopValue();
                break;
            }
            case I_INSET: {
                Aptr set = pop(stack);
                Aword element = pop(stack);
                traceInstruction2("INSET", element, set);
                push(stack, inSet((Set*)fromAptr(set), element));
                freeSet((Set *)fromAptr(set));
                traceBooleanTopValue();
                break;
            }
            case I_USE: {
                Aid act = pop(stack);
                Aint scr = pop(stack);
                traceInstruction2("USE", act, scr);
                use(act, scr);
                break;
            }
            case I_STOP: {
                Aid actor = pop(stack);
                traceInstruction1("STOP", actor);
                stop(actor);
                break;
            }
            case I_DESCRIBE: {
                Aid id = pop(stack);
                if (traceInstructionOption) {
                    traceInstruction1("DESCRIBE", id);
                    col = 41;		/* To format output better! */
                }
                describe(id);
                break;
            }
            case I_SAY: {
                Aint form = pop(stack);
                Aid id = pop(stack);
                traceInstruction2sd("SAY", printForm(form), id);
                if (form == SAY_SIMPLE)
                    say(id);
                else
                    sayForm(id, form);
                if (traceInstructionOption)
                    traceEndOfOutput("\"");
                break;
            }
            case I_SAYINT: {
                Aword val = pop(stack);
                traceInstruction1("SAYINT", val);
                sayInteger(val);
                if (traceInstructionOption)
                    traceEndOfOutput("\"");
                break;
            }
            case I_SAYSTR: {
                Aptr adr = pop(stack);
                traceInstruction1("SAYSTR", adr);
                sayString((char *)fromAptr(adr));
                if (traceInstructionOption)
                    traceEndOfOutput("\"");
                break;
            }
            case I_IF: {
                Aword v = pop(stack);
                traceInstruction1s("IF", booleanValue(v));
                interpretIf(v);
                break;
            }
            case I_ELSE: {
                traceInstruction0("ELSE");
                interpretElse();
                break;
            }
            case I_ENDIF: {
                traceInstruction0("ENDIF");
                break;
            }
            case I_AND: {
                Aword rh = pop(stack);
                Aword lh = pop(stack);
                traceInstruction2s("AND", booleanValue(lh), booleanValue(rh));
                push(stack, lh && rh);
                traceBooleanTopValue();
                break;
            }
            case I_OR: {
                Aword rh = pop(stack);
                Aword lh = pop(stack);
                traceInstruction2s("OR", booleanValue(lh), booleanValue(rh));
                push(stack, lh || rh);
                traceBooleanTopValue();
                break;
            }
            case I_NE: {
                Aword rh = pop(stack);
                Aword lh = pop(stack);
                traceInstruction2("NE", lh, rh);
                push(stack, lh != rh);
                traceBooleanTopValue();
                break;
            }
            case I_EQ: {
                Aword rh = pop(stack);
                Aword lh = pop(stack);
                traceInstruction2("EQ", lh, rh);
                push(stack, lh == rh);
                traceBooleanTopValue();
                break;
            }
            case I_STREQ: {
                Aptr rh = pop(stack);
                Aptr lh = pop(stack);
                traceInstruction2("STREQ", lh, rh);
                push(stack, streq((char *)fromAptr(lh), (char *)fromAptr(rh)));
                traceBooleanTopValue();
                deallocate(fromAptr(lh));
                deallocate(fromAptr(rh));
                break;
            }
            case I_STREXACT: {
                Aptr rh = pop(stack);
                Aptr lh = pop(stack);
                traceInstruction2("STREXACT", lh, rh);
                push(stack, strcmp((char *)fromAptr(lh), (char *)fromAptr(rh)) == 0);
                traceBooleanTopValue();
                deallocate(fromAptr(lh));
                deallocate(fromAptr(rh));
                break;
            }
            case I_LE: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                traceInstruction2("LE", lh, rh);
                push(stack, lh <= rh);
                traceBooleanTopValue();
                break;
            }
            case I_GE: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                traceInstruction2("GE", lh, rh);
                push(stack, lh >= rh);
                traceBooleanTopValue();
                break;
            }
            case I_LT: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                traceInstruction2("LT", lh, rh);
                push(stack, lh < rh);
                traceBooleanTopValue();
                break;
            }
            case I_GT: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                traceInstruction2("GT", lh, rh);
                push(stack, lh > rh);
                traceBooleanTopValue();
                break;
            }
            case I_PLUS: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                traceInstruction2("PLUS", lh, rh);
                push(stack, lh + rh);
                traceIntegerTopValue();
                break;
            }
            case I_MINUS: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                traceInstruction2("MINUS", lh, rh);
                push(stack, lh - rh);
                traceIntegerTopValue();
                break;
            }
            case I_MULT: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                traceInstruction2("MULT", lh, rh);
                push(stack, lh * rh);
                traceIntegerTopValue();
                break;
            }
            case I_DIV: {
                Aint rh = pop(stack);
                Aint lh = pop(stack);
                traceInstruction2("DIV", lh, rh);
                if (rh == 0)
                    apperr("Division by zero");
                push(stack, lh / rh);
                traceIntegerTopValue();
                break;
            }
            case I_NOT: {
                Aword val = pop(stack);
                traceInstruction1s("NOT", booleanValue(val));
                push(stack, !val);
                traceBooleanTopValue();
                break;
            }
            case I_RND: {
                Aint from = pop(stack);
                Aint to = pop(stack);
                traceInstruction2("RANDOM", from, to);
                push(stack, randomInteger(from, to));
                traceIntegerTopValue();
                break;
            }
            case I_BTW: {
                Aint high = pop(stack);
                Aint low = pop(stack);
                Aint val = pop(stack);
                traceInstruction3("BETWEEN", val, low, high);
                push(stack, between(val, low, high));
                traceIntegerTopValue();
                break;
            }

                /*------------------------------------------------------------* \
                  String functions
                \*------------------------------------------------------------*/
            case I_CONCAT: {
                Aptr s2 = pop(stack);
                Aptr s1 = pop(stack);
                traceInstruction2s("CONCAT", pointerValue(s1), pointerValue(s2));
                push(stack, concat(s1, s2));
                traceStringTopValue();
                deallocate(fromAptr(s1));
                deallocate(fromAptr(s2));
                break;
            }

            case I_CONTAINS: {
                Aptr substring = pop(stack);
                Aptr string = pop(stack);
                traceInstruction2s("CONTAINS", pointerValue(string), pointerValue(substring));
                push(stack, contains(string, substring));
                traceIntegerTopValue();
                deallocate(fromAptr(string));
                deallocate(fromAptr(substring));
                break;
            }

            case I_STRIP: {
                Aint atr = pop(stack);
                Aid id = pop(stack);
                Aint words = pop(stack);
                Aint count = pop(stack);
                Aint first = pop(stack);
                traceInstruction5("STRIP", first, count, words, id, atr);
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
                    traceInstruction1("MAX", attribute);
                    if (aggregate < attribute)
                        push(stack, attribute);
                    else
                        push(stack, aggregate);
                    break;
                case I_MIN:
                    traceInstruction1("MIN", attribute);
                    if (aggregate > attribute)
                        push(stack, attribute);
                    else
                        push(stack, aggregate);
                    break;
                case I_SUM:
                    traceInstruction1("SUM", attribute);
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
                traceInstruction0("COUNT");
                push(stack, pop(stack) + 1);
                traceIntegerTopValue();
                push(stack, limit);
                push(stack, loopIndex);
                break;
            }
            case I_TRANSCRIPT: {
                Aint on_or_off = pop(stack);
                traceInstruction0("TRANSCRIPT");
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
                traceInstruction0("DEPEND");
                break;

            case I_DEPCASE:
                traceInstruction0("DEPCASE");
                depcase();
                break;

            case I_DEPEXEC: {
                traceInstruction0("DEPEXEC");
                traceBooleanTopValue();
                depexec(pop(stack));
                break;
            }

            case I_DEPELSE:
                traceInstruction0("DEPELSE");
                depcase();
                break;

            case I_ENDDEP:
                traceInstruction0("ENDDEP");
                pop(stack);
                break;

            case I_ISA: {
                Aid rh = pop(stack);
                Aid lh = pop(stack);
                traceInstruction2("ISA", lh, rh);
                push(stack, isA(lh, rh));
                traceBooleanTopValue();
                break;
            }

            case I_FRAME: {
                Aint size = pop(stack);
                traceInstruction1("FRAME", size);
                newFrame(stack, size);
                break;
            }

            case I_GETLOCAL: {
                Aint framesBelow = pop(stack);
                Aint variableNumber = pop(stack);
                traceInstruction2("GETLOCAL", framesBelow, variableNumber);
                push(stack, getLocal(stack, framesBelow, variableNumber));
                traceIntegerTopValue();
                break;
            }

            case I_SETLOCAL: {
                Aint framesBelow = pop(stack);
                Aint variableNumber = pop(stack);
                Aint value = pop(stack);
                traceInstruction3("SETLOCAL", framesBelow, variableNumber, value);
                setLocal(stack, framesBelow, variableNumber, value);
                break;
            }

            case I_ENDFRAME: {
                traceInstruction0("ENDFRAME");
                endFrame(stack);
                break;
            }

            case I_LOOP: {
                Aint index = pop(stack);
                Aint limit = pop(stack);
                traceInstruction0("LOOP");
                push(stack, limit);
                push(stack, index);
                if (index > limit)
                    goToLOOPEND();
                break;
            }

            case I_LOOPNEXT: {
                traceInstruction0("LOOPNEXT");
                nextLoop();
                break;
            }

            case I_LOOPEND: {
                Aint index = pop(stack);
                Aint limit = pop(stack);
                traceInstruction0("LOOPEND");
                endLoop(index, limit);
                break;
            }

            case I_RETURN:
                traceInstruction0("RETURN");
                if (traceInstructionOption)
                    printf("\n--------------------------------------------------\n");
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
                    traceStack(stack);
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
