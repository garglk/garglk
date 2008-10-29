#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/DBGRUN.C,v 1.3 1999/07/11 00:46:29 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  dbgrun.c - functions needed when actually debugging
Function
  This module contains functions actually used while debugging, as
  opposed to functions needed in the compiler and/or runtime to set
  up debugging without activating an interactive debugger session.
  These functions are in a separate file to reduce the size of the
  runtime and compiler, which don't need to link these functions.
Notes
  None
Modified
  04/11/99 CNebel        - Fix signing errors.
  04/20/92 MJRoberts     - creation
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "os.h"
#include "dbg.h"
#include "tio.h"
#include "obj.h"
#include "prp.h"
#include "tok.h"
#include "run.h"
#include "tok.h"
#include "prs.h"
#include "linf.h"
#include "bif.h"
#include "lst.h"

/* indicate that the debugger is present */
int dbgpresent()
{
    return TRUE;
}

/* format a display of where execution is stopped */
void dbgwhere(dbgcxdef *ctx, char *p)
{
    dbgfdef *f;

    f = &ctx->dbgcxfrm[ctx->dbgcxfcn - 1];
    if (f->dbgftarg == MCMONINV)
        p += dbgnam(ctx, p, TOKSTBIFN, f->dbgfbif);
    else
        p += dbgnam(ctx, p,
                    (f->dbgfself == MCMONINV ? TOKSTFUNC : TOKSTOBJ),
                    (int)f->dbgftarg);

    if (f->dbgfself != MCMONINV && f->dbgfself != f->dbgftarg)
    {
        memcpy(p, "<self=", (size_t)6);
        p += 6;
        p += dbgnam(ctx, p, TOKSTOBJ, (int)f->dbgfself);
        *p++ = '>';
    }
    if (f->dbgfprop)
    {
        *p++ = '.';
        p += dbgnam(ctx, p, TOKSTPROP, (int)f->dbgfprop);
    }
    if (f->dbgfself == MCMONINV || f->dbgfbif)
    {
        *p++ = '(';
        *p++ = ')';
    }
    *p = '\0';
}

/*
 *   Evaluate breakpoint condition; returns TRUE if breakpoint condition
 *   was non-nil, FALSE if it was nil or had no value. 
 */
static int dbgbpeval(dbgcxdef *ctx, dbgbpdef *bp, dbgfdef *fr)
{
    uchar   *objptr;
    runsdef *oldsp;
    runsdef  val;
    int      err;
                
    objptr = mcmlck(ctx->dbgcxmem, bp->dbgbpcond);
            
    ERRBEGIN(ctx->dbgcxerr)
            
    oldsp = ctx->dbgcxrun->runcxsp;
    runexe(ctx->dbgcxrun, objptr, fr->dbgfself, bp->dbgbpcond,
           (prpnum)0, 0);
            
    ERRCATCH(ctx->dbgcxerr, err)
        /* no error recover - just proceed as normal */
        ;
    ERREND(ctx->dbgcxerr)

    /* done with condition code; unlock it */
    mcmunlck(ctx->dbgcxmem, bp->dbgbpcond);
            
    /* if condition evaluated to non-nil, break now */
    if (ctx->dbgcxrun->runcxsp != oldsp)
    {
        runpop(ctx->dbgcxrun, &val);
        if (val.runstyp == DAT_NIL)
            return FALSE;               /* value was nil - condition failed */
        else
            return TRUE;               /* value was non-nil - condition met */
    }
    else
        return FALSE;        /* expression returned no value - treat as nil */
}

/* single-step interrupt */
void dbgss(dbgcxdef *ctx, uint ofs, int instr, int err,
           uchar *noreg *instrp)
{
    dbgbpdef     *bp;
    int           i;
    dbgfdef      *fr;
    int           bphit = 0;
    voccxdef     *vcx = ctx->dbgcxrun->runcxvoc;
    unsigned int  exec_ofs;
    errdef       *inner_frame;
    int           stepping;

    /* don't re-enter debugger - return if running from tdb */
    if (ctx->dbgcxflg & DBGCXFIND)
        return;

    /*
     *   If the current debugger stack context is in a built-in function,
     *   and we can resume from an error, pop a level of context -- we
     *   always show the context inside of the p-code, because we want to
     *   be able to move the execution point before resuming execution.
     *   If we can't resume from an error, leave the stack as it is so
     *   that the user can see exactly which built-in function was called
     *   to cause the error.  
     */
    if (dbgu_err_resume(ctx))
    {
        /* remove any built-in functions from the top of the stack */
        while (ctx->dbgcxfcn > 1
               && ctx->dbgcxfrm[ctx->dbgcxfcn-1].dbgftarg == MCMONINV)
            ctx->dbgcxfcn--;
    }

    /* get the frame pointer */
    fr = (ctx->dbgcxfcn == 0 ? 0 : &ctx->dbgcxfrm[ctx->dbgcxfcn - 1]);

    /* presume we're not single-stepping */
    stepping = FALSE;

    /* 
     *   Check to see if we should stop due to single-stepping; if so,
     *   there's no need to consider breakpoints at all.
     */
    if ((ctx->dbgcxflg & DBGCXFSS) != 0)
    {
        /* single-step mode - check for step-over mode */
        if ((ctx->dbgcxflg & DBGCXFSO) != 0)
        {
            /* 
             *   step-over mode - stop only if we're in the stack level
             *   where we started step-over mode (not in a routine called
             *   from that stack level, which is what we're stepping over) 
             */
            stepping = (ctx->dbgcxdep <= ctx->dbgcxsof);
        }
        else
        {
            /* normal step-in mode - always stop */
            stepping = TRUE;
        }
    }

    /* 
     *   if we're not going to stop for single-stepping or because of a
     *   run-time error, look for breakpoints 
     */
    if (!stepping && err == 0)
    {
        /* check the instruction that cause debugger entry */
        if (instr == OPCBP)
        {
            /* look up breakpoint, and make sure 'self' is correct */
            for (i = 0, bp = ctx->dbgcxbp ; i < DBGBPMAX ; ++bp, ++i)
            {
                if (fr != 0
                    && (bp->dbgbpflg & DBGBPFUSED)
                    && !(bp->dbgbpflg & DBGBPFDISA)
                    && (bp->dbgbpself == fr->dbgfself
                        || bp->dbgbpself == MCMONINV
                        || bifinh(vcx, vocinh(vcx, fr->dbgfself),
                                  bp->dbgbpself))
                    && bp->dbgbptarg == fr->dbgftarg
                    && bp->dbgbpofs + 3 == ofs)
                    break;
            }
            
            /* if we didn't find the breakpoint, ignore the stop */
            if (i == DBGBPMAX)
                return;
        
            /* if this is a conditional bp, make sure condition is true */
            if (!(ctx->dbgcxflg & DBGCXFSS) && (bp->dbgbpflg & DBGBPFCOND)
                && !dbgbpeval(ctx, bp, fr))
                return;

            /* note that we hit a breakpoint */
            bphit = i + 1;
        }
        else
        {
            int brk;

            /* check for global breakpoints */
            if (fr != 0 && (ctx->dbgcxflg & DBGCXFGBP) != 0)
            {
                /*
                 *   Not single-stepping, but one or more global
                 *   breakpoints are in effect.  for each global
                 *   breakpoint, eval condition; if any are true, stop
                 *   execution, otherwise resume execution.  
                 */
                for (brk = FALSE, i = 0, bp = ctx->dbgcxbp ;
                     i < DBGBPMAX ; ++bp, ++i)
                {
                    if ((bp->dbgbpflg & DBGBPFUSED)
                        && !(bp->dbgbpflg & DBGBPFDISA)
                        && bp->dbgbptarg == MCMONINV
                        && dbgbpeval(ctx, bp, fr))
                    {
                        brk = TRUE;
                        bp->dbgbpflg |= DBGBPFDISA;         /* auto disable */
                        bphit = i + 1;
                        break;
                    }
                }
            }
            else
            {
                /* we're not at a global breakpoint */
                brk = FALSE;
            }

            /* if there's no breakpoint, don't stop */
            if (!brk)
                return;
        }
    }

    /* note that we're in the debugger, to avoid re-entry */
    ctx->dbgcxflg |= DBGCXFIND;

    /* 
     *   unlock the object, in case we mess with it in the debugger;
     *   before we do, though, remember the current execution offset
     *   within the object, so that we can restore the pointer before we
     *   return 
     */
    if (instrp != 0 && fr != 0 && fr->dbgftarg != MCMONINV)
    {
        exec_ofs = *instrp - mcmobjptr(ctx->dbgcxmem, (mcmon)fr->dbgftarg);
        mcmunlck(ctx->dbgcxmem, (mcmon)fr->dbgftarg);
    }
    else
        exec_ofs = 0;

    /* remember the current error frame, so we can copy its arguments */
    inner_frame = ctx->dbgcxerr->errcxptr;

    ERRBEGIN(ctx->dbgcxerr)
        /* 
         *   if we're stopping on an error, copy the error parameters from
         *   the inner frame so they'll be available for messages within
         *   this new enclosing frame 
         */
        if (err != 0)
            errcopyargs(ctx->dbgcxerr, inner_frame);
        
        /* call user-interface main command processor routine */
        dbgucmd(ctx, bphit, err, &exec_ofs);
    ERRCLEAN(ctx->dbgcxerr)
        /* make sure we clear the in-debugger flag if we exit via an error */
        ctx->dbgcxflg &= ~DBGCXFIND;
    ERRENDCLN(ctx->dbgcxerr);

    /* relock the object */
    if (instrp != 0 && fr != 0 && fr->dbgftarg != MCMONINV)
        *instrp = mcmlck(ctx->dbgcxmem, (mcmon)fr->dbgftarg) + exec_ofs;

    /* we're no longer in the debugger */        
    ctx->dbgcxflg &= ~DBGCXFIND;
}

/* activate debugger; returns TRUE if no debugger is present */
int dbgstart(dbgcxdef *ctx)
{
    if (!ctx || !(ctx->dbgcxflg & DBGCXFOK)) return TRUE;
    ctx->dbgcxflg |= DBGCXFSS;                 /* activate single-step mode */
    return FALSE;
}

/* save a text string in the dbgcxname buffer; TRUE ==> can't store it */
static int dbgnamsav(dbgcxdef *ctx, char *nam, uint *ofsp)
{
    size_t len = strlen(nam) + 1;
    
    if (ctx->dbgcxnam
        && ctx->dbgcxnamf + len < ctx->dbgcxnams)
    {
        memcpy(ctx->dbgcxnam + ctx->dbgcxnamf, nam, len);
        *ofsp = ctx->dbgcxnamf;
        ctx->dbgcxnamf += len;
        return FALSE;
    }
    else
        return TRUE;                      /* uanble to store - return error */
}

/* delete a string from the dbgcxname area, adjusting bp/wx offsets */
static void dbgnamdel(dbgcxdef *ctx, uint ofs)
{
    int       i;
    dbgbpdef *bp;
    dbgwxdef *wx;
    uint      delsiz;
    
    /* if no name text is being stored, we need do nothing */
    if (!ctx->dbgcxnam) return;
    
    /* compute size of area to be deleted */
    delsiz = strlen(ctx->dbgcxnam + ofs) + 1;
    
    /* go through breakpoints, moving text if necessary */
    for (i = DBGBPMAX, bp = ctx->dbgcxbp ; i ; ++bp, --i)
    {
        if ((bp->dbgbpflg & DBGBPFUSED) && (bp->dbgbpflg & DBGBPFNAME)
            && bp->dbgbpnam > ofs)
            bp->dbgbpnam -= delsiz;
        if ((bp->dbgbpflg & DBGBPFUSED) && (bp->dbgbpflg & DBGBPFCONDNAME)
            && bp->dbgbpcondnam > ofs)
            bp->dbgbpcondnam -= delsiz;
    }
    
    /* do the same for the watch expressions */
    for (i = DBGWXMAX, wx = ctx->dbgcxwx ; i ; ++wx, --i)
    {
        if ((wx->dbgwxflg & DBGWXFUSED) && (wx->dbgwxflg & DBGWXFNAME)
            && wx->dbgwxnam > ofs)
            wx->dbgwxnam -= delsiz;
    }
    
    /* now actually remove the string from the dbgcxname area */
    if (ctx->dbgcxnamf - ofs - delsiz)
        memmove(ctx->dbgcxnam + ofs, ctx->dbgcxnam + ofs + delsiz,
                (size_t)(ctx->dbgcxnamf - ofs - delsiz));
    ctx->dbgcxnamf -= delsiz;
}

/*
 *   Set a breakpoint at an object + offset location, optionally with a
 *   condition.  We'll find a new slot for the breakpoint.  
 */
int dbgbpat(dbgcxdef *ctx, objnum target, objnum self,
            uint ofs, int *bpnum, char *bpname, int toggle, char *cond,
            int *did_set)
{
    int       i;
    dbgbpdef *bp;

    /* find a slot for the new breakpoint */
    for (i = 0, bp = ctx->dbgcxbp ; i < DBGBPMAX ; ++bp, ++i)
    {
        /* if this one's free, use it */
        if (!(bp->dbgbpflg & DBGBPFUSED))
            break;
    }

    /*
     *   Note that we don't check for a valid slot yet, because we may not
     *   end up setting a new breakpoint. 
     */

    /* tell the caller which breakpoint we set */
    *bpnum = i + 1;

    /* go set the breakpoint at the ID we allocated */
    return dbgbpatid(ctx, *bpnum, target, self, ofs,
                     bpname, toggle, cond, did_set);
}

/* 
 *   Set a breakpoint at an object + offset location, optionally with a
 *   condition, using an existing breakpoint slot.  If the slot is already
 *   in use, we'll return an error.  
 */
int dbgbpatid(dbgcxdef *ctx, int bpnum, objnum target, objnum self,
              uint ofs, char *bpname, int toggle, char *cond,
              int *did_set)
{
    uchar    *objp;
    int       done;
    dbgbpdef *bp;
    int       err;
    objnum    condobj = MCMONINV;

    /* get the slot */
    --bpnum;
    bp = &ctx->dbgcxbp[bpnum];

    /* if this is a global breakpoint, no OPCBP is needed */
    if (target == MCMONINV)
    {
        /* compile the condition, with no local frame */
        dbgfdef fr;

        /* make sure the slot is valid and not in use */
        if (bpnum == DBGBPMAX)
            return ERR_MANYBP;
        else if ((bp->dbgbpflg & DBGBPFUSED) != 0)
            return ERR_BPINUSE;
        
        fr.dbgffr = 0;
        fr.dbgftarg = MCMONINV;
        if ((err = dbgcompile(ctx, cond, &fr, &condobj, FALSE)) != 0)
            return err;

        /* set up the breakpoint record */
        bp->dbgbpflg  = DBGBPFUSED | DBGBPFCOND;
        bp->dbgbpself =
        bp->dbgbptarg = MCMONINV;
        bp->dbgbpofs  = 0;
        bp->dbgbpcond = condobj;

        /* save the name */
        if (!dbgnamsav(ctx, bpname, &bp->dbgbpnam))
            bp->dbgbpflg |= DBGBPFNAME;

        /* save the condition */
        if (!dbgnamsav(ctx, cond, &bp->dbgbpcondnam))
            bp->dbgbpflg |= DBGBPFCONDNAME;

        /* it's a global breakpoint */
        ctx->dbgcxflg |= DBGCXFGBP;

        /* success */
        return 0;
    }

    /* lock the object */
    objp = mcmlck(ctx->dbgcxmem, (mcmon)target);
    
    /* skip any ENTER, CHKARGC, or FRAME instructions */
    for (done = FALSE ; !done ; )
    {
        switch(*(objp + ofs))
        {
        case OPCENTER:
            ofs += 3;
            break;
        case OPCCHKARGC:
            ofs += 2;
            break;
        case OPCFRAME:
            ofs += 1 + osrp2(objp + ofs + 1);
            break;
        default:
            done = TRUE;
            break;
        }
    }
    
    /* presume we're going to set a new breakpoint */
    if (did_set != 0)
        *did_set = TRUE;

    /* see what kind of instruction we've found */
    if (*(objp + ofs) == OPCBP)
    {
        /* if we're toggling and not adding a condition, remove the bp */
        if (toggle && (cond == 0 || *cond == '\0'))
        {
            int i;
            
            /* tell the caller we're removing a breakpoint */
            if (did_set != 0)
                *did_set = FALSE;
            
            /* remove all breakpoints matching this description */
            for (i = 0, bp = ctx->dbgcxbp ; i < DBGBPMAX ; ++bp, ++i)
            {
                if ((bp->dbgbpflg & DBGBPFUSED) != 0
                    && bp->dbgbptarg == target
                    && bp->dbgbpofs == ofs
                    && bp->dbgbpself == self)
                {
                    err = dbgbpdel(ctx, i+1);
                    goto return_error;
                }
            }
        }

        /* no error */
        err = 0;
    }
    else if (*(objp + ofs) != OPCLINE)
    {
        /* can't set a breakpoint here - it's not a line */
        err = ERR_BPNOTLN;
    }
    else if (bpnum == DBGBPMAX)
    {
        /* no slots left for a new breakpoint */
        err = ERR_MANYBP;
    }
    else if ((bp->dbgbpflg & DBGBPFUSED) != 0)
    {
        /* slot is already in use */
        return ERR_BPINUSE;
    }
    else
    {
        /* presume we won't have an error */
        err = 0;
    }

    /* check for a condition attached to the breakpoint */
    if (cond && *cond)
    {
        dbgfdef fr;

        fr.dbgffr = osrp2(objp + ofs + 2);
        fr.dbgftarg = target;
        err = dbgcompile(ctx, cond, &fr, &condobj, FALSE);
    }

    if (err == 0)
    {
        /* set up the breakpoint structure */
        bp->dbgbpflg = DBGBPFUSED;
        bp->dbgbpself = self;
        bp->dbgbptarg = target;
        bp->dbgbpofs = ofs;
        
        if (cond && *cond)
        {
            /* remember the condition */
            bp->dbgbpcond = condobj;
            bp->dbgbpflg |= DBGBPFCOND;
            if (!dbgnamsav(ctx, cond, &bp->dbgbpcondnam))
                bp->dbgbpflg |= DBGBPFCONDNAME;
        }

        /* 
         *   replace the OPCLINE in the p-code with a breakpoint
         *   instruction, so that the interpreter actually stops when it
         *   reaches this point 
         */
        *(objp + ofs) = OPCBP;
        mcmtch(ctx->dbgcxmem, (mcmon)target);
    }

return_error:
    mcmunlck(ctx->dbgcxmem, (mcmon)target);
    
    /* free condition object if we have one */
    if (err && condobj != MCMONINV)
        mcmfre(ctx->dbgcxmem, condobj);
    
    /* store breakpoint name for displaying to user */
    if (!err && !dbgnamsav(ctx, bpname, &bp->dbgbpnam))
        bp->dbgbpflg |= DBGBPFNAME;

    return err;
}

/*
 *   Set a new condition for an existing breakpoint
 */
int dbgbpsetcond(dbgcxdef *ctx, int bpnum, char *cond)
{
    dbgbpdef *bp;
    int       err;
    objnum    condobj;
    dbgfdef   fr;
    
    /* make sure we have a valid breakpoint */
    --bpnum;
    bp = &ctx->dbgcxbp[bpnum];
    if (bpnum < 0 || bpnum >= DBGBPMAX || !(bp->dbgbpflg & DBGBPFUSED))
        return ERR_BPNSET;

    /* check to see if it's a global breakpoint */
    if (bp->dbgbptarg == MCMONINV)
    {
        /* it's global - compile the condition with no local frame */
        fr.dbgffr = 0;
        fr.dbgftarg = MCMONINV;
        err = dbgcompile(ctx, cond, &fr, &condobj, FALSE);
    }
    else
    {
        uchar *objp;

        /* lock the object containing the code containing the breakpoint */
        objp = mcmlck(ctx->dbgcxmem, (mcmon)bp->dbgbptarg);

        /* compile the condition */
        fr.dbgffr = osrp2(objp + bp->dbgbpofs + 2);
        fr.dbgftarg = bp->dbgbptarg;
        err = dbgcompile(ctx, cond, &fr, &condobj, FALSE);

        /* done with the object */
        mcmunlck(ctx->dbgcxmem, (mcmon)bp->dbgbptarg);
    }

    /* don't proceed if we couldn't compile the expression */
    if (err != 0)
        return err;

    /* delete the old condition object */
    if (bp->dbgbpflg & DBGBPFCOND)
        mcmfre(ctx->dbgcxmem, bp->dbgbpcond);
    
    /* delete the old condition name */
    if (bp->dbgbpflg & DBGBPFCONDNAME)
    {
        dbgnamdel(ctx, bp->dbgbpcondnam);
        bp->dbgbpflg &= ~DBGBPFCONDNAME;
    }

    /* remember the new condition object */
    bp->dbgbpcond = condobj;
    
    /* remember the new condition name */
    if (!dbgnamsav(ctx, cond, &bp->dbgbpcondnam))
        bp->dbgbpflg |= DBGBPFCONDNAME;
    
    /* success */
    return 0;
}

/*
 *   Determine if there's a breakpoint at a given code location.  If there
 *   is, we fill in *bpnum with the identifier for the breakpoint and
 *   return true, otherwise we return false.  
 */
int dbgisbp(dbgcxdef *ctx, objnum target, objnum self, uint ofs, int *bpnum)
{
    dbgbpdef *bp;
    int       i;

    /* scan the breakpoints for one matching the given description */
    for (i = 0, bp = ctx->dbgcxbp ; i < DBGBPMAX ; ++bp, ++i)
    {
        /* check to see if this one matches the one we're looking for */
        if ((bp->dbgbpflg & DBGBPFUSED) != 0
            && bp->dbgbptarg == target
            && bp->dbgbpself == self
            && bp->dbgbpofs == ofs)
        {
            /* 
             *   this is the one - set its ID (which is 1-based) and
             *   return success 
             */
            if (bpnum != 0)
                *bpnum = i+1;
            return TRUE;
        }
    }

    /* didn't find any breakpoints matching this description */
    return FALSE;
}

/*
 *   Determine if the given breakpoint is enabled 
 */
int dbgisbpena(dbgcxdef *ctx, int bpnum)
{
    dbgbpdef *bp;

    /* make sure the identifier is in range */
    --bpnum;
    if (bpnum < 0 || bpnum >= DBGBPMAX)
        return FALSE;

    /* get the breakpoint */
    bp = &ctx->dbgcxbp[bpnum];

    /* return true if it exists and isn't disabled, false otherwise */
    return ((bp->dbgbpflg & DBGBPFUSED) != 0
            && (bp->dbgbpflg & DBGBPFDISA) == 0);
}

/* set a breakpoint at a symbolic address: function or object.property */
int dbgbpset(dbgcxdef *ctx, char *addr, int *bpnum)
{
    char     *p;
    char      psav;
    char     *tok1;
    char      tok1sav;
    char     *tok1end;
    char     *tok2;
    toksdef   sym1;
    toksdef   sym2;
    toktdef  *tab;
    uint      hsh1;
    uint      hsh2;
    objnum    objn;
    uint      ofs;
    uchar    *objp;
    prpdef   *prp;
    dattyp    typ;
    objnum    target;
    char     *end;
    char      buf1[40];
    char      buf2[40];
    
    /* determine what kind of address we have */
    for (p = addr ; *p && t_isspace(*p) ; ++p) ;
    for (tok1 = addr = p ; *p && TOKISSYM(*p) ; ++p) ;
    
    /* see if the very first thing is "when" */
    if (!strnicmp(tok1, "when ", (size_t)5))
    {
        for (end = tok1 + 5 ; t_isspace(*end) ; ++end) ;
        return(dbgbpat(ctx, MCMONINV, MCMONINV, 0, bpnum,
                       addr, FALSE, end, 0));
    }
    
    /* see if we have a second token */
    for (tok1end = p ; t_isspace(*p) ; ++p) ;
    if (*p == '.')
    {
        for (++p ; t_isspace(*p) ; ++p) ;
        for (tok2 = p ; TOKISSYM(*p) ; ++p) ;
        psav = *p;
        *p = '\0';
        end = (psav ? p + 1 : p);
    }
    else
    {
        tok2 = (char *)0;
        end = tok1end;
        if (*end) ++end;
        p = (char *)0;
    }
    
    tok1sav = *tok1end;
    *tok1end = '\0';
    
    /* look up the symbols */
    tab = (toktdef *)ctx->dbgcxtab;
    if (ctx->dbgcxprs->prscxtok->tokcxflg & TOKCXCASEFOLD)
    {
        strcpy(buf1, tok1);
        os_strlwr(buf1);
        tok1 = buf1;
    }
    hsh1 = tokhsh(tok1);
    if (!(*tab->toktfsea)(tab, tok1, (int)strlen(tok1), hsh1, &sym1))
        return ERR_BPSYM;
    objn = sym1.toksval;
    
    if (tok2)
    {
        /* we have "object.property" */
        if (ctx->dbgcxprs->prscxtok->tokcxflg & TOKCXCASEFOLD)
        {
            strcpy(buf2, tok2);
            os_strlwr(buf2);
            tok2 = buf2;
        }
        hsh2 = tokhsh(tok2);
        if (!(*tab->toktfsea)(tab, tok2, (int)strlen(tok2), hsh2, &sym2))
            return ERR_BPSYM;
        if (sym1.tokstyp != TOKSTOBJ) return ERR_BPOBJ;
        
        /* we need to look up the property */
        ofs = objgetap(ctx->dbgcxmem, objn, (prpnum)sym2.toksval,
                       &target, FALSE);
        if (!ofs) return ERR_BPNOPRP;
        
        /* make sure the property is code */
        objp = mcmlck(ctx->dbgcxmem, (mcmon)target);
        prp = objofsp((objdef *)objp, ofs);
        typ = prptype(prp);
        ofs = ((uchar *)prpvalp(prp)) - objp;
        mcmunlck(ctx->dbgcxmem, (mcmon)target);
        
        if (typ != DAT_CODE) return ERR_BPPRPNC;
    }
    else
    {
        /* we have just "function" */
        if (sym1.tokstyp != TOKSTFUNC) return ERR_BPFUNC;
        ofs = 0;                   /* functions always start at offset zero */
        target = objn;     /* for function, target is always same as object */
        objn = MCMONINV;               /* there is no "self" for a function */
    }
    
    /* undo our changes to the string text */
    if (p) *p = psav;
    *tok1end = tok1sav;
    
    /* check for a "when" expression */
    if (*end)
    {
        while (t_isspace(*end)) ++end;
        if (*end)
        {
            if (strnicmp(end, "when ", (size_t)5))
                return ERR_EXTRTXT;
            for (end += 5 ; t_isspace(*end) ; ++end) ;
        }
    }

    /* set a breakpoint at this object + offset */
    return(dbgbpat(ctx, target, objn, ofs, bpnum, addr, FALSE, end, 0));
}

/* delete a breakpoint */
int dbgbpdel(dbgcxdef *ctx, int bpnum)
{
    dbgbpdef *bp = &ctx->dbgcxbp[--bpnum];
    dbgbpdef *bp2;
    uchar    *objp;
    int       i;
    int       bpset;
    
    /* make sure it's a valid breakpoint */
    if (bpnum < 0 || bpnum >= DBGBPMAX || !(bp->dbgbpflg & DBGBPFUSED))
        return ERR_BPNSET;
    
    /* see if we now have NO breakpoints set on this location */
    for (bpset = FALSE, i = DBGBPMAX, bp2 = ctx->dbgcxbp ; i ; ++bp2, --i)
    {
        if (bp != bp2 && (bp2->dbgbpflg & DBGBPFUSED)
            && bp2->dbgbptarg == bp->dbgbptarg
            && bp2->dbgbpofs  == bp->dbgbpofs)
        {
            bpset = TRUE;
            break;
        }
    }
    
    /* if no other bp's here, convert the OPCBP back into an OPCLINE */
    if (!bpset)
    {
        if (bp->dbgbptarg == MCMONINV)
        {
            /* no more global breakpoints - delete global BP flag */
            ctx->dbgcxflg &= ~DBGCXFGBP;
        }
        else
        {
            /* convert the OPCBP back into OPCLINE */
            objp = mcmlck(ctx->dbgcxmem, (mcmon)bp->dbgbptarg);
            *(objp + bp->dbgbpofs) = OPCLINE;
            mcmtch(ctx->dbgcxmem, (mcmon)bp->dbgbptarg);
            mcmunlck(ctx->dbgcxmem, (mcmon)bp->dbgbptarg);
        }
    }

    /* delete the names from the buffer */
    if (bp->dbgbpflg & DBGBPFNAME)
        dbgnamdel(ctx, bp->dbgbpnam);       /* delete name from text buffer */
    if (bp->dbgbpflg & DBGBPFCONDNAME)
        dbgnamdel(ctx, bp->dbgbpcondnam);

    /* the slot is no longer in use */
    bp->dbgbpflg &= ~DBGBPFUSED;

    /* free condition object, if one has been allocated */
    if (bp->dbgbpflg & DBGBPFCOND)
    {
        mcmfre(ctx->dbgcxmem, bp->dbgbpcond);
        bp->dbgbpflg &= ~DBGBPFCOND;
    }
    
    return 0;
}

/* list breakpoints using user callback */
void dbgbplist(dbgcxdef *ctx,
               void (*dispfn)(void *ctx, const char *str, int len),
               void *dispctx)
{
    dbgbpdef *bp = ctx->dbgcxbp;
    int       i;
    char     *p;
    char      buf[10];

    /* if we're not recording names, there's nothing we can do */
    if (!ctx->dbgcxnam) return;
    
    for (i = 0 ; i < DBGBPMAX ; ++bp, ++i)
    {
        if ((bp->dbgbpflg & DBGBPFUSED) && (bp->dbgbpflg & DBGBPFNAME))
        {
            p = ctx->dbgcxnam + bp->dbgbpnam;
            sprintf(buf, "%d: ", i + 1);
            (*dispfn)(dispctx, buf, (int)strlen(buf));
            (*dispfn)(dispctx, p, (int)strlen(p));
            
            if (bp->dbgbpflg & DBGBPFDISA)
                (*dispfn)(dispctx, " [disabled]", 11);
            
            (*dispfn)(dispctx, "\n", 1);
        }
    }
}

/* enumerate breakpoints */
void dbgbpenum(dbgcxdef *ctx,
               void (*cbfunc)(void *cbctx, int bpnum, const char *desc,
                              const char *cond, int disabled), void *cbctx)
{
    dbgbpdef *bp = ctx->dbgcxbp;
    int       i;
    char     *bpname;
    char     *cond;

    /* if we're not recording names, there's nothing we can do */
    if (ctx->dbgcxnam == 0)
        return;

    /* run through our list of breakpoints */
    for (i = 0 ; i < DBGBPMAX ; ++bp, ++i)
    {
        /* 
         *   if this slot is in use, and has a name, include it in the
         *   enumeration 
         */
        if ((bp->dbgbpflg & DBGBPFUSED) && (bp->dbgbpflg & DBGBPFNAME))
        {
            /* get the name */
            bpname = ctx->dbgcxnam + bp->dbgbpnam;

            /* if there's a condition, get the condition */
            cond = ((bp->dbgbpflg & DBGBPFCONDNAME) != 0
                    ? ctx->dbgcxnam + bp->dbgbpcondnam : 0);

            /* invoke the callback */
            (*cbfunc)(cbctx, i + 1, bpname, cond,
                      (bp->dbgbpflg & DBGBPFDISA) != 0);
        }
    }
}

/* enable/disable a breakpoint */
int dbgbpdis(dbgcxdef *ctx, int bpnum, int disable)
{
    dbgbpdef *bp = &ctx->dbgcxbp[--bpnum];
    
    /* make sure it's a valid breakpoint */
    if (bpnum < 0 || bpnum >= DBGBPMAX || !(bp->dbgbpflg & DBGBPFUSED))
        return ERR_BPNSET;
    
    if (disable) bp->dbgbpflg |= DBGBPFDISA;
    else bp->dbgbpflg &= ~DBGBPFDISA;
    return 0;
}

/*
 *   Call user callback with lindef data for each breakpoint; global
 *   breakpoints are NOT passed to user callback, since they don't
 *   correspond to source locations.
 */
void dbgbpeach(dbgcxdef *ctx, void (*fn)(void *, int, uchar *, uint),
               void *fnctx)
{
    int       i;
    dbgbpdef *bp = ctx->dbgcxbp;
    uchar    *p;
    uint      len;

    for (i = 0 ; i < DBGBPMAX ; ++bp, ++i)
    {
        if ((bp->dbgbpflg & DBGBPFUSED)
            && bp->dbgbptarg != MCMONINV)
        {
            p = mcmlck(ctx->dbgcxmem, (mcmon)bp->dbgbptarg);
            p += bp->dbgbpofs + 1;
            len = *p - 4;
            p += 3;
            (*fn)(fnctx, (int)(*p), p + 1, len);
            mcmunlck(ctx->dbgcxmem, (mcmon)bp->dbgbptarg);
        }
    }
}

/* 
 *   Get information on a specific breakpoint.  Returns zero on success,
 *   non-zero on failure.  
 */
int dbgbpgetinfo(dbgcxdef *ctx, int bpnum, char *descbuf, size_t descbuflen,
                 char *condbuf, size_t condbuflen)
{
    dbgbpdef *bp = &ctx->dbgcxbp[--bpnum];

    /* make sure it's a valid breakpoint */
    if (bpnum < 0 || bpnum >= DBGBPMAX || !(bp->dbgbpflg & DBGBPFUSED))
        return ERR_BPNSET;

    /* if the caller wants the name, copy the name */
    if (descbuf != 0 && descbuflen != 0)
    {
        /* check to make sure we have a name */
        if ((bp->dbgbpflg & DBGBPFNAME) != 0)
        {
            size_t outlen;
            char *bpname;

            /* get the name */
            bpname = ctx->dbgcxnam + bp->dbgbpnam;

            /* limit the copy to the buffer size */
            outlen = strlen(bpname);
            if (outlen > descbuflen - 1)
                outlen = descbuflen - 1;

            /* copy it and null-terminate the result */
            memcpy(descbuf, bpname, outlen);
            descbuf[outlen] = '\0';
        }
        else
        {
            /* there's no name; clear out the caller's buffer */
            descbuf[0] = '\0';
        }
    }

    /* if the caller wants the condition, copy it */
    if (condbuf != 0 && condbuflen != 0)
    {
        /* check to make sure we have a name */
        if ((bp->dbgbpflg & DBGBPFCONDNAME) != 0)
        {
            size_t outlen;
            char *condname;

            /* get the name */
            condname = ctx->dbgcxnam + bp->dbgbpcondnam;

            /* limit the copy to the buffer size */
            outlen = strlen(condname);
            if (outlen > condbuflen - 1)
                outlen = condbuflen - 1;

            /* copy it and null-terminate the result */
            memcpy(condbuf, condname, outlen);
            condbuf[outlen] = '\0';
        }
        else
        {
            /* there's no name; clear out the caller's buffer */
            condbuf[0] = '\0';
        }
    }

    /* success */
    return 0;
}


/* ====================== debug line source routines ====================== */
static int dbglinget(lindef *lin)
{
    if (lin->linflg & LINFEOF)
        return TRUE;           /* expression has only one line - now at EOF */
    else
    {
        lin->linflg |= LINFEOF;
        return FALSE;
    }
}

static void dbglincls(lindef *lin)
{
    VARUSED(lin);
}

/* never called */
static void dbglinpos(lindef *lin, char *buf, uint bufl)
{
    VARUSED(lin);
    VARUSED(buf);
    VARUSED(bufl);
}

/* never called */
static void dbglinglo(lindef *lin, uchar *buf)
{
    VARUSED(lin);
    VARUSED(buf);
}

/* never called */
static int dbglinwrt(lindef *lin, osfildef *fp)
{
    VARUSED(lin);
    VARUSED(fp);
    return TRUE;
}

/* never called */
static void dbglincmp(lindef *lin, uchar *buf)
{
    VARUSED(lin);
    VARUSED(buf);
}

/* maximum number of symbols that can be mentioned in an eval expression */
#define DBGMAXSYM 30

struct toktfdef
{
    toktdef   toktfsc;
    mcmcxdef *toktfmem;
    dbgcxdef *toktfdbg;
    dbgfdef  *toktffr;                       /* frame to be used for symbol */
};
typedef struct toktfdef toktfdef;

/* search debug frame tables for a symbol */
int dbgtabsea(toktdef *tab, char *name, int namel, int hash, toksdef *ret)
{
    dbgfdef  *fr;
    uchar    *objp;
    toktfdef *ftab = (toktfdef *)tab;
    mcmcxdef *mctx = ftab->toktfmem;
    uint      len;
    uint      symlen;
    uint      symval;
    uint      ofs;
    uint      encofs;
    uchar    *framep;

    fr = ftab->toktffr;
    
    /* if there is no local context, we obviously can't find a symbol */
    if (fr->dbgftarg == MCMONINV)
        return FALSE;
    
    /* lock object and get its frame pointer */
    objp = mcmlck(mctx, fr->dbgftarg);

    for (ofs = fr->dbgffr ; ofs ; ofs = encofs)
    {
        /* look through this frame table */
        framep = objp + ofs;
        len = osrp2(framep) - 4;                 /* get length out of table */
        encofs = osrp2(framep + 2);         /* get enclosing frame's offset */
        framep += 4;                                     /* skip the header */
            
        while (len)
        {
            symval = osrp2(framep);
            framep += 2;
            symlen = *framep++;
            len -= 3 + symlen;
                
            if (symlen == (uint)namel && !memcmp(name, framep, (size_t)namel))
            {
                /* unlock the object and set up the toksdef for return */
                mcmunlck(mctx, fr->dbgftarg);
                ret->toksval = symval;
                ret->tokstyp = TOKSTLOCAL;
                ret->tokslen = namel;
                ret->toksfr  = ofs;
                return TRUE;
            }
            
            /* advance past this symbol */
            framep += symlen;
        }
    }

    /* nothing found - unlock object and return */
    mcmunlck(mctx, fr->dbgftarg);
    return FALSE;
}

/* compile an expression; returns error if one occurs */
int dbgcompile(dbgcxdef *ctx, char *expr, dbgfdef *fr, objnum *objnp,
               int speculative)
{
    lindef    lin;
    ushort    oldflg = 0;
    int       err;
    prscxdef *prs = ctx->dbgcxprs;
    toktfdef  tab;
    mcmon     objn;
    
    /* set up a lindef to provide the source line to the compiler */
    prs->prscxtok->tokcxlin = &lin;
    lin.lingetp = dbglinget;
    lin.linclsp = dbglincls;
    lin.linppos = dbglinpos;
    lin.linglop = dbglinglo;
    lin.linwrtp = dbglinwrt;
    lin.lincmpp = dbglincmp;
    lin.linpar = (lindef *)0;
    lin.linnxt = (lindef *)0;
    lin.linid = 0;
    lin.linbuf = expr;
    lin.linflg = LINFNOINC;
    lin.linlen = strlen(expr);
    lin.linlln = 0;
    
    /* set emit frame object */
    prs->prscxemt->emtcxfrob = fr->dbgftarg;
    
    /* set up special frame-searching symbol table */
    tab.toktfsc.toktfadd = 0;      /* we'll never add symbols to this table */
    tab.toktfsc.toktfsea = dbgtabsea;        /* we will, however, search it */
    tab.toktfsc.toktnxt  = prs->prscxtok->tokcxstab;
    tab.toktfsc.tokterr  = prs->prscxerr;
    tab.toktfmem = ctx->dbgcxrun->runcxmem;
    tab.toktfdbg = ctx;
    tab.toktffr  = fr;
    
    /* presume no error and no object allocated */
    err = 0;

    /* allocate an object for emitting the code */
    prs->prscxemt->emtcxptr = mcmalo(prs->prscxmem, (ushort)512, &objn);
    prs->prscxemt->emtcxobj = objn;
    prs->prscxemt->emtcxofs = 0;

    ERRBEGIN(ctx->dbgcxerr)

    /* set the parsing context to generate no debug records */
    oldflg = prs->prscxflg;
    prs->prscxflg &= ~(PRSCXFLIN + PRSCXFLCL);
    
    /* set topmost symbol table to the debug frame table */
    prs->prscxtok->tokcxstab = &tab.toktfsc;

    /* parse the expression */
    prsrstn(prs);
    prs->prscxtok->tokcxlen = 0;           /* purge anything left in buffer */
    toknext(prs->prscxtok);

    /* generate code for the expression */
    if (speculative)
        prsxgen_spec(prs);
    else
        prsxgen(prs);

    /* make certain it returns */
    emtop(prs->prscxemt, OPCDBGRET);
    
    ERRCATCH(ctx->dbgcxerr, err)
        /* unlock and free the object used for holding the pcode */
        mcmunlck(prs->prscxmem, objn);
        mcmfre(prs->prscxmem, objn);

        /* restore original parsing flags and symbol table */
        prs->prscxflg = oldflg;
        prs->prscxtok->tokcxstab = tab.toktfsc.toktnxt;
    
        return err;
    ERREND(ctx->dbgcxerr)
        
    /* restore original parsing flags and symbol table */
    prs->prscxflg = oldflg;
    prs->prscxtok->tokcxstab = tab.toktfsc.toktnxt;
    
    mcmunlck(prs->prscxmem, objn);
    *objnp = objn;
    return 0;
}

/* evaluate an expression */
int dbgeval(dbgcxdef *ctx, char *expr,
            void (*dispfn)(void *, const char *, int),
            void *dispctx, int level,
            int showtype)
{
    /* invoke extended version, using a null callback */
    return dbgevalext(ctx, expr, dispfn, dispctx, level, showtype, 0, 0, 0,
                      FALSE);
}

/*
 *   Enumerate the properties of an object 
 */
static void dbgenumprop(dbgcxdef *ctx, objnum objn, objnum self,
                        void (*func)(void *ctx, const char *propname,
                                     int propnamelen,
                                     const char *relationship),
                        void *cbctx)
{
    objdef *objptr;
    int     cnt;
    prpdef *p;
    int     sccnt;
    uchar  *sc;

    /* if it's not a valid object, there's nothing to do */
    if (objn == MCMONINV)
        return;

    /* 
     *   make sure the object hasn't been deleted - check the vocab
     *   inheritance records for confirmation 
     */
    if (vocinh(ctx->dbgcxrun->runcxvoc, objn) == 0)
        return;

    /* get the object's memory */
    objptr = (objdef *)mcmlck(ctx->dbgcxmem, objn);

    /* get the number of properties */
    cnt = objnprop(objptr);

    /* loop through its properties */
    for (p = objprp(objptr) ; cnt != 0 ; p = objpnxt(p), --cnt)
    {
        prpnum prpn;
        char   prpname[TOKNAMMAX + 1];
        int    len;
        objnum srcobj;
        
        /* get this property's ID */
        prpn = prpprop(p);

        /* 
         *   check the type; if it's code or another non-evaluable type,
         *   skip it 
         */
        switch(prptype(p))
        {
        case DAT_BASEPTR:
        case DAT_CODE:
        case DAT_DSTRING:
        case DAT_TPL:
        case DAT_DEMAND:
        case DAT_SYN:
        case DAT_REDIR:
        case DAT_TPL2:
            /* 
             *   these types do not represent values that can be evaluated
             *   in the debugger, so skip properties of these types 
             */
            continue;
        }

        /*
         *   If we've recursed into a superclass, make sure that this
         *   property actually comes from this superclass in the object.
         *   If it doesn't, skip it, because we'll pick it up on the
         *   overriding object. 
         */
        if (objn != self
            && (objgetap(ctx->dbgcxmem, self, prpn, &srcobj, FALSE) == 0
                || srcobj != objn))
            continue;

        /* get the name of this property */
        len = dbgnam(ctx, prpname, TOKSTPROP, prpn);

        /* tell the callback about it */
        (*func)(cbctx, prpname, len, ".");
    }

    /*
     *   Go through the object's superclasses, and enumerate the
     *   properties for each of them. 
     */
    sccnt = objnsc(objptr);
    for (sc = objsc(objptr) ; sccnt != 0 ; sc += 2, --sccnt)
    {
        /* recurse into this superclass object */
        dbgenumprop(ctx, (objnum)osrp2(sc), self, func, cbctx);
    }
    
    /* done with the object - unlock it */
    mcmunlck(ctx->dbgcxmem, objn);
}

/* 
 *   Evaluate an expression, extended version.  For aggregate values
 *   (objects, lists), we'll invoke a callback function for each value
 *   contained by the aggregate value. 
 */
int dbgevalext(dbgcxdef *ctx, char *expr,
               void (*dispfn)(void *, const char *, int), void *dispctx,
               int level, int showtype, dattyp *dat,
               void (*aggcb)(void *, const char *, int, const char *),
               void *aggctx, int speculative)
{
    runsdef  *oldsp;
    runsdef   val;
    objnum    objn;
    uchar    *objptr;
    int       err;
    int       oldfcn;
    int       olddep;

    /* compile the expression */
    if ((err = dbgcompile(ctx, expr, &ctx->dbgcxfrm[level], &objn,
                          speculative)) != 0)
        return err;

    /* 
     *   remember the current stack frame position -- running code will
     *   mess with it, and if we get an error it won't be set back
     *   properly without us doing so explicitly 
     */
    oldfcn = ctx->dbgcxfcn;
    olddep = ctx->dbgcxdep;

    ERRBEGIN(ctx->dbgcxerr)

    objptr = mcmlck(ctx->dbgcxmem, objn);

    /* execute the generated code */
    oldsp = ctx->dbgcxrun->runcxsp;  /* remember sp so we can detect change */
    runexe(ctx->dbgcxrun, objptr, ctx->dbgcxfrm[level].dbgfself,
           objn, (prpnum)0, 0);

    /* if the expression left a value on the stack, display it */
    if (ctx->dbgcxrun->runcxsp != oldsp)
    {
        /* get the value */
        runpop(ctx->dbgcxrun, &val);

        /* display it via the callback, then display a newline */
        dbgpval(ctx, &val, dispfn, dispctx, showtype);
        (*dispfn)(dispctx, "\n", 1);

        /* if the caller wants to know the type, inform the caller */
        if (dat != 0)
            *dat = val.runstyp;
    }

    /*
     *   If the value is an aggregate, and an aggregate callback was
     *   provided, invoke the aggregate callback for each item contained
     *   by the value. 
     */
    if (aggcb != 0)
    {
        switch(val.runstyp)
        {
        case DAT_OBJECT:
            /* 
             *   go through the object's properties, and those of its
             *   superclasses 
             */
            dbgenumprop(ctx, val.runsv.runsvobj, val.runsv.runsvobj,
                        aggcb, aggctx);
            break;

        case DAT_LIST:
            {
                uchar *p;
                uint   rem;
                uint   idxcnt;
                uint   idx;

                /* count the number of elements */
                p = val.runsv.runsvstr;
                rem = osrp2(p) - 2;
                p += 2;
                for (idxcnt = 0 ; rem != 0 ; ++idxcnt, lstadv(&p, &rem)) ;

                /* invoke the callback once per element */
                for (idx = 1 ; idx <= idxcnt ; ++idx)
                {
                    char idxbuf[30];
                    
                    /* build the name of this subitem */
                    sprintf(idxbuf, "[%u]", idx);

                    /* invoke the callback with this subitem */
                    (*aggcb)(aggctx, idxbuf, strlen(idxbuf), "");
                }
            }
            break;
        }
    }

    ERRCATCH(ctx->dbgcxerr, err)

        /* 
         *   an error occurred evaluating the expression - in case we
         *   entered a stack frame and threw an error without resetting
         *   it, go back to the original frame position now 
         */
        ctx->dbgcxfcn = oldfcn;
        ctx->dbgcxdep = olddep;

    ERREND(ctx->dbgcxerr)

    /* free the object that we used to hold the compiled code */
    mcmunlck(ctx->dbgcxmem, objn);
    mcmfre(ctx->dbgcxmem, objn);

    /* return the error status, if any */
    return err;
}

/* 
 *   enumerate local variables at a given stack context level by calling
 *   the given function once for each local variable 
 */
void dbgenumlcl(dbgcxdef *ctx, int level,
                void (*func)(void *ctx, const char *lclnam, size_t lclnamlen),
                void *cbctx)
{
    dbgfdef  *fr;
    mcmcxdef *mctx = ctx->dbgcxmem;
    uchar    *objp;
    uint      ofs;
    uint      encofs;
    int       err;

    /* get the frame at this level */
    fr = &ctx->dbgcxfrm[level];

    /* if there's no local context, there are no symbols */
    if (fr->dbgftarg == MCMONINV)
        return;

    ERRBEGIN(ctx->dbgcxerr)
        /* lock the object and get its frame pointer */
        objp = mcmlck(mctx, fr->dbgftarg);
    ERRCATCH(ctx->dbgcxerr, err)
        /* can't enumerate locals */
        return;
    ERREND(ctx->dbgcxerr)

    ERRBEGIN(ctx->dbgcxerr)

    for (ofs = fr->dbgffr ; ofs != 0 ; ofs = encofs)
    {
        uchar *framep;
        uint   len;

        /* look through this frame table */
        framep = objp + ofs;
        len = osrp2(framep) - 4;                 /* get length out of table */
        encofs = osrp2(framep + 2);         /* get enclosing frame's offset */
        framep += 4;                                     /* skip the header */

        /* scan the frame table */
        while (len != 0)
        {
            uint symlen;

            /* read this symbol's information */
            framep += 2;
            symlen = *framep++;
            len -= 3 + symlen;

            /* invoke the callback with this symbol */
            (*func)(cbctx, (char *)framep, (size_t)symlen);

            /* move on to the next symbol */
            framep += symlen;
        }
    }

    ERRCATCH(ctx->dbgcxerr, err)
        /* ignore the error */
    ERREND(ctx->dbgcxerr)

    /* unlock the target, and we're done */
    mcmunlck(mctx, fr->dbgftarg);
}

/* Set a watchpoint; returns error if any */
int dbgwxset(dbgcxdef *ctx, char *expr, int *wxnum, int level)
{
    dbgwxdef *wx;
    int       i;
    objnum    objn;
    int       err;
    uint      oldflg;
    
    /* scan off leading spaces in expression text */
    while (t_isspace(*expr)) ++expr;
    
    /* get a free watch slot */
    for (i = 0, wx = ctx->dbgcxwx ; i < DBGWXMAX ; ++wx, ++i)
        if (!(wx->dbgwxflg & DBGWXFUSED)) break;

    if (i == DBGWXMAX) return ERR_MANYWX;
    *wxnum = i + 1;
    
    /* compile the expression */
    oldflg = ctx->dbgcxprs->prscxflg;                   /* save parse flags */
    ctx->dbgcxprs->prscxflg |= PRSCXFWTCH;     /* note this is a watchpoint */
    err = dbgcompile(ctx, expr, &ctx->dbgcxfrm[level], &objn, FALSE);
    ctx->dbgcxprs->prscxflg = oldflg;                  /* restore old flags */
    if (err)
        return err;
    
    /* record the watch data */
    wx->dbgwxobj = objn;
    wx->dbgwxflg = DBGWXFUSED;
    wx->dbgwxself = ctx->dbgcxfrm[level].dbgfself;
    if (!dbgnamsav(ctx, expr, &wx->dbgwxnam)) wx->dbgwxflg |= DBGWXFNAME;
    
    return 0;
}

/* delete a watch expression */
int dbgwxdel(dbgcxdef *ctx, int wxnum)
{
    dbgwxdef *wx = &ctx->dbgcxwx[--wxnum];
    
    /* make sure it's valid */
    if (wxnum < 0 || wxnum > DBGWXMAX || !(wx->dbgwxflg & DBGWXFUSED))
        return ERR_WXNSET;
    
    mcmfre(ctx->dbgcxmem, wx->dbgwxobj);         /* delete pcode for object */
    wx->dbgwxflg &= ~DBGWXFUSED;    /* watch expression is no longer in use */
    if (wx->dbgwxflg & DBGWXFNAME)
        dbgnamdel(ctx, wx->dbgwxnam);  /* delete expr text from name buffer */

    return 0;
}

/* update all watch expressions */
void dbgwxupd(dbgcxdef *ctx, void (*dispfn)(void *, const char *, int),
              void *dispctx)
{
    int       i;
    dbgwxdef *wx;
    int       err;
    uchar    *objptr;
    runsdef  *oldsp;
    runsdef   val;
    char      buf[50];
    int       first = TRUE;
    
    /* suppress all output while processing watchpoints */
    outwx(1);
    
    for (i = 0, wx = ctx->dbgcxwx ; i < DBGWXMAX ; ++wx, ++i)
    {
        if (!(wx->dbgwxflg & DBGWXFUSED)) continue;
        
        /* display watch number and expression text */
        if (!first) (*dispfn)(dispctx, "\n", 1);
        else first = FALSE;
        
        sprintf(buf, "[%d] %s = ", i + 1, ctx->dbgcxnam + wx->dbgwxnam);
        (*dispfn)(dispctx, buf, (int)strlen(buf));

        /* lock the object containing the compiled expression */
        objptr = mcmlck(ctx->dbgcxmem, wx->dbgwxobj);

        ERRBEGIN(ctx->dbgcxerr)
            
        oldsp = ctx->dbgcxrun->runcxsp;
        runexe(ctx->dbgcxrun, objptr, wx->dbgwxself, wx->dbgwxobj, 
               (prpnum)0, 0);

        if (ctx->dbgcxrun->runcxsp != oldsp)
        {
            runpop(ctx->dbgcxrun, &val);
            dbgpval(ctx, &val, dispfn, dispctx, FALSE);
        }
        else
            (*dispfn)(dispctx, "<no value>", 10);
        
        ERRCATCH(ctx->dbgcxerr, err)
            if (err == ERR_INACTFR)
                (*dispfn)(dispctx, "<not available>", 15);
            else
            {
                sprintf(buf, "<error %d>", err);
                (*dispfn)(dispctx, buf, (int)strlen(buf));
            }
        ERREND(ctx->dbgcxerr)
                
        mcmunlck(ctx->dbgcxmem, wx->dbgwxobj);
    }
    
    /* stop suppressing output */
    outwx(0);
}


/*
 *   Find a base pointer, given the object+offset of the frame.  If the
 *   frame is not active, this routine signals ERR_INACTFR; otherwise, the
 *   bp value for the frame is returned. 
 */
runsdef *dbgfrfind(dbgcxdef *ctx, objnum frobj, uint frofs)
{
    int      i;
    dbgfdef *f;
    mcmon    objn;
    uchar   *objptr;
    uint     ofs;
    
    /* search stack */
    for (i = ctx->dbgcxfcn, f = &ctx->dbgcxfrm[i-1] ; i ; --f, --i)
    {
        /* ignore this stack level if we have the wrong object */
        objn = f->dbgftarg;
        if (frobj != objn) continue;
        
        /* search nested frames within this stack level */
        objptr = mcmlck(ctx->dbgcxmem, objn);
        
        for (ofs = f->dbgffr ; ofs ; )
        {
            if (ofs == frofs)
            {
                /* object and offset match - this is the correct frame */
                mcmunlck(ctx->dbgcxmem, objn);
                return f->dbgfbp;
            }
            
            /* this isn't it; move on to the enclosing frame */
            ofs = osrp2(objptr + ofs + 2);
        }
        
        /* done with this object - unlock it */
        mcmunlck(ctx->dbgcxmem, objn);
    }
    
    /* couldn't find the frame at all - signal INACTIVE error */
    errsig(ctx->dbgcxerr, ERR_INACTFR);
    NOTREACHEDV(runsdef *);
    return 0;
}

/*
 *   Switch to a new current lindef, closing the file referenced by the
 *   current lindef.  This lets us keep only one file open at a time when
 *   running under the stand-alone source debugger, which reads the source
 *   from disk files.  
 */
void dbgswitch(lindef **linp, lindef *newlin)
{
    if (*linp) lindisact(*linp);
    if (newlin) linactiv(newlin);
    *linp = newlin;
}

