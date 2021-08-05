#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/PRS.C,v 1.2 1999/05/17 02:52:13 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  prs.c - code parser
Function
  Parse TADS code
Notes
  This module contains the TADS code parser.  TADS code is basically
  a series of object definitions (a function is just a special kind
  of object).  So, this module has a function that reads from a line
  source and generates a compiled object.  Compiled objects are
  completely self-contained, apart from references to other objects.
  We initially allocate a block of space for the object, then make
  it larger if need be.  If it's a class object, we will reduce its
  size down to its actual size, because class objects generally are
  not modified at run-time.
Modified
  04/11/99 CNebel        - Fix signing errors.
  09/14/92 MJRoberts     - note where an object was created
  08/27/91 MJRoberts     - creation
*/

#include <string.h>
#include <assert.h>
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


/* allocate space from node pool */
static prsndef *prsalo(prscxdef *ctx, int subnodes);

/* parse an expression (can be called recursively) */
static prsndef *prsxexp(prscxdef *ctx, int inlist);

/* add a symbol to the symbol table if it's not already there */
void prsdef(prscxdef *ctx, tokdef *tok, int typ)
{
    toktdef *tab;
    
    tab = (typ == TOKSTLABEL ? ctx->prscxgtab : ctx->prscxstab);
    
    if (tok->toktyp == TOKTSYMBOL && tok->toksym.tokstyp == TOKSTUNK)
    {
        if (!tab) errsig(ctx->prscxerr, ERR_UNDFSYM);
                         
        /* see if it's been defined since we last looked */
        if (!(*tab->toktfsea)(tab, tok->toknam, tok->toklen,
                              tok->tokhash, &tok->toksym))
        {
            /* still not defined; add to the symbol table */
            (*tab->toktfadd)(tab, tok->toknam, tok->toklen,
                             TOKSTUNK, 0, tok->tokhash);
            (*tab->toktfsea)(tab, tok->toknam, tok->toklen,
                             tok->tokhash, &tok->toksym);
        }
    }
    if (tok->toktyp == TOKTSYMBOL && tok->toksym.tokstyp == TOKSTUNK)
    {
        if (!tab) errsig(ctx->prscxerr, ERR_UNDFSYM);

        switch(typ)
        {
        case TOKSTPROP:
            tok->toksym.toksval = ctx->prscxprp++;
            break;

        case TOKSTFWDOBJ:
        case TOKSTFWDFN:
            /* fill in the symbol with the object information */
            prsdefobj(ctx, tok, typ);
            break;

        case TOKSTEXTERN:
            tok->toksym.toksval = ctx->prscxextc++;
            break;
            
        case TOKSTLABEL:
            tok->toksym.toksval = emtglbl(ctx->prscxemt);
            break;
        }
        tok->toksym.tokstyp = typ;
        (*tab->toktfset)(tab, &tok->toksym);
    }
}

/* define a symbol as an object or forward object */
void prsdefobj(prscxdef *ctx, tokdef *tok, int typ)
{
    mcmon  newobj;
    uchar *p;

    /* allocate the object and clear its memory */
    p = mcmalo(ctx->prscxmem, (ushort)PRSOBJSIZ, &newobj);
    memset(p, 0, (size_t)OBJDEFSIZ);

    /* save the line position after the object header */
    linppos(ctx->prscxtok->tokcxlin, (char *)p + OBJDEFSIZ,
            (uint)(PRSOBJSIZ - OBJDEFSIZ));

#ifdef OS_ERRLINE
    /* save the source line for error displays later on */
    {
        int   len;
        int   need;

                    /* compute how much space we'll need for the line */
        len = ctx->prscxtok->tokcxlin->linlen + 1;
        need = len + OBJDEFSIZ
               + strlen((char *)p + OBJDEFSIZ) + 1;

                    /* reallocate the object if we need more room */
        if (need > PRSOBJSIZ)
            p = mcmrealo(ctx->prscxmem, newobj, need);

                    /* copy the line */
        p += OBJDEFSIZ;
        p += strlen((char *)p) + 1;
        memcpy(p, ctx->prscxtok->tokcxlin->linbuf, (size_t)len);
        p[len] = '\0';
    }
#endif

    /* unlock the object */
    mcmtch(ctx->prscxmem, newobj);
    mcmunlck(ctx->prscxmem, newobj);

    /* set the symbol to point to the new object */
    tok->toksym.toksval = newobj;
}

/* require a property identifier, returning the property number */
prpnum prsrqpr(prscxdef *ctx)
{
    tokdef *tok = &ctx->prscxtok->tokcxcur;
    prpnum  ret;
        
    /* make sure we get a symbol */
    if (tok->toktyp != TOKTSYMBOL)
        errsig(ctx->prscxerr, ERR_REQSYM);
    
    /* if it's an undefined symbol, make it a property id */
    if (tok->toksym.tokstyp == TOKSTUNK)
        prsdef(ctx, tok, TOKSTPROP);
    
    /* make sure we have a property */
    if (tok->toksym.tokstyp != TOKSTPROP)
        errsig(ctx->prscxerr, ERR_REQPRP);
    
    ret = tok->toksym.toksval;
    toknext(ctx->prscxtok);
    return(ret);
}

/* signal a "missing required token" error, finding token name */
void prssigreq(prscxdef *ctx, int t)
{
    int   i;
    char *p;
    static struct
    {
        int   tokid;
        char *toknam;
    } toklist[] =
    {
        { TOKTSEM, "semicolon" },
        { TOKTCOLON, "colon" },
        { TOKTFUNCTION, "\"function\"" },
        { TOKTCOMMA, "comma" },
        { TOKTLBRACE, "left brace ('{')" },
        { TOKTRPAR, "right paren (')')" },
        { TOKTRBRACK, "right square bracket (']')" },
        { TOKTWHILE, "\"while\"" },
        { TOKTLPAR, "left paren ('(')" },
        { TOKTEQ, "'='" },
        { 0, (char *)0 }
    };
                    
    for (i = 0 ; toklist[i].toknam ; ++i)
    {
        if (toklist[i].tokid == t)
        {
            p = toklist[i].toknam;
            break;
        }
    }
    if (!toklist[i].toknam)
        p = "<unknown token>";
    errsig1(ctx->prscxerr, ERR_REQTOK, ERRTSTR, p);
}

/* check for and skip a required token */
void prsreq(prscxdef *ctx, int t)
{
    tokdef *tok = &ctx->prscxtok->tokcxcur;
    
    if (tok->toktyp != t)
        prssigreq(ctx, t);
    toknext(ctx->prscxtok);
}

/* get next token, require it to be a specific token, and skip it */
void prsnreq(prscxdef *ctx, int t)
{
    tokdef *tok = &ctx->prscxtok->tokcxcur;

    toknext(ctx->prscxtok);
    if (tok->toktyp != t)
        prssigreq(ctx, t);
    toknext(ctx->prscxtok);
}

/* allocate bytes from node pool */
uchar *prsbalo(prscxdef *ctx, uint siz)
{
    uchar *ret;

    /* round size for alignment, and see if we have room for it */
    siz = osrndsz(siz);
    if (ctx->prscxnrem < siz) errsig(ctx->prscxerr, ERR_NONODE);

    /* adjust sizes and pointers, and return the new node */
    ret = ctx->prscxnode;
    ctx->prscxnrem -= siz;
    ctx->prscxnode += siz;
    return(ret);
}

/* allocate a node with a given number of subnodes from node pool */
static prsndef *prsalo(prscxdef *ctx, int subnodes)
{
    uint siz;

    /* figure size of node we actually need */
    siz = offsetof(prsndef, prsnv.prsnvt);
    if (subnodes)
        siz += (subnodes * sizeof(prsndef *));
    else
        siz += sizeof(tokdef);
    
    return((prsndef *)prsbalo(ctx, siz));
}

/* begin a string in expression evaluation mode (puts in node pool) */
ushort prsxsst(prscxdef *ctx)
{
    /* make sure there's space for the length prefix (a ushort) */
    if (ctx->prscxnrem < 2)
        errsig(ctx->prscxerr, ERR_NONODE);
    
    ctx->prscxsofs = ctx->prscxnode - &ctx->prscxpool[0];
    ctx->prscxnode += 2;
    ctx->prscxnrem -= 2;
    ctx->prscxslen = 2;
    return(ctx->prscxsofs);
}

/* add bytes to a string in expression mode */
void prsxsad(prscxdef *ctx, char *p, ushort len)
{
    /* make sure there's room for the extra bytes */
    if (ctx->prscxnrem < len)
        errsig(ctx->prscxerr, ERR_NONODE);
    
    memcpy(ctx->prscxnode, p, (size_t)len);
    ctx->prscxnode += len;
    ctx->prscxnrem -= len;
    ctx->prscxslen += len;
}

/* terminate a string in expression evaluation mode */
void prsxsend(prscxdef *ctx)
{
    ushort siz;
    ushort rounded;
    
    /* figure total size of string, including padding for alignment */
    siz = ctx->prscxslen;
    
    /* write length at beginning of string */
    oswp2(&ctx->prscxpool[ctx->prscxsofs], siz);
    
    /* if desired, write the string to the strings file */
    if (ctx->prscxstrfile != 0)
    {
        uchar *str;

        str = &ctx->prscxpool[ctx->prscxsofs] + 2;
        str[siz - 2] = '\0';
        osfputs((char *)str, ctx->prscxstrfile);
        osfputs("\n", ctx->prscxstrfile);
    }

    /* round size of string if necessary */
    rounded = osrndsz(siz);
    if (rounded - siz)
    {
        rounded -= siz;
        if (ctx->prscxnrem < rounded)
            errsig(ctx->prscxerr, ERR_NONODE);
        ctx->prscxnode += rounded;
        ctx->prscxnrem -= rounded;
    }
}

/* parse a list */
static prsndef *prslst(prscxdef *ctx)
{
    prsndef *node = (prsndef *)0;
    
    for (;;)
    {
        if (ctx->prscxtok->tokcxcur.toktyp == TOKTRBRACK) break;
        node = prsnew2(ctx, TOKTRBRACK, prsxexp(ctx, TRUE), node);
        
        /* allow optional comma (just skip it if one is present) */
        if (ctx->prscxtok->tokcxcur.toktyp == TOKTCOMMA)
            toknext(ctx->prscxtok);
    }
    toknext(ctx->prscxtok);                       /* skip the right bracket */
    
    /* if it's an empty list, make an empty list token for it */
    if (!node)
    {
        tokdef t;
        
        t.toktyp = TOKTLIST;
        t.tokofs = 0;
        node = prsnew0(ctx, &t);
    }
    
    return(node);
}

/* build a four-way operator node */
prsndef *prsnew4(prscxdef *ctx, int t, prsndef *n1, prsndef *n2,
                 prsndef *n3, prsndef *n4)
{
    prsndef *n = prsalo(ctx, 4);
    
    n->prsntyp = t;
    n->prsnnlf = 4;
    n->prsnv.prsnvn[0] = n1;
    n->prsnv.prsnvn[1] = n2;
    n->prsnv.prsnvn[2] = n3;
    n->prsnv.prsnvn[3] = n4;
    return(n);
}

/* build a tertiary operator node - three subnodes */
prsndef *prsnew3(prscxdef *ctx, int t, prsndef *n1, prsndef *n2, prsndef *n3)
{
    prsndef *n = prsalo(ctx, 3);
    
    n->prsntyp = t;
    n->prsnnlf = 3;
    n->prsnv.prsnvn[0] = n1;
    n->prsnv.prsnvn[1] = n2;
    n->prsnv.prsnvn[2] = n3;
    return(n);
}

/* build a binary operator node - two subnodes */
prsndef *prsnew2(prscxdef *ctx, int t, prsndef *n1, prsndef *n2)
{
    prsndef *n = prsalo(ctx, 2);
    
    n->prsntyp = t;
    n->prsnnlf = 2;
    n->prsnv.prsnvn[0] = n1;
    n->prsnv.prsnvn[1] = n2;
    return(n);
}

/* build a unary operator node - one subnode */
prsndef *prsnew1(prscxdef *ctx, int t, prsndef *n)
{
    prsndef *newnode = prsalo(ctx, 1);

    newnode->prsntyp = t;
    newnode->prsnnlf = 1;
    newnode->prsnv.prsnvn[0] = n;
    return newnode;
}

/* build a new value node - no subnodes */
prsndef *prsnew0(prscxdef *ctx, tokdef *tokp)
{
    prsndef *n = prsalo(ctx, 0);
    
    n->prsntyp = 0;                                           /* value node */
    n->prsnnlf = 0;
    OSCPYSTRUCT(n->prsnv.prsnvt, *tokp);
    return(n);
}

/* binary operator definition */
typedef struct prsbdef prsbdef;
struct prsbdef
{
    int       *prsblst;          /* list of tokens at this precedence level */
    prsndef *(*prsblf)(prscxdef *ctx, prsbdef *binctx);
    prsndef *(*prsbrf)(prscxdef *ctx, prsbdef *binctx);
    prsbdef   *prsbctx;               /* secondary context for left & right */
    int        prsbrpt;                   /* TRUE ==> iterate right operand */
};


/* generate code for a function call */
static void prsgfn(prscxdef *ctx, prsndef *n, int argcnt);

/* generate code for an argument list; returns argument count */
static int prsgargs(prscxdef *ctx, prsndef *n);

/* generate code for a property lookup */
static void prsgdot(prscxdef *ctx, prsndef *n, int argcnt);

/* generate code for a property lookup during speculative evaluation */
static void prsgdotspec(prscxdef *ctx, prsndef *n);

/* generate code to assign an l-value */
static void prsglval(prscxdef *ctx, int typ, prsndef *n);

/* Build a general binary node.  The context tells us how. */
static prsndef *prsxbin(prscxdef *ctx, prsbdef *binctx);

/* parse an argument list */
static prsndef *prsxarg(prscxdef *ctx);

/* parse an atom */
static prsndef *prsxatm(prscxdef *ctx);

/* parse a structure member expression */
static prsndef *prsxmem(prscxdef *ctx);

/* parse a unary operator expression */
static prsndef *prsxun(prscxdef *ctx, prsbdef *binctx);

/* parse a conditional expression */
static prsndef *prsxcnd(prscxdef *ctx, prsbdef *binctx);

/* factor */
static int prsl_fact[] = { TOKTTIMES, TOKTDIV, TOKTMOD, 0 };
static prsbdef prsb_fact = { prsl_fact, prsxun, prsxun, 0, TRUE };

/* term */
static int prsl_term[] = { TOKTPLUS, TOKTMINUS, 0 };
static prsbdef prsb_term = { prsl_term, prsxbin, prsxbin, &prsb_fact, TRUE };

/* shift */
static int prsl_shift[] = { TOKTSHL, TOKTSHR, 0 };
static prsbdef prsb_shift = { prsl_shift, prsxbin, prsxbin, &prsb_term, TRUE };

/* relational (magnitude comparison) */
static int prsl_rel[] = { TOKTGT, TOKTGE, TOKTLT, TOKTLE, 0 };
static prsbdef prsb_rel = { prsl_rel, prsxbin, prsxbin, &prsb_shift, FALSE };

/* comparison */
/* WARNING - keep prsl_cmp and prsl_cmp_C in sync with one another */
static int prsl_cmp[] =
    { TOKTEQ, TOKTNE, 0 };
static prsbdef prsb_cmp = { prsl_cmp, prsxbin, prsxbin, &prsb_rel, FALSE };
static int prsl_cmp_C[] =
    { TOKTEQEQ, TOKTNE, 0 };
static prsbdef prsb_cmp_C =
    { prsl_cmp_C, prsxbin, prsxbin, &prsb_rel, FALSE };

/* bitwise AND */
static int prsl_band[] = { TOKTBOR, 0 };
static prsbdef prsb_band = { prsl_band, prsxbin, prsxbin, &prsb_cmp, TRUE };
static prsbdef prsb_band_C = {prsl_band, prsxbin, prsxbin, &prsb_cmp_C, TRUE };

/* bitwise XOR */
static int prsl_xor[] = { TOKTXOR, 0 };
static prsbdef prsb_xor = { prsl_xor, prsxbin, prsxbin, &prsb_band, TRUE };
static prsbdef prsb_xor_C = { prsl_xor, prsxbin, prsxbin, &prsb_band_C, TRUE };

/* bitwise OR */
static int prsl_bor[] = { TOKTBAND, 0 };
static prsbdef prsb_bor = { prsl_bor, prsxbin, prsxbin, &prsb_xor, TRUE };
static prsbdef prsb_bor_C = { prsl_bor, prsxbin, prsxbin, &prsb_xor_C, TRUE };

/* logical AND */
static int prsl_and[] = { TOKTAND, 0 };
static prsbdef prsb_and = { prsl_and, prsxbin, prsxbin, &prsb_bor, TRUE };
static prsbdef prsb_and_C = { prsl_and, prsxbin, prsxbin, &prsb_bor_C, TRUE };

/* logical OR */
static int prsl_or[] = { TOKTOR, 0 };
static prsbdef prsb_or = { prsl_or, prsxbin, prsxbin, &prsb_and, TRUE };
static prsbdef prsb_or_C = { prsl_or, prsxbin, prsxbin, &prsb_and_C, TRUE };

/* assignment */
/* WARNING - keep prsl_asi and prsl_asi_C in sync with one another */
static int prsl_asi[] =
    { TOKTASSIGN, TOKTPLEQ, TOKTMINEQ, TOKTDIVEQ, TOKTTIMEQ, TOKTMODEQ,
      TOKTBANDEQ, TOKTBOREQ, TOKTXOREQ, TOKTSHLEQ, TOKTSHREQ, 0 };
static int prsl_asi_C[] =
    { TOKTEQ, TOKTPLEQ, TOKTMINEQ, TOKTDIVEQ, TOKTTIMEQ, TOKTMODEQ,
      TOKTBANDEQ, TOKTBOREQ, TOKTXOREQ, TOKTSHLEQ, TOKTSHREQ, 0 };
static prsbdef prsb_asi = { prsl_asi, prsxcnd, prsxbin, &prsb_asi, FALSE };
static prsbdef prsb_asi_C =
    { prsl_asi_C, prsxcnd, prsxbin, &prsb_asi_C, FALSE };

/* comma */
static int prsl_cma[] = { TOKTCOMMA, 0 };
static prsbdef prsb_cma = { prsl_cma, prsxbin, prsxbin, &prsb_asi, TRUE };
static prsbdef prsb_cma_C = { prsl_cma, prsxbin, prsxbin, &prsb_asi_C, TRUE };


/* determine if we're in C-mode - return true if so, false otherwise */
static int prs_cmode(prscxdef *ctx)
{
    return (ctx->prscxtok->tokcxflg & TOKCXFCMODE) != 0;
}

/* build general binary node, iterating to the right if desired */
static prsndef *prsxbin(prscxdef *ctx, prsbdef *binctx)
{
    prsndef *n;
    int      t;
    int     *lp;
    
    n = (*binctx->prsblf)(ctx, binctx->prsbctx);
    do
    {
        t = ctx->prscxtok->tokcxcur.toktyp;
        for (lp = binctx->prsblst ; *lp && *lp != t ; ++lp) ;
        if (*lp == t)
        {
            /*
             *   if inside a list, check to see if this is a binary
             *   operator that can also server as a unary operator -- if
             *   so, interpret it as a unary operator and warn about it 
             */
            if (ctx->prscxflg & PRSCXFLST)
            {
                static int ambig_ops[] = { TOKTBAND, TOKTPLUS, TOKTMINUS, 0 };
                static char *ambig_names[] = { "&", "+", "-" };

                for (lp = ambig_ops ; *lp && *lp != t ; ++lp) ;
                if (*lp)
                {
                    errlog1(ctx->prscxerr, ERR_AMBIGBIN,
                            ERRTSTR, ambig_names[lp - ambig_ops]);
                    break;
                }
            }

            /* skip the operator */
            toknext(ctx->prscxtok);

            /* use C-style operators if in C mode */
            if (prs_cmode(ctx))
            {
                switch(t)
                {
                case TOKTEQEQ:
                    t = TOKTEQ;
                    break;

                case TOKTEQ:
                    t = TOKTASSIGN;
                    break;
                }
            }

            /* generate a node for the binary operator */
            n = prsnew2(ctx, t, n, (*binctx->prsbrf)(ctx, binctx->prsbctx));
        }
        else
            break;
    } while (binctx->prsbrpt);
    
    return(n);
}

static prsndef *prsxarg(prscxdef *ctx)
{
    prsndef *n;
    int      t;
    
    /* arguments are assignments:  just under comma-expressions */
    n = prsxbin(ctx, prs_cmode(ctx) ? &prsb_asi_C : &prsb_asi);
    
    t = ctx->prscxtok->tokcxcur.toktyp;
    toknext(ctx->prscxtok);
    switch(t)
    {
    case TOKTRPAR:
        return(n);
        
    case TOKTCOMMA:
        return(prsnew2(ctx, TOKTRPAR, n, prsxarg(ctx)));
        
    default:
        errsig(ctx->prscxerr, ERR_REQARG);
        NOTREACHEDV(prsndef *);
        return 0;
    }
}

static prsndef *prsxatm(prscxdef *ctx)
{
    prsndef *n;
    tokdef  *tok = &ctx->prscxtok->tokcxcur;
    tokdef   newtok;
    prpnum   pr;
    
    switch(tok->toktyp)
    {
    case TOKTPOUND:
    case TOKTBAND:
        toknext(ctx->prscxtok);
        if (ctx->prscxtok->tokcxcur.toktyp == TOKTSYMBOL &&
            (ctx->prscxtok->tokcxcur.toksym.tokstyp == TOKSTFWDFN ||
             ctx->prscxtok->tokcxcur.toksym.tokstyp == TOKSTFUNC))
        {
            n = prsnew0(ctx, tok);
            toknext(ctx->prscxtok);
            return(n);
        }
        else
        {
            pr = prsrqpr(ctx);
            newtok.toktyp = TOKTPOUND;
            newtok.tokofs = pr;
            return(prsnew0(ctx, &newtok));
        }
        
    case TOKTLPAR:
        toknext(ctx->prscxtok);
        n = prsxexp(ctx, FALSE);
        prsreq(ctx, TOKTRPAR);
        return(n);
        
    case TOKTLBRACK:
        toknext(ctx->prscxtok);
        return(prslst(ctx));
        
    case TOKTNUMBER:
    case TOKTNIL:
    case TOKTTRUE:
    case TOKTSSTRING:
    case TOKTDSTRING:
        /* create and return a new terminal node for this item */
        n = prsnew0(ctx, tok);
        toknext(ctx->prscxtok);
        return n;
        
    case TOKTSYMBOL:
        /* create a new terminal node for the symbol */
        n = prsnew0(ctx, tok);
        toknext(ctx->prscxtok);

        /* 
         *   if it was 'inherited', and there's another symbol name
         *   following, we have the explicit superclass notation - parse
         *   it 
         */
        if (n->prsnv.prsnvt.toksym.tokstyp == TOKSTINHERIT
            && tok->toktyp == TOKTSYMBOL)
        {
            /* build a new node for the explicit superclass */
            n = prsnew0(ctx, tok);

            /* build a containing node for the inheritance operator */
            n = prsnew1(ctx, TOKTEXPINH, n);

            /* skip the superclass name token */
            toknext(ctx->prscxtok);
        }

        /* return the new node */
        return n;
        
    default:
        errsig(ctx->prscxerr, ERR_REQOPN);
        return 0;
        NOTREACHEDV(prsndef *);
    }
}

static prsndef *prsxmem(prscxdef *ctx)
{
    prsndef *n1, *n2;
    tokdef  *tok;
    tokdef   newtok;
    prpnum   prop;
    
    n1 = prsxatm(ctx);
    for ( ;; )
    {
        tok = &ctx->prscxtok->tokcxcur;
        switch(tok->toktyp)
        {
        case TOKTDOT:
            if (toknext(ctx->prscxtok) == TOKTLPAR)
            {
                toknext(ctx->prscxtok);
                n2 = prsxexp(ctx, FALSE);
                prsreq(ctx, TOKTRPAR);
            }
            else if (ctx->prscxtok->tokcxcur.toktyp == TOKTSYMBOL &&
                     ctx->prscxtok->tokcxcur.toksym.tokstyp == TOKSTLOCAL)
            {
                n2 = prsnew0(ctx, &ctx->prscxtok->tokcxcur);
                toknext(ctx->prscxtok);
            }
            else
            {
                prop = prsrqpr(ctx);
                newtok.toktyp = TOKTPOUND;
                newtok.tokofs = prop;
                n2 = prsnew0(ctx, &newtok);
            }

            /* see if there's an argument list for the method */
            if (ctx->prscxtok->tokcxcur.toktyp == TOKTLPAR)
            {
                toknext(ctx->prscxtok);
                n1 = prsnew3(ctx, TOKTDOT, n1, n2, prsxarg(ctx));
            }
            else
                n1 = prsnew2(ctx, TOKTDOT, n1, n2);

            break;
            
        case TOKTLBRACK:
            if (ctx->prscxflg & PRSCXFLST) return(n1);
            toknext(ctx->prscxtok);
            n2 = prsxexp(ctx, FALSE);
            n1 = prsnew2(ctx, TOKTLBRACK, n1, n2);
            prsreq(ctx, TOKTRBRACK);
            break;
            
        case TOKTLPAR:
            if (toknext(ctx->prscxtok) == TOKTRPAR)
            {
                toknext(ctx->prscxtok);
                n1 = prsnew1(ctx, TOKTLPAR, n1);
            }
            else
                n1 = prsnew2(ctx, TOKTLPAR, n1, prsxarg(ctx));
            break;
            
        case TOKTINC:
        case TOKTDEC:
            n1 = prsnew1(ctx, tok->toktyp + 1, n1);
            toknext(ctx->prscxtok);
            return(n1);
            
        default:
            return(n1);
        }
    }
}

/* parse a unary operator expression */
static prsndef *prsxun(prscxdef *ctx, prsbdef *binctx)
{
    int      t;
    
    VARUSED(binctx);

    t = ctx->prscxtok->tokcxcur.toktyp;
    if (t == TOKTPLUS || t == TOKTMINUS || t == TOKTNOT
        || t == TOKTINC || t == TOKTDEC || t == TOKTTILDE
        || t == TOKTDELETE)
    {
        toknext(ctx->prscxtok);
        return(prsnew1(ctx, t, prsxun(ctx, (prsbdef *)0)));
    }
    else if (t == TOKTNEW)
    {
        if (toknext(ctx->prscxtok) == TOKTOBJECT)
        {
            tokdef tok;

            toknext(ctx->prscxtok);
            tok.toktyp = TOKTNEW;
            return prsnew0(ctx, &tok);
        }
        else
            return prsnew1(ctx, t, prsxun(ctx, (prsbdef *)0));
    }
    return(prsxmem(ctx));
}

/* parse a ternary conditional expression */
static prsndef *prsxcnd(prscxdef *ctx, prsbdef *binctx)
{
    prsndef *n1, *n2;
    
    VARUSED(binctx);

    n1 = prsxbin(ctx, prs_cmode(ctx) ? &prsb_or_C : &prsb_or);
    for (;;)
    {
        if (ctx->prscxtok->tokcxcur.toktyp != TOKTQUESTION) break;
        toknext(ctx->prscxtok);
        n2 = prsxexp(ctx, FALSE);
        prsreq(ctx, TOKTCOLON);
        n1 = prsnew3(ctx, TOKTQUESTION, n1, n2,
                     prsxbin(ctx, prs_cmode(ctx) ? &prsb_asi_C : &prsb_asi));
    }
    return(n1);
}

/* parse an expression (called recursively during expression parsing) */
static prsndef *prsxexp(prscxdef *ctx, int inlist)
{
    ushort   oldflg = ctx->prscxflg;
    prsndef *retval;
    
    if (inlist)
    {
        ctx->prscxflg |= PRSCXFLST;
        retval = prsxbin(ctx, (prs_cmode(ctx) ? &prsb_asi_C : &prsb_asi));
    }
    else
    {
        ctx->prscxflg &= ~PRSCXFLST;
        retval = prsxbin(ctx, prs_cmode(ctx) ? &prsb_cma_C : &prsb_cma);
    }
    
    ctx->prscxflg = oldflg;
    return(retval);
}

/* fold constants in a parse tree */
static prsndef *prsfold(prscxdef *ctx, prsndef *node)
{
    int       i;
    prsndef **n;
    int       can_fold;
    int       typ;
    tokdef   *tok1, *tok2;
    int       typ1, typ2;
    long      val1, val2;
    prsndef  *ncur;
    
    can_fold = TRUE;                       /* assume we can do some folding */

    /* if this is a list-construction node, special handling is needed */
    if (node->prsntyp == TOKTRBRACK)
    {
        for (i=0, ncur = node ; ncur ; ++i, ncur = ncur->prsnv.prsnvn[1])
        {
            ncur->prsnv.prsnvn[0] = prsfold(ctx, ncur->prsnv.prsnvn[0]);
            typ = TOKTINVALID;
            if (ncur->prsnv.prsnvn[0]->prsnnlf != 0 ||
                ((typ = ncur->prsnv.prsnvn[0]->prsnv.prsnvt.toktyp)
                 != TOKTNUMBER && typ != TOKTSSTRING && typ != TOKTLIST
                 && typ != TOKTNIL && typ != TOKTTRUE && typ != TOKTPOUND
                 && typ != TOKTSYMBOL))
                can_fold = FALSE;              /* not constant - can't fold */
            
            if (typ == TOKTSYMBOL
                && (ncur->prsnv.prsnvn[0]->prsnv.prsnvt.toksym.tokstyp
                    == TOKSTLOCAL
                    || ncur->prsnv.prsnvn[0]->prsnv.prsnvt.toksym.tokstyp
                    == TOKSTSELF))
                can_fold = FALSE;
            
            if (ncur->prsnv.prsnvn[0]->prsnnlf == 0 && typ == TOKTSYMBOL)
                prsdef(ctx, &ncur->prsnv.prsnvn[0]->prsnv.prsnvt,
                       TOKSTFWDOBJ);
        }
        if (can_fold)
        {
            tokdef    t;
            emtlidef *lst;
            prsndef  *retval;
            emtledef *ele;
            
            lst = (emtlidef *)prsbalo(ctx, (uint)(sizeof(emtlidef) +
                                                 (i - 1) * sizeof(emtledef)));
            t.toktyp = TOKTLIST;
            t.tokofs = ((uchar *)lst) - &ctx->prscxpool[0];
            retval = prsnew0(ctx, &t);
            
            lst->emtlicnt = i;
            
            for (ele = &lst->emtliele[0] ; node ;
                   node = node->prsnv.prsnvn[1], ++ele)
            {
                ncur = node->prsnv.prsnvn[0];
                switch(ele->emtletyp = ncur->prsnv.prsnvt.toktyp)
                {
                case TOKTLIST:
                case TOKTSSTRING:
                case TOKTPOUND:
                    ele->emtleval = ncur->prsnv.prsnvt.tokofs;
                    break;
                    
                case TOKTSYMBOL:
                    switch(ncur->prsnv.prsnvt.toksym.tokstyp)
                    {
                    case TOKSTFUNC:
                    case TOKSTFWDFN:
                        ele->emtletyp = TOKTFUNCTION;
                        break;
                    case TOKSTOBJ:
                    case TOKSTFWDOBJ:
                        ele->emtletyp = TOKTOBJECT;
                        break;
                    case TOKSTPROP:
                        ele->emtletyp = TOKTDOT;
                        break;
                    case TOKSTPROPSPEC:
                        ele->emtletyp = TOKTDOT;
                        break;
                    default:
                        errsig(ctx->prscxerr, ERR_INVLSTE);
                    }
                    ele->emtleval = ncur->prsnv.prsnvt.toksym.toksval;
                    break;
                    
                default:
                    ele->emtleval = ncur->prsnv.prsnvt.tokval;
                    break;
                }
            }

            node = retval;
        }
        return(node);
    }
    
    /*
     *   If we have sub-nodes, try to fold them.  If they fold down to
     *   leaf nodes, we can try to apply the expression to them. 
     */
    for (i = node->prsnnlf, n = &node->prsnv.prsnvn[0] ; i ; ++n, --i)
    {
        *n = prsfold(ctx, *n);
        if ((*n)->prsnnlf != 0 ||
            ((typ = (*n)->prsnv.prsnvt.toktyp) != TOKTNUMBER &&
             typ != TOKTSSTRING && typ != TOKTLIST &&
             typ != TOKTNIL && typ != TOKTTRUE && typ != TOKTPOUND))
            can_fold = FALSE;                  /* not constant - can't fold */
    }

    /* if at a leaf, or subnodes are non-constant, can't fold anything */
    if (node->prsnnlf == 0 || !can_fold) return(node);

    switch(node->prsnnlf)
    {
    case 1:
        tok1 = &node->prsnv.prsnvn[0]->prsnv.prsnvt;
        typ1 = tok1->toktyp;
        val1 = tok1->tokval;
        
        switch(node->prsntyp)
        {
        case TOKTPLUS:
            break;
            
        case TOKTMINUS:
            if (typ1 != TOKTNUMBER)
                errsig(ctx->prscxerr, ERR_INVOP);
            tok1->tokval = -val1;
            break;
            
        case TOKTNOT:
            if (!prsvlog(typ1))
                errsig(ctx->prscxerr, ERR_INVOP);
            tok1->toktyp = (prs2log(typ1, val1) == TOKTNIL ?
                            TOKTTRUE : TOKTNIL);
            break;

        case TOKTTILDE:
            if (typ1 != TOKTNUMBER)
                errsig(ctx->prscxerr, ERR_INVOP);
            tok1->tokval = ~val1;
            break;
            
        default:
            return(node);
        }
        break;
        
    case 2:
        tok1 = &node->prsnv.prsnvn[0]->prsnv.prsnvt;
        tok2 = &node->prsnv.prsnvn[1]->prsnv.prsnvt;
        val1 = tok1->tokval;
        val2 = tok2->tokval;
        typ1 = tok1->toktyp;
        typ2 = tok2->toktyp;
        
        switch(node->prsntyp)
        {
        case TOKTOR:
        case TOKTAND:
            if (!prsvlog(typ1) || !prsvlog(typ2))
                errsig(ctx->prscxerr, ERR_INVOP);
            typ1 = (prs2log(typ1, val1) == TOKTTRUE);
            typ2 = (prs2log(typ2, val2) == TOKTTRUE);

            if (node->prsntyp == TOKTAND) typ1 = typ1 && typ2;
            else                          typ1 = typ1 || typ2;

            tok1->toktyp = (typ1 ? TOKTTRUE : TOKTNIL);
            break;

        case TOKTPLUS:
        case TOKTMINUS:
            if (typ1 == TOKTSSTRING || typ1 == TOKTLIST || typ2 == TOKTLIST)
                return(node);
            /* FALLTHROUGH */
            
        case TOKTEQ:
        case TOKTNE:
        case TOKTGT:
        case TOKTGE:
        case TOKTLT:
        case TOKTLE:
            if (typ1 != TOKTNUMBER || typ2 != TOKTNUMBER)
                return(node);
            /* FALLTHROUGH */

        case TOKTXOR:
            if ((typ1 == TOKTTRUE || typ1 == TOKTNIL)
                && (typ2 == TOKTTRUE || typ2 == TOKTNIL))
            {
                int a, b;

                a = (typ1 == TOKTTRUE ? 1 : 0);
                b = (typ2 == TOKTTRUE ? 1 : 0);
                tok1->toktyp = ((a ^ b) ? TOKTTRUE : TOKTNIL);
                break;
            }
            /* FALLTHROUGH */

        case TOKTDIV:
        case TOKTTIMES:
        case TOKTMOD:
        case TOKTSHL:
        case TOKTSHR:
            if (typ1 != TOKTNUMBER || typ2 != TOKTNUMBER)
                errsig(ctx->prscxerr, ERR_INVOP);
            switch(node->prsntyp)
            {
            case TOKTPLUS:
                val1 += val2;
                break;
            case TOKTMINUS:
                val1 -= val2;
                break;
            case TOKTTIMES:
                val1 *= val2;
                break;
            case TOKTDIV:
                if (val2 == 0)
                    errsig(ctx->prscxerr, ERR_DIVZERO);
                val1 /= val2;
                break;
            case TOKTMOD:
                if (val2 == 0)
                    errsig(ctx->prscxerr, ERR_DIVZERO);
                val1 %= val2;
                break;
            case TOKTXOR:
                val1 ^= val2;
                break;
            case TOKTSHL:
                val1 <<= val2;
                break;
            case TOKTSHR:
                val1 >>= val2;
                break;
            case TOKTEQ:
                typ1 = (val1 == val2 ? TOKTTRUE : TOKTNIL);
                break;
            case TOKTNE:
                typ1 = (val1 != val2 ? TOKTTRUE : TOKTNIL);
                break;
            case TOKTGT:
                typ1 = (val1 > val2 ? TOKTTRUE : TOKTNIL);
                break;
            case TOKTLT:
                typ1 = (val1 < val2 ? TOKTTRUE : TOKTNIL);
                break;
            case TOKTLE:
                typ1 = (val1 <= val2 ? TOKTTRUE : TOKTNIL);
                break;
            case TOKTGE:
                typ1 = (val1 >= val2 ? TOKTTRUE : TOKTNIL);
                break;
                
            default:
                return(node);
            }
            tok1->toktyp = typ1;
            tok1->tokval = val1;
            break;
            
        default:
            return(node);
        }
        break;
        
    case 3:
        tok1 = &node->prsnv.prsnvn[0]->prsnv.prsnvt;
        if (!prsvlog(tok1->toktyp)) errsig(ctx->prscxerr, ERR_INVOP);
        if (prs2log(tok1->toktyp, tok1->tokval) == TOKTTRUE)
            return(node->prsnv.prsnvn[1]);
        else
            return(node->prsnv.prsnvn[2]);
        
    default:
        return(node);
    }

    /* return the first sub-node, which has the folded value */
    return(node->prsnv.prsnvn[0]);
}

/* start parsing an initializer expression (resets node pool) */
prsndef *prsxini(prscxdef *ctx)
{
    prsndef *node;
    
    node = prsxbin(ctx, (prs_cmode(ctx) ? &prsb_asi_C : &prsb_asi));
    return(prsfold(ctx, node));                           /* fold constants */
}

/* start parsing an expression (folds constants) */
prsndef *prsexpr(prscxdef *ctx)
{
    return(prsfold(ctx, prsxbin(ctx,
                                prs_cmode(ctx) ? &prsb_cma_C : &prsb_cma)));
}

/* parse an expression and generate code for it immediately */
void prsxgen(prscxdef *ctx)
{
    prsndef *exprnode;

    exprnode = prsexpr(ctx);
    prsgexp(ctx, exprnode);
    prsrstn(ctx);
}

/*
 *   Check a parse tree for speculative execution problems.  
 */
static void prs_check_spec(prscxdef *ctx, prsndef *expr)
{
    int i;
    
    /* 
     *   check the type of this node - if it's an assignment or a function
     *   call, prohibit it 
     */
    if (expr->prsnnlf == 0)
    {
        /* value node - check for a symbol */
        if (expr->prsnv.prsnvt.toktyp == TOKTSYMBOL)
        {
            switch(expr->prsnv.prsnvt.toksym.tokstyp)
            {
            case TOKSTPROP:
                /* 
                 *   convert this to a speculative evaluation property, so
                 *   that the emitter knows to emit the correct opcode for
                 *   it 
                 */
                expr->prsnv.prsnvt.toksym.tokstyp = TOKSTPROPSPEC;
                break;

            case TOKSTBIFN:
            case TOKSTINHERIT:
            case TOKSTEXTERN:
                /* don't allow calls */
                errsig(ctx->prscxerr, ERR_BADSPECEXPR);

            default:
                /* others are okay */
                break;
            }
        }
        else if (expr->prsnv.prsnvt.toktyp == TOKTDSTRING)
        {
            /* don't allow double-quoted strings in speculative evaluation */
            errsig(ctx->prscxerr, ERR_BADSPECEXPR);
        }
    }
    else
    {
        switch(expr->prsntyp)
        {
        case TOKTINC:
        case TOKTDEC:
        case TOKTPOSTINC:
        case TOKTPOSTDEC:
        case TOKTNEW:
        case TOKTDELETE:
        case TOKTLPAR:
        case TOKTASSIGN:
        case TOKTPLEQ:
        case TOKTMINEQ:
        case TOKTDIVEQ:
        case TOKTMODEQ:
        case TOKTTIMEQ:
        case TOKTBANDEQ:
        case TOKTBOREQ:
        case TOKTXOREQ:
        case TOKTSHLEQ:
        case TOKTSHREQ:
        case TOKTRPAR:
            /* assignments and function calls are all prohibited */
            errsig(ctx->prscxerr, ERR_BADSPECEXPR);
            
        case TOKTDOT:
            /* property lookup - only allow no argument version */
            if (expr->prsnnlf == 3)
                errsig(ctx->prscxerr, ERR_BADSPECEXPR);
            
            /* replace with data-only (no method call) version */
            expr->prsntyp = TOKTDOTSPEC;
            break;
            
        default:
            /* other expressions are acceptable */
            break;
        }
        
        /* go through all subnodes of this node */
        for (i = 0 ; i < expr->prsnnlf ; ++i)
            prs_check_spec(ctx, expr->prsnv.prsnvn[i]);
    }
}

/*
 *   Parse an expression and generate code for it using speculative
 *   evaluation rules 
 */
void prsxgen_spec(prscxdef *ctx)
{
    prsndef *exprnode;

    /* parse the expression */
    exprnode = prsexpr(ctx);

    /* check for speculative execution problems */
    prs_check_spec(ctx, exprnode);

    /* generate the expression, then reset the parse node pool */
    prsgexp(ctx, exprnode);
    prsrstn(ctx);
}


/*
 *   Parse an expression and generate code, checking the expression for a
 *   possibly incorrect assigment if in C mode. 
 */
void prsxgen_pia(prscxdef *ctx)
{
    prsndef *exprnode;

    /* parse the expression */
    exprnode = prsexpr(ctx);

    /* check for a possibly incorrect assignment if in C mode */
    if ((ctx->prscxtok->tokcxflg & TOKCXFCMODE)
        && exprnode && exprnode->prsntyp == TOKTASSIGN)
        errlog(ctx->prscxerr, ERR_PIA);

    /* generate the expression, and reset the parse node pool */
    prsgexp(ctx, exprnode);
    prsrstn(ctx);
}



/* opcodes for general binary operators */
uchar prsbopl[] =
{
    OPCADD, OPCSUB, OPCDIV, OPCMUL, 0, OPCEQ, OPCNE, OPCGT, OPCGE,
    OPCLT, OPCLE, OPCMOD, OPCBAND, OPCBOR, OPCXOR, OPCSHL, OPCSHR
};

/* descend an expression tree and generate code for the expression */
void prsgexp(prscxdef *ctx, prsndef *n)
{
    int cnt;
    
    switch(n->prsnnlf)
    {
    case 0:
        prsdef(ctx, &n->prsnv.prsnvt, TOKSTFWDOBJ);
        emtval(ctx->prscxemt, &n->prsnv.prsnvt, ctx->prscxpool);
        break;
        
    case 1:
        switch(n->prsntyp)
        {
        case TOKTINC:
        case TOKTDEC:
        case TOKTPOSTINC:
        case TOKTPOSTDEC:
            prsglval(ctx, n->prsntyp, n->prsnv.prsnvn[0]);
            break;
            
        case TOKTPLUS:
        case TOKTMINUS:
        case TOKTNOT:
        case TOKTTILDE:
        case TOKTNEW:
        case TOKTDELETE:
            prsgexp(ctx, n->prsnv.prsnvn[0]);
            if (n->prsntyp != TOKTPLUS)
                emtop(ctx->prscxemt,
                      n->prsntyp == TOKTMINUS ? OPCNEG :
                      n->prsntyp == TOKTTILDE ? OPCBNOT :
                      n->prsntyp == TOKTNEW ? OPCNEW :
                      n->prsntyp == TOKTDELETE ? OPCDELETE :
                      OPCNOT);
            break;

        case TOKTLPAR:
            prsgfn(ctx, n->prsnv.prsnvn[0], 0);
            break;

        default:
            errsig(ctx->prscxerr, ERR_REQUNO);
        }
        break;
        
    case 2:
        switch(n->prsntyp)
        {
        case TOKTAND:
        case TOKTOR:
        {
            uint lab;
            
            lab = emtglbl(ctx->prscxemt);
            prsgexp(ctx, n->prsnv.prsnvn[0]);
            emtjmp(ctx->prscxemt,
                   (uchar)(n->prsntyp == TOKTAND ? OPCJSF : OPCJST),
                   lab);
            prsgexp(ctx, n->prsnv.prsnvn[1]);
            emtslbl(ctx->prscxemt, &lab, TRUE);
            break;
        }
        
        case TOKTPLUS:
        case TOKTMINUS:
        case TOKTDIV:
        case TOKTMOD:
        case TOKTTIMES:
        case TOKTEQ:
        case TOKTNE:
        case TOKTGT:
        case TOKTGE:
        case TOKTLT:
        case TOKTLE:
        case TOKTBAND:
        case TOKTBOR:
        case TOKTXOR:
        case TOKTSHL:
        case TOKTSHR:
            prsgexp(ctx, n->prsnv.prsnvn[0]);
            prsgexp(ctx, n->prsnv.prsnvn[1]);
            emtop(ctx->prscxemt, prsbopl[n->prsntyp - TOKTPLUS]);
            break;
            
        case TOKTCOMMA:
            prsgexp(ctx, n->prsnv.prsnvn[0]);
            prsgexp(ctx, n->prsnv.prsnvn[1]);
            break;
            
        case TOKTLPAR:                      /* function call with arguments */
            cnt = prsgargs(ctx, n->prsnv.prsnvn[1]);      /* push arguments */
            prsgfn(ctx, n->prsnv.prsnvn[0], cnt);
            break;
        
        case TOKTASSIGN:
        case TOKTPLEQ:
        case TOKTMINEQ:
        case TOKTDIVEQ:
        case TOKTMODEQ:
        case TOKTTIMEQ:
        case TOKTBANDEQ:
        case TOKTBOREQ:
        case TOKTXOREQ:
        case TOKTSHLEQ:
        case TOKTSHREQ:
            prsgexp(ctx, n->prsnv.prsnvn[1]);      /* eval right side of := */
            prsglval(ctx, n->prsntyp, n->prsnv.prsnvn[0]);
            break;
            
        case TOKTDOT:
            prsgdot(ctx, n, 0);
            break;

        case TOKTDOTSPEC:
            prsgdotspec(ctx, n);
            break;
            
        case TOKTLBRACK:
            prsgexp(ctx, n->prsnv.prsnvn[0]);    /* push list being indexed */
            prsgexp(ctx, n->prsnv.prsnvn[1]);       /* push the index value */
            emtop(ctx->prscxemt, OPCINDEX);
            break;
        
        case TOKTRBRACK:
        {
            prsndef *nsub;
            uint     cnt;
            
            for (cnt = 0, nsub = n ; nsub
                 ; ++cnt, nsub = nsub->prsnv.prsnvn[1])
                prsgexp(ctx, nsub->prsnv.prsnvn[0]);
            emtop(ctx->prscxemt, OPCCONS);         /* construction operator */
            emtint2(ctx->prscxemt, cnt);
            break;
        }
            
        default:
            errsig(ctx->prscxerr, ERR_INVBIN);
        }
        break;

    case 3:
        switch(n->prsntyp)
        {
        case TOKTQUESTION:
        {
            uint lfalse;                          /* label for FALSE branch */
            uint ldone;                       /* label for end of condition */
            
            prsgexp(ctx, n->prsnv.prsnvn[0]);         /* evaluate condition */
            lfalse = emtglbl(ctx->prscxemt);
            emtjmp(ctx->prscxemt, OPCJF, lfalse);
            prsgexp(ctx, n->prsnv.prsnvn[1]);             /* eval TRUE expr */
            ldone = emtglbl(ctx->prscxemt);
            emtjmp(ctx->prscxemt, OPCJMP, ldone);
            emtslbl(ctx->prscxemt, &lfalse, TRUE);
            prsgexp(ctx, n->prsnv.prsnvn[2]);          /* eval FALSE branch */
            emtslbl(ctx->prscxemt, &ldone, TRUE);
            break;
        }
        case TOKTDOT:
            cnt = prsgargs(ctx, n->prsnv.prsnvn[2]);
            prsgdot(ctx, n, cnt);
            break;
        }
        break;
    }
}

/* generate code for initializer */
void prsgini(prscxdef *ctx, prsndef *node, uint curfr)
{
    int lcl;
    
    /* descend tree - always generate in order specified by user */
    if (node->prsnv.prsnvn[2])
        prsgini(ctx, node->prsnv.prsnvn[2], curfr);
    
    /* generate OPCLINE if one was specified */
    if (node->prsnnlf == 4)
    {
        uchar *p;
        uint   oldofs;
        
        /* tell the line source about the OPCLINE instruction */
        dbgclin(ctx->prscxtok, ctx->prscxemt->emtcxobj,
                (uint)ctx->prscxemt->emtcxofs);
        
        /* now actually emit the OPCLINE instruction */
        oldofs = ctx->prscxemt->emtcxofs;
        p = ctx->prscxpool + node->prsnv.prsnvn[3]->prsnv.prsnvt.tokofs;
        emtmem(ctx->prscxemt, p, (uint)(*(p + 1) + 1));
        
        /* fix it up with the correct frame */
        emtint2at(ctx->prscxemt, curfr, oldofs + 2);
    }
    
    prsgexp(ctx, node->prsnv.prsnvn[0]);
    emtop(ctx->prscxemt, OPCSETLCL);
    lcl = node->prsnv.prsnvn[1]->prsnv.prsnvt.tokval;
    emtint2(ctx->prscxemt, lcl);
}

/* generate code for a function all */
static void prsgfn(prscxdef *ctx, prsndef *n, int cnt)
{
    if (n->prsnnlf == 0 && n->prsnv.prsnvt.toktyp == TOKTSYMBOL
        && (n->prsnv.prsnvt.toksym.tokstyp == TOKSTUNK
            || n->prsnv.prsnvt.toksym.tokstyp != TOKSTLOCAL))
    {
        tokdef *t = &n->prsnv.prsnvt;
        
        /* if symbol is undefined, add to symbol table as a new function */
        prsdef(ctx, t, TOKSTFWDFN);
        
        /* non-local-var symbol: interpret as a function */
        switch(t->toksym.tokstyp)
        {
        case TOKSTFUNC:
        case TOKSTFWDFN:
            emtop(ctx->prscxemt, OPCCALL);
            break;
        case TOKSTEXTERN:
            emtop(ctx->prscxemt, OPCCALLEXT);
            break;
        case TOKSTBIFN:
            emtop(ctx->prscxemt, OPCBUILTIN);
            break;
        default:
            errlog(ctx->prscxerr, ERR_REQFCN);
        }
        emtbyte(ctx->prscxemt, cnt);            /* emit argument count byte */
        emtint2(ctx->prscxemt, t->toksym.toksval);
    }
    else
    {
        /* expression - evaluate, and assume it yields a function pointer */
        prsgexp(ctx, n);
        emtop(ctx->prscxemt, OPCPTRCALL);
        emtbyte(ctx->prscxemt, cnt);                 /* argument count byte */
    }
}

/* generate code for a property lookup */
static void prsgdot(prscxdef *ctx, prsndef *n, int cnt)
{
    prsndef *n0 = n->prsnv.prsnvn[0];
    prsndef *n1 = n->prsnv.prsnvn[1];
    int      inh;                                 /* flag: "inherited.prop" */
    int      slf;                                      /* flag: "self.prop" */
    int      objflg;           /* flag: LHS of dot is constant object value */
    int      exp_inh;                  /* flag: "inherited superclass.prop" */
    uchar    reg_op;                /* operator to use for regular property */
    uchar    ptr_op;                /* operator to use for property pointer */
    objnum   exp_inh_obj;     /* explicit superclass object to inherit from */

    /* presume we'll have a regular object expression for the LHS */
    inh = exp_inh = slf = objflg = FALSE;

    /* set up the default opcodes for ordinary object expressions */
    reg_op = OPCGETP;
    ptr_op = OPCPTRGETP;

    /* check for special left-hand-side values */
    if (n0->prsnnlf == 0 && n0->prsnv.prsnvt.toktyp == TOKTSYMBOL)
    {
        switch(n0->prsnv.prsnvt.toksym.tokstyp)
        {
        case TOKSTINHERIT:
            inh = TRUE;
            reg_op = OPCINHERIT;
            ptr_op = OPCPTRINH;
            break;
            
        case TOKSTSELF:
            if (ctx->prscxflg & PRSCXFFUNC) errlog(ctx->prscxerr, ERR_NOSELF);
            slf = TRUE;
            reg_op = OPCGETPSELF;
            ptr_op = OPCGETPPTRSELF;
            break;
            
        case TOKSTOBJ:
        case TOKSTFWDOBJ:
            objflg = TRUE;
            reg_op = OPCGETPOBJ;
            break;
        }
    }
    else if (n0->prsnnlf == 1 && n0->prsntyp == TOKTEXPINH)
    {
        tokdef *sc_tok;
        
        /* 
         *   explicit superclass inheritance - this is inheritance, but
         *   with an explicit superclass value 
         */
        inh = TRUE;
        exp_inh = TRUE;
        reg_op = OPCEXPINH;
        ptr_op = OPCEXPINHPTR;

        /* get the explicit superclass subnode */
        sc_tok = &n0->prsnv.prsnvn[0]->prsnv.prsnvt;

        /* make sure the superclass is an object */
        prsdef(ctx, sc_tok, TOKSTFWDOBJ);

        /* get the explicit superclass - it's the subnode */
        exp_inh_obj = sc_tok->toksym.toksval;
    }

    /* 
     *   push object whose property we're getting unless inheriting or
     *   getting a property of 'self' or of a fixed object (in which cases
     *   the object is implicit in the opcode, rather than being on the
     *   stack) 
     */
    if (!inh && !slf && !objflg)
        prsgexp(ctx, n0);

    if (n1->prsnnlf == 0
        && n1->prsnv.prsnvt.toktyp == TOKTPOUND)
    {
        /* RHS is a property symbol - do a simple GETP (or INHERIT) */
        prpnum prop = n1->prsnv.prsnvt.tokofs;

        /* 
         *   emit the opcode and argument count - use the regular
         *   (non-pointer) opcode, since we have a regular property value 
         */
        emtop(ctx->prscxemt, reg_op);
        emtbyte(ctx->prscxemt, cnt);

        /* if we are encoding the object in the instruction, emit it now */
        if (objflg)
            emtint2(ctx->prscxemt, n0->prsnv.prsnvt.toksym.toksval);

        /* add the property value */
        emtint2(ctx->prscxemt, prop);

        /* 
         *   if this is an "inherited" with an explicit superclass, add
         *   the explicit superclass value 
         */
        if (exp_inh)
            emtint2(ctx->prscxemt, exp_inh_obj);
    }
    else
    {
        /* 
         *   evaluate the LHS unconditionally, even if it's just a fixed
         *   object, since we have no special instruction that encodes an
         *   object value in a pointer property evaluation 
         */
        if (objflg)
            prsgexp(ctx, n0);

        /* RHS is an expression - evaluate it */
        prsgexp(ctx, n1);

        /* 
         *   emit the opcode and the byte count - use the pointer opcode,
         *   since we have a property pointer 
         */
        emtop(ctx->prscxemt, ptr_op);
        emtbyte(ctx->prscxemt, cnt);

        /* 
         *   if this is an "inherited" with an explicit superclass, add
         *   the explicit superclass value 
         */
        if (exp_inh)
            emtint2(ctx->prscxemt, exp_inh_obj);
    }
}

/* generate code for a property lookup during speculative evaluation */
static void prsgdotspec(prscxdef *ctx, prsndef *n)
{
    prsndef *n0 = n->prsnv.prsnvn[0];
    prsndef *n1 = n->prsnv.prsnvn[1];

    /* push object whose property we're getting */
    prsgexp(ctx, n0);

    if (n1->prsnnlf == 0
        && n1->prsnv.prsnvt.toktyp == TOKTPOUND)
    {
        /* RHS is a property symbol - do a simple GETP (or INHERIT) */
        prpnum prop = n1->prsnv.prsnvt.tokofs;

        /* push the data property */
        emtop(ctx->prscxemt, OPCGETPDATA);
        emtint2(ctx->prscxemt, prop);
    }
    else
    {
        /* RHS is an expression - evaluate it */
        prsgexp(ctx, n1);

        /* get the data property */
        emtop(ctx->prscxemt, OPCPTRGETPDATA);
    }
}

/* token -> assignment type field conversions */
static uchar prsglcvt[] =
{
    OPCASIINC | OPCASIPRE,                                        /* pre ++ */
    OPCASIINC | OPCASIPOST,                                      /* post ++ */
    OPCASIDEC | OPCASIPRE,                                        /* pre -- */
    OPCASIDEC | OPCASIPOST,                                      /* post ++ */
    OPCASIADD,                                                        /* += */
    OPCASISUB,                                                        /* -= */
    OPCASIDIV,                                                        /* /= */
    OPCASIMUL,                                                        /* *= */
    OPCASIDIR                                                         /* := */
};

/* extended assignment type field conversions */
static uchar prsglcvt_ext[] =
{
    OPCASIMOD,                                                        /* %= */
    OPCASIBAND,                                                       /* &= */
    OPCASIBOR,                                                        /* |= */
    OPCASIXOR,                                                        /* ^= */
    OPCASISHL,                                                       /* <<= */
    OPCASISHR                                                        /* >>= */
};
    
/* generate code for an l-value */
static void prsglval(prscxdef *ctx, int typ, prsndef *n)
{
    uchar op;
    uchar op2;

    if (typ - TOKTINC < sizeof(prsglcvt)/sizeof(prsglcvt[0]))
    {
        op = OPCASI_MASK | prsglcvt[typ - TOKTINC];   /* form basic op code */
        op2 = 0;
    }
    else
    {
        op = OPCASI_MASK | OPCASIEXT;
        op2 = prsglcvt_ext[typ - TOKTMODEQ];
    }
    
    switch(n->prsnnlf)
    {
    case 0:
        {
            int lclnum;
            int styp;

            /* DON'T assume this is a 'self.' property if unknown */
            /* DON'T prsdef(ctx, &n->prsnv.prsnvt, TOKSTPROP); */

            if (n->prsnv.prsnvt.toktyp == TOKTSYMBOL)
            {
                if (ctx->prscxflg & PRSCXFWTCH)
                    errsig(ctx->prscxerr, ERR_WTCHLCL);

                if ((styp = n->prsnv.prsnvt.toksym.tokstyp) == TOKSTLOCAL)
                {
                    emtop(ctx->prscxemt, op | OPCASILCL);
                    if (op2) emtop(ctx->prscxemt, op2);
                    lclnum = n->prsnv.prsnvt.toksym.toksval;
                    emtint2(ctx->prscxemt, lclnum);
                }
                else if (styp == TOKSTPROP)
                {
                    if (ctx->prscxflg & PRSCXFFUNC)
                        errlog(ctx->prscxerr, ERR_NOSELF);
                    
                    emtop(ctx->prscxemt, OPCPUSHSELF);
                    emtop(ctx->prscxemt, op | OPCASIPRP);
                    if (op2) emtop(ctx->prscxemt, op2);
                    lclnum = n->prsnv.prsnvt.toksym.toksval;
                    emtint2(ctx->prscxemt, lclnum);
                }
                else if (styp == TOKSTPROPSPEC)
                    errsig(ctx->prscxerr, ERR_BADSPECEXPR);
                else
                    errlog(ctx->prscxerr, ERR_INVASI);
            }
            else
                errlog(ctx->prscxerr, ERR_INVASI);
        }
        break;
        
    case 2:
        switch(n->prsntyp)
        {
        case TOKTDOT:
            {
                prsndef *n1 = n->prsnv.prsnvn[1];
                int      base_opc;
                int      direct_prop;
                prpnum   prop;
                    
                /* generate the expression providing the object */
                prsgexp(ctx, n->prsnv.prsnvn[0]);

                /* determine the correct base opcode */
                if (n1->prsnnlf == 0 && n1->prsnv.prsnvt.toktyp == TOKTPOUND)
                {

                    /* simple property assignment */
                    direct_prop = TRUE;
                    base_opc = OPCASIPRP;

                    /* remember the property id */
                    prop = n1->prsnv.prsnvt.tokofs;
                }
                else
                {
                    /* assignment through property pointer */
                    direct_prop = FALSE;
                    base_opc = OPCASIPRPPTR;

                    /* generate the property pointer expression */
                    prsgexp(ctx, n1);
                }

                /* generate the appropriate property assignment opcode */
                emtop(ctx->prscxemt, op | base_opc);
                if (op2) emtop(ctx->prscxemt, op2);

                /* if it's a simple assignment, add the property code */
                if (direct_prop) emtint2(ctx->prscxemt, prop);

                break;
            }
            
        case TOKTLBRACK:
            prsgexp(ctx, n->prsnv.prsnvn[0]);             /* evaluate list  */
            prsgexp(ctx, n->prsnv.prsnvn[1]);             /* evaluate index */
            emtop(ctx->prscxemt, op | OPCASIIND);
            if (op2) emtop(ctx->prscxemt, op2);
            prsglval(ctx, TOKTASSIGN, n->prsnv.prsnvn[0]);      /* set list */
            break;
            
        default:
            errlog(ctx->prscxerr, ERR_INVASI);
        }
        break;
        
    default:
        errlog(ctx->prscxerr, ERR_INVASI);
    }
}

/* generate code for an argument list; returns argument count */
static int prsgargs(prscxdef *ctx, prsndef *n)
{
    int cnt = 0;
    
    if (n->prsnnlf == 2 && n->prsntyp == TOKTRPAR)
    {
        cnt = prsgargs(ctx, n->prsnv.prsnvn[1]);
        prsgexp(ctx, n->prsnv.prsnvn[0]);
    }
    else
        prsgexp(ctx, n);
    
    return(cnt + 1);
}

