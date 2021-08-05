#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/msdos/DBGU.C,v 1.3 1999/07/11 00:46:37 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  dbgu.c - user interface for debugger
Function
  Implements the user interface for the command-line version of the
  debugger.  This portion of the debugger performs only user-interface
  functions; the UI-indenpendent section actually performs most of
  the work of debugging.
Notes
  None
Modified
  04/12/92 MJRoberts     - creation
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
#include "lin.h"
#include "linf.h"
#include "osdbg.h"
#include "osgen.h"

#ifdef TURBO
#include <dos.h>
#endif

#ifdef MICROSOFT
extern char   **__argv;
#define ARG0    __argv[0]
#endif /* MICROSOFT */

#ifdef __IBMC__
extern char   **_argv;
#endif /* __IBMC__ */

#ifdef __EMX__
# define ARG0   (char *)0
#endif /* __EMX__ */

#ifndef ARG0
# define ARG0   _argv[0]
#endif /* !ARG0 */

/* some special characters for PC's */
#define DBGUHLIN   196                                   /* horizontal line */
#define DBGARROW    16                              /* right-pointing arrow */
#define DBGBPCHAR   15                    /* breakpoint indicator character */

/* debug text, status, source, and watch windows */
static oswdef dbgtxtwin;
static oswdef dbgstawin;
static oswdef dbgsrcwin;
static oswdef dbgwatwin;

/* space for lindef containing arbitrary file not part of game source */
static char filesrc[sizeof(linfdef) + 128];

/* location in source file of top current source window and current line */
static uchar dbgsrctop[LINLLNMAX];         /* current display seek location */
static uchar dbgsrccsr[LINLLNMAX];                   /* current cursor line */
static uchar dbgsrcloc[LINLLNMAX];           /* current source line in file */
static uchar dbgsrchome[LINLLNMAX];       /* seek location of "home" screen */

/* lindef for current source file */
static lindef *dbgsrclin;            /* lindef for currently displayed file */
static lindef *dbgsrclinhome;                      /* lindef of "home" file */

/* color for current line/breakpoint line in source window */
static int dbgcurcolor = 30;
static int dbgbpcolor = 0x47;

/* private breakpoint data - maintained with dbgbpeach() */
static int dbgbpcnt;                           /* number of breakpoints set */
typedef struct dbgubpdef dbgubpdef;
static struct dbgubpdef
{
    int  dbgubpid;                                        /* line source id */
    char dbgubpseek[LINLLNMAX];              /* seek position of breakpoint */
}
dbgbps[DBGBPMAX];                          /* seek locations of breakpoints */

/* forward declarations */
static void dbgugetbp(dbgcxdef *ctx);


/* move back or forward by one or more lines within a file */
static char *dbgbuf;
static size_t dbgbufsiz;
static void dbgmove(lindef *lin, int cur, int *destlinep)
{
   int     dest = *destlinep;
   char   *p;
   size_t  max;
   long    backup;
   long    delta;
   uchar   pos[LINLLNMAX];
   uchar   prvpos[LINLLNMAX];

   /* find current location in line source */
   lintell(lin, pos);
   
   if (cur < dest)
   {
      while (cur != dest)
      {
         max = linread(lin, (uchar *)dbgbuf, (int)dbgbufsiz);
         if (max == 0) break;
         for (delta = 0, p = dbgbuf ; p < dbgbuf + max && cur != dest ;
              ++p, ++delta)
            if (*p == '\n') ++cur;
         linpadd(lin, pos, delta);
      }
   }
   else
   {
      ++cur;
      while (cur != dest)
      {
         backup = linofs(lin);
         if (backup > (long)dbgbufsiz) backup = dbgbufsiz;
         memcpy(prvpos, pos, (size_t)sizeof(prvpos));
         linpadd(lin, prvpos, -(long)dbgbufsiz);
         linseek(lin, prvpos);
         max = linread(lin, (uchar *)dbgbuf, backup);
         if (max == 0) break;
         p = dbgbuf + max;
         delta = 0;
         do
         {
            --p;
            --delta;
            if (*p == '\n') --cur;
         } while (p > dbgbuf && cur != dest);
         linpadd(lin, pos, delta);
         if (linqtop(lin, prvpos)) break;
      }
      if (!linqtop(lin, pos)) linpadd(lin, pos, (long)1);
      else cur = 1;
   }

   linseek(lin, pos);
   *destlinep = cur;
}

/* show current source page */
static void dbgsrcshow(lindef *lin, oswdef *win, uchar *top, uchar *loc)
{
    int        y;
    char       outbuf[128];
    int        wid = dbgsrcwin.oswx2 - dbgsrcwin.oswx1 + 1;
    extern     int sdesc_color, text_color;
    char       prefix[3];
    uchar      curloc[LINLLNMAX];
    int        i;
    dbgubpdef *bp;
    int        thisid;

    /* if there's no line source, we can't show anything */
    if (lin == 0)
        return;

    thisid = lin->linid;
    prefix[2] = '\0';
    osdbgclr(win);
    for (y = win->oswy1 ; y <= win->oswy2 ; ++y)
    {
        win->oswx = win->oswx1;
        win->oswy = y;
        linftell(lin, curloc);
        
        /* special display if this is the currently executing line */
        if (!memcmp(curloc, loc, lin->linlln) && lin == dbgsrclin)
        {
            win->oswcolor = dbgcurcolor;
            prefix[1] = DBGARROW;
        }
        else
        {
            win->oswcolor = sdesc_color;
            prefix[1] =  ' ';
        }
        
        /* special display if there is a breakpoint at this line */
        prefix[0] = ' ';         /* presume nothing special about this line */
        for (i = dbgbpcnt, bp = dbgbps ; i ; ++bp, --i)
        {
            
            if (!memcmp(bp->dbgubpseek, curloc, lin->linlln)
                && (bp->dbgubpid == thisid))
            {
                prefix[0] = DBGBPCHAR;
                win->oswcolor = dbgbpcolor;
                break;
            }
        }

        /* display prefix with current execution/breakpoint info */
        osdbgpt(win, "%s", prefix);

        /* display the line itself */
        if (!lingets(lin, (uchar *)outbuf, sizeof(outbuf)))
            break;
        else
        {
            int l;

            l = strlen(outbuf);
            while (l && outbuf[l-1] == '\n' || outbuf[l-1] == '\r')
                --l;

            if (l + 2 >= wid)
                outbuf[wid - 2] = '\0';
            else
            {
                outbuf[l] = '\0';
                if (prefix[0] == DBGBPCHAR)
                {
                    /* breakpoint - pad line out with spaces to full width */
                    while (l < wid - 2) outbuf[l++] = ' ';
                    outbuf[l] = '\0';
                }
            }
            osdbgpt(win, "%s", outbuf);
            if (y != win->oswy2) osdbgpt(win, "\n");
        }
    }
    
    /* restore normal color */
    win->oswcolor = sdesc_color;
}

/* search for a string in a line source */
static void dbgusearch(lindef *lin, uchar *pos, uchar *toppos,
                       oswdef *win, char *str)
{
    int dest;
    
    /* search is case-insensitive - convert search string to lowercase */
    os_strlwr(str);

    /* seek to starting location, and read and discard current line */
    linseek(lin, pos);
    if (!lingets(lin, (uchar *)dbgbuf, (int)dbgbufsiz)) return;
    
    for (;;)
    {
        lintell(lin, pos);
        if (!lingets(lin, (uchar *)dbgbuf, (int)dbgbufsiz)) break;
        os_strlwr(dbgbuf);
        if (strstr(dbgbuf, str)) break;
    }
    
    /* move search expression to middle of screen */
    linseek(lin, pos);
    dest = 1;
    dbgmove(lin, (win->oswy2 - win->oswy1)/2, &dest);
    lintell(lin, toppos);
    dbgsrcshow(lin, win, toppos, dbgsrcloc);
}

/* redraw status line */
static char dbg_curstat[81];
static void dbgustat(dbgcxdef *ctx)
{
    VARUSED(ctx);
    
    dbgstawin.oswx = dbgstawin.oswx1;
    dbgstawin.oswy = dbgstawin.oswy1;
    osdbgpt(&dbgstawin, dbg_curstat);
}

/* show the current line */
static void dbgulsho(dbgcxdef *ctx, uchar *buf)
{
    lindef    *lin;
    int        cur;
    int        dest;
    uchar      loc[LINLLNMAX];
    char       fnam[182];
    int        i;
    
    /* search for a line source with this id */
    for (lin = ctx->dbgcxlin ; lin ; lin = lin->linnxt)
        if (lin->linid == *buf) break;
    dbgswitch(&dbgsrclin, lin);
    dbgsrclinhome = lin;
    
    if (lin == 0)
    {
        osdbgclr(&dbgsrcwin);
        osdbgpt(&dbgsrcwin, "*** source line not found ***\n");
        return;
    }

    /* seek to line in file, then back up by half a screen to center line */
    memcpy(loc, buf+1, lin->linlln);
    linseek(lin, loc);
    cur = (dbgsrcwin.oswy2 - dbgsrcwin.oswy1) / 2;
    dest = 1;
    dbgmove(lin, cur, &dest);
    lintell(lin, dbgsrctop);
    memcpy(dbgsrchome, dbgsrctop, (size_t)lin->linlln);
    memcpy(dbgsrcloc, loc, (size_t)lin->linlln);
    memcpy(dbgsrccsr, loc, (size_t)lin->linlln);
    dbgsrcshow(lin, &dbgsrcwin, dbgsrctop, dbgsrcloc);

    /* update status line to show current file */
    for (i = 0 ; i < 5 ; ++i) dbg_curstat[i] = (uchar)DBGUHLIN;

    linnam(lin, fnam);
    sprintf(&dbg_curstat[5], "%s    ", fnam);
    dbgwhere(ctx, &dbg_curstat[strlen(dbg_curstat)]);

    for (i = strlen(dbg_curstat) ;
         i <= dbgstawin.oswx2 - dbgstawin.oswx1 ; ++i)
        dbg_curstat[i] = (uchar)DBGUHLIN;
    dbg_curstat[i] = '\0';
    
    /* actually display the status line now */
    dbgustat(ctx);
}

/* display callback */
static void dbgudisp(void *ctx, const char *str, int strl)
{
    oswdef *win = (oswdef *)ctx;
    int     oldpg;
    
    oldpg = ossvpg(1);             /* display values on debugger video page */
    osdbgpt(win, "%.*s", strl, str);
    ossvpg((char)oldpg);                           /* restore previous page */
}

/* display an error message */
void dbguerr(dbgcxdef *ctx, int errnum, char *msg)
{
    int oldpg;
    
    VARUSED(ctx);
    
    oldpg = ossvpg((char)1);               /* figure out what page we're on */
    ossvpg((char)oldpg);                           /* restore previous page */
    if (oldpg == 0)                         /* if we're on the game page... */
    {
        char buf[256];
        sprintf(buf, "[TADS-%d: %s]\n", errnum, msg);
        os_printz(buf);
    }
    else
        osdbgpt(&dbgtxtwin, "TADS-%d: %s\n", errnum, msg);
}

/* show user screen, wait for key hit, then change back to dbg screen */
static void dbguuser(dbgcxdef *ctx)
{
    VARUSED(ctx);
    
    ossvpg(0);
    os_waitc();
    ossvpg(1);
}

/* execute a special keyboard command key */
static int dbgukbc(void *ctx0, char cmd)
{
    dbgcxdef *ctx = (dbgcxdef *)ctx0;
    int       cur = dbgsrcwin.oswy2;
    int       dest = cur;
    uchar     newpos[LINLLNMAX];
    int       scrht = dbgsrcwin.oswy2 - dbgsrcwin.oswy1 - 2;
    
    switch(cmd)
    {
    case CMD_F5:       /* show user screen */
        dbguuser(ctx);
        return(0);

    case CMD_TAB:      /* other screen */
    case CMD_F7:       /* step into */
    case CMD_F8:       /* step over */
    case CMD_F9:       /* go */
    case CMD_SCR:      /* help */
        return(cmd);
    }

    /* the remaining commands pertain to the source display only */
    switch(cmd)
    {
    case CMD_CHOME:
        if (dbgsrclinhome != 0 && dbgsrclin != 0
            && (dbgsrctop != dbgsrchome || dbgsrclin != dbgsrclinhome))
        {
            dbgswitch(&dbgsrclin, dbgsrclinhome);

            linseek(dbgsrclin, dbgsrchome);
            memcpy(dbgsrctop, dbgsrchome, dbgsrclin->linlln);
            memcpy(dbgsrccsr, dbgsrcloc, dbgsrclin->linlln);
            dbgsrcshow(dbgsrclin, &dbgsrcwin, dbgsrctop, dbgsrcloc);
        }
        break;
        
    case CMD_TOP:
        if (dbgsrctop != 0 && dbgsrclin != 0)
        {
            lingoto(dbgsrclin, LINGOTOP);
            lintell(dbgsrclin, dbgsrccsr);
            memcpy(dbgsrctop, dbgsrccsr, dbgsrclin->linlln);
            dbgsrcshow(dbgsrclin, &dbgsrcwin, dbgsrctop, dbgsrcloc);
        }
        break;

    case CMD_BOT:
        if (dbgsrclin != 0)
        {
            lingoto(dbgsrclin, LINGOEND);
            dest -= scrht;
        }
        goto move_noseek;
        
    case CMD_UP:
        --dest;
        goto move_source;

    case CMD_DOWN:
        ++dest;
        goto move_source;
        
    case CMD_PGUP:
        dest -= scrht;
        goto move_source;
        
    case CMD_PGDN:
        dest += scrht;

    move_source:
        if (dbgsrclin != 0)
            linseek(dbgsrclin, dbgsrctop);

    move_noseek:
        if (dbgsrclin != 0)
        {
            dbgmove(dbgsrclin, cur, &dest);
            lintell(dbgsrclin, newpos);
            if (memcmp(dbgsrctop, newpos, dbgsrclin->linlln))
            {
                memcpy(dbgsrccsr, newpos, dbgsrclin->linlln);
                memcpy(dbgsrctop, newpos, dbgsrclin->linlln);
                dbgsrcshow(dbgsrclin, &dbgsrcwin, dbgsrctop, dbgsrcloc);
            }
        }
        break;
    }
    return(0);
}

/* get commands in source window */
static int dbgurunsrc(dbgcxdef *ctx)
{
    int       y;
    char      c;
    oswdef   *win = &dbgsrcwin;
    uchar     pos[LINLLNMAX];
    uchar     newpos[LINLLNMAX];
    int       dest;
    int       cur;
    int       scrht = win->oswy2 - win->oswy1 - 2;
    char     *cond;
    char      condbuf[128];

    if (dbgsrclin == 0)
        return 0;
    
    /* figure out where we are in the source window */
find_srcpos:
    linseek(dbgsrclin, dbgsrctop);
    memcpy(pos, dbgsrctop, dbgsrclin->linlln);
    for (y = win->oswy1 ; y < win->oswy2 ; ++y)
    {
        lintell(dbgsrclin, newpos);
        if (!memcmp(dbgsrccsr, newpos, dbgsrclin->linlln)) break;
        lingets(dbgsrclin, (uchar *)dbgbuf, (int)dbgbufsiz);
    }

    /* get and process commands */
    for (;;)
    {
        ossdbgloc((char)y, (char)2);              /* locate cursor properly */
        cond = (char *)0;       /* no condition for breakpoints, by default */

        c = os_getc();
        switch(c)
        {
        case '/':
            return(CMD_UP);
            
        case '\0':
            c = os_getc();
            switch(c)
            {
            case CMD_TAB:
                return(0);
                
            case CMD_CHOME:
                if (dbgsrclinhome
                    && (memcmp(dbgsrctop, dbgsrchome, dbgsrclin->linlln)
                        || dbgsrclin != dbgsrclinhome))
                {
                    dbgswitch(&dbgsrclin, dbgsrclinhome);

                    linseek(dbgsrclin, dbgsrchome);
                    memcpy(pos, dbgsrchome, dbgsrclin->linlln);
                    memcpy(dbgsrctop, dbgsrchome, dbgsrclin->linlln);
                    memcpy(dbgsrccsr, dbgsrcloc, dbgsrclin->linlln);
                    dbgsrcshow(dbgsrclin, &dbgsrcwin, dbgsrctop, dbgsrcloc);
                    goto find_srcpos;
                }
                break;
                
            case CMD_UP:
                if (y > win->oswy1)
                {
                    --y;
                    goto find_cur_y;
                }
                else
                {
                    dest = 1;
                    cur = 2;
                    goto move_source;
                }
                
            case CMD_DOWN:
                if (y < win->oswy2)
                {
                    ++y;
                    goto find_cur_y;
                }
                else
                {
                    dest = 2;
                    cur = 1;
                    goto move_source;
                }
                
            case CMD_PGUP:
                cur = scrht;
                dest = 1;
                goto move_source;
                
            case CMD_PGDN:
                cur = 1;
                dest = scrht;
            move_source:
                linseek(dbgsrclin, pos);
            move_noseek:
                dbgmove(dbgsrclin, cur, &dest);
                lintell(dbgsrclin, newpos);
                if (memcmp(pos, newpos, dbgsrclin->linlln))
                {
                    int i;

                    memcpy(dbgsrctop, newpos, dbgsrclin->linlln);
                    memcpy(pos, newpos, dbgsrclin->linlln);
                    dbgsrcshow(dbgsrclin, win, pos, dbgsrcloc);
                    
                    /* figure out where dbgsrccsr is */
                find_cur_y:
                    linseek(dbgsrclin, pos);
                    for (i = win->oswy1 ; i < y ; ++i)
                        lingets(dbgsrclin, (uchar *)dbgbuf, (int)dbgbufsiz);
                    lintell(dbgsrclin, dbgsrccsr);
                }
                break;
                
            case CMD_TOP:
                if (pos != 0)
                {
                    lingoto(dbgsrclin, LINGOTOP);
                    lintell(dbgsrclin, pos);
                    memcpy(dbgsrccsr, pos, dbgsrclin->linlln);
                    memcpy(dbgsrctop, pos, dbgsrclin->linlln);
                    y = win->oswy1;
                    dbgsrcshow(dbgsrclin, win, pos, dbgsrcloc);
                }
                break;
                
            case CMD_BOT:
                lingoto(dbgsrclin, LINGOEND);
                dest = 1;
                cur = scrht;
                goto move_noseek;
                
            case CMD_HOME:
                y = win->oswy1;
                goto find_cur_y;
                
            case CMD_END:
                y = win->oswy2;
                goto find_cur_y;

            case CMD_SF2:
                if (dbgsrclin)
                {
                    osdbgpt(&dbgtxtwin, "\nCondition: ");
                    while (osdbggts(&dbgtxtwin, condbuf, dbgukbc, ctx));
                    cond = condbuf;
                    /* FALLTHROUGH */
                }
                else break;

            case CMD_F2:
                if (dbgsrclin)
                {
                    objnum objn;
                    uint   ofs;
                    int    newbp;
                    char   nambuf[128];
                    int    len;
                    
                    linfind(dbgsrclin, (char *)dbgsrccsr, &objn, &ofs);
                    
                    memcpy(nambuf, "<F2> ", (size_t)4);
                    len = dbgnam(ctx, nambuf+4, TOKSTOBJ, objn);
                    if (!memcmp(nambuf+4, "<UNKNOWN>", (size_t)len))
                        len = dbgnam(ctx, nambuf+4, TOKSTFUNC, objn);
                    if (cond)
                    {
                        memcpy(nambuf + 4 + len, " when ", (size_t)6);
                        strcpy(nambuf + 10 + len, cond);
                    }
                    else
                        nambuf[len + 4] = '\0';
                    
                    (void)dbgbpat(ctx, objn, MCMONINV, ofs, &newbp, nambuf,
                                  TRUE, cond, 0);
                    dbgugetbp(ctx);
                }
                break;
                
            case CMD_F5:
                dbguuser(ctx);
                break;
                
            case CMD_F7:
            case CMD_F8:
            case CMD_F9:
            case CMD_EOF:
                return(c);
            }
        }
    }
}

/* callback for dbgugetp */
static void dbgubp(void *ctx0, int id, uchar *buf, uint bufsiz)
{
    dbgubpdef *bp = &dbgbps[dbgbpcnt++];

    bp->dbgubpid = id;
    memcpy(bp->dbgubpseek, buf, (size_t)bufsiz);
}

/* reset private breakpoint data - call after setting/clearing bp */
static void dbgugetbp(dbgcxdef *ctx)
{
    dbgbpcnt = 0;
    dbgbpeach(ctx, dbgubp, ctx);
    if (dbgsrclin != 0)
    {
        linseek(dbgsrclin, dbgsrctop);
        dbgsrcshow(dbgsrclin, &dbgsrcwin, dbgsrctop, dbgsrcloc);
    }
}

/* output support for hidden output */
void trchid(void)
{
    outflushn(1);
    os_printz("---Hidden output---\n");
}
void trcsho(void)
{
    outflushn(1);
    os_printz("---End of hidden output, normal output resumes---\n");
}

/* update display of watchpoints */
static void dbguwxupd(dbgcxdef *ctx)
{
    dbgwatwin.oswy = dbgwatwin.oswy1;
    dbgwatwin.oswx = dbgwatwin.oswx1;
    if (dbgwatwin.oswy1 <= dbgwatwin.oswy2) osdbgclr(&dbgwatwin);
    dbgwxupd(ctx, dbgudisp, &dbgwatwin);
}

/* update window sizes to account for more/fewer watches */
static void dbguwxadd(dbgcxdef *ctx, int cnt)
{
    /* move status line up by the number of new watch lines */
    dbgstawin.oswy1 -= cnt;
    dbgstawin.oswy2 -= cnt;
    
    /* move bottom of code window up by the number of watch lines */
    dbgsrcwin.oswy2 -= cnt;
    
    /* move top of watch window up by the number of new watch lines */
    dbgwatwin.oswy1 -= cnt;
    
    /* update the status, source and watch windows */
    dbgustat(ctx);
    if (dbgsrclin)
    {
        linseek(dbgsrclin, dbgsrctop);
        dbgsrcshow(dbgsrclin, &dbgsrcwin, dbgsrctop, dbgsrcloc);
    }
    dbguwxupd(ctx);
}

/* flags whether a prompt is needed the next time we run command line */
static int  needprompt = TRUE;

/* main command loop */
static void dbgucmd1(dbgcxdef *ctx)
{
    char        cmdbuf[128];
    int         err;
    int         disable;
    int         bpnum;
    char       *buf;
    int         cmd;
    int         i;
    lindef     *lin;
    static int  stay_in_src;
    static int  first_search;
    static char lastsearch[128];
    int         level = ctx->dbgcxfcn - 1;
    
    dbglget(ctx, (uchar *)cmdbuf);       /* get information on current line */
    dbgulsho(ctx, (uchar *)cmdbuf);                 /* display current line */
    dbguwxupd(ctx);                            /* udpate watchpoint display */

    for (;;)
    {
        if (stay_in_src)
            goto run_other_window;
        else
        {
            if (needprompt) osdbgpt(&dbgtxtwin, "tdb> ");
            cmd = osdbggts(&dbgtxtwin, cmdbuf, dbgukbc, ctx);
        }

    process_cmd:
        needprompt = (cmd == 0);   /* ordinary command ==> prompt next time */
        switch(cmd)
        {
        case 0:
            for (buf = cmdbuf ; isspace((unsigned char)*buf) ; ++buf);
            break;
            
        case CMD_F7:
            buf = "t";
            break;
            
        case CMD_F8:
            buf = "p";
            break;
            
        case CMD_F9:
            buf = "g";
            break;
            
        case CMD_TAB:
            buf = "\t";
            break;
            
        case CMD_SCR:
            buf = "?";
            break;
            
        default:
            continue;
        }

        if (isupper((unsigned char)buf[0]))
            buf[0] = tolower((unsigned char)buf[0]);
        switch(buf[0])
        {
        case 'g':
            ctx->dbgcxflg &= ~(DBGCXFSS + DBGCXFSO);
            return;
            
        case 't':
            ctx->dbgcxflg |= DBGCXFSS;
            ctx->dbgcxflg &= ~DBGCXFSO;
            return;
            
        case 'p':
            ctx->dbgcxsof = ctx->dbgcxdep; /* step until back at this depth */
            ctx->dbgcxflg |= (DBGCXFSS + DBGCXFSO);
            return;
            
        case 'e':
            ossvpg(0);                /* eval expression on game video page */
            err = dbgeval(ctx, buf+1, dbgudisp, &dbgtxtwin, level, TRUE);
            ossvpg(1);                     /* return to debugger video page */
            if (err) errlog(ctx->dbgcxerr, err);
            dbguwxupd(ctx);                    /* udpate watchpoint display */
            break;

        case 'c':                                           /* Call history */
            if (isupper((unsigned char)buf[1]))
                buf[1] = tolower((unsigned char)buf[1]);
            switch(buf[1])
            {
            case '+':
                ctx->dbgcxhstf = 0;
                ctx->dbgcxflg |= DBGCXFTRC;
                osdbgpt(&dbgtxtwin, "Call tracing activated.\n");
                break;

            case '-':
                ctx->dbgcxflg &= ~DBGCXFTRC;
                osdbgpt(&dbgtxtwin, "Call tracing disactivated.\n");
                break;

            case 'c':
                ctx->dbgcxhstf = 0;
                osdbgpt(&dbgtxtwin, "Call trace log cleared.\n");
                break;

            case '\0':
            case ' ':
                {
                    char *p;
                    char *endp;
                    
                    for (p = ctx->dbgcxhstp, endp = p + ctx->dbgcxhstf
                         ; p < endp ; p += strlen(p) + 1)
                        osdbgpt(&dbgtxtwin, "%s\n", p);
                }
            }
            break;
            
        case 'b':
            if (isupper((unsigned char)buf[1]))
                buf[1] = tolower((unsigned char)buf[1]);
            switch(buf[1])
            {
            case 'p':
                if (err = dbgbpset(ctx, buf+2, &bpnum))
                    errlog(ctx->dbgcxerr, err);
                else
                {
                    osdbgpt(&dbgtxtwin, "breakpoint %d set\n", bpnum);
                    dbgugetbp(ctx);
                }
                break;
                
            case 'c':
                if (err = dbgbpdel(ctx, atoi(buf + 2)))
                    errlog(ctx->dbgcxerr, err);
                else
                {
                    osdbgpt(&dbgtxtwin, "breakpoint cleared\n");
                    dbgugetbp(ctx);
                }
                break;

            case 'd':
                disable = TRUE;
                goto bp_enable_or_disable;
            case 'e':
                disable = FALSE;
            bp_enable_or_disable:
                if (err = dbgbpdis(ctx, atoi(buf+2), disable))
                    errlog(ctx->dbgcxerr, err);
                else
                    osdbgpt(&dbgtxtwin, "breakpoint %sabled\n",
                            disable ? "dis" : "en");
                break;
                
            case 'l':
                dbgbplist(ctx, dbgudisp, &dbgtxtwin);
                break;

            default:
                osdbgpt(&dbgtxtwin, "invalid breakpoint command\n");
                break;
            }
            break;
            
        case 'w':
            if (isupper((unsigned char)buf[1]))
                buf[1] = tolower((unsigned char)buf[1]);
            switch(buf[1])
            {
            case 's':
                if (err = dbgwxset(ctx, buf+2, &bpnum, level))
                    errlog(ctx->dbgcxerr, err);
                else
                    dbguwxadd(ctx, 1);       /* adjust windows for a new wx */
                break;
                
            case 'd':
                if (err = dbgwxdel(ctx, atoi(buf + 2)))
                    errlog(ctx->dbgcxerr, err);
                else
                    dbguwxadd(ctx, -1);   /* adjust windows for one less wx */
                break;
                
            default:
                osdbgpt(&dbgtxtwin, "invalid watch command\n");
                break;
            }
            break;
            
        case 'f':
            if (isupper((unsigned char)buf[1]))
                buf[1] = tolower((unsigned char)buf[1]);
            switch(buf[1])
            {
            case 'l':
                for (i = 1, lin = ctx->dbgcxlin ; lin ;
                     lin = lin->linnxt, ++i)
                {
                    osdbgpt(&dbgtxtwin, "#%-2d   %s", i,
                            ((linfdef *)lin)->linfnam);
                    osdbgpt(&dbgtxtwin, "\n");
                }
                break;
                
            case 'v':
                if (atoi(buf + 2) == 0)
                {
                    osfildef *fp;
                    
                    /* file by name - read the filename */
                    for (buf += 2 ; isspace((unsigned char)*buf) ; ++buf);
                    if (!(fp = osfoprb(buf, OSFTTEXT)))
                        osdbgpt(&dbgtxtwin, "file not found\n");
                    else
                    {
                        dbgswitch(&dbgsrclin, (lindef *)0);
                        *(buf-1) = '\0';
                        linfini2(ctx->dbgcxmem, (linfdef *)filesrc, buf-1,
                                 (int)strlen(buf) + 2, fp, FALSE);
                        dbgsrclin = (lindef *)filesrc;
                        lintell(dbgsrclin, dbgsrctop);
                        lintell(dbgsrclin, dbgsrccsr);
                        dbgsrcshow(dbgsrclin, &dbgsrcwin, dbgsrctop,
                                   dbgsrcloc);
                    }
                    break;
                }
                
                /* file by number - look up in line source list */
                for (i = 1, lin = ctx->dbgcxlin ; lin ;
                     lin = lin->linnxt, ++i)
                {
                    if (i == atoi(buf + 2)) break;
                }
                
                if (lin != 0)
                {
                    dbgswitch(&dbgsrclin, lin);
                    lingoto(dbgsrclin, LINGOTOP);
                    lintell(dbgsrclin, dbgsrccsr);
                    memcpy(dbgsrctop, dbgsrccsr, dbgsrclin->linlln);
                    dbgsrcshow(lin, &dbgsrcwin, dbgsrctop, dbgsrcloc);
                }
                else
                    osdbgpt(&dbgtxtwin, "invalid file number\n");
                break;
                
            default:
                osdbgpt(&dbgtxtwin, "invalid file command\n");
                break;
            }
            break;
            
        case '\0':
            break;
            
        case 'q':
            if (strcmp(buf, "quit"))
                osdbgpt(&dbgtxtwin, "invalid debugger command\n");
            else
            {
                ossvpg(0);                           /* return to game page */
                errsig(ctx->dbgcxerr, ERR_RUNQUIT);          /* signal QUIT */
            }
            break;
            
        case 'k':
            dbgstktr(ctx, dbgudisp, &dbgtxtwin, level, FALSE, TRUE);
            break;
            
        case 'h':
            if (!stricmp(buf, "help"))
                goto do_help;
            else
            {
                extern int dbghid;
                
                dbghid = !dbghid;
                osdbgpt(&dbgtxtwin, "Hidden text will be %s.\n",
                        dbghid ? "displayed" : "suppressed");
            }
            break;
            
        case '\\':
            dbguuser(ctx);
            break;
            
        case '/':
            if (dbgsrclin == 0)
                break;
            
            ++buf;                                     /* skip the '/' part */
            strcpy(lastsearch, buf);
        another_search:
            dbgusearch(dbgsrclin, dbgsrccsr, dbgsrctop,
                       &dbgsrcwin, buf);
            needprompt = TRUE;
            goto run_other_window;
            
        case '\t':
            /* 
             *   if there's no current source file, we can't switch to the
             *   source window 
             */
            if (dbgsrclin == 0)
                break;

            /* turn off prompting while we're in the source window */
            needprompt = FALSE;

            /* reset searching */
            first_search = TRUE;

            /* get a command from the source window */
        run_other_window:
            cmd = dbgurunsrc(ctx);
            if (cmd == CMD_UP)               /* special return for "search" */
            {
                if (first_search)
                {
                    osdbgpt(&dbgtxtwin, "\n");
                    first_search = FALSE;
                }
                
                /* present default if there is one */
                if (lastsearch[0])
                    osdbgpt(&dbgtxtwin, "search [%s]: ", lastsearch);
                else
                    osdbgpt(&dbgtxtwin, "search: ");
                
                /* ignore command keys from search call */
                while (osdbggts(&dbgtxtwin, cmdbuf, dbgukbc, ctx)) ;
                
                /* apply default if appropriate */
                if (cmdbuf[0])
                {
                    strcpy(lastsearch, (char *)cmdbuf);
                    buf = cmdbuf;
                }
                else
                    buf = lastsearch;

                stay_in_src = TRUE;
                goto another_search;
            }
            if (cmd == 0)
            {
                stay_in_src = FALSE;
                continue;
            }
            stay_in_src = TRUE;
            goto process_cmd;
            
        case 'u':
            if (level < ctx->dbgcxfcn - 1) ++level;
            osdbgpt(&dbgtxtwin, "evaluations are now at level %d\n", level+1);
            break;
            
        case 'd':
            if (level > 0) --level;
            osdbgpt(&dbgtxtwin, "evaluations are now at level %d\n", level+1);
            break;
            
        case '?':
            {
                char      buf[128];
                osfildef *fp;
            do_help:
                if ( !os_locate("help.tdb", 8, ARG0, buf, sizeof(buf))
                    || !(fp = osfoprb(buf, OSFTTEXT)))
                    osdbgpt(&dbgtxtwin, "HELP.TDB not found\n");
                else
                {
                    dbgswitch(&dbgsrclin, (lindef *)0);
                    linfini2(ctx->dbgcxmem, (linfdef *)filesrc,
                             buf, (int)strlen(buf), fp, FALSE);
                    dbgsrclin = (lindef *)filesrc;
                    lintell(dbgsrclin, dbgsrctop);
                    lintell(dbgsrclin, dbgsrccsr);
                    dbgsrcshow(dbgsrclin, &dbgsrcwin, dbgsrctop, dbgsrcloc);
                }
            }
            break;
            
        default:
            osdbgpt(&dbgtxtwin, "invalid debugger command\n");
            break;
        }
    }
}

void dbgucmd(dbgcxdef *ctx, int bphit, int err, unsigned int *exec_ofs)
{
    VARUSED(exec_ofs);

    ossvpg(1);                           /* change to debugger's video page */

    /* display cause of breakpoint if not single-stepping */
    if (err || bphit)
    {
        if (!needprompt) osdbgpt(&dbgtxtwin, "\n");
        if (err)
            errprelog(ctx->dbgcxerr, err);
        else if (bphit)
            osdbgpt(&dbgtxtwin, "Stop at breakpoint %d\n", bphit);
        needprompt = TRUE;
    }

    dbgucmd1(ctx);                              /* run the normal main loop */
    ossvpg(0);                         /* change back to default video page */
}

/*
 *   initialization phase one (called before the .GAM file is loaded) 
 */
void dbguini(dbgcxdef *ctx, const char *game_filename)
{
    char       buf[150];
    int        i;
    extern int sdesc_color;
    extern int text_color;
    int        ldesc_color = 112;
    int        watch_color = 0x47;
    extern int usebios;
    int        maxline;
    int        maxcol;
#ifdef MSOS2
    int        divline;
#endif

    ossgmx(&maxline, &maxcol);
    osdbgini(maxline+1, maxcol+1);
    
    usebios = 0;
    
    ossvpg(1);
    ossclr(0, 0, maxline, maxcol, 7);
    
    if (ossmon())
    {
        ldesc_color = 7;
        dbgcurcolor = 10;
        dbgbpcolor = 7;
        watch_color = 10;
    }
    
    /* initialize windows */
#ifdef MSOS2
    divline = maxline * 2 / 3;
    osdbgwop(&dbgsrcwin, 0, 0, maxcol, divline-1, sdesc_color);
    osdbgwop(&dbgstawin, 0, divline, maxcol, divline, ldesc_color);
    osdbgwop(&dbgtxtwin, 0, divline+1, maxcol, maxline, text_color);
    osdbgwop(&dbgwatwin, 0, divline+1, maxcol, divline, watch_color);
#else /* !MSOS2 */
    osdbgwop(&dbgsrcwin, 0, 0, 79, 15, sdesc_color);
    osdbgwop(&dbgstawin, 0, 16, 79, 16, ldesc_color);
    osdbgwop(&dbgtxtwin, 0, 17, 79, 24, text_color);
    osdbgwop(&dbgwatwin, 0, 17, 79, 16, watch_color);
#endif /* MSOS2 */
    dbgsrcwin.oswflg = OSWFCLIP;
    dbgstawin.oswflg = OSWFCLIP;
    dbgwatwin.oswflg = OSWFCLIP;
    dbgtxtwin.oswflg = OSWFMORE;
    
    /* display status line with bar */
    for (i = 0 ; i <= maxcol && i+1 < sizeof(buf) ; ++i)
        buf[i] = (uchar)DBGUHLIN;
    buf[i] = '\0';
    osdbgpt(&dbgstawin, buf);

    ossvpg(0);

    /* allocate buffer */
    dbgbuf = (char *)malloc(dbgbufsiz = 4096);
}

/*
 *   initialization phase two (called after the .GAM file is loaded) 
 */
void dbguini2(dbgcxdef *ctx)
{
    /* we don't need to do any more setup after loading the .GAM file */
}

/* terminate the UI */
void dbguterm(dbgcxdef *ctx)
{
    /* there's nothing extra we need to do here */
}

/*
 *   Determine if we can resume from an error.  This UI doesn't allow the
 *   user to change the instruction pointer, so we can't resume from an
 *   error. 
 */
int dbgu_err_resume(dbgcxdef *ctx)
{
    VARUSED(ctx);
    return FALSE;
}

/*
 *   Quitting - do nothing; let the system terminate 
 */
void dbguquitting(dbgcxdef *ctx)
{
    VARUSED(ctx);
}

/*
 *   Locate a source file using UI-dependent mechanisms.  We don't have
 *   anything more we can do beyond what the core debugger does, so we'll
 *   simply return a failure indication. 
 */
int dbgu_find_src(const char *origname, int origlen,
                  char *fullname, size_t full_len, int must_find_file)
{
    VARUSED(origname);
    VARUSED(origlen);
    VARUSED(fullname);
    VARUSED(full_len);
    VARUSED(must_find_file);

    /* indicate failure */
    return FALSE;
}
