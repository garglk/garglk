#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/RUNSTAT.C,v 1.2 1999/05/17 02:52:13 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  runstat.c - tads 1 compatible runstat()
Function
  generates status line
Notes
  none
Modified
  04/04/92 MJRoberts     - creation
*/

#include "os.h"
#include "std.h"
#include "mcm.h"
#include "obj.h"
#include "run.h"
#include "tio.h"
#include "voc.h"
#include "dat.h"

static runcxdef *runctx;
static voccxdef *vocctx;
static tiocxdef *tioctx;

void runstat(void)
{
    objnum  locobj;
    int     savemoremode;

    /* get the location of the Me object */
    runppr(runctx, vocctx->voccxme, PRP_LOCATION, 0);

    /* if that's no an object, there's nothing we can do */
    if (runtostyp(runctx) != DAT_OBJECT)
    {
        rundisc(runctx);
        return;
    }

    /* get Me.location */
    locobj = runpopobj(runctx);

    /* flush any pending output */
    outflushn(0);

    /* switch to output display mode 1 (status line) */
    os_status(1);

    /* turn off MORE mode */
    savemoremode = setmore(0);

    /* call the statusLine method of the current room */
    runppr(runctx, locobj, PRP_STATUSLINE, 0);

    /* if we're in the status line, make sure the line gets flushed */
    if (os_get_status() != 0)
        tioputs(tioctx, "\\n");
    outflushn(0);

    /* restore the previous MORE mode */
    setmore(savemoremode);

    /* switch to output display mode 0 (main text area) */
    os_status(0);
}

void runistat(voccxdef *vctx, runcxdef *rctx, tiocxdef *tctx)
{
    runctx = rctx;
    vocctx = vctx;
    tioctx = tctx;
}

