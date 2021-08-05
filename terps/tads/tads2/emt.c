#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/EMT.C,v 1.2 1999/05/17 02:52:11 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  emt.c - emitter
Function
  Code emitter
Notes
  None
Modified
  09/23/91 MJRoberts     - creation
*/

#include <assert.h>
#include "os.h"
#include "std.h"
#include "emt.h"
#include "opc.h"
#include "err.h"
#include "tok.h"

void emtval(emtcxdef *ctx, tokdef *tok, uchar *base)
{
    ushort oplen;
        
    switch(tok->toktyp)
    {
    case TOKTSYMBOL:
        switch(tok->toksym.tokstyp)
        {
        case TOKSTARGC:
            emtop(ctx, OPCARGC);
            return;
            
        case TOKSTSELF:
            emtop(ctx, OPCPUSHSELF);
            return;
            
        case TOKSTOBJ:
        case TOKSTFWDOBJ:
            emtop(ctx, OPCPUSHOBJ);
            break;
        case TOKSTLOCAL:
            if (tok->toksym.toksfr)
            {
                emtop(ctx, OPCGETDBLCL);   /* debug local - specifies frame */
                emtint2(ctx, ctx->emtcxfrob);          /* emit frame object */
                emtint2(ctx, tok->toksym.toksfr);      /* emit frame offset */
            }
            else
                emtop(ctx, OPCGETLCL);
            break;
            
        case TOKSTBIFN:
            emtop(ctx, OPCBUILTIN);
            emtbyte(ctx, 0);
            break;
        case TOKSTFUNC:
        case TOKSTFWDFN:
            emtop(ctx, OPCPUSHFN);
            break;
        case TOKSTPROP:
            emtop(ctx, OPCGETPSELF);
            emtbyte(ctx, 0);
            break;
        case TOKSTPROPSPEC:
            emtop(ctx, OPCGETPSELFDATA);
            break;
        default:
            errsig(ctx->emtcxerr, ERR_REQOBJ);
            /* NOTREACHED */
        }
        emtint2(ctx, tok->toksym.toksval);
        return;

    case TOKTNUMBER:
        emtop(ctx, OPCPUSHNUM);
        emtint4(ctx, tok->tokval);
        return;

    case TOKTNIL:
        emtop(ctx, OPCPUSHNIL);
        return;
    case TOKTTRUE:
        emtop(ctx, OPCPUSHTRUE);
        return;
        
    case TOKTPOUND:
        emtop(ctx, OPCPUSHPN);
        emtint2(ctx, tok->tokofs);
        return;

    case TOKTSSTRING:
        emtop(ctx, OPCPUSHSTR);
        break;
    case TOKTDSTRING:
        emtop(ctx, OPCSAY);
        break;
    case TOKTLIST:
        emtop(ctx, OPCPUSHLST);
        emtlst(ctx, (uint)tok->tokofs, base);
        return;
    }

    /* if we haven't returned already, we have a large operand */
    oplen = osrp2(base + tok->tokofs);
    emtmem(ctx, base + tok->tokofs, oplen);
}

/* emit a list value */
void emtlst(emtcxdef *ctx, uint ofs, uchar *base)
{
    uint      initofs;
    ushort    i;
    emtlidef *lst;
    emtledef *ele;
        
    initofs = ctx->emtcxofs;                        /* save starting offset */
    emtint2(ctx, 2);                             /* emit placeholder length */
    if (ofs == 0) return;              /* nothing more to do for empty list */

    lst = (emtlidef *)(base + ofs);

    for (i = lst->emtlicnt, ele = &lst->emtliele[i-1] ; i ; --ele, --i)
    {
        uchar dat;
        
        switch(ele->emtletyp)
        {
        case TOKTLIST:
            emtbyte(ctx, DAT_LIST);
            emtlst(ctx, (uint)ele->emtleval, base);
            continue;

        case TOKTSSTRING:
        {
            uint oplen = osrp2(base + ele->emtleval);
            
            emtbyte(ctx, DAT_SSTRING);
            emtmem(ctx, base + ele->emtleval, oplen);
            continue;
        }

        case TOKTNUMBER:
            dat = DAT_NUMBER;
            break;
        case TOKTNIL:
            dat = DAT_NIL;
            break;
        case TOKTTRUE:
            dat = DAT_TRUE;
            break;

        case TOKTPOUND:
            dat = DAT_PROPNUM;
            goto symbol;
        case TOKTFUNCTION:
            dat = DAT_FNADDR;
            goto symbol;
        case TOKTOBJECT:
            dat = DAT_OBJECT;
            goto symbol;
        case TOKTDOT:
            dat = DAT_PROPNUM;
            /* FALLTHROUGH */
        symbol:
            emtbyte(ctx, dat);
            emtint2(ctx, (ushort)ele->emtleval);
            continue;

        default:
            assert(FALSE);
            break;
        }
        
        /* if we get here, emit datatype in dat, plus value */
        emtbyte(ctx, dat);
        if (dat != DAT_TRUE && dat != DAT_NIL)
            emtint4(ctx, ele->emtleval);
    }
    oswp2(ctx->emtcxptr + initofs, ctx->emtcxofs - initofs);
}

/* reserve space for code, expanding object if needed */
void emtres(emtcxdef *ctx, ushort bytes)
{
    ushort oldsiz = mcmobjsiz(ctx->emtcxmem, ctx->emtcxobj);
    ushort need;
    
    need = ctx->emtcxofs + bytes + 1;
    
    if (need > oldsiz)
    {
        ushort newsiz;
        
        newsiz = need + EMTINC;
        if (newsiz < oldsiz) errsig(ctx->emtcxerr, ERR_OBJOVF);
        ctx->emtcxptr = mcmrealo(ctx->emtcxmem, ctx->emtcxobj, newsiz);
        assert(mcmobjsiz(ctx->emtcxmem, ctx->emtcxobj) >= need);
    }
}

/* get a new temporary label */
uint emtglbl(emtcxdef *ctx)
{
    uint ret = ctx->emtcxlfre;

    if (ctx->emtcxlfre == EMTLLNKEND) errsig(ctx->emtcxerr, ERR_NOLBL);
    ctx->emtcxlfre = ctx->emtcxlbl[ret].emtllnk;
    ctx->emtcxlbl[ret].emtllnk = EMTLLNKEND;     /* nothing in label's list */
    ctx->emtcxlbl[ret].emtlflg = 0;                 /* label is not yet set */
    return(ret);
}

/* get a new label and set to the current position */
uint emtgslbl(emtcxdef *ctx)
{
    uint ret = ctx->emtcxlfre;

    if (ctx->emtcxlfre == EMTLLNKEND) errsig(ctx->emtcxerr, ERR_NOLBL);
    ctx->emtcxlfre = ctx->emtcxlbl[ret].emtllnk;
    ctx->emtcxlbl[ret].emtllnk = EMTLLNKEND;     /* nothing in label's list */
    ctx->emtcxlbl[ret].emtlflg = EMTLFSET;          /* label is not yet set */
    ctx->emtcxlbl[ret].emtlofs = ctx->emtcxofs;             /* set position */
    return(ret);
}

/* set a temporary label */
void emtslbl(emtcxdef *ctx, noreg uint *lblp, int release)
{
    uint     lbl = *lblp;
    emtldef *p = &ctx->emtcxlbl[lbl];
    uint     fwd;
    emtldef *fwdp;
    ushort   nxt;
    short    diff;
    ushort   curpos = ctx->emtcxofs;
    
    p->emtlofs = curpos;
    p->emtlflg |= EMTLFSET;
    
    /* run through forward reference list, fixing up each reference */
    for (fwd = p->emtllnk ; fwd != EMTLLNKEND ; fwd = nxt)
    {
        fwdp = &ctx->emtcxlbl[fwd];        /* get pointer to current record */
        nxt = fwdp->emtllnk;                    /* remember the next record */

        /* write the fixup, and release the forward reference */
        diff = curpos - fwdp->emtlofs;
        oswp2(ctx->emtcxptr + fwdp->emtlofs, diff);
        fwdp->emtllnk = EMTLLNKEND;
        emtdlbl(ctx, &fwd);
    }

    p->emtllnk = EMTLLNKEND;    /* there are no more forward refs for label */
    if (release) emtdlbl(ctx, lblp);
}

/* release a temporary label */
void emtdlbl(emtcxdef *ctx, noreg uint *lblp)
{
    /* if label has forward references, internal error: label never set */
    if (ctx->emtcxlbl[*lblp].emtllnk != EMTLLNKEND)
        errsig(ctx->emtcxerr, ERR_LBNOSET);
          
    ctx->emtcxlbl[*lblp].emtllnk = ctx->emtcxlfre;
    ctx->emtcxlfre = *lblp;
    *lblp = EMTLLNKEND;
}

/* clear a temporary label without error checking */
void emtclbl(emtcxdef *ctx, noreg uint *lblp)
{
    emtslbl(ctx, lblp, TRUE);         /* clear up forward ref's, and delete */
}

/* emit a jump to a temporary label */
void emtjmp(emtcxdef *ctx, uchar op, uint lbl)
{
    emtldef *p = &ctx->emtcxlbl[lbl];
    short    diff;
    uint     fwd;
    
    emtop(ctx, op);
    
    if (p->emtlflg & EMTLFSET)
        diff = p->emtlofs - ctx->emtcxofs;      /* back ref - compute delta */
    else
    {
        diff = 0;
        fwd = emtglbl(ctx);             /* get a forward ref fixup reminder */
        ctx->emtcxlbl[fwd].emtllnk = p->emtllnk;  /* link into label's list */
        ctx->emtcxlbl[fwd].emtlofs = ctx->emtcxofs;       /* remember where */
        p->emtllnk = fwd;          /* make it new head of this label's list */
    }
    emtint2s(ctx, diff);
}

/* set up the labels - put all in free list */
void emtlini(emtcxdef *ctx)
{
    int      i;
    emtldef *p;
    int      max;
    
    for (max = ctx->emtcxlcnt, i = 1, p = ctx->emtcxlbl ; i < max ; ++p, ++i)
        p->emtllnk = i;
    p->emtllnk = EMTLLNKEND;
    ctx->emtcxlfre = 0;
}
