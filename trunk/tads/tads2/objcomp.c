#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/OBJCOMP.C,v 1.3 1999/07/11 00:46:30 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  objcomp.c - object manipulation routines for compiler
Function
  Provides routines used only by the compiler
Notes
  Split off from obj main module to make run-time smaller
Modified
  12/18/92 MJRoberts     - creation
*/

#include "os.h"
#include "std.h"
#include "err.h"
#include "mcm.h"
#include "obj.h"

/*
 *   Set up for emitting code into an object.  No undo information is
 *   kept for this type of operation, as it is presumed that the object is
 *   being compiled rather than being manipulated at run-time.  Any
 *   previous value for the property is deleted, a property header is set
 *   up, and the offset of the next free byte in the object is returned.
 */
uint objemt(mcmcxdef *ctx, objnum objn, prpnum prop, dattyp typ)
{
    objdef *objptr;
    prpdef *p;
    
    objptr = (objdef *)mcmlck(ctx, (mcmon)objn);
    
    ERRBEGIN(ctx->mcmcxgl->mcmcxerr)

    objdelp(ctx, objn, prop, FALSE);   /* delete old property value, if any */
    p = objpfre(objptr);                        /* get top of property area */
    
    if ((char *)p - (char *)objptr + PRPHDRSIZ >=
        mcmobjsiz(ctx, (mcmon)objn))
    {
        ushort newsiz = 64 + ((objfree(objptr) + PRPHDRSIZ) -
                              mcmobjsiz(ctx, (mcmon)objn));
        objptr = objexp(ctx, objn, &newsiz);
        p = objpfre(objptr);                       /* object may have moved */
    }
    
    /* set up property header as much as we can (don't know size yet) */
    prpsetprop(p, prop);
    prptype(p) = typ;
    prpflg(p) = 0;
    objsnp(objptr, objnprop(objptr) + 1);              /* one more property */

    ERRCLEAN(ctx->mcmcxgl->mcmcxerr)
        mcmunlck(ctx, (mcmon)objn);
    ERRENDCLN(ctx->mcmcxgl->mcmcxerr)

    /* dirty the cache object and release the lock before return */
    mcmtch(ctx, objn);
    mcmunlck(ctx, (mcmon)objn);
    return(((uchar *)prpvalp(p)) - ((uchar *)objptr));
}

/* done emitting code into property - finish setting object information */
void objendemt(mcmcxdef *ctx, objnum objn, prpnum prop, uint endofs)
{
    objdef *objptr;
    prpdef *p;
    uint    siz;
    
    objptr = (objdef *)mcmlck(ctx, (mcmon)objn);
    p = objofsp(objptr, objgetp(ctx, objn, prop, (dattyp *)0));
    
    siz = endofs - (((uchar *)prpvalp(p)) - ((uchar *)objptr));
    
    prpsetsize(p, siz);
    objsfree(objptr, objfree(objptr) + siz + PRPHDRSIZ);
    
    /* mark the object as changed, and unlock it */
    mcmtch(ctx, (mcmon)objn);
    mcmunlck(ctx, (mcmon)objn);
}

/* add superclasses to an object */
void objaddsc(mcmcxdef *mctx, int sccnt, objnum objn)
{
    objdef *o;
    ushort  siz;
    
    /* get lock on object */
    o = (objdef *)mcmlck(mctx, objn);
    
    /* make sure there's enough space, adding space if needed */
    if (mcmobjsiz(mctx, (mcmon)objn) - objfree(o) < 2 * sccnt)
    {
        siz = 64 + ((2 * sccnt + objfree(o)) -
                    mcmobjsiz(mctx, (mcmon)objn));
        o = objexp(mctx, objn, &siz);                  /* expand the object */
    }

    /* move properties, if any, above added superclasses */
    if (objnprop(o))
        memmove(objprp(o), ((uchar *)objprp(o)) + 2 * sccnt,
                (size_t)(((uchar *)o) + objfree(o) - (uchar *)objprp(o)));
    
    /* set new free pointer */
    objsfree(o, objfree(o) + 2 * sccnt);
    
    /* mark cache object modified and unlock it */
    mcmtch(mctx, objn);
    mcmunlck(mctx, objn);
}

/* delete an object's properties and superclasses */
void objclr(mcmcxdef *mctx, objnum objn, prpnum mindel)
{
    objdef *o;
    prpdef *p;
    int     cnt;
    prpnum  prop;
    int     indexed;
    
    /* get a lock on the object */
    o = (objdef *)mcmlck(mctx, objn);
    indexed = objflg(o) & OBJFINDEX;
    
    /* delete superclasses - move properties down over former sc array */
    if (objnprop(o))
        memmove(objsc(o), objprp(o),
                (size_t)(((uchar *)o) + objfree(o) - (uchar *)objprp(o)));
    objsnsc(o, 0);                                 /* zero superclasses now */
    
    /* delete non-"system" properties (propnum < mindel) */
    for (p = objprp(o), cnt = objnprop(o) ; cnt ; --cnt)
    {
        if ((prop = prpprop(p)) >= mindel)
        {
            prpflg(p) &= ~PRPFIGN;   /* delete even if it was marked ignore */
            objdelp(mctx, objn, prop, FALSE);  /* remove prpdef from object */
            /* p is left pointing at next prop, as it was moved down */
        }
        else
            p = objpnxt(p);                   /* advance over this property */
    }
    
    /* mark cache object modified and unlock it */
    mcmtch(mctx, objn);
    mcmunlck(mctx, objn);
    if (indexed) objindx(mctx, objn);
}

/* set up just-compiled object:  mark static part and original props */
void objcomp(mcmcxdef *mctx, objnum objn, int for_debug)
{
    objdef *objptr;
    prpdef *p;
    prpdef *nxt;
    int     cnt;
    
    /* lock object */
    objptr = (objdef *)mcmlck(mctx, (mcmon)objn);

    /* 
     *   first, go through the properties, and delete each one that's
     *   marked as ignored 
     */
    for (cnt = objnprop(objptr), p = objprp(objptr) ; cnt != 0 ;
         p = nxt, --cnt)
    {
        /* remember the next property */
        nxt = objpnxt(p);

        /* if this is marked as being ignored, delete it */
        if ((prpflg(p) & PRPFIGN) != 0)
        {
            /* 
             *   Delete the property.  If we're compiling in debug mode,
             *   don't really delete anything, but simply mark the
             *   property as deleted; this is necessary because certain
             *   debug records are not self-relative, hence are not
             *   tolerant of changes to the internal structure of the
             *   object.  If we're not compiling for debugging, we can
             *   actually delete properties, because all non-debug code is
             *   completely self-relative and thus can be moved around
             *   inside the object without any problems.  
             */
            if (for_debug)
            {
                /* simply mark the property as deleted */
                prpflg(p) |= PRPFDEL;

                /* 
                 *   clear the IGNORE flag, since we IGNORE and DELETED
                 *   are mutually exclusive 
                 */
                prpflg(p) &= ~PRPFIGN;
            }
            else
            {
                /* clear the flags so we can really delete it */
                prpflg(p) &= ~(PRPFORG | PRPFIGN | PRPFDEL);

                /* delete it */
                objdelp(mctx, objn, (prpnum)prpprop(p), FALSE);

                /* 
                 *   continue from the present location, since we moved
                 *   the next property to the current location 
                 */
                nxt = p;
            }
        }
    }
    
    /* set static entries:  free space pointer, and number of properties */
    objsetst(objptr, objnprop(objptr));
    objsetrst(objptr, objfree(objptr));

    /* go through properties, marking each as original */
    for (cnt = objnprop(objptr), p = objprp(objptr) ; cnt != 0 ;
         p = objpnxt(p), --cnt)
    {
        assert(p < objptr + mcmobjsiz(mctx, (mcmon)objn));
        prpflg(p) |= PRPFORG;             /* set ORIGINAL flag for property */
    }
    
    /* mark object changed, and unlock it */
    mcmtch(mctx, objn);
    mcmunlck(mctx, objn);
}

