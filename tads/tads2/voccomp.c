#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/VOCCOMP.C,v 1.2 1999/05/17 02:52:13 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  voccomp.c - vocabulary routines not used in runtime
Function
  some extra vocabulary routines not needed in runtime
Notes
  removed from base voc.c to save space in runtime
Modified
  12/18/92 MJRoberts     - creation
*/

#include "os.h"
#include "voc.h"

/* delete all inherited vocabulary records */
void vocdelinh(voccxdef *ctx)
{
    int       i;
    vocdef   *v;
    vocdef   *prv;
    vocdef   *nxt;
    vocdef  **vp;
    vocwdef  *vw;
    vocwdef  *prvw;
    vocwdef  *nxtw;
    uint      idx;
    uint      nxtidx;
    int       deleted_vocdef;
    
    /* go through each hash value looking for matching words */
    for (i = VOCHASHSIZ, vp = ctx->voccxhsh ; i ; ++vp, --i)
    {
        /* go through all words in this hash chain */
        for (prv = (vocdef *)0, v = *vp ; v ; v = nxt)
        {
            nxt = v->vocnxt;
            deleted_vocdef = FALSE;

            /* go through each vocwdef relation in the word's list */
            for (prvw = 0, idx = v->vocwlst, vw = vocwget(ctx, idx) ; vw ;
                 vw = nxtw, idx = nxtidx)
            {
                /* remember the next item in the vocwdef list */
                nxtidx = vw->vocwnxt;
                nxtw = vocwget(ctx, nxtidx);

                /* delete word if it's inherited */
                if (vw->vocwflg & VOCFINH)
                {
                    /* unlink this vocwdef from the vocdef's list */
                    if (prvw)
                        prvw->vocwnxt = vw->vocwnxt;
                    else
                        v->vocwlst = vw->vocwnxt;

                    /* link the vocwdef into the vocwdef free list */
                    vw->vocwnxt = ctx->voccxwfre;
                    ctx->voccxwfre = idx;

                    /*
                     *   if there's nothing left in the vocdef's list,
                     *   delete the vocdef as well 
                     */
                    if (v->vocwlst == VOCCXW_NONE)
                    {
                        /* unlink from hash chain */
                        if (prv) prv->vocnxt = v->vocnxt;
                        else *vp = v->vocnxt;
                        
                        /* link into free chain */
                        v->vocnxt = ctx->voccxfre;
                        ctx->voccxfre = v;

                        /* note that it's been deleted */
                        deleted_vocdef = TRUE;
                    }
                }
                else
                    prvw = vw;  /* this one is still in list - it's now prv */
            }

            /* if we didn't delete this vocdef, advance prv onto it */
            if (!deleted_vocdef)
                prv = v;
        }
    }
}

/* renumber an object (used with 'modify') */
void vociren(voccxdef *ctx, objnum oldnum, objnum newnum)
{
    int       i;
    vocdef   *v;
    vocdef  **vp;
    vocwdef  *vw;


    /* make sure we have a page table entry for the original object */
    vocialo(ctx, newnum);

    /* move the old object's inheritance record to the new slot */
    vocinh(ctx, newnum) = vocinh(ctx, oldnum);
    vocinh(ctx, oldnum) = 0;

    /* make the old object an honorary class object */
    vocinh(ctx, newnum)->vociflg |= VOCIFCLASS;

    /* 
     *   Renumber any vocabulary associated with the old object to the new
     *   object.
     */
    for (i = VOCHASHSIZ, vp = ctx->voccxhsh ; i != 0 ; ++vp, --i)
    {
        /* go through all words in this hash chain */
        for (v = *vp ; v != 0 ; v = v->vocnxt)
        {
            /* go through all vocwdef's defined for this word */
            for (vw = vocwget(ctx, v->vocwlst) ; vw ;
                 vw = vocwget(ctx, vw->vocwnxt))
            {
                /* 
                 *   renumber this word and mark it as being associated
                 *   with a class if it's associated with the original
                 *   object 
                 */
                if (vw->vocwobj == oldnum)
                {
                    vw->vocwobj = newnum;
                    vw->vocwflg |= VOCFCLASS;
                }
            }
        }
    }
}

