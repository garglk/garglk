#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/PRSCOMP.C,v 1.3 1999/07/11 00:46:30 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1993, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  prscomp.c - parser functions needed only in compiler
Function
  Provides parser functions that are used only by the compiler
  (but not by the debugger).  Separated from prs.c so that the
  debugger doesn't need to link these functions unnecessarily.
Notes
  None
Modified
  02/07/93 MJRoberts - Creation
*/

#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "os.h"
#include "std.h"
#include "mcm.h"
#include "tok.h"
#include "prs.h"
#include "lst.h"
#include "prp.h"
#include "obj.h"
#include "opc.h"
#include "emt.h"
#include "mch.h"
#include "voc.h"

/* generate an OPCLINE instruction */
static void prsclin(prscxdef *ctx, uint curfr, lindef *lin,
                    int tellsrc, const uchar *loc);

/* parse an argument or local variable list */
static toktldef *prsvlst(prscxdef *ctx, toktldef *lcltab, prsndef **inits,
                         int *locals, int args, int *varargs, uint curfr)
{
    tokdef   *t;

    /* skip whatever token encouraged caller to parse a variable list */
    toknext(ctx->prscxtok);
    
    /* if caller doesn't already have a local table, allocate one now */
    if (!lcltab)
    {
        /* ensure there's enough space */
        if (ctx->prscxslcl < sizeof(toktldef) + 20)
            errsig(ctx->prscxerr, ERR_NOMEMLC);
        
        /* set up space for linear table and initialize it */
        lcltab = (toktldef *)osrndpt(ctx->prscxplcl);
        toktlini(ctx->prscxerr, lcltab, (uchar *)(lcltab + 1),
                 (int)ctx->prscxslcl - (uint)sizeof(toktldef));
        
        /* make new local symbol table the head of search list */
        lcltab->toktlsc.toktnxt = ctx->prscxtok->tokcxstab;
        ctx->prscxtok->tokcxstab = &lcltab->toktlsc;
    }

    for (;;)
    {
        int tnum;
        
        if (ctx->prscxtok->tokcxcur.toktyp != TOKTSYMBOL) break;
        
        /* add the symbol */
        ++(*locals);
        t = &ctx->prscxtok->tokcxcur;
        (*lcltab->toktlsc.toktfadd)((toktdef *)lcltab, t->toknam, t->toklen,
                                    TOKSTLOCAL, (args ? -(*locals) : *locals),
                                    t->tokhash);
            
        /* check for initializer if initializers are ok */
        if (((tnum = toknext(ctx->prscxtok)) == TOKTASSIGN || tnum == TOKTEQ)
            && inits)
        {
            prsndef *expr;

            toknext(ctx->prscxtok);                          /* skip the := */
            
            /* save the OPCLINE if necessary */
            if ((ctx->prscxflg & PRSCXFLIN)
                && !(ctx->prscxtok->tokcxlin->linflg & LINFDBG))
            {
                uint     oldofs = ctx->prscxemt->emtcxofs;       /* save PC */
                uchar   *linrec;
                prsndef *n4;
                tokdef   t2;
                uint     siz;

                /* gen the OPCLINE, and copy into temp storage in tree */
                prsclin(ctx, curfr, ctx->prscxtok->tokcxlin, FALSE, 0);
                siz = ctx->prscxemt->emtcxofs - oldofs;
                linrec = prsbalo(ctx, siz);
                memcpy(linrec, ctx->prscxemt->emtcxptr + oldofs,
                       (size_t)(ctx->prscxemt->emtcxofs - oldofs));
                
                /* now remove the OPCLINE - this isn't the place for it */
                ctx->prscxemt->emtcxofs = oldofs;

                /* allocate a token node to hold the line information */
                t2.tokofs = linrec - ctx->prscxpool;
                n4 = prsnew0(ctx, &t2);

                /* now build a four-way node with the line information */
                expr = prsxini(ctx);
                *inits = prsnew4(ctx, TOKTLOCAL, expr, prsnew0(ctx, t),
                                 *inits, n4);
            }
            else
            {
                expr = prsxini(ctx);               /* parse the initializer */
                *inits = prsnew3(ctx, TOKTLOCAL, expr,
                                 prsnew0(ctx, t), *inits);
            }
            (*inits)->prsnv.prsnvn[1]->prsnv.prsnvt.tokval = *locals;
        }

        /* if we don't have a comma, we're done */
        if (ctx->prscxtok->tokcxcur.toktyp != TOKTCOMMA) break;
        toknext(ctx->prscxtok);                           /* skip the comma */
    }
    
    /* check for varargs specifier */
    if (varargs && ctx->prscxtok->tokcxcur.toktyp == TOKTELLIPSIS)
    {
        *varargs = TRUE;
        toknext(ctx->prscxtok);                        /* skip the ellipsis */
    }
    
    /* update the local table pool pointers to follow the table */
    ctx->prscxplcl = lcltab->toktlnxt;
    ctx->prscxslcl = lcltab->toktlsiz;
    
    return(lcltab);
}

/* callback to generate one symbol's record in a debug frame table */
static void prsvgf0(void *ctx0, toksdef *sym)
{
    prscxdef *ctx = (prscxdef *)ctx0;
    emtcxdef *ec = ctx->prscxemt;

    /* write out value (offset from base ptr) and name of symbol */
    emtint2(ec, sym->toksval);
    emtbyte(ec, sym->tokslen);
    emtmem(ec, sym->toksnam, (size_t)sym->tokslen);
}

/* generate a local symbol table debug frame record, if debugging enabled */
static void prsvgfr(prscxdef *ctx, toktldef *lcltab, uint *encfr)
{
    if (ctx->prscxflg & PRSCXFLCL)
    {
        emtcxdef *ec = ctx->prscxemt;
        uint      ofs;
        uint      diff;

        /* write the FRAME instruction and operand header */
        emtop(ec, OPCFRAME);
        ofs = ec->emtcxofs;          /* remember the location of the length */
        emtint2(ec, 0);           /* placeholder - we'll fill this in later */
        emtint2(ec, *encfr);                /* point to the enclosing frame */
        
        /* write out the symbols using the callback iterator */
        toktleach(&lcltab->toktlsc, prsvgf0, ctx);
        
        /* now write the length of the table back at the beginning */
        diff = ctx->prscxemt->emtcxofs - ofs;
        emtint2at(ec, diff, ofs);
        
        /* the new table is now the current frame */
        *encfr = ofs;
    }
}

/* generate an OPCLINE instruction for the current line */
static void prsclin(prscxdef *ctx, uint curfr, lindef *lin, int tellsrc,
                    const uchar *saved_loc)
{
    int diff = FALSE;

    /* tell the line source the location of the line */
    if (tellsrc)
    {
        dbgclin(ctx->prscxtok, ctx->prscxemt->emtcxobj,
                (uint)ctx->prscxemt->emtcxofs);
    }
        
    /* emit a LINE instruction, noting location and frame */
    emtop(ctx->prscxemt, OPCLINE);
    emtbyte(ctx->prscxemt, lin->linlln + 4);       /* note length of record */
    emtint2(ctx->prscxemt, curfr);           /* store current frame pointer */
    emtbyte(ctx->prscxemt, lin->linid);              /* save line source ID */
    emtres(ctx->prscxemt, lin->linlln);        /* make room for source part */

    /* use the saved location, or the current location if there isn't one */
    if (saved_loc != 0)
    {
        uchar cur[LINLLNMAX];
        
        /* use the saved location */
        memcpy(ctx->prscxemt->emtcxptr + ctx->prscxemt->emtcxofs,
               saved_loc, lin->linlln);

        /* check to see if it's different from the current location */
        linglop(lin, cur);
        diff = memcmp(cur, saved_loc, lin->linlln);
    }
    else
    {
        /* no saved location - use the current location from the lindef */
        linglop(lin, ctx->prscxemt->emtcxptr + ctx->prscxemt->emtcxofs);
    }

    /* advance the output pointer */
    ctx->prscxemt->emtcxofs += lin->linlln;

    /* 
     *   indicate in line source that we've generated a debug record, as long
     *   as the generated location isn't different from current location (if
     *   it is, don't mark this line as used yet) 
     */
    if (!diff)
        ctx->prscxtok->tokcxlin->linflg |= LINFDBG;
}

/* parse a statement (simple or compound) */
void prsstm(prscxdef *ctx, uint brk, uint cont, int parms, int locals,
            uint entofs, prscsdef *swctl, uint curfr)
{
    tokdef         *t;
    noreg uint      ldone = EMTLLNKEND;
    noreg uint      lfalse = EMTLLNKEND;
    noreg uint      lloop = EMTLLNKEND;
    ushort          oldrrst = ctx->prscxrrst;
    uchar          *oldnrst = ctx->prscxnrst;
    uchar          *oldlclp = ctx->prscxplcl;
    uint            oldlcls = ctx->prscxslcl;
    toktdef        *oldltab = ctx->prscxtok->tokcxstab;
    int             compound = FALSE;
    uint            casecnt = 0;                      /* current case count */
    prsctdef       *noreg casetab = (prsctdef *)0;            /* case table */
    prsctdef       *curctab;
    lindef         *lin = ctx->prscxtok->tokcxlin;           /* line source */
    
    NOREG((&ldone, &lfalse, &lloop, &casetab))
    
    ERRBEGIN(ctx->prscxerr)
        
    /* if no 'enter' instruction, emit one */
    if (!entofs)
    {
        emtop(ctx->prscxemt, OPCENTER);     /* emit the 'enter' instruction */
        entofs = ctx->prscxemt->emtcxofs;         /* save offset of operand */
        emtint2(ctx->prscxemt, 0);                      /* no locals so far */
    }

    /* set up with first case table, if in a switch body */
    if (swctl) curctab = swctl->prscstab;

    /* see if this is going to be a compound statement... */
    if (ctx->prscxtok->tokcxcur.toktyp == TOKTLBRACE)
    {
        prsndef  *inits;
        toktldef *lcltab;

        compound = TRUE;          /* note that we have a compound statement */
        prsrstn(ctx);                                    /* reset node pool */
        toknext(ctx->prscxtok);                             /* skip the '{' */
        lcltab = (toktldef *)0;                /* no local symbol table yet */

        /* allow arbitrarily many consecutive 'local' statements */
        for (inits = (prsndef *)0 ;
             ctx->prscxtok->tokcxcur.toktyp == TOKTLOCAL ; )
        {
            lcltab = prsvlst(ctx, lcltab, &inits, &locals, FALSE, (int *)0,
                             curfr);
            prsreq(ctx, TOKTSEM);
        }
        if (lcltab) prsvgfr(ctx, lcltab, &curfr);

        /* adjust the 'enter' instruction to make room for new locals */
        if (lcltab && locals > emtint2from(ctx->prscxemt, entofs))
            emtint2at(ctx->prscxemt, locals, entofs);
        
        /* generate code for initializers, if any */
        if (inits) prsgini(ctx, inits, curfr);
    }

startover:
    do
    {
    /* generate a line record if debugging if we haven't already done so */
    if ((ctx->prscxflg & PRSCXFLIN)
        && !(ctx->prscxtok->tokcxlin->linflg & LINFDBG))
        prsclin(ctx, curfr, lin, TRUE, 0);

    switch (ctx->prscxtok->tokcxcur.toktyp)
    {
    case TOKTSEM:
        toknext(ctx->prscxtok);    /* skip the semicolon - end of statement */
        break;
        
    case TOKTRBRACE:
        if (compound)
        {
            compound = FALSE;     /* we're done with the compound statement */
            toknext(ctx->prscxtok);                         /* skip the '}' */
        }
        break;
        
    case TOKTLBRACE:
        prsstm(ctx, brk, cont, parms, locals, entofs, (prscsdef *)0, curfr);
        break;
        
    case TOKTDSTRING:
        prsxgen(ctx);
        prsreq(ctx, TOKTSEM);
        break;
        
    case TOKTIF:
        prsnreq(ctx, TOKTLPAR);
        lfalse = emtglbl(ctx->prscxemt);
        prsxgen_pia(ctx);
        emtjmp(ctx->prscxemt, OPCJF, lfalse);
        prsreq(ctx, TOKTRPAR);
        prsstm(ctx, brk, cont, parms, locals, entofs, (prscsdef *)0, curfr);
        
        /* in v1 compatibility mode, ignore ';' after 'if{...}' */
        if ((ctx->prscxflg & PRSCXFV1E)
            && ctx->prscxtok->tokcxcur.toktyp == TOKTSEM)
            toknext(ctx->prscxtok);

        if (ctx->prscxtok->tokcxcur.toktyp == TOKTELSE)
        {
            toknext(ctx->prscxtok);                      /* skip the 'else' */
            ldone = emtglbl(ctx->prscxemt);
            emtjmp(ctx->prscxemt, OPCJMP, ldone);
            emtslbl(ctx->prscxemt, &lfalse, TRUE);
            prsstm(ctx, brk, cont, parms, locals, entofs, (prscsdef *)0,
                   curfr);
            emtslbl(ctx->prscxemt, &ldone, TRUE);
        }
        else
            emtslbl(ctx->prscxemt, &lfalse, TRUE);             /* no 'else' */
        break;
        
    case TOKTWHILE:
        prsnreq(ctx, TOKTLPAR);
        ldone = emtglbl(ctx->prscxemt);
        lloop = emtgslbl(ctx->prscxemt);        /* loop point is right here */
        prsxgen_pia(ctx);                   /* parse and generate condition */

        prsreq(ctx, TOKTRPAR);
        emtjmp(ctx->prscxemt, OPCJF, ldone);
        prsstm(ctx, ldone, lloop, parms, locals, entofs, (prscsdef *)0,
               curfr);
        emtjmp(ctx->prscxemt, OPCJMP, lloop);
        emtslbl(ctx->prscxemt, &ldone, TRUE);
        emtdlbl(ctx->prscxemt, &lloop);
        break;
        
    case TOKTDO:
        toknext(ctx->prscxtok);                    /* skip the 'do' keyword */
        
        /* get labels - one for loopback, one for break, one for continue */
        ldone = emtglbl(ctx->prscxemt);
        lloop = emtgslbl(ctx->prscxemt);        /* loop point is right here */
        lfalse = emtglbl(ctx->prscxemt);          /* get label for continue */

        /* parse loop body */
        prsstm(ctx, ldone, lfalse, parms, locals, entofs, (prscsdef *)0,
               curfr);

        /* set continue label to just before condition */
        emtslbl(ctx->prscxemt, &lfalse, TRUE);
        
        /* parse the while(condition) clause and loop back if true */
        prsreq(ctx, TOKTWHILE);
        prsxgen_pia(ctx);                            /* parse the condition */
        emtjmp(ctx->prscxemt, OPCJT, lloop);
        emtdlbl(ctx->prscxemt, &lloop);
        
        /* set break label to just after the loop */
        emtslbl(ctx->prscxemt, &ldone, TRUE);
        break;
        
    case TOKTFOR:
        {
            prsndef *reinit = (prsndef *)0;
            ushort   for_oldrrst = ctx->prscxrrst;
            uchar   *for_oldnrst = ctx->prscxnrst;

            prsnreq(ctx, TOKTLPAR);

            /* get loop/end labels */
            ldone = emtglbl(ctx->prscxemt);    /* bottom of loop (continue) */
            lloop = emtglbl(ctx->prscxemt);           /* top of loop (test) */
            lfalse = emtglbl(ctx->prscxemt);         /* end of loop (break) */

            /* parse initializer, if present */
            if (ctx->prscxtok->tokcxcur.toktyp != TOKTSEM) prsxgen(ctx);
            emtop(ctx->prscxemt, OPCDISCARD);     /* don't need initializer */
            prsreq(ctx, TOKTSEM);
            
            /* parse condition, if present */
            emtslbl(ctx->prscxemt, &lloop, FALSE);
            if (ctx->prscxtok->tokcxcur.toktyp != TOKTSEM)
            {
                prsxgen_pia(ctx);
                emtjmp(ctx->prscxemt, OPCJF, lfalse);
            }
            prsreq(ctx, TOKTSEM);
            
            /* parse re-initializer, if present */
            if (ctx->prscxtok->tokcxcur.toktyp != TOKTRPAR)
                reinit = prsexpr(ctx);
            prsreq(ctx, TOKTRPAR);
            
            /* save expression context - re-initizlier is in pool */
            ctx->prscxrrst = ctx->prscxnrem;
            ctx->prscxnrst = ctx->prscxnode;

            /* parse loop body */
            prsstm(ctx, lfalse, ldone, parms, locals, entofs, (prscsdef *)0,
                   curfr);
            
            /* generate re-initializer code if needed, and loop back */
            emtslbl(ctx->prscxemt, &ldone, FALSE);
            if (reinit)
            {
                prsgexp(ctx, reinit);
                emtop(ctx->prscxemt, OPCDISCARD);       /* don't save value */
            }
            emtjmp(ctx->prscxemt, OPCJMP, lloop);

            /* set post-loop label, and reset everything */
            emtslbl(ctx->prscxemt, &lfalse, TRUE);
            emtdlbl(ctx->prscxemt, &lloop);
            emtdlbl(ctx->prscxemt, &ldone);
            ctx->prscxrrst = for_oldrrst;
            ctx->prscxnrst = for_oldnrst;
/*          prsrstn(ctx); */
            break;
        }
        
    case TOKTBREAK:
        prsnreq(ctx, TOKTSEM);
        if (brk == EMTLLNKEND)
            errlog(ctx->prscxerr, ERR_BADBRK);
        else
            emtjmp(ctx->prscxemt, OPCJMP, brk);
        break;

    case TOKTCONTINUE:
        prsnreq(ctx, TOKTSEM);
        if (cont == EMTLLNKEND)
            errlog(ctx->prscxerr, ERR_BADCNT);
        else
            emtjmp(ctx->prscxemt, OPCJMP, cont);
        break;

    case TOKTGOTO:
        /* make sure we get some sort of symbol name */
        toknext(ctx->prscxtok);
        if (ctx->prscxtok->tokcxcur.toktyp != TOKTSYMBOL)
            errsig(ctx->prscxerr, ERR_REQLBL);

        /* require a label; if it's not already a label, make it one */
        prsdef(ctx, &ctx->prscxtok->tokcxcur, TOKSTLABEL);
        if (ctx->prscxtok->tokcxcur.toksym.tokstyp != TOKSTLABEL)
            errsig(ctx->prscxerr, ERR_REQLBL);
        
        emtjmp(ctx->prscxemt, OPCJMP,
               (uint)ctx->prscxtok->tokcxcur.toksym.toksval);
        prsnreq(ctx, TOKTSEM);
        break;

    case TOKTSWITCH:
    {
        prsctdef *curctab;
        prsctdef *nextctab;
        prscsdef  myswctl;
        ushort    sw_oldrrst = ctx->prscxrrst;
        uchar    *sw_oldnrst = ctx->prscxnrst;

        /* parse the selection expression */
        prsnreq(ctx, TOKTLPAR);
        prsxgen(ctx);
        prsreq(ctx, TOKTRPAR);
        
        /* get 'break' label - this jumps past case table */
        ldone = emtglbl(ctx->prscxemt);

        /* get label for case table, and emit SWITCH instruction for it */
        lloop = emtglbl(ctx->prscxemt);
        emtjmp(ctx->prscxemt, OPCSWITCH, lloop);        
        
        /* allocate the first case table (more can be added later) */
        casetab = MCHNEW(ctx->prscxerr, prsctdef, "prsstm: case table");
        casetab->prsctnxt = (prsctdef *)0;

        /* parse switch body */
        myswctl.prscstab = casetab;
        myswctl.prscscnt = 0;
        myswctl.prscsdflt = 0;
        prsstm(ctx, ldone, cont, parms, locals, entofs, &myswctl, curfr);

        /* emit case table */
        emtjmp(ctx->prscxemt, OPCJMP, ldone);       /* jump over case table */
        emtslbl(ctx->prscxemt, &lloop, TRUE);       /* set case table label */
        emtint2(ctx->prscxemt, myswctl.prscscnt);        /* number of cases */
        
        for (curctab = casetab ; curctab ; curctab = nextctab)
        {
            int lastlbl;
            int i;
            int diff;
            
            nextctab = curctab->prsctnxt;
            lastlbl = (nextctab ? PRSCTSIZE : myswctl.prscscnt);
            for (i = 0 ; i < lastlbl ; --(myswctl.prscscnt), ++i)
            {
                /* emit case value, then offset of code for case */
                emtval(ctx->prscxemt, &curctab->prsctcase[i].prscttok,
                       ctx->prscxpool);
                diff = (int)(curctab->prsctcase[i].prsctofs
                             - ctx->prscxemt->emtcxofs);
                emtint2s(ctx->prscxemt, diff);
            }
            
            /* done with this table - free it */
            mchfre(curctab);
        }
        casetab = (prsctdef *)0;

        /* emit 'default' entry - use ldone if no 'default' was given */
        if (myswctl.prscsdflt)
        {
            int diff = (int)(myswctl.prscsdflt - ctx->prscxemt->emtcxofs);
            emtint2s(ctx->prscxemt, diff);
        }
        else
            emtint2(ctx->prscxemt, 2);           /* no default - skip ahead */

        /* we're done with the saved values in the case table */
        ctx->prscxrrst = sw_oldrrst;
        ctx->prscxnrst = sw_oldnrst;
        
        /* set end label, and we're done */
        emtslbl(ctx->prscxemt, &ldone, TRUE);
        break;
    }
    
    case TOKTCASE:
    {
        prsndef *node;
        int      typ;
        int      styp;

        /* make sure the case occurs in a switch body */
        if (!swctl)
            errsig(ctx->prscxerr, ERR_NOSW);

        /* get the case value, make sure it's valid */
        toknext(ctx->prscxtok);
        node = prsexpr(ctx);
        prsreq(ctx, TOKTCOLON);
        if (node->prsnnlf != 0) errsig(ctx->prscxerr, ERR_SWRQCN);
        typ = node->prsnv.prsnvt.toktyp;
        if (typ == TOKTDSTRING ||
            (typ == TOKTSYMBOL &&
             ((styp = node->prsnv.prsnvt.toksym.tokstyp) == TOKSTLOCAL
              || styp == TOKSTBIFN || styp == TOKSTPROP
              || styp == TOKSTSELF)))
            errsig(ctx->prscxerr, ERR_SWRQCN);

        /* assume we have an object if it's an undefined symbol */
        if (typ == TOKTSYMBOL)
            prsdef(ctx, &node->prsnv.prsnvt, TOKSTFWDOBJ);
        
        /* if we need another case table, add one */
        if (casecnt == PRSCTSIZE)
        {
            curctab->prsctnxt = MCHNEW(ctx->prscxerr, prsctdef,
                                       "prsstm: case table extension");
            curctab = curctab->prsctnxt;
            curctab->prsctnxt = (prsctdef *)0;
            casecnt = 0;
        }
                
        /* set up this entry with value and code location */
        OSCPYSTRUCT(curctab->prsctcase[casecnt].prscttok,
                    node->prsnv.prsnvt);
        curctab->prsctcase[casecnt].prsctofs =
                    ctx->prscxemt->emtcxofs;
        ++casecnt;
        ++(swctl->prscscnt);
        
        /* save anything in the node table - we'll need it later */
        ctx->prscxnrst = ctx->prscxnode;
        ctx->prscxrrst = ctx->prscxnrem;

        prsrstn(ctx);
        break;
    }

    case TOKTDEFAULT:
        if (!swctl) errsig(ctx->prscxerr, ERR_NOSW);
        prsnreq(ctx, TOKTCOLON);
        swctl->prscsdflt = ctx->prscxemt->emtcxofs;
        break;
        
    case TOKTELSE:
        errlog(ctx->prscxerr, ERR_BADELS);
        toknext(ctx->prscxtok);
        break;
        
    case TOKTRETURN:
        if (toknext(ctx->prscxtok) == TOKTSEM)
        {       
            toknext(ctx->prscxtok);
            emtop(ctx->prscxemt, OPCRETURN);
        }
        else
        {
            prsxgen_pia(ctx);                    /* parse return expression */
            prsreq(ctx, TOKTSEM);
            emtop(ctx->prscxemt, OPCRETVAL);
        }
        emtint2(ctx->prscxemt, parms);
        break;
        
    case TOKTPASS:
        {
            prpnum p;

            toknext(ctx->prscxtok);
            p = prsrqpr(ctx);                 /* we need a property id here */
            emtop(ctx->prscxemt, OPCPASS);
            emtint2(ctx->prscxemt, p);
            prsreq(ctx, TOKTSEM);
        }
        break;
        
    case TOKTEXIT:
        prsnreq(ctx, TOKTSEM);
        emtop(ctx->prscxemt, OPCEXIT);
        break;
        
    case TOKTABORT:
        prsnreq(ctx, TOKTSEM);
        emtop(ctx->prscxemt, OPCABORT);
        break;
        
    case TOKTASKDO:
        prsnreq(ctx, TOKTSEM);
        emtop(ctx->prscxemt, OPCASKDO);
        break;
        
    case TOKTASKIO:
        prsnreq(ctx, TOKTLPAR);
        t = &ctx->prscxtok->tokcxcur;
        if (t->toktyp != TOKTSYMBOL)
            errsig(ctx->prscxerr, ERR_REQOBJ);
        prsdef(ctx, t, TOKSTFWDOBJ);
        switch (t->toksym.tokstyp)
        {
        case TOKSTOBJ:
        case TOKSTFWDOBJ:
            {
                mcmon obj;
                
                obj = t->toksym.toksval;
                emtop(ctx->prscxemt, OPCASKIO);
                emtint2(ctx->prscxemt, obj);
            }
            break;
            
        default:
            errsig(ctx->prscxerr, ERR_REQOBJ);
        }
        prsnreq(ctx, TOKTRPAR);
        prsreq(ctx, TOKTSEM);
        break;
        
    case TOKTSYMBOL:
    case TOKTLPAR:
    case TOKTNUMBER:
    case TOKTINC:
    case TOKTDEC:
    case TOKTDELETE:
        {
            prsndef *expr;
            int      needsem;
            int      t;
            
            /*needsem = (ctx->prscxtok->tokcxcur.toktyp != TOKTLPAR);*/
            needsem = TRUE;
            expr = prsexpr(ctx);
            
            if (ctx->prscxtok->tokcxcur.toktyp == TOKTCOLON &&
                expr->prsnnlf == 0 && expr->prsnv.prsnvt.toktyp == TOKTSYMBOL
                && ((t = expr->prsnv.prsnvt.toksym.tokstyp) == TOKSTLABEL
                    || t == TOKSTUNK))
            {
                uint lbl;
                
                prsdef(ctx, &expr->prsnv.prsnvt, TOKSTLABEL);
                lbl = expr->prsnv.prsnvt.toksym.toksval;
                emtslbl(ctx->prscxemt, &lbl, FALSE);
                toknext(ctx->prscxtok);
                goto startover;
            }

            if (expr->prsntyp == TOKTEQ) errlog(ctx->prscxerr, ERR_WEQASI);
            prsgexp(ctx, expr);
            prsrstn(ctx);              /* reset prior to reading next token */
            if (needsem) prsreq(ctx, TOKTSEM);
            emtop(ctx->prscxemt, OPCDISCARD);
        }
        break;
        
    case TOKTEOF:
        errsig(ctx->prscxerr, ERR_EOF);

    case TOKTLOCAL:
        /* log the error */
        errlog(ctx->prscxerr, ERR_BADLCL);

        /* eat tokens up to the semicolon */
        while (ctx->prscxtok->tokcxcur.toktyp != TOKTSEM
               && ctx->prscxtok->tokcxcur.toktyp != TOKTEOF)
            toknext(ctx->prscxtok);

        /* ignore the statement and continue parsing */
        break;
        
    default:
        errsig(ctx->prscxerr, ERR_SYNTAX);
    }
    } while (compound);

    /* undo local symbol table if we created one */
    ctx->prscxplcl = oldlclp;
    ctx->prscxslcl = oldlcls;
    ctx->prscxtok->tokcxstab = oldltab;

    ERRCLEAN(ctx->prscxerr)
        /* undo any labels that have been set */
        if (ldone != EMTLLNKEND) emtclbl(ctx->prscxemt, &ldone);
        if (lloop != EMTLLNKEND) emtclbl(ctx->prscxemt, &lloop);
        if (lfalse != EMTLLNKEND) emtclbl(ctx->prscxemt, &lfalse);

        /* reset expression evaluation context */
        ctx->prscxrrst = oldrrst;
        ctx->prscxnrst = oldnrst;
        ctx->prscxplcl = oldlclp;
        ctx->prscxslcl = oldlcls;
        ctx->prscxtok->tokcxstab = oldltab;
        prsrstn(ctx);
    
        /* free case tables if any were allocated */
        {
            prsctdef *curctab;
            prsctdef *nextctab;
            
            for (curctab = casetab ; curctab ; curctab = nextctab)
            {
                nextctab = curctab->prsctnxt;
                mchfre(curctab);
            }
        }
    ERRENDCLN(ctx->prscxerr)
}

/* callback for prsdelgoto - delete one label */
static void prsdel1(void *ctx0, toksdef *sym)
{
    prscxdef *ctx = (prscxdef *)ctx0;
    uint label;

    label = sym->toksval;
    if (!emtqset(ctx->prscxemt, sym->toksval))
    {
        errlog1(ctx->prscxerr, ERR_NOGOTO, ERRTSTR,
                errstr(ctx->prscxerr, sym->toksnam, sym->tokslen));
        emtclbl(ctx->prscxemt, &label);
        sym->toksval = label;
    }
    else
    {
        emtdlbl(ctx->prscxemt, &label);
        sym->toksval = label;
    }
}

/* delete 'goto' symbols, and warn if any are undefined */
void prsdelgoto(prscxdef *ctx)
{
    toktldef *tab = (toktldef *)ctx->prscxgtab;

    toktleach(&tab->toktlsc, prsdel1, ctx);
    toktldel(tab);
}

/* build a template name from a root name, and add a property for it */
static prpnum prstpl(prscxdef *ctx, uchar *root, char *prefix)
{
    tokdef tok;
    int    pfllen = strlen(prefix);
    int    rootlen = osrp2(root) - 2;

    /* build the symbol from the prefix and the root */
    tok.toklen = pfllen + rootlen;
    memcpy(tok.toknam, prefix, (size_t)pfllen);
    memcpy(tok.toknam + pfllen, root+2, (size_t)rootlen);

    /* fold case if we're in case-insensitive mode */
    /* 
     *   convert the symbol to lower-case if we're compiling the game in
     *   case-insensitive mode 
     */
    tok_case_fold(ctx->prscxtok, &tok);

    /* build the token definition */
    tok.toknam[pfllen + rootlen] = '\0';
    tok.toktyp = TOKTSYMBOL;
    tok.tokhash = tokhsh(tok.toknam);
    tok.toksym.tokstyp = TOKSTUNK;

    /* define or look up the property */
    prsdef(ctx, &tok, TOKSTPROP);

    /* return the property number */
    return(tok.toksym.toksval);
}

/* build a property string for a synonym and get its property number */
static prpnum prssynp(prscxdef *ctx, uchar *prefix, size_t pfxlen,
                      uchar *suffix)
{
    tokdef   tok;
    toksdef  sym;
    size_t   suflen;
    toktdef *tab;
    
    /* construct the token */
    suflen = osrp2(suffix) - 2;
    suffix += 2;

    /* build the synonym */
    tok.toklen = pfxlen + suflen;
    memcpy(tok.toknam, prefix, pfxlen);
    memcpy(tok.toknam + pfxlen, suffix, suflen);

    /* fold case if we're in case-insensitive mode */
    tok_case_fold(ctx->prscxtok, &tok);

    /* build the token */
    tok.toknam[tok.toklen] = '\0';
    tok.tokhash = tokhsh(tok.toknam);
    tok.toktyp = TOKTSYMBOL;

    /* look up the symbol, if it's already in the table */
    tab = ctx->prscxstab;
    tok.toksym.tokstyp = TOKSTUNK;
    (*tab->toktfsea)(tab, tok.toknam, tok.toklen, tok.tokhash, &tok.toksym);

    /* make it a property if it's not already */
    prsdef(ctx, &tok, TOKSTPROP);
    
    /* now look it up and return its property number */
    (*tab->toktfsea)(tab, tok.toknam, tok.toklen, tok.tokhash, &sym);
    return(sym.toksval);
}

/* add a synonym property */
static void prssyn(prscxdef *ctx, objnum objn, uchar *prefix,
                   uchar *synto, uchar *synfrom)
{
    size_t   plen = strlen((char *)prefix);
    prpnum   propto;
    prpnum   propfrom;
    uchar    buf[2];

    /* get the property numbers for the 'to' and 'from' strings */
    propto = prssynp(ctx, prefix, plen, synto);
    propfrom = prssynp(ctx, prefix, plen, synfrom);

    /* set the synonym property */
    oswp2(buf, propto);
    objsetp(ctx->prscxmem, (mcmon)objn, propfrom, DAT_SYN, buf,
            (objucxdef *)0);
}


/* maximum number of templates for a single object */
#define PRSTPMAX 30

/* parse an object definition (superclass list is already parsed) */
static void prsobj(prscxdef *ctx, noreg tokdef *objtok, int numsc,
                   objnum *sclist, int classflg)
{
    objnum   *sclistp;
    objdef   *objptr;
    int       t;
    prpnum    p;
    objnum    objn;
    void     *val;
    uchar     valbuf[4];
    int       typ;
    objnum    oval;
    prpnum    pval;
    uchar    *oldplcl = ctx->prscxplcl;
    uint      oldslcl = ctx->prscxslcl;
    int       parms;
    uint      codeofs;
    uchar    *scptr;
    toktdef  *oldltab = ctx->prscxtok->tokcxstab;
    int       varargs;
    objnum    prep;
    uchar     tpl[1 + PRSTPMAX*VOCTPL2SIZ]; /* templates built during parse */
    int       tplcur = 1;            /* start writing at offset 1 in buffer */
    int       contset = FALSE;       /* check if we see 'contents' property */
    objnum    locobj = MCMONINV;                 /* location of this object */
    int       locnil = FALSE;                     /* location is set to nil */
    int       locok = FALSE;           /* 'locationOK = true' was specified */
    int       locwarn = FALSE;   /* need to warn if locationOK=true not set */
    int       hasvoc = FALSE;              /* true if object has vocabulary */
    int       i;
    uint      curfr;                       /* offset of current debug frame */
    toktldef *ltab;                                   /* local symbol table */
    ushort    freeofs;
    int       indexprop;
    tokdef    proptok;
    uchar     tplflags;
    size_t    tplsiz;
    dattyp    tpldat;
    prpnum    tplprop;
    
    /* use new-style or old-style templates, as appropriate */
    if (ctx->prscxflg & PRSCXFTPL1)
    {
        tplsiz = VOCTPLSIZ;
        tpldat = DAT_TPL;
        tplprop = PRP_TPL;
    }
    else
    {
        tplsiz = VOCTPL2SIZ;
        tpldat = DAT_TPL2;
        tplprop = PRP_TPL2;
    }
    tpl[0] = 0;                             /* no templates in array so far */
    
    IF_DEBUG(printf("*** compiling object #%d (%s) ***\n",
                    objtok->toksym.toksval, objtok->toknam));

    /* lock the object, and initialize its object header */
    objn = objtok->toksym.toksval;
    objptr = (objdef *)mcmlck(ctx->prscxmem, (mcmon)objn);
    objini(ctx->prscxmem, numsc, objn, classflg);
    
    /* set up superclasses in object */
    for (scptr = objsc(objptr), i = numsc, sclistp = sclist ; i
        ; scptr += 2, ++sclistp, --i)
        oswp2(scptr, *sclistp);
    
    /* catch any errors that occur while we have the object locked */
    ERRBEGIN(ctx->prscxerr)
    
    for (;;)
    {
        ctx->prscxtok->tokcxstab = oldltab;    /* restore old symbol tables */
        t = ctx->prscxtok->tokcxcur.toktyp;
        if (t == TOKTSEM) break;               /* end of object - quit loop */
        
        /* check for special doSynonym or ioSyonym property */
        if (t == TOKTDOSYN || t == TOKTIOSYN)
        {
            uchar *synto;                      /* prefix to set synonyms to */
            uchar *synfrom;      /* current prefix to set to point to synto */
            uchar *verpre = (uchar *)(t == TOKTDOSYN ? "verDo" : "verIo");
            uchar *pre    = (uchar *)(t == TOKTDOSYN ? "do"    : "io");

            /* scan the 'to' suffix, which should be enclosed in parens */
            prsnreq(ctx, TOKTLPAR);              /* check for an open paren */
            if (ctx->prscxtok->tokcxcur.toktyp != TOKTSSTRING)
                errsig(ctx->prscxerr, ERR_BADSYN);
            synto = ctx->prscxpool + ctx->prscxtok->tokcxcur.tokofs;
            prsnreq(ctx, TOKTRPAR);              /* require the close paren */
            prsreq(ctx, TOKTEQ);                             /* and the '=' */
            
            /* loop through suffixes to point to 'synto' */
            for (;;)
            {
                /* if we don't have another string, we're done */
                if (ctx->prscxtok->tokcxcur.toktyp != TOKTSSTRING) break;

                /* get the 'from' suffix, and add the synonym properties */
                synfrom = ctx->prscxpool + ctx->prscxtok->tokcxcur.tokofs;
                mcmunlck(ctx->prscxmem, (mcmon)objn);
                prssyn(ctx, objn, verpre, synto, synfrom);
                prssyn(ctx, objn, pre, synto, synfrom);
                objptr = mcmlck(ctx->prscxmem, (mcmon)objn);
                
                toknext(ctx->prscxtok);
            }
            
            /* entirely done with this property - move along */
            continue;
        }

        /* check for a "replace" keyword on the property */
        if (t == TOKTREPLACE && numsc >= 1)
        {
            objnum  prvobj;
            objnum  prvprv;
            objdef *prvptr;
            uint    prvmod;

            /* skip the "replace" keyword and make sure we have a property */
            t = toknext(ctx->prscxtok);
            OSCPYSTRUCT(proptok, ctx->prscxtok->tokcxcur);
            p = prsrqpr(ctx);

            /* delete the property in all previous definitions of this obj */
            for (prvobj = sclist[0] ; prvobj != MCMONINV ; prvobj = prvprv)
            {
                /* lock the superclass object and get its flags */
                prvptr = mcmlck(ctx->prscxmem, (mcmon)prvobj);
                prvmod = objflg(prvptr) & OBJFMOD;

                /* if the superclass is a superseded obj, get its sc */
                if (objnsc(prvptr) == 1 && prvmod)
                    prvprv = osrp2(objsc(prvptr));
                else
                    prvprv = MCMONINV;

                /* done with superclass object - unlock it */
                mcmunlck(ctx->prscxmem, (mcmon)prvobj);

                /* 
                 *   If the superclass was superseded, delete this
                 *   property.  Note that if we're generating debugging
                 *   information, we can only mark the property as
                 *   deleted, whereas in non-debug mode we can actually
                 *   remove the data; the reason for the difference is
                 *   that p-code is all self-relative, and hence can be
                 *   relocated within an object, except for certain
                 *   debugging instructions.  
                 */
                if (prvmod)
                    objdelp(ctx->prscxmem, prvobj, p,
                            (ctx->prscxflg & (PRSCXFLIN | PRSCXFLCL)) != 0);
            }
        }
        else
        {
            OSCPYSTRUCT(proptok, ctx->prscxtok->tokcxcur);
            p = prsrqpr(ctx);                 /* get property being defined */
        }

        /* check for template-defining properties */
        if (p == PRP_IOACTION)
        {
            prsreq(ctx, TOKTLPAR);                   /* look for left paren */
            if (ctx->prscxtok->tokcxcur.toktyp != TOKTSYMBOL)
                errsig(ctx->prscxerr, ERR_REQSYM);
            prsdef(ctx, &ctx->prscxtok->tokcxcur, TOKSTFWDOBJ);
            typ = ctx->prscxtok->tokcxcur.toksym.tokstyp;
            if (typ != TOKSTOBJ && typ != TOKSTFWDOBJ)
                errsig(ctx->prscxerr, ERR_REQOBJ);
            prep = ctx->prscxtok->tokcxcur.toksym.toksval;
            prsnreq(ctx, TOKTRPAR);
        }

        if (p == PRP_DOACTION || p == PRP_IOACTION)
        {
            uchar   *root;
            uchar   *thistpl;

            if (tplcur >= sizeof(tpl)) errsig(ctx->prscxerr, ERR_MANYTPL);
            thistpl = tpl + tplcur;
            
            /* presume template flags will be zero, if any */
            tplflags = 0;

            /* skip the '=' */
            prsreq(ctx, TOKTEQ);

            /*
             *   Check for flags - flags are tokens (non-reserved words)
             *   listed in parens after the equals sign.
             */
            if (ctx->prscxtok->tokcxcur.toktyp == TOKTLBRACK)
            {
                /* flags are not allowed with pre-'C' file format */
                if (ctx->prscxflg & PRSCXFTPL1)
                    errlog(ctx->prscxerr, ERR_NOTPLFLG);

                /* read all flags */
                for (;;)
                {
                    int     t;
                    int     i;
                    size_t  l;
                    tokdef *tokp;
                    static  struct
                    {
                        char *nam;
                        int   flagval;
                    } *kw, kwlist[] =
                    {
                        { "disambigDobjFirst", VOCTPLFLG_DOBJ_FIRST },
                        { "disambigIobjFirst", 0 }
                    };
                    
                    /* get the next token, and stop if it's the ']' */
                    if ((t = toknext(ctx->prscxtok)) == TOKTRBRACK)
                        break;

                    /* we need a symbol */
                    if (t != TOKTSYMBOL)
                        errsig(ctx->prscxerr, ERR_REQSYM);

                    tokp = &ctx->prscxtok->tokcxcur;

                    /* find the symbol and apply the flag */
                    for (kw = kwlist, i = sizeof(kwlist)/sizeof(kwlist[0]) ;
                         i ; ++kw, --i)
                    {
                        l = strlen(kw->nam);
                        if (l == (size_t)tokp->toklen &&
                            !memcmp(kw->nam, tokp->toknam, l))
                        {
                            tplflags |= kw->flagval;
                            break;
                        }
                    }

                    /* if we didn't find the flag, it's an error */
                    if (i == 0)
                        errsig(ctx->prscxerr, ERR_BADTPLF);
                }

                /* skip the closing bracket */
                toknext(ctx->prscxtok);
            }

            /* we need a string to define the template */
            if (ctx->prscxtok->tokcxcur.toktyp != TOKTSSTRING)
                errsig(ctx->prscxerr, ERR_BADTPL);
            root = ctx->prscxpool + ctx->prscxtok->tokcxcur.tokofs;
            if (osrp2(root) + 3 > TOKNAMMAX)
                errsig(ctx->prscxerr, ERR_LONGTPL);
            
            /* build the template based on the root name and prep */
            if (p == PRP_IOACTION)
            {
                oswp2(thistpl, prep);
                oswp2(thistpl+2, prstpl(ctx, root, "verIo"));
                oswp2(thistpl+4, prstpl(ctx, root, "io"));
            }
            else
            {
                oswp2(thistpl, MCMONINV);
                oswp2(thistpl+2, 0);
                oswp2(thistpl+4, 0);
            }
            oswp2(thistpl+6, prstpl(ctx, root, "verDo"));
            oswp2(thistpl+8, prstpl(ctx, root, "do"));

            /* add the flags */
            if (!(ctx->prscxflg & PRSCXFTPL1))
                thistpl[10] = tplflags;

            /* we're done with this property entirely - move on */
            tplcur += tplsiz;               /* move past space we just used */
            tpl[0]++;              /* increment count of templates in array */
            toknext(ctx->prscxtok);
            continue;
        }
        
        /* error if already defined */
        if (objgetp(ctx->prscxmem, objn, p, (dattyp *)0) != 0)
            errlog(ctx->prscxerr, ERR_PREDEF);
        
        /* check for a formal parameter list */
        prsrstn(ctx);
        varargs = FALSE;
        parms = 0;
        curfr = 0;                             /* no local symbol frame yet */
        ltab = (toktldef *)0;
        if (ctx->prscxtok->tokcxcur.toktyp == TOKTLPAR)
        {
            ltab = prsvlst(ctx, (toktldef *)0, (prsndef **)0, &parms, TRUE,
                           &varargs, curfr);
            prsreq(ctx, TOKTRPAR);
        }

        /*
         *   Check for redirection syntax: xoVerb -> obj.  This means to
         *   route xoVerb and verXoVerb to the given object's methods of
         *   the same name. 
         */
        if (ctx->prscxtok->tokcxcur.toktyp == TOKTPOINTER)
        {
            char     buf[2];
            int      typ;
            toksdef *sym;
            tokdef   tok2;
            toktdef *tab;
            
            /* get the object (and make sure it's an object) */
            if (toknext(ctx->prscxtok) != TOKTSYMBOL)
                errsig(ctx->prscxerr, ERR_REQSYM);
            prsdef(ctx, &ctx->prscxtok->tokcxcur, TOKSTFWDOBJ);
            sym = &ctx->prscxtok->tokcxcur.toksym;
            typ = sym->tokstyp;
            if (typ != TOKSTOBJ && typ != TOKSTFWDOBJ)
                errsig(ctx->prscxerr, ERR_REQOBJ);

            /* add the definition for this property */
            oswp2(buf, ctx->prscxtok->tokcxcur.toksym.toksval);
            mcmunlck(ctx->prscxmem, (mcmon)objn);
            objsetp(ctx->prscxmem, (mcmon)objn, p, DAT_REDIR, buf,
                    (objucxdef *)0);
            objptr = mcmlck(ctx->prscxmem, (mcmon)objn);

            /* figure out the other property name */
            memcpy(tok2.toknam, "ver", (size_t)3);
            memcpy(&tok2.toknam[3], proptok.toknam, (size_t)proptok.toklen);
            tok2.toknam[proptok.toklen + 3] = '\0';
            if (vocislower(tok2.toknam[3]))
                tok2.toknam[3] = toupper(tok2.toknam[3]);
            tok2.toklen = proptok.toklen + 3;

            /* fold case if we're in case-insensitive mode */
            tok_case_fold(ctx->prscxtok, &tok2);

            /* find the other property, and make sure it's a property */
            tok2.tokhash = tokhsh(tok2.toknam);
            tok2.toktyp = TOKTSYMBOL;
            tok2.toksym.tokstyp = TOKSTUNK;
            tab = ctx->prscxstab;
            (*tab->toktfsea)(tab, tok2.toknam, tok2.toklen, tok2.tokhash,
                             &tok2.toksym);
            prsdef(ctx, &tok2, TOKSTPROP);
            if (tok2.toksym.tokstyp != TOKSTPROP)
                errsig1(ctx->prscxerr, ERR_IMPPROP, ERRTSTR, tok2.toknam);

            /* add the equivalent definition for this property */
            mcmunlck(ctx->prscxmem, (mcmon)objn);
            objsetp(ctx->prscxmem, (mcmon)objn, tok2.toksym.toksval,
                    DAT_REDIR, buf, (objucxdef *)0);
            objptr = mcmlck(ctx->prscxmem, (mcmon)objn);

            /* skip the object name and move on to the next property */
            toknext(ctx->prscxtok);
            continue;
        }
        
        /* parse the property definition */
        prsreq(ctx, TOKTEQ);
        if (ctx->prscxtok->tokcxcur.toktyp == TOKTLBRACE)
        {
            /* make sure this isn't a vocabulary property */
            if (prpisvoc(p)) errsig(ctx->prscxerr, ERR_BADVOC);

            /* set up to emit code into new property */
            mcmunlck(ctx->prscxmem, (mcmon)objn);
            codeofs = objemt(ctx->prscxmem, objn, p, DAT_CODE);
            ctx->prscxemt->emtcxptr = mcmlck(ctx->prscxmem, (mcmon)objn);
            ctx->prscxemt->emtcxofs = codeofs;
            ctx->prscxemt->emtcxobj = objn;
            
            /* emit argument count checking instruction */
            if (ctx->prscxflg & PRSCXFARC)
            {
                emtop(ctx->prscxemt, OPCCHKARGC);
                emtbyte(ctx->prscxemt, (varargs ? 0x80 : 0) + parms);
            }

            /* emit debug frame if there are arguments */
            if (ltab) prsvgfr(ctx, ltab, &curfr);

            /* parse code block, and emit return at the end */
            prsstm(ctx, EMTLLNKEND, EMTLLNKEND, parms, 0, 0, (prscsdef *)0,
                   curfr);
            emtop(ctx->prscxemt, OPCRETURN);
            emtint2(ctx->prscxemt, 0);
            prsdelgoto(ctx);

            /* end code generation, and "close" the property */
            objendemt(ctx->prscxmem, objn, p, ctx->prscxemt->emtcxofs);
                
            /* recache objptr in case it changed during code generation */
            objptr = ctx->prscxemt->emtcxptr;

            /* restore local symbol table information in context */
            ctx->prscxplcl = oldplcl;
            ctx->prscxslcl = oldslcl;
        }
        else
        {
            prsndef *expr;
            lindef *saved_lin;
            uchar saved_loc[LINLLNMAX];
            
            /* allow vocabulary list to be enclosed in square brackets */
            if (prpisvoc(p) && ctx->prscxtok->tokcxcur.toktyp == TOKTLBRACK)
                toknext(ctx->prscxtok);

            /* 
             *   save the line position at the START of the expression - if
             *   it's a one-liner, as most expression properties are, we want
             *   to generate the debug record at the start rather than at the
             *   next token, which will probably be on the next line 
             */
            saved_lin = ctx->prscxtok->tokcxlin;
            linglop(saved_lin, saved_loc);

            /* parse the expression */
            expr = prsexpr(ctx);
            if (expr->prsnnlf != 0)
            {
                /* make sure this isn't a vocabulary property */
                if (prpisvoc(p)) errsig(ctx->prscxerr, ERR_BADVOC);

                /* set up to emit code into new property */
                mcmunlck(ctx->prscxmem, (mcmon)objn);
                codeofs = objemt(ctx->prscxmem, objn, p, DAT_CODE);
                ctx->prscxemt->emtcxptr = mcmlck(ctx->prscxmem, (mcmon)objn);
                ctx->prscxemt->emtcxofs = codeofs;
                ctx->prscxemt->emtcxobj = objn;

                /* emit debug frame if there are arguments */
                if (ltab) prsvgfr(ctx, ltab, &curfr);

                /* emit argument count checking instruction */
                if (ctx->prscxflg & PRSCXFARC)
                {
                    emtop(ctx->prscxemt, OPCCHKARGC);
                    emtbyte(ctx->prscxemt, (varargs ? 0x80 : 0 ) + parms);
                }
                
                /* generate an ENTER for no locals */
                emtop(ctx->prscxemt, OPCENTER);
                emtint2(ctx->prscxemt, 0);

                /* generate an OPCLINE instruction if debugging */
                if (ctx->prscxflg & PRSCXFLIN)
                    prsclin(ctx, curfr, saved_lin, TRUE, saved_loc);

                /* generate code for the expression */
                prsgexp(ctx, expr);
                
                /* emit a RETVAL instruction and close the property */
                emtop(ctx->prscxemt, OPCRETVAL);
                emtint2(ctx->prscxemt, 0);
                prsdelgoto(ctx);
                objendemt(ctx->prscxmem, objn, p, ctx->prscxemt->emtcxofs);
                
                /* recache objptr in case it changed during code generation */
                objptr = ctx->prscxemt->emtcxptr;
            }
            else
            {
                /* set val pointer for list/string types */
                val = &ctx->prscxpool[0] + expr->prsnv.prsnvt.tokofs;
                
                /* special handling for vocabulary property */
                if (prpisvoc(p))
                {
                    uchar *wrdtxt;
                    
                    hasvoc = TRUE;           /* note presence of vocabulary */

                    /* single-quoted string value required */
                    if (expr->prsnv.prsnvt.toktyp != TOKTSSTRING)
                        errsig(ctx->prscxerr, ERR_BADVOC);
                    wrdtxt = ctx->prscxpool + expr->prsnv.prsnvt.tokofs;

                    for (;;)
                    {
                        /* add the vocabulary word */
                        vocadd(ctx->prscxvoc, p, objn,
                               (classflg ? VOCFCLASS : 0), (char *)wrdtxt);
                    
                        /* check for additional words in list */
                        if (ctx->prscxtok->tokcxcur.toktyp != TOKTSSTRING)
                            break;
                        
                        /* another word - get text and skip it */
                        wrdtxt = ctx->prscxpool +
                                ctx->prscxtok->tokcxcur.tokofs;
                        toknext(ctx->prscxtok);
                    }
                    
                    /* allow vocabulary list to terminate in ']' */
                    if (ctx->prscxtok->tokcxcur.toktyp == TOKTRBRACK)
                        toknext(ctx->prscxtok);
                }
                else
                {
                    switch(expr->prsnv.prsnvt.toktyp)
                    {
                    case TOKTSYMBOL:
                        prsdef(ctx, &expr->prsnv.prsnvt, TOKSTFWDOBJ);
                        switch(expr->prsnv.prsnvt.toksym.tokstyp)
                        {
                        case TOKSTOBJ:
                        case TOKSTFWDOBJ:
                            typ = DAT_OBJECT;
                            val = valbuf;
                            oval = expr->prsnv.prsnvt.toksym.toksval;
                            oswp2(valbuf, oval);
                            break;

                        case TOKSTFUNC:
                        case TOKSTFWDFN:
                            typ = DAT_FNADDR;
                            val = valbuf;
                            oval = expr->prsnv.prsnvt.toksym.toksval;
                            oswp2(valbuf, oval);
                            break;

                        case TOKSTPROP:
                            typ = DAT_PROPNUM;
                            val = valbuf;
                            pval = expr->prsnv.prsnvt.toksym.toksval;
                            oswp2(valbuf, pval);
                            break;

                        default:
                            errsig(ctx->prscxerr, ERR_BADPVL);
                        }
                        break;

                    case TOKTDSTRING:
                        typ = DAT_DSTRING;
                        break;

                    case TOKTSSTRING:
                        typ = DAT_SSTRING;
                        break;
                        
                    case TOKTLIST:
                        /* set up for code/list generation */
                        mcmunlck(ctx->prscxmem, (mcmon)objn);
                        codeofs = objemt(ctx->prscxmem, objn, p, DAT_LIST);
                        ctx->prscxemt->emtcxptr = mcmlck(ctx->prscxmem,
                                                         (mcmon)objn);
                        ctx->prscxemt->emtcxofs = codeofs;
                        ctx->prscxemt->emtcxobj = objn;
                        
                        /* generate the list value */
                        emtlst(ctx->prscxemt, expr->prsnv.prsnvt.tokofs,
                               ctx->prscxpool);
                        
                        /* finish code/list generation */
                        objendemt(ctx->prscxmem, objn, p,
                                  ctx->prscxemt->emtcxofs);
                        objptr = ctx->prscxemt->emtcxptr;
                        goto skip_for_list;                 /* already set! */

                    case TOKTNUMBER:
                        typ = DAT_NUMBER;
                        val = valbuf;
                        oswp4s(valbuf, expr->prsnv.prsnvt.tokval);
                        break;
                        
                    case TOKTNIL:
                        typ = DAT_NIL;
                        break;
                        
                    case TOKTTRUE:
                        typ = DAT_TRUE;
                        break;
                        
                    case TOKTPOUND:
                        typ = DAT_PROPNUM;
                        val = valbuf;
                        pval = expr->prsnv.prsnvt.tokofs;
                        oswp2(valbuf, pval);
                        break;

                    default:
                        errsig(ctx->prscxerr, ERR_BADPVL);
                    }

                    /* now set the property value */
                    mcmunlck(ctx->prscxmem, (mcmon)objn);
                    objsetp(ctx->prscxmem, objn, p, typ, val, (objucxdef *)0);
                    objptr = mcmlck(ctx->prscxmem, (mcmon)objn);
                    
                skip_for_list:
                    /* take note of certain special properties */
                    switch(p)
                    {
                    case PRP_LOCATION:
                        if (typ == DAT_OBJECT) locobj = oval;
                        else if (typ == DAT_NIL) locnil = TRUE;
                        else locwarn = TRUE;
                        break;
                        
                    case PRP_CONTENTS:
                        contset = TRUE;
                        break;
                        
                    case PRP_LOCOK:
                        if (typ == DAT_TRUE) locok = TRUE;
                        break;
                    }
                }
            }
        }
    }
    
    /* error cleanup: unlock the object, restore context data */
    ERRCLEAN(ctx->prscxerr)
        ctx->prscxslcl = oldslcl;
        ctx->prscxplcl = oldplcl;
        prsdelgoto(ctx);
        ctx->prscxtok->tokcxstab = oldltab;
        mcmunlck(ctx->prscxmem, (mcmon)objn);
    ERRENDCLN(ctx->prscxerr)
        
    /* done parsing - unlock object to set some special properties */
    mcmtch(ctx->prscxmem, (mcmon)objn);
    mcmunlck(ctx->prscxmem, (mcmon)objn);

    /* add templates if any were defined */
    if (tplcur != 1)
        objsetp(ctx->prscxmem, objn, tplprop, tpldat, tpl, (objucxdef *)0);

    /* add contents for set-on-demand if not already set */
    if (!contset)
        objsetp(ctx->prscxmem, objn, PRP_CONTENTS, DAT_DEMAND, (void *)0,
                (objucxdef *)0);
    
    /* add location record */
    vociadd(ctx->prscxvoc, objn, locobj, numsc, sclist,
            (classflg | (hasvoc ? VOCIFVOC : 0) | (locnil ? VOCIFLOCNIL : 0)));
    
    /* issue location type warning if appropriate */
    if (locwarn && !locok)
        errlog1(ctx->prscxerr, ERR_LOCNOBJ, ERRTSTR,
                errstr(ctx->prscxerr, (char *)objtok->toknam, objtok->toklen));
    
    /* later: build an index on the properties if it's a class */
    indexprop = FALSE;
    if (indexprop) objindx(ctx->prscxmem, objn);

    /* resize the object:  exact size for classes, a little room for others */
    objptr = mcmlck(ctx->prscxmem, (mcmon)objn);
    freeofs = objfree(objptr);
    mcmunlck(ctx->prscxmem, (mcmon)objn);
    
    if (indexprop) freeofs += objnprop(objptr) * 4;

    mcmrealo(ctx->prscxmem, (mcmon)objn,
             (ushort)(freeofs + (classflg ? 0 : OBJEXTRA)));
    mcmunlck(ctx->prscxmem, (mcmon)objn);      /* unlock (realloc locks it) */
}

/* parse a special word definition */
static void prsspec(prscxdef *ctx, int modflag)
{
    uchar  *wrd;
    uchar  *p;
    int     tok;
    size_t  len;
    uchar  *typ;
    int     end_of_list;
    static  uchar wordtypes[] =
    {
        VOCW_OF, VOCW_AND, VOCW_THEN, VOCW_ALL, VOCW_BOTH,
        VOCW_BUT, VOCW_ONE, VOCW_ONES, VOCW_IT, VOCW_THEM,
        VOCW_HIM, VOCW_HER, VOCW_ANY, 0
    };
    static struct
    {
        char   typ;
        char  *def;
    } *defp, defaults[] =
    {
        /* keep equivalent words contiguous */
        { VOCW_ANY, "any" },
        { VOCW_ANY, "either" },
        { 0, 0}
    };

    /* delete old special words */
    if ((modflag == 0 || modflag == TOKTREPLACE) && ctx->prscxspp)
    {
        if (modflag == 0) errlog(ctx->prscxerr, ERR_RPLSPEC);
        mchfre(ctx->prscxspp);
        ctx->prscxspp = 0;
        ctx->prscxsps = 0;
        ctx->prscxspf = 0;
    }

    /* start the defaults at the bottom of the list */
    defp = defaults;

    /* loop through the word types */
    for (end_of_list = FALSE, typ = wordtypes ; *typ ; ++typ)
    {
        /* loop through synonyms for current word */
        do
        {
            if (!end_of_list
                && (tok = toknext(ctx->prscxtok)) != TOKTSSTRING
                && tok != TOKTNIL)
                errsig(ctx->prscxerr, ERR_BADSPEC);

            /* check for 'nil', which keeps original list intact */
            if (!end_of_list && tok == TOKTNIL)
            {
                if (modflag != TOKTMODIFY)
                    errsig(ctx->prscxerr, ERR_SPECNIL);
                tok = toknext(ctx->prscxtok);
                break;
            }

            /*
             *   If we've reached the end of the user's list, see if
             *   there are defaults.  If not, it's an error, since the
             *   list ended prematurely (the set of words defined in the
             *   v2.0 specialWords list did not have defaults; only words
             *   added later have defaults). 
             */
            if (end_of_list)
            {
                /*
                 *   Find this type in the defaults.  If not available,
                 *   it's an error. 
                 */
                while (defp->def && defp->typ != *typ)
                    ++defp;
                if (!defp->def)
                    errsig(ctx->prscxerr, ERR_BADSPEC);

                /* take this word */
            add_another_default:
                wrd = (uchar *)defp->def;
                len = strlen((char *)wrd);
            }
            else
            {
                /* get the word from the user's token */
                wrd = ctx->prscxpool + ctx->prscxtok->tokcxcur.tokofs;
                len = osrp2(wrd) - 2;
                wrd += 2;
            }
            
            /* make sure the word will fit - get more storage if not */
            if (!ctx->prscxspp
                || len + 2 > (size_t)(ctx->prscxsps - ctx->prscxspf))
            {
                uint   newsiz;
                uchar *newptr;
                
                newsiz = ctx->prscxsps + len + 128;
                newptr = mchalo(ctx->prscxerr, newsiz, "prsspec");
                
                /* copy old area, if there was one */
                if (ctx->prscxspp)
                {
                    memcpy(newptr, ctx->prscxspp, (size_t)ctx->prscxsps);
                    mchfre(ctx->prscxspp);
                }
                ctx->prscxspp = (char *)newptr;
                ctx->prscxsps = newsiz;
            }
            
            /* set up another entry in the list */
            p = (uchar *)ctx->prscxspp + ctx->prscxspf;
            *p++ = *typ;
            *p++ = len;
            memcpy(p, wrd, (size_t)len);
            p += len;
            ctx->prscxspf += len + 2;

            /*
             *   If we've hit the end of the list, and we're adding
             *   default words, add the next default word for this type.
             */
            if (end_of_list)
            {
                ++defp;
                if (defp->typ == *typ)
                    goto add_another_default;
            }

            /* get the next token, if we haven't already ended the list */
            if (!end_of_list)
                tok = toknext(ctx->prscxtok);
        } while (tok == TOKTEQ);

        /* if we hit a semicolon, it's the end of the list */
        if (!end_of_list && tok == TOKTSEM)
            end_of_list = TRUE;

        /* make sure a comma separates sets of words */
        if (*(typ + 1) && !end_of_list && tok != TOKTCOMMA)
            prssigreq(ctx, TOKTCOMMA);
    }
    
    prsreq(ctx, TOKTSEM);
}

/* parse a format string definition */
static void prsfmt(prscxdef *ctx)
{
    uchar  *wrd;
    uchar  *src;
    uchar  *dst;
    size_t  cnt;
    prpnum  p;
    uchar  *ptr;
    size_t  len;
    size_t  newlen;
    
    /* get string to be translated */
    if (toknext(ctx->prscxtok) != TOKTSSTRING)
        errsig(ctx->prscxerr, ERR_BADFMT);
    wrd = ctx->prscxpool + ctx->prscxtok->tokcxcur.tokofs;
    len = osrp2(wrd);
    
    /* convert any (\') sequences into just plain (') */
    for (src = dst = (wrd + 2), cnt = 0, newlen = len ;
         cnt < len - 2 ; *dst++ = *src++, ++cnt)
    {
        if (*src == '\\')
        {
            ++src;                          /* skip this character of input */
            ++cnt;                /* note that a character has been skipped */
            --newlen;                /* decrease length counter accordingly */
        }
    }
    len = newlen;
    oswp2(wrd, len);

    /* get property to translate it into */
    toknext(ctx->prscxtok);
    p = prsrqpr(ctx);
    
    /* add to format string list */
    if (!ctx->prscxfsp || len + 2 > (size_t)(ctx->prscxfss - ctx->prscxfsf))
    {
        uint   newsiz;
        uchar *newptr;
        
        /* get more storage */
        newsiz = ctx->prscxfss + len + 256;
        newptr = mchalo(ctx->prscxerr, newsiz, "prsfmt");
        
        /* set up new storage */
        if (ctx->prscxfsp)
        {
            memcpy(newptr, ctx->prscxfsp, (size_t)ctx->prscxfss);
            mchfre(ctx->prscxfsp);
        }
        ctx->prscxfsp = newptr;
        ctx->prscxfss = newsiz;
    }
    
    /* enter new format string information in format string storage */
    ptr = ctx->prscxfsp + ctx->prscxfsf;
    oswp2(ptr, p);
    memcpy(ptr + 2, wrd, (size_t)len);
    
    ctx->prscxfsf += len + 2;

    prsreq(ctx, TOKTSEM);                  /* statement ends with semicolon */
}

/* parse a compound word definition */
static void prscmpd(prscxdef *ctx)
{
    uchar *word1;
    uchar *word2;
    uchar *word3;
    uint   need;
    uchar *p;

    /* get 'word1' */
    if (toknext(ctx->prscxtok) != TOKTSSTRING)
        errsig(ctx->prscxerr, ERR_BADCMPD);
    word1 = ctx->prscxpool + ctx->prscxtok->tokcxcur.tokofs;

    /* get 'word2' */
    if (toknext(ctx->prscxtok) != TOKTSSTRING)
        errsig(ctx->prscxerr, ERR_BADCMPD);
    word2 = ctx->prscxpool + ctx->prscxtok->tokcxcur.tokofs;

    /* get 'resultword' */
    if (toknext(ctx->prscxtok) != TOKTSSTRING)
        errsig(ctx->prscxerr, ERR_BADCMPD);
    word3 = ctx->prscxpool + ctx->prscxtok->tokcxcur.tokofs;

    prsnreq(ctx, TOKTSEM);                 /* statement ends with semicolon */
    
    /* add the new compound word definition */
    need = osrp2(word1) + osrp2(word2) + osrp2(word3);
    if (!ctx->prscxcpp || need > (ctx->prscxcps - ctx->prscxcpf))
    {
        uint   newsiz;
        uchar *newptr;
        
        /* get more storage */
        newsiz = ctx->prscxcps + need + 256;
        newptr = mchalo(ctx->prscxerr, newsiz, "prscmpd");

        /* set up new storage */
        if (ctx->prscxcpp)
        {
            memcpy(newptr, ctx->prscxcpp, (size_t)ctx->prscxcps);
            mchfre(ctx->prscxcpp);
        }
        ctx->prscxcpp = (char *)newptr;
        ctx->prscxcps = newsiz;
    }
    
    /* copy the three parts into the compound word area */
    p = (uchar *)ctx->prscxcpp + ctx->prscxcpf;
    memcpy(p, word1, (size_t)osrp2(word1));
    p += osrp2(word1);
    memcpy(p, word2, (size_t)osrp2(word2));
    p += osrp2(word2);
    memcpy(p, word3, (size_t)osrp2(word3));
    p += osrp2(word3);
    
    ctx->prscxcpf += need;
}

/*
 *   Parse a function or object definition.  If the 'markcomp' flag is
 *   true, we'll mark any objects as in initial state (with objcomp) when
 *   done.  This flag should *not* by the command-line compiler when not
 *   compiling for debugging, because the objects shouldn't be marked
 *   until after preinit runs.  Under other circumstances (TADS/Pro,
 *   compiling for debugging), since preinit will be called before each
 *   new run of the game anyway, there is no need to wait until after
 *   preinit to objcomp each object, hence this can be done immediately
 *   after compilation.
 */
void prscode(prscxdef *ctx, int markcomp)
{
    int           t;
    noreg tokdef  tok;
    int           parms;
    uchar        *oldplcl;
    uint          oldslcl;
    int           numsc = 0;           /* number of superclasses for object */
    objnum        sc[PRSMAXSC];           /* superclasses of current object */
    objnum       *scp;
    int           classflg = 0;  /* VOCIFCLASS ==> object is a class object */
    toktdef      *oldltab;
    int           varargs;
    uint          curfr;
    toktldef     *ltab;
    int           oldflg;
    int           modflag = 0;                      /* modify/replace flags */
    
    NOREG((&tok))
    
    t = ctx->prscxtok->tokcxcur.toktyp;
    if (t == TOKTEOF) return;            /* end of file; nothing more to do */
    
    /* allow null statements */
    if (t == TOKTSEM)
    {
        toknext(ctx->prscxtok);
        return;
    }

    /* check for special "compound word" construction */
    if (t == TOKTCOMPOUND)
    {
        prscmpd(ctx);
        return;
    }
    
    /* check for special formatString construct */
    if (t == TOKTFORMAT)
    {
        prsfmt(ctx);
        return;
    }
    
    /* check for 'replace' or 'modify' keywords */
    if (t == TOKTREPLACE || t == TOKTMODIFY)
    {
        modflag = t;
        t = toknext(ctx->prscxtok);
    }

    /* check for specialWords construct */
    if (t == TOKTSPECIAL)
    {
        prsspec(ctx, modflag);
        return;
    }

    /* check for 'class' prefix */
    if (t == TOKTCLASS)
    {
        classflg = VOCIFCLASS;
        t = toknext(ctx->prscxtok);
    }

    if (t != TOKTSYMBOL) errsig(ctx->prscxerr, ERR_REQSYM);
    OSCPYSTRUCT(tok, ctx->prscxtok->tokcxcur);

    /* 'modify <object>' skips the colon and goes directly to the body */
    if (modflag == TOKTMODIFY)
    {
        mcmon   newobj;
        uchar  *newptr;
        uchar  *oldptr;
        char    newnam[TOKNAMMAX+1];
        int     len;
        ushort  objsiz;
        lindef *lin;
        
        /* require a previously defined object */
        if (tok.toksym.tokstyp != TOKSTOBJ
            || vocinh(ctx->prscxvoc, tok.toksym.toksval) == 0)
            errsig(ctx->prscxerr, ERR_MODOBJ);

        /* create a copy of the original object */
        objsiz = mcmobjsiz(ctx->prscxmem, (mcmon)tok.toksym.toksval);
        newptr = mcmalo(ctx->prscxmem, objsiz, &newobj);
        oldptr = mcmlck(ctx->prscxmem, (mcmon)tok.toksym.toksval);
        memcpy(newptr, oldptr, (size_t)objsiz);

        /* remember whether the original object was a class */
        if (objflg(newptr) & OBJFCLASS)
            classflg = VOCIFCLASS;

        /*
         *   Set the "superseded by modified object" flag in the
         *   original.  Also set the class flag, because we want the
         *   modified version to inherit location and vocabulary. 
         */
        objsflg(newptr, objflg(newptr) | OBJFMOD | OBJFCLASS);
        mcmtch(ctx->prscxmem, (mcmon)newobj);

        /* done with the objects - unlock them */
        mcmunlck(ctx->prscxmem, (mcmon)tok.toksym.toksval);
        mcmunlck(ctx->prscxmem, newobj);

        /* create a fake symbol table entry for the original data */
        len = tok.toksym.tokslen;
        if (len > TOKNAMMAX - 5) len = TOKNAMMAX - 5;
        sprintf(newnam, "%.*s@%d", len, tok.toksym.toksnam,
                (int)newobj);
        (*ctx->prscxstab->toktfadd)(ctx->prscxstab, newnam,
                                    (int)strlen(newnam), TOKSTOBJ,
                                    (int)newobj, tokhsh(newnam));

        /* the superclass for the new object is simply the old object */
        numsc = 1;
        sc[0] = newobj;

        /* renumber the inheritance records for the old object */
        vociren(ctx->prscxvoc, tok.toksym.toksval, newobj);

        /* go through all line sources and renumber this object */
        for (lin = ctx->prscxvoc->voccxrun->runcxdbg->dbgcxlin ; lin ;
             lin = lin->linnxt)
        {
            /* renumber instances of the object in this line source */
            linrenum(lin, tok.toksym.toksval, newobj);
        }

        /* go parse the object body as normal */
        toknext(ctx->prscxtok);
        goto objbody;
    }
    
    prsnreq(ctx, TOKTCOLON);
    t = ctx->prscxtok->tokcxcur.toktyp;
    switch(t)
    {
    case TOKTEXTERN:
        if (modflag) errlog(ctx->prscxerr, ERR_MODRPLX);
        prsnreq(ctx, TOKTFUNCTION);
        prsreq(ctx, TOKTSEM);
        prsdef(ctx, (tokdef *)&tok, TOKSTEXTERN);
        if (tok.toksym.tokstyp != TOKSTEXTERN)
            errsig(ctx->prscxerr, ERR_REQEXT);
        break;
        
    case TOKTFUNCTION:
        if (modflag == TOKTMODIFY) errlog(ctx->prscxerr, ERR_MODFCN);
        prsdef(ctx, (tokdef *)&tok, TOKSTFWDFN);
        t = toknext(ctx->prscxtok);
        if (t == TOKTSEM)
        {
            if (modflag) errlog(ctx->prscxerr, ERR_MODFWD);
            if (tok.toksym.tokstyp != TOKSTFWDFN &&
                tok.toksym.tokstyp != TOKSTFUNC)
                errsig(ctx->prscxerr, ERR_REQFCN);
            toknext(ctx->prscxtok);
            break;
        }

        /* check that we're not redefining the symbol, then define as fcn */
        if (!(tok.toksym.tokstyp == TOKSTFWDFN
              || (tok.toksym.tokstyp == TOKSTFUNC && modflag == TOKTREPLACE)))
        {
            /* log the error */
            errlog(ctx->prscxerr, ERR_FREDEF);

            /* 
             *   Since the symbol was already defined as something else,
             *   we haven't given it a proper function definition yet.  In
             *   particular, we haven't assigned an object number to the
             *   function.  Do so now by forcing the symbol to undefined
             *   then defining it as a function again.
             *   
             *   Note that we need to force the change in the global
             *   symbol table itself.  This will hose down any previous
             *   definition of the symbol, but this doesn't matter much
             *   since the game is already not playable due to this error.
             */

            /* set it to unknown */
            tok.toksym.tokstyp = TOKSTUNK;

            /* force it back into the global table as unknown */
            (*ctx->prscxstab->toktfset)(ctx->prscxstab,
                                        (toksdef *)&tok.toksym);

            /* define it as a forward function to assign an object ID */
            prsdef(ctx, (tokdef *)&tok, TOKSTFWDFN);
        }

        /* 
         *   make sure the symbol is in the symbol table as a function if
         *   it's not already - it could have been a forward function, in
         *   which case it's time to make it a defined function, since
         *   this is the definition 
         */
        tok.toksym.tokstyp = TOKSTFUNC;
        (*ctx->prscxstab->toktfset)(ctx->prscxstab, (toksdef *)&tok.toksym);
        
        oldltab = ctx->prscxtok->tokcxstab;           /* remember old table */
        oldplcl = ctx->prscxplcl;                /* and old table pool data */
        oldslcl = ctx->prscxslcl;

        varargs = FALSE;
        parms = 0;
        curfr = 0;                                /* no enclosing frame yet */
        ltab = (toktldef *)0;                  /* no local symbol table yet */
        if (t == TOKTLPAR)
        {
            ltab = prsvlst(ctx, (toktldef *)0, (prsndef **)0, &parms, TRUE,
                           &varargs, curfr);
            prsreq(ctx, TOKTRPAR);
        }
        
        /*
         *   If we're replacing this function, delete any references in
         *   line number records to this object; this will prevent the
         *   debugger from attempting to use these records, which will be
         *   invalid after we replace the object's pcode with the new
         *   pcode here.  
         */
        if (modflag == TOKTREPLACE)
        {
            lindef *lin;
            for (lin = ctx->prscxvoc->voccxrun->runcxdbg->dbgcxlin ; lin ;
                 lin = lin->linnxt)
            {
                /* delete instances of the object in this line source */
                lindelnum(lin, (objnum)tok.toksym.toksval);
            }
        }

        /* set up emit context for the new object */
        ctx->prscxemt->emtcxptr = mcmlck(ctx->prscxmem,
                                         (mcmon)tok.toksym.toksval);
        ctx->prscxemt->emtcxofs = 0;
        ctx->prscxemt->emtcxobj = tok.toksym.toksval;

        /* flag that we're doing a function - no "self" */
        oldflg = ctx->prscxflg;
        ctx->prscxflg |= PRSCXFFUNC;
        
        /* now parse the body of the function */
        ERRBEGIN(ctx->prscxerr)
            
        if (ctx->prscxflg & PRSCXFARC)
        {
            emtop(ctx->prscxemt, OPCCHKARGC);
            emtbyte(ctx->prscxemt, (varargs ? 0x80 : 0 ) + parms);
        }
        
        /* if there's a local symbol table, set up debug record */
        if (ltab) prsvgfr(ctx, ltab, &curfr);
        
        if (ctx->prscxtok->tokcxcur.toktyp != TOKTLBRACE)
            prssigreq(ctx, TOKTLBRACE);
        prsstm(ctx, EMTLLNKEND, EMTLLNKEND, parms, 0, 0, (prscsdef *)0,
               curfr);
        
        /* be sure to emit a 'return' at the end of the function */
        emtop(ctx->prscxemt, OPCRETURN);
        emtint2(ctx->prscxemt, 0);
        
        /* restore local symbol table information in context */
        ctx->prscxplcl = oldplcl;
        ctx->prscxslcl = oldslcl;
        
        /* resize the object down to the actual space needed */
        mcmrealo(ctx->prscxmem, (mcmon)tok.toksym.toksval,
                 (ushort)ctx->prscxemt->emtcxofs);
        
        /* tell cache manager the object's been changed, and unlock it */
        mcmtch(ctx->prscxmem, (mcmon)tok.toksym.toksval);
        mcmunlck(ctx->prscxmem, (mcmon)tok.toksym.toksval);

        /* to prevent stray writes to this object... */
        ctx->prscxemt->emtcxptr = (uchar *)0;
        
        /* clean up context changes, and delete labels */
        ctx->prscxtok->tokcxstab = oldltab;
        ctx->prscxflg = oldflg;
        prsdelgoto(ctx);

        ERRCLEAN(ctx->prscxerr)
            ctx->prscxplcl = oldplcl;
            ctx->prscxslcl = oldslcl;
            ctx->prscxtok->tokcxstab = oldltab;
            ctx->prscxflg = oldflg;
            prsdelgoto(ctx);
            mcmunlck(ctx->prscxmem, (mcmon)tok.toksym.toksval);
        ERRENDCLN(ctx->prscxerr)
        
        break;  
        
    case TOKTSYMBOL:
        for (scp = sc ;;)
        {
            int typ;
            
            /* check that we have room for a new superclass */
            if (numsc >= PRSMAXSC) errsig(ctx->prscxerr, ERR_MANYSC);

            /* define superclass symbol as object if not already done */
            prsdef(ctx, &ctx->prscxtok->tokcxcur, TOKSTFWDOBJ);
            typ = ctx->prscxtok->tokcxcur.toksym.tokstyp;
            if (typ != TOKSTOBJ && typ != TOKSTFWDOBJ)
                errsig(ctx->prscxerr, ERR_REQOBJ);
            
            /* add the object to the superclass array */
            *scp++ = ctx->prscxtok->tokcxcur.toksym.toksval;
            ++numsc;
            
            /* skip the token, and keep going as long as the list continues */
            if (toknext(ctx->prscxtok) != TOKTCOMMA) break;
            typ = toknext(ctx->prscxtok);        /* get next object in list */
        }
        goto objbody;
        /* FALLTHROUGH */
        
    case TOKTOBJECT:
        toknext(ctx->prscxtok);
    objbody:
        prsdef(ctx, (tokdef *)&tok, TOKSTFWDOBJ);
        if (!(tok.toksym.tokstyp == TOKSTFWDOBJ
              || (tok.toksym.tokstyp == TOKSTOBJ && modflag != 0)))
        {
            /* 
             *   it's already defined globally as something other than an
             *   object - log an error 
             */
            errlog(ctx->prscxerr, ERR_OREDEF);

            /* 
             *   actually redefine the symbol as an object, so that we can
             *   proceed with the compilation 
             */
            tok.toksym.tokstyp = TOKSTUNK;
            prsdefobj(ctx, (tokdef *)&tok, TOKSTFWDOBJ);
        }

        /* if we're replacing the object, delete its vocabulary records */
        if (modflag == TOKTREPLACE && tok.toksym.tokstyp == TOKSTOBJ)
        {
            vocidel(ctx->prscxvoc, (objnum)tok.toksym.toksval);
            vocdel(ctx->prscxvoc, (objnum)tok.toksym.toksval);
        }

        /* make the symbol refer to an object now */
        tok.toksym.tokstyp = TOKSTOBJ;
        (*ctx->prscxstab->toktfset)(ctx->prscxstab, (toksdef *)&tok.toksym);

        /* compile the object */
        prsobj(ctx, &tok, numsc, sc, classflg);
        if (markcomp)
            objcomp(ctx->prscxmem, (objnum)tok.toksym.toksval,
                    (ctx->prscxflg & PRSCXFLIN) != 0);
        
        prsreq(ctx, TOKTSEM);
        break;
        
    default:
        errsig(ctx->prscxerr, ERR_SYNTAX);
    }
}

