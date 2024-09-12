
/* advexe.c - adventure code executer */
/*
        Copyright (c) 1986, by David Michael Betz
        All rights reserved
*/

#include "header.h"
#include "advint.h"
#include "advdbs.h"

/* external variables */
extern char line[];
extern int nouns[],*adjectives[];

/* local variables */
int pc,opcode,p2,p3,sts;
int stack[STKSIZE],*sp,*fp,*top;
long rseed = 1L;

/* Function Prototypes.  Gosh */
void exe_one(void);
int getboperand();
int getwoperand();
void print(int msg);
int vowel(int msg) ;
void pnumber(int n);
int getrand(int n);
void setrand(long n);

/* execute - execute adventure code */
int execute(int code)
{
    /* setup initial program counter */
    if ((pc = code) == NIL)
        return (CHAIN);

    /* initialize */
    sp = fp = top = stack + STKSIZE;

    /* execute the code */
    for (sts = 0; sts == 0; )
        exe_one();

    return (sts);
}

/* exe_one - execute one instruction */
void exe_one()
{
    /* get the opcode */
    opcode = getcbyte(pc); pc++;

    /* execute the instruction */
    switch (opcode) {
    case OP_CALL:
                *--sp = getboperand();
                *--sp = pc;
                *--sp = (int)(top - fp);
                fp = sp;
                pc = getafield(fp[fp[2]+3],A_CODE);
                break;
    case OP_SEND:
                *--sp = getboperand();
                *--sp = pc;
                *--sp = (int)(top - fp);
                fp = sp;
                if (p2 = fp[fp[2]+3])
                    p2 = getofield(p2,O_CLASS);
                else
                    p2 = fp[fp[2]+2];
                if (p2 && (p2 = getp(p2,fp[fp[2]+1]))) {
                    pc = getafield(p2,A_CODE);
                    break;
                }
                *sp = NIL;
                /* return NIL if there is no method for this message */
    case OP_RETURN:
                if (fp == top)
                    sts = CHAIN;
                else {
                    p2 = *sp;
                    sp = fp;
                    fp = top - *sp++;
                    pc = *sp++;
                    p3 = *sp++;
                    sp += p3;
                    *sp = p2;
                }
                break;
    case OP_TSPACE:
                sp -= getboperand();
                break;
    case OP_TMP:
                p2 = getboperand();
                *sp = fp[-p2-1];
                break;
    case OP_TSET:
                p2 = getboperand();
                fp[-p2-1] = *sp;
                break;
    case OP_ARG:
                p2 = getboperand();
                if (p2 >= fp[2])
                    error("too few arguments");
                *sp = fp[p2+3];
                break;
    case OP_ASET:
                p2 = getboperand();
                if (p2 >= fp[2])
                    error("too few arguments");
                fp[p2+3] = *sp;
                break;
    case OP_BRT:
                pc = (*sp ? getwoperand() : pc+2);
                break;
    case OP_BRF:
                pc = (*sp ? pc+2 : getwoperand());
                break;
    case OP_BR:
                pc = getwoperand();
                break;
    case OP_T:
                *sp = T;
                break;
    case OP_NIL:
                *sp = NIL;
                break;
    case OP_PUSH:
                *--sp = NIL;
                break;
    case OP_NOT:
                *sp = (*sp ? NIL : T);
                break;
    case OP_ADD:
                p2 = *sp++;
                *sp += p2;
                break;
    case OP_SUB:
                p2 = *sp++;
                *sp -= p2;
                break;
    case OP_MUL:
                p2 = *sp++;
                *sp *= p2;
                break;
    case OP_DIV:
                p2 = *sp++;
                *sp = (p2 == 0 ? 0 : *sp / p2);
                break;
    case OP_REM:
                p2 = *sp++;
                *sp = (p2 == 0 ? 0 : *sp % p2);
                break;
    case OP_BAND:
                p2 = *sp++;
                *sp &= p2;
                break;
    case OP_BOR:
                p2 = *sp++;
                *sp |= p2;
                break;
    case OP_BNOT:
                *sp = ~*sp;
                break;
    case OP_LT:
                p2 = *sp++;
                *sp = (*sp < p2 ? T : NIL);
                break;
    case OP_EQ:
                p2 = *sp++;
                *sp = (*sp == p2 ? T : NIL);
                break;
    case OP_GT:
                p2 = *sp++;
                *sp = (*sp > p2 ? T : NIL);
                break;
    case OP_LIT:
                *sp = getwoperand();
                break;
    case OP_SPLIT:
                *sp = getboperand();
                break;
    case OP_SNLIT:
                *sp = -getboperand();
                break;
    case OP_VAR:
                *sp = getvalue(getwoperand());
                break;
    case OP_SVAR:
                *sp = getvalue(getboperand());
                break;
    case OP_SET:
                setvalue(getwoperand(),*sp);
                break;
    case OP_SSET:
                setvalue(getboperand(),*sp);
                break;
    case OP_GETP:
                p2 = *sp++;
                *sp = getp(*sp,p2);
                break;
    case OP_SETP:
                p3 = *sp++;
                p2 = *sp++;
                *sp = setp(*sp,p2,p3);
                break;
    case OP_PRINT:
                print(*sp);
                break;
    case OP_PNUMBER:
                pnumber(*sp);
                break;
    case OP_PNOUN:
                show_noun(*sp);
                break;
    case OP_TERPRI:
                trm_chr('\n');
                break;
    case OP_FINISH:
                sts = FINISH;
                break;
    case OP_CHAIN:
                sts = CHAIN;
                break;
    case OP_ABORT:
                sts = ABORT;
                break;
    case OP_EXIT:
                trm_done();
                glk_exit();
                break;
    case OP_YORN:
                trm_get(line);
                *sp = (line[0] == 'Y' || line[0] == 'y' ? T : NIL);
                break;
    case OP_CLASS:
                *sp = getofield(*sp,O_CLASS);
                break;
    case OP_MATCH:
                p2 = *sp++;
                *sp = (match(*sp,nouns[p2-1],adjectives[p2-1]) ? T : NIL);
                break;
    case OP_SAVE:
                if ((*sp = db_save()) == NIL)     // VB: added if
                   trm_str("Sorry, I couldn't save to this file.\n");
                break;
    case OP_RESTORE:
                if ((*sp = db_restore()) == NIL)  // VB: added if
                   trm_str("Sorry, I couldn't restore from this file.\n");
                break;
    case OP_RESTART:
                *sp = db_restart();
                break;
    case OP_RAND:
                *sp = getrand(*sp);
                break;
    case OP_RNDMIZE:
                setrand(time(0L));
                *sp = NIL;
                break;
    case OP_VOWEL:                 // patch for smart articles by MRP
                *sp = vowel(*sp);  // patch for smart articles by MRP
                break;             // patch for smart articles by MRP
    default:
            if (opcode >= OP_XVAR && opcode < OP_XSET)
                *sp = getvalue(opcode - OP_XVAR);
            else if (opcode >= OP_XSET && opcode < OP_XPLIT)
                setvalue(opcode - OP_XSET,*sp);
            else if (opcode >= OP_XPLIT && opcode < OP_XNLIT)
                *sp = opcode - OP_XPLIT;
            else if (opcode >= OP_XNLIT && opcode < 256)
                *sp = OP_XNLIT - opcode;
            else
                trm_str("Bad opcode\n");
            break;
    }
}

/* getboperand - get data byte */
int getboperand()
{
    int data;
    data = getcbyte(pc); pc += 1;
    return (data);
}

/* getwoperand - get data word */
int getwoperand()
{
    int data;
    data = getcword(pc); pc += 2;
    return (data);
}

/* print - print a message */
void print(int msg)
{
    int ch;

    msg_open(msg);
    while (ch = msg_byte())
        trm_chr(ch);
}

/* vowel - check for vowel */      // patch for smart articles by MRP
int vowel(int msg)                         // patch for smart articles by MRP
{                                  // patch for smart articles by MRP
    int ch,r;                      // patch for smart articles by MRP
                                   // patch for smart articles by MRP
    msg_open(msg);                 // patch for smart articles by MRP
    ch = msg_byte();               // patch for smart articles by MRP
    if (ch=='a'||ch=='e'||ch=='i'||ch=='o'||ch=='u')  // patch by MRP
      r = T;                       // patch for smart articles by MRP
    else                           // patch for smart articles by MRP
      r = NIL;                     // patch for smart articles by MRP
    while (ch)                     // patch for smart articles by MRP
      ch = msg_byte();             // patch for smart articles by MRP
    return (r);                    // patch for smart articles by MRP
}                                  // patch for smart articles by MRP

/* pnumber - print a number */
void pnumber(int n)
{
    char buf[10];

    sprintf(buf,"%d",n);
    trm_str(buf);
}

/* getrand - get a random number between 0 and n-1 */
int getrand(int n)
{
    long k1;

    /* make sure we don't get stuck at zero */
    if (rseed == 0L) rseed = 1L;

    /* algorithm taken from Dr. Dobbs Journal, November 1985, page 91 */
    k1 = rseed / 127773L;
    if ((rseed = 16807L * (rseed - k1 * 127773L) - k1 * 2836L) < 0L)
        rseed += 2147483647L;

    /* return a random number between 0 and n-1 */
    return ((int)(rseed % (long)n));
}

/* setrand - set the random number seed */
void setrand(long n)
{
    rseed = n;
}

