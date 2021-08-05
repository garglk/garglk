#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/FIOWRT.C,v 1.3 1999/07/11 00:46:29 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  fiowrt.c - write game to binary file
Function
  Writes a game to binary file.  Separated from fio.c so that run-time
  doesn't have to link in this function, which is unnecessary for playing
  a game.
Notes
  None
Modified
  09/14/92 MJRoberts     - note location of undefined objects in errors
  04/11/92 MJRoberts     - creation
*/

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "os.h"
#include "std.h"
#include "mch.h"
#include "mcm.h"
#include "mcl.h"
#include "tok.h"
#include "obj.h"
#include "voc.h"
#include "fio.h"
#include "dat.h"
#include "sup.h"
#include "cmap.h"


/* write a resource header; returns pointer to next-res field in file */
static ulong fiowhd(osfildef *fp, errcxdef *ec, char *resname)
{
    ulong ret;

    if (osfwb(fp, resname, (int)(resname[0] + 1))) errsig(ec, ERR_WRTGAM);
    ret = osfpos(fp);
    if (osfwb(fp, "\000\000\000\000", 4)) errsig(ec, ERR_WRTGAM);
    return(ret);
}

/* close a resource by writing next-res pointer */
static void fiowcls(osfildef *fp, errcxdef *ec, ulong respos)
{
    ulong endpos;
    char  buf[4];
    
    endpos = osfpos(fp);
    osfseek(fp, respos, OSFSK_SET);
    oswp4(buf, endpos);
    if (osfwb(fp, buf, 4)) errsig(ec, ERR_WRTGAM);
    osfseek(fp, endpos, OSFSK_SET);
}

/* write a required object (just an object number) */
static void fiowrq(errcxdef *ec, osfildef *fp, objnum objn)
{
    uchar buf[2];
    
    oswp2(buf, objn);
    if (osfwb(fp, buf, 2)) errsig(ec, ERR_WRTGAM);
}

/* context for fiowrtobj callback */
struct fiowcxdef
{
    mcmcxdef *fiowcxmem;
    errcxdef *fiowcxerr;
    osfildef *fiowcxfp;
    uint      fiowcxflg;
    int       fiowcxund;
    osfildef *fiowcxffp;
    uint      fiowcxseed;
    uint      fiowcxinc;
    int       fiowcxdebug;
};
typedef struct fiowcxdef fiowcxdef;

/* write out a symbol table entry */
static void fiowrtsym(void *ctx0, toksdef *t)
{
    fiowcxdef  *ctx = (fiowcxdef *)ctx0;
    uchar       buf[TOKNAMMAX + 50];
    errcxdef   *ec = ctx->fiowcxerr;
    osfildef   *fp = ctx->fiowcxfp;

    buf[0] = t->tokslen;
    buf[1] = t->tokstyp;
    oswp2(buf + 2, t->toksval);
    memcpy(buf + 4, t->toksnam, (size_t)buf[0]);
    if (osfwb(fp, buf, 4 + t->tokslen)) errsig(ec, ERR_WRTGAM);
}

/* write an object given by a symbol table entry */
static void fiowrtobj(void *ctx0, toksdef *t)
{
    fiowcxdef *ctx = (fiowcxdef *)ctx0;
    uchar      buf[TOKNAMMAX + 50];
    mcmon      obj;
    mcmcxdef  *mctx = ctx->fiowcxmem;
    errcxdef  *ec = ctx->fiowcxerr;
    osfildef  *fp = ctx->fiowcxfp;
    uint       flags = ctx->fiowcxflg;
    uchar     *p;
    uint       siz;
    uint       used;
    int        err = 0;
    ulong      startpos = osfpos(fp);
    
    /* set up start of buffer to write */
    buf[0] = t->tokstyp;
    obj = t->toksval;
    oswp2(buf + 1, obj);
    
    switch(t->tokstyp)
    {
    case TOKSTOBJ:
        /* 
         *   Mark object as finished with compilation.  Note that we must
         *   do this even though tcdmain() does this as well, because
         *   running preinit() might have updated properties since the
         *   last time we marked objects.  
         */
        objcomp(mctx, (objnum)obj, ctx->fiowcxdebug);

        /* get the object's size information */
        p = mcmlck(mctx, (mcmon)obj);
        siz = mcmobjsiz(mctx, (mcmon)obj);         /* size in cache */
        used = objfree(p);          /* size actually used in object */
        if (objflg(p) & OBJFINDEX) used += objnprop(p) * 4;
        goto write_func_or_obj;
                
    case TOKSTFUNC:
        /* size of function is entire object */
        p = mcmlck(mctx, (mcmon)obj);
        siz = used = mcmobjsiz(mctx, (mcmon)obj);

    write_func_or_obj:
        /* write type(OBJ) + objnum + size + sizeused */
        oswp2(buf+3, siz);
        oswp2(buf+5, used);
        if (osfwb(fp, buf, 7)) err = ERR_WRTGAM;
                
        /* write contents of object */
        if (flags & FIOFCRYPT)
            fioxor(p, used, ctx->fiowcxseed, ctx->fiowcxinc);
        if (osfwb(fp, p, used)) err = ERR_WRTGAM;
        
        /* write fast-load record if applicable */
        if (ctx->fiowcxffp)
        {
            oswp4(buf + 7, startpos);
            if (osfwb(ctx->fiowcxffp, buf, 11)) err = ERR_WRTGAM;
        }
                
        /*
         *   We're done with the object - delete it so that
         *   it doesn't have to be swapped out (which would
         *   be pointless, since we'll never need it again).
         */
        mcmunlck(mctx, (mcmon)obj);
        mcmfre(mctx, (mcmon)obj);
        break;
                
    case TOKSTEXTERN:
        /* all we must write is the name & number of ext func */
        buf[3] = t->tokslen;
        memcpy(buf + 4, t->toksnam, (size_t)t->tokslen);
        if (osfwb(fp, buf, t->tokslen + 4)) err = ERR_WRTGAM;
        
        /* write fast-load record if applicable */
        if (ctx->fiowcxffp
            && osfwb(ctx->fiowcxffp, buf, t->tokslen + 4)) err = ERR_WRTGAM;
        break;
                
    case TOKSTFWDOBJ:
    case TOKSTFWDFN:
        {
            int  e = (t->tokstyp == TOKSTFWDFN ? ERR_UNDEFF : ERR_UNDEFO);

            if (flags & FIOFBIN)
            {
                /* write record for the forward reference */
                p = mcmlck(mctx, (mcmon)obj);
                siz = mcmobjsiz(mctx, (mcmon)obj);
                oswp2(buf+3, siz);
                if (osfwb(fp, buf, 5)
                    || osfwb(fp, p, siz))
                    err = ERR_WRTGAM;
            }
            else
            {
                /* log the undefined-object error */
                sup_log_undefobj(mctx, ec, e,
                                 t->toksnam, (int)t->tokslen, obj);

                /* count the undefined object */
                ++(ctx->fiowcxund);

                /*
                 *   we don't need this object any longer - delete it so
                 *   that we don't have to bother swapping it in or out
                 */
                mcmfre(mctx, (mcmon)obj);
            }
        }
        break;
    }
                    
    /* if an error occurred, signal it */
    if (err) errsig(ec, err);
}

/* write game to binary file */
static void fiowrt1(mcmcxdef *mctx, voccxdef *vctx, tokcxdef *tokctx,
                    tokthdef *tab, uchar *fmts, uint fmtl, osfildef *fp,
                    uint flags, objnum preinit, int extc, uint prpcnt,
                    char *filever)
{
    int         i;
    int         j;
    int         k;
    uchar       buf[TOKNAMMAX + 50];
    errcxdef   *ec = vctx->voccxerr;
    ulong       fpos;
    int         obj;
    vocidef  ***vpg;
    vocidef   **v;
    objnum     *sc;
    vocdef     *voc;
    vocwdef    *vw;
    vocdef    **vhsh;
    struct tm  *tblock;
    time_t      timer;
    fiowcxdef   cbctx;                    /* callback context for toktheach */
    uint        xor_seed = 63;
    uint        xor_inc = 64;
    char       *vsnhdr;
    uint        vsnlen;
    char        fastnamebuf[OSFNMAX];    /* fast-load record temp file name */
    long        flag_seek;

    /* generate appropriate file version */
    switch(filever[0])
    {
    case 'a':          /* generate .GAM compatible with pre-2.0.15 runtimes */
        vsnhdr = FIOVSNHDR2;
        vsnlen = sizeof(FIOVSNHDR2);
        xor_seed = 17;                                /* use old xor values */
        xor_inc = 29;
        break;

    case 'b':                                    /* generate 2.0.15+ format */
        vsnhdr = FIOVSNHDR3;
        vsnlen = sizeof(FIOVSNHDR3);
        break;

    case 'c':
    case '*':                                            /* current version */
        vsnhdr = FIOVSNHDR;
        vsnlen = sizeof(FIOVSNHDR);
        break;

    default:
        errsig(ec, ERR_WRTVSN);
    }

    /* write file header and version header */
    if (osfwb(fp, FIOFILHDR, sizeof(FIOFILHDR))
          || osfwb(fp, vsnhdr, vsnlen))
        errsig(ec, ERR_WRTGAM);

    /* 
     *   write the flags - remember where we wrote it in case we need to
     *   change the flags later 
     */
    flag_seek = osfpos(fp);
    oswp2(buf, flags);
    if (osfwb(fp, buf, 2))
        errsig(ec, ERR_WRTGAM);

    /* write the timestamp */
    timer = time(NULL);
    tblock = localtime(&timer);
    strcpy(vctx->voccxtim, asctime(tblock));
    if (osfwb(fp, vctx->voccxtim, (size_t)26))
        errsig(ec, ERR_WRTGAM);

    /* write xor parameters if generating post 2.0.15 .GAM file */
    if (filever[0] != 'a')
    {
        fpos = fiowhd(fp, ec, "\003XSI");
        buf[0] = xor_seed;
        buf[1] = xor_inc;
        if (osfwb(fp, buf, 2)) errsig(ec, ERR_WRTGAM);
        fiowcls(fp, ec, fpos);
    }
    
    /* write count of external functions */
    if (extc)
    {
        fpos = fiowhd(fp, ec, "\006EXTCNT");
        oswp2(buf, extc);              /* write the external function count */
        if (osfwb(fp, buf, 2)) errsig(ec, ERR_WRTGAM);

        /* write XFCN-seek-location header if post 2.0.15 file version */
        if (filever[0] != 'a')
        {
            oswp4(buf, 0);      /* placeholder - TADSRSC sets this up later */
            if (osfwb(fp, buf, 4)) errsig(ec, ERR_WRTGAM);
        }

        /* close the resource */
        fiowcls(fp, ec, fpos);
    }
    
    /* 
     *   write the character set information, if a character set was
     *   specified 
     */
    if (G_cmap_id[0] != '\0')
    {
        size_t len;

        /* this is not allowed with explicit file versions a, b, or c */
        if (filever[0] == 'a' || filever[0] == 'b' || filever[0] == 'c')
            errsig(ec, ERR_VNOCTAB);

        /* write out the CHRSET resource header */
        fpos = fiowhd(fp, ec, "\006CHRSET");

        /* write out the ID and LDESC of the internal character set */
        len = strlen(G_cmap_ldesc) + 1;
        oswp2(buf, len);
        if (osfwb(fp, G_cmap_id, 4)
            || osfwb(fp, buf, 2)
            || osfwb(fp, G_cmap_ldesc, len))
            errsig(ec, ERR_WRTGAM);

        /* close the resource */
        fiowcls(fp, ec, fpos);
    }

    /* write OBJ resource header */
    fpos = fiowhd(fp, ec, "\003OBJ");

    /* set up the symbol table callback context for writing the objects */
    cbctx.fiowcxmem = mctx;
    cbctx.fiowcxerr = ec;
    cbctx.fiowcxfp  = fp;
    cbctx.fiowcxund = 0;
    cbctx.fiowcxseed = xor_seed;
    cbctx.fiowcxinc = xor_inc;
    cbctx.fiowcxdebug = (flags & FIOFSYM);
    if (flags & FIOFFAST)
    {
        /* try creating the temp file */
        cbctx.fiowcxffp = os_create_tempfile(0, fastnamebuf);

        /* if that failed, we don't have a fast-load table after all */
        if (cbctx.fiowcxffp == 0)
        {
            long curpos;
            char flag_buf[2];
            
            /* clear the fast-load flag */
            flags &= ~FIOFFAST;

            /* 
             *   go back to the flags we wrote to the .gam file header, and
             *   re-write the new flags without the fast-load bit - since
             *   we're not going to write a fast-load table, we don't want
             *   readers thinking they're going to find one 
             */

            /* first, remember where we are right now */
            curpos = osfpos(fp);

            /* seek back to the flags and re-write the new flags */
            osfseek(fp, flag_seek, OSFSK_SET);
            oswp2(flag_buf, flags);
            if (osfwb(fp, flag_buf, 2))
                errsig(ec, ERR_WRTGAM);

            /* seek back to where we started */
            osfseek(fp, curpos, OSFSK_SET);
        }
    }
    else
        cbctx.fiowcxffp = (osfildef *)0;
    cbctx.fiowcxflg = flags;

    /* go through symbol table, and write each object */
    toktheach((toktdef *)tab, fiowrtobj, &cbctx);

    /* also write all objects created with 'new' */
    for (vpg = vctx->voccxinh, i = 0 ; i < VOCINHMAX ; ++vpg, ++i)
    {
        objnum obj;

        if (!*vpg) continue;
        for (v = *vpg, obj = (i << 8), j = 0 ; j < 256 ; ++v, ++obj, ++j)
        {
            /* if the object was dynamically allocated, write it out */
            if (*v && (*v)->vociflg & VOCIFNEW)
            {
                toksdef t;

                /* clear the 'new' flag, as this is a static object now */
                (*v)->vociflg &= ~VOCIFNEW;

                /* set up a toksdef, and write it out */
                t.tokstyp = TOKSTOBJ;
                t.toksval = obj;
                fiowrtobj(&cbctx, &t);
            }
        }
    }
                    
    /* close the resource */
    fiowcls(fp, ec, fpos);
    
    /* copy fast-load records to output file if applicable */
    if (cbctx.fiowcxffp)
    {
        osfildef *fp2 = cbctx.fiowcxffp;
        char      copybuf[1024];
        ulong     len;
        ulong     curlen;
        
        /* start with resource header for fast-load records */
        fpos = fiowhd(fp, ec, "\003FST");

        /* get length of temp file, then rewind it */
        len = osfpos(fp2);
        osfseek(fp2, 0L, OSFSK_SET);

        /* copy the whole thing to the output file */
        while (len)
        {
            curlen = (len > sizeof(copybuf) ? sizeof(copybuf) : len);
            if (osfrb(fp2, copybuf, (size_t)curlen)
                || osfwb(fp, copybuf, (size_t)curlen))
                errsig(ec, ERR_WRTGAM);
            len -= curlen;
        }

        /* close the fast-load resource */
        fiowcls(fp, ec, fpos);
        
        /* close and delete temp file */
        osfcls(fp2);
        osfdel_temp(fastnamebuf);
    }
    
    /* write vocabulary inheritance records - start with header */
    fpos = fiowhd(fp, ec, "\003INH");
    
    /* go through inheritance records and write to file */
    for (vpg = vctx->voccxinh, i = 0 ; i < VOCINHMAX ; ++vpg, ++i)
    {
        if (!*vpg) continue;
        for (v = *vpg, obj = (i << 8), j = 0 ; j < 256 ; ++v, ++obj, ++j)
        {
            if (*v)
            {
                buf[0] = (*v)->vociflg;
                oswp2(buf + 1, obj);
                oswp2(buf + 3, (*v)->vociloc);
                oswp2(buf + 5, (*v)->vociilc);
                oswp2(buf + 7, (*v)->vocinsc);
                for (k = 0, sc = (*v)->vocisc ; k < (*v)->vocinsc ; ++sc, ++k)
                {
                    if (k + 9 >= sizeof(buf)) errsig(ec, ERR_FIOMSC);
                    oswp2(buf + 9 + (k << 1), (*sc));
                }
                if (osfwb(fp, buf, 9 + (2 * (*v)->vocinsc)))
                    errsig(ec, ERR_WRTGAM);
            }
        }
    }
    
    /* close resource */
    fiowcls(fp, ec, fpos);
    
    /* write format strings */
    if (fmtl)
    {
        fpos = fiowhd(fp, ec, "\006FMTSTR");
        oswp2(buf, fmtl);
        if (flags & FIOFCRYPT) fioxor(fmts, fmtl, xor_seed, xor_inc);
        if (osfwb(fp, buf, 2) || osfwb(fp, fmts, fmtl))
            errsig(ec, ERR_WRTGAM);
        fiowcls(fp, ec, fpos);
    }
    
    /* write preinit if preinit object was specified */
    if (flags & FIOFPRE)
    {
        fpos = fiowhd(fp, ec, "\007PREINIT");
        oswp2(buf, preinit);
        if (osfwb(fp, buf, 2)) errsig(ec, ERR_WRTGAM);
        fiowcls(fp, ec, fpos);
    }
    
    /* write required objects out of voccxdef */
    fpos = fiowhd(fp, ec, "\003REQ");
    fiowrq(ec, fp, vctx->voccxme);
    fiowrq(ec, fp, vctx->voccxvtk);
    fiowrq(ec, fp, vctx->voccxstr);
    fiowrq(ec, fp, vctx->voccxnum);
    fiowrq(ec, fp, vctx->voccxprd);
    fiowrq(ec, fp, vctx->voccxvag);
    fiowrq(ec, fp, vctx->voccxini);
    fiowrq(ec, fp, vctx->voccxpre);
    fiowrq(ec, fp, vctx->voccxper);
    fiowrq(ec, fp, vctx->voccxprom);
    fiowrq(ec, fp, vctx->voccxpdis);
    fiowrq(ec, fp, vctx->voccxper2);
    fiowrq(ec, fp, vctx->voccxpdef);
    fiowrq(ec, fp, vctx->voccxpask);
    fiowrq(ec, fp, vctx->voccxppc);
    fiowrq(ec, fp, vctx->voccxpask2);
    fiowrq(ec, fp, vctx->voccxperp);
    fiowrq(ec, fp, vctx->voccxpostprom);
    fiowrq(ec, fp, vctx->voccxinitrestore);
    fiowrq(ec, fp, vctx->voccxpuv);
    fiowrq(ec, fp, vctx->voccxpnp);
    fiowrq(ec, fp, vctx->voccxpostact);
    fiowrq(ec, fp, vctx->voccxendcmd);
    fiowrq(ec, fp, vctx->voccxprecmd);
    fiowrq(ec, fp, vctx->voccxpask3);
    fiowrq(ec, fp, vctx->voccxpre2);
    fiowrq(ec, fp, vctx->voccxpdef2);
    fiowcls(fp, ec, fpos);
    
    /* write compound words */
    if (vctx->voccxcpl)
    {
        fpos = fiowhd(fp, ec, "\004CMPD");
        oswp2(buf, vctx->voccxcpl);
        if (flags & FIOFCRYPT)
            fioxor((uchar *)vctx->voccxcpp, (uint)vctx->voccxcpl,
                   xor_seed, xor_inc);
        if (osfwb(fp, buf, 2)
            || osfwb(fp, vctx->voccxcpp, (uint)vctx->voccxcpl))
            errsig(ec, ERR_WRTGAM);
        fiowcls(fp, ec, fpos);
    }
    
    /* write special words */
    if (vctx->voccxspl)
    {
        fpos = fiowhd(fp, ec, "\010SPECWORD");
        oswp2(buf, vctx->voccxspl);
        if (flags & FIOFCRYPT)
            fioxor((uchar *)vctx->voccxspp, (uint)vctx->voccxspl,
                   xor_seed, xor_inc);
        if (osfwb(fp, buf, 2)
            || osfwb(fp, vctx->voccxspp, (uint)vctx->voccxspl))
            errsig(ec, ERR_WRTGAM);
        fiowcls(fp, ec, fpos);
    }
    
    /* write vocabulary */
    fpos = fiowhd(fp, ec, "\003VOC");

    /* go through each hash chain */
    for (i = 0, vhsh = vctx->voccxhsh ; i < VOCHASHSIZ ; ++i, ++vhsh)
    {
        /* go through each word in this hash chain */
        for (voc = *vhsh ; voc ; voc = voc->vocnxt)
        {
            /* encrypt the word if desired */
            if (flags & FIOFCRYPT)
                fioxor(voc->voctxt, (uint)(voc->voclen + voc->vocln2),
                       xor_seed, xor_inc);

            /* go through each object relation attached to the word */
            for (vw = vocwget(vctx, voc->vocwlst) ; vw ;
                 vw = vocwget(vctx, vw->vocwnxt))
            {
                /* clear the 'new' flag, as this is a static object now */
                vw->vocwflg &= ~VOCFNEW;

                /* write out this object relation */
                oswp2(buf, voc->voclen);
                oswp2(buf+2, voc->vocln2);
                oswp2(buf+4, vw->vocwtyp);
                oswp2(buf+6, vw->vocwobj);
                oswp2(buf+8, vw->vocwflg);
                if (osfwb(fp, buf, 10)
                    || osfwb(fp, voc->voctxt, voc->voclen + voc->vocln2))
                    errsig(ec, ERR_WRTGAM);
            }
        }
    }
    fiowcls(fp, ec, fpos);
    
    /* write the symbol table, if desired */
    if (flags & FIOFSYM)
    {
        fpos = fiowhd(fp, ec, "\006SYMTAB");
        toktheach((toktdef *)tab, fiowrtsym, &cbctx);

        /* indicate last symbol with a length of zero */
        buf[0] = 0;
        if (osfwb(fp, buf, 4)) errsig(ec, ERR_WRTGAM);
        fiowcls(fp, ec, fpos);
    }
    
    /* write line source chain, if desired */
    if ((flags & (FIOFLIN | FIOFLIN2)) != 0 && vctx->voccxrun->runcxdbg != 0)
    {
        lindef *lin;

        /* write the SRC header */
        fpos = fiowhd(fp, ec, "\003SRC");

        /* write the line records */
        for (lin = vctx->voccxrun->runcxdbg->dbgcxlin ; lin ;
             lin = lin->linnxt)
        {
            /* write this record */
            if (linwrt(lin, fp))
                errsig(ec, ERR_WRTGAM);
        }

        /* done with this section */
        fiowcls(fp, ec, fpos);

        /* 
         *   if we have new-style line records, put a SRC2 header (with no
         *   block contents) in the file, so that the debugger realizes
         *   that it has new-style line records (with line numbers rather
         *   than seek offsets) 
         */
        if ((flags & FIOFLIN2) != 0)
        {
            fpos = fiowhd(fp, ec, "\004SRC2");
            fiowcls(fp, ec, fpos);
        }
    }

    /* if writing a precompiled header, write some more information */
    if (flags & FIOFBIN)
    {
        /* write property count */
        fpos = fiowhd(fp, ec, "\006PRPCNT");
        oswp2(buf, prpcnt);
        if (osfwb(fp, buf, 2)) errsig(ec, ERR_WRTGAM);
        fiowcls(fp, ec, fpos);

        /* write preprocessor symbol table */
        fpos = fiowhd(fp, ec, "\006TADSPP");
        tok_write_defines(tokctx, fp, ec);
        fiowcls(fp, ec, fpos);
    }

    /* write end-of-file resource header */
    (void)fiowhd(fp, ec, "\004$EOF");
    osfcls(fp);
    
    /* if there are undefined functions/objects, signal an error */
    if (cbctx.fiowcxund) errsig(ec, ERR_UNDEF);
}

/* write game to binary file */
void fiowrt(mcmcxdef *mctx, voccxdef *vctx, tokcxdef *tokctx, tokthdef *tab,
            uchar *fmts, uint fmtl, char *fname, uint flags, objnum preinit,
            int extc, uint prpcnt, char *filever)
{
    osfildef *fp;
    
    /* open the file */
    if (!(fp = osfoprwtb(fname, OSFTGAME)))
        errsig(vctx->voccxerr, ERR_OPWGAM);
    
    ERRBEGIN(vctx->voccxerr)
    
    /* write the file */
    fiowrt1(mctx, vctx, tokctx, tab, fmts, fmtl, fp, flags, preinit,
            extc, prpcnt, filever);
    os_settype(fname, OSFTGAME);
   
    ERRCLEAN(vctx->voccxerr)
        /* clean up by closing the file */
        osfcls(fp);
    ERRENDCLN(vctx->voccxerr)
}
