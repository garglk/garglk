#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/SUPRUN.C,v 1.3 1999/07/11 00:46:30 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  suprun.c - setup functions for run-time
Function
  This module implements the set-up functions needed at run-time
Notes
  Separated from sup.c to avoid having to link functions needed only
  in the compiler into the runtime.
Modified
  12/16/92 MJRoberts     - add TADS/Graphic extensions
  04/11/92 MJRoberts     - creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "os.h"
#include "std.h"
#include "obj.h"
#include "prp.h"
#include "dat.h"
#include "tok.h"
#include "mcm.h"
#include "mch.h"
#include "sup.h"
#include "bif.h"

supbidef osfar_t supbitab[] =
{
    { "say", bifsay },
    { "car", bifcar },
    { "cdr", bifcdr },
    { "length", biflen },
    { "randomize", bifsrn },
    { "rand", bifrnd },
    { "substr", bifsub },
    { "cvtstr", bifcvs },
    { "cvtnum", bifcvn },
    { "upper", bifupr },
    { "lower", biflwr },
    { "caps", bifcap },
    { "find", biffnd },
    { "getarg", bifarg },
    { "datatype", biftyp },
    { "setdaemon", bifsdm },
    { "setfuse", bifsfs },
    { "setversion", bifsvn },
    { "notify", bifnfy },
    { "unnotify", bifunn },
    { "yorn", bifyon },
    { "remfuse", bifrfs },
    { "remdaemon", bifrdm },
    { "incturn", bifinc },
    { "quit", bifqui },
    { "save", bifsav },
    { "restore", bifrso },
    { "logging", biflog },
    { "input", bifinp },
    { "setit", bifsit },
    { "askfile", bifask },
    { "setscore", bifssc },
    { "firstobj", biffob },
    { "nextobj", bifnob },
    { "isclass", bifisc },
    { "restart", bifres },
    { "debugTrace", biftrc },
    { "undo", bifund },
    { "defined", bifdef },
    { "proptype", bifpty },
    { "outhide", bifoph },
    { "runfuses", bifruf },
    { "rundaemons", bifrud },
    { "gettime", biftim },
    { "getfuse", bifgfu },
    { "intersect", bifsct },
    { "inputkey", bifink },
    { "objwords", bifwrd },
    { "addword", bifadw },
    { "delword", bifdlw },
    { "getwords", bifgtw },
    { "nocaps", bifnoc },
    { "skipturn", bifskt },
    { "clearscreen", bifcls },
    { "firstsc", bif1sc },
    { "verbinfo", bifvin },
    { "fopen", biffopen },
    { "fclose", biffclose },
    { "fwrite", biffwrite },
    { "fread", biffread },
    { "fseek", biffseek },
    { "fseekeof", biffseekeof },
    { "ftell", bifftell },
    { "outcapture", bifcapture },

    { "systemInfo", bifsysinfo },
    { "morePrompt", bifmore },
    { "parserSetMe", bifsetme },
    { "parserGetMe", bifgetme },

    { "reSearch", bifresearch },
    { "reGetGroup", bifregroup },
    { "inputevent", bifinpevt },
    { "timeDelay", bifdelay },
    { "setOutputFilter", bifsetoutfilter },
    { "execCommand", bifexec },
    { "parserGetObj", bifgetobj },
    { "parseNounList", bifparsenl },
    { "parserTokenize", bifprstok },
    { "parserGetTokTypes", bifprstoktyp },
    { "parserDictLookup", bifprsdict },
    { "parserResolveObjects", bifprsrslv },
    { "parserReplaceCommand", bifprsrplcmd },
    { "exitobj", bifexitobj },
    { "inputdialog", bifinpdlg },
    { "resourceExists", bifresexists },

    /* 
     *   To accommodate systemInfo, we've removed g_readpic.  This has been
     *   present for a while and didn't do anything, since tads/g was never
     *   released.  We can therefore use its function slot without requiring
     *   a format change.  
     */
    /* { "g_readpic", bifgrp }, */

    /* likewise these */
    /* { "g_showpic", bifgsp }, */
    /* { "g_sethot", bifgsh }, */
    /* { "g_inventory", bifgin }, */

    /* 
     *   more tads/g functions - these are kept around mostly as
     *   placeholders in case we want to use these slots in the future 
     */
    /* { "g_compass", bifgco }, - removed for reSearch */
    /* { "g_overlay", bifgov }, - removed for reGetGroup */
    /* { "g_mode", bifgmd }, - removed for inputevent */
    /* { "g_music", bifgmu }, - removed for timeDelay */
    /* { "g_pause", bifgpa }, - removed for setOutputFilter */
    /* { "g_effect", bifgef }, - removed for execCommand */
    /* { "g_sound", bifgsn }, - removed for parserGetObj */

    /* a few more placeholder slots for future expansion */
    /* { "__reserved_func_0", bifgsn }, - removed for parseNounList */
    /* { "__reserved_func_2", bifgsn }, - removed for parserTokenize */
    /* { "__reserved_func_3", bifgsn }, - removed for parserGetTokTypes */
    /* { "__reserved_func_4", bifgsn }, - removed for parserDictLookup */
    /* { "__reserved_func_5", bifgsn }, - removed for parserResolveObjects */
    /* { "__reserved_func_6", bifgsn }, - removed for parserReplaceCommand */
    /* { "__reserved_func_7", bifgsn }, - removed for exitobj */
    /* { "__reserved_func_8", bifgsn }, - removed for inputdialog */
    /* { "__reserved_func_9", bifgsn }, - removed for resourceExists */

    /* more slots added in 2.3.1 */
    { "__reserved_func_10", bifgsn },
    { "__reserved_func_11", bifgsn },
    { "__reserved_func_12", bifgsn },
    { "__reserved_func_13", bifgsn },
    { "__reserved_func_14", bifgsn },
    { "__reserved_func_15", bifgsn },
    { "__reserved_func_16", bifgsn },
    { "__reserved_func_17", bifgsn },
    { "__reserved_func_18", bifgsn },
    { "__reserved_func_19", bifgsn },
    

    { 0, 0 }
};

/* set up built-in functions array without symbol table (for run-time) */
void supbif(supcxdef *sup, void (*bif[])(bifcxdef*, int), int bifsiz)
{
    supbidef *p;
    int       i;

    for (p = supbitab, i = 0 ; p->supbinam ; ++i, ++p)
    {
        if (i >= bifsiz) errsig(sup->supcxerr, ERR_MANYBIF);
        bif[i] = p->supbifn;
    }
}

/* set up contents property for load-on-demand */
void supcont(void *ctx0, objnum obj, prpnum prp)
{
    supcxdef  *ctx = (supcxdef *)ctx0;
    vocidef ***vpg;
    vocidef  **v;
    voccxdef  *voc = ctx->supcxvoc;
    int        i;
    int        j;
    int        len = 2;
    objnum     chi;
    objnum     loc;

    /* be sure the buffer is allocated */
    if (!ctx->supcxbuf)
    {
        ctx->supcxlen = 512;
        ctx->supcxbuf = mchalo(ctx->supcxerr, ctx->supcxlen,
                               "supcont");
    }

    assert(prp == PRP_CONTENTS);         /* the only thing that makes sense */
    for (vpg = voc->voccxinh, i = 0 ; i < VOCINHMAX ; ++vpg, ++i)
    {
        if (!*vpg) continue;                     /* no entries on this page */
        for (v = *vpg, chi = (i << 8), j = 0 ; j < 256 ; ++v, ++chi, ++j)
        {
            /* if there's no record at this location, skip it */
            if (!*v) continue;

            /* inherit the location if it hasn't been set to any value */
            if ((*v)->vociloc == MCMONINV
                && !((*v)->vociflg & VOCIFLOCNIL))
                loc = (*v)->vociilc;
            else
                loc = (*v)->vociloc;

            /* if this object is in the indicated location, add it */
            if (loc == obj && !((*v)->vociflg & VOCIFCLASS))
            {
                /* see if we have room in list buffer; expand buffer if not */
                if (len + 3 > ctx->supcxlen)
                {
                    uchar *newbuf;

                    /* allocate a new buffer */
                    newbuf = mchalo(ctx->supcxerr,
                                    (len + 512), "supcont");

                    /* copy the old buffer's contents into the new buffer */
                    memcpy(newbuf, ctx->supcxbuf, ctx->supcxlen);

                    /* remember the new buffer length */
                    ctx->supcxlen = len + 512;

                    /* free the old buffer */
                    mchfre(ctx->supcxbuf);

                    /* remember the new buffer */
                    ctx->supcxbuf = newbuf;

                    /* sanity check for integer overflow */
                    if (len + 3 > ctx->supcxlen)
                        errsig(ctx->supcxmem->mcmcxgl->mcmcxerr, ERR_SUPOVF);
                }
                ctx->supcxbuf[len] = DAT_OBJECT;
                oswp2(ctx->supcxbuf + len + 1, chi);
                len += 3;
            }
        }
    }

    oswp2(ctx->supcxbuf, len);
    objsetp(ctx->supcxmem, obj, prp, DAT_LIST, ctx->supcxbuf,
            ctx->supcxrun->runcxundo);
}

static void supiwrds(voccxdef *ctx, objnum sc, objnum target, int flags)
{
    int       i;
    vocdef   *v;
    vocdef  **vp;
    vocwdef  *vw;
    
    /* go through each hash value looking for superclass object */
    for (i = VOCHASHSIZ, vp = ctx->voccxhsh ; i != 0 ; ++vp, --i)
    {
        /* go through all words in this hash chain */
        for (v = *vp ; v != 0 ; v = v->vocnxt)
        {
            /* go through all vocwdef's defined for this word */
            for (vw = vocwget(ctx, v->vocwlst) ; vw ;
                 vw = vocwget(ctx, vw->vocwnxt))
            {
                /* add word to target if it's defined for this superclass */
                if (vw->vocwobj == sc)
                    vocadd2(ctx, vw->vocwtyp, target, VOCFINH + flags,
                            v->voctxt, v->voclen,
                            (v->vocln2 ? v->voctxt + v->voclen : (uchar *)0),
                            v->vocln2);
            }
        }
    }
}

/* set up inherited vocabulary for a particular object */
void supivoc1(supcxdef *sup, voccxdef *ctx, vocidef *v, objnum target,
              int inh_from_obj, int flags)
{
    objnum   *sc;
    int       numsc;
    vocidef  *scv;
    
    for (numsc = v->vocinsc, sc = v->vocisc ; numsc ; ++sc, --numsc)
    {
        scv = vocinh(ctx, *sc);
        if (scv)
        {
            /* inherit from its superclasses first */
            supivoc1(sup, ctx, scv, target, FALSE, flags);
            
            /* if it's a class object, we can inherit from it */
            if (scv->vociflg & VOCIFCLASS)
            {
                /* inherit location, if we haven't already done so */
                if (v->vociilc == MCMONINV)
                {
                    if (scv->vociloc == MCMONINV)
                        v->vociilc = scv->vociilc;
                    else
                        v->vociilc = scv->vociloc;
                }
            }

            /*
             *   inherit from superclass if it's a class, or if we're
             *   supposed to inherit from any object 
             */
            if (inh_from_obj || (scv->vociflg & VOCIFCLASS))
            {
                /* inherit vocabulary if this superclass has any words */
                if (scv->vociflg & VOCIFVOC)
                    supiwrds(ctx, *sc, target, flags);
            }
        }
        else
        {
            char  buf[TOKNAMMAX + 1];

            /* get the symbol's name */
            supgnam(buf, sup->supcxtab, *sc);

            /* log an error with the symbol's name and location of first use */
            sup_log_undefobj(ctx->voccxmem, ctx->voccxerr, ERR_UNDFOBJ,
                             buf, (int)strlen(buf), *sc);
        }
    }    
}

void sup_log_undefobj(mcmcxdef *mctx, errcxdef *ec, int err,
                      char *nm, int nmlen, objnum objn)
{
    uchar  *p;
    size_t  len;

    /* 
     *   if the object has any superclasses defined, what must have
     *   happened is that we encountered an error in the course of
     *   defining the object; the object is partially defined, hence it
     *   won't show up in our records of defined objects, yet it really
     *   shouldn't count as an undefined object; simply suppress the
     *   message in this case 
     */
    if (objget1sc(mctx, objn) != MCMONINV)
        return;

    /* get the object - it contains the location where it was defined */
    p = mcmlck(mctx, (mcmon)objn);

    /* skip the object header */
    p += OBJDEFSIZ;

    /* get the length of the name */
    len = strlen((char *)p);

#ifdef OS_ERRLINE
    len += strlen((char *)p + len + 1);
#endif

    /* log the error */
    errlog2(ec, err, ERRTSTR, errstr(ec, nm, nmlen),
            ERRTSTR, errstr(ec, (char *)p, len));

    /* done with the object */
    mcmunlck(mctx, (mcmon)objn);
}

