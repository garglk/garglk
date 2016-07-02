#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/TDD.C,v 1.4 1999/07/11 00:46:30 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2000 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tdd.c - TADS 2 debugger main
Function
  Main entrypoint for TADS 2 debugger
Notes
  None
Modified
  04/11/99 CNebel        - Use new headers.
  04/12/92 MJRoberts     - creation
*/

#include <stdio.h>
#include <stdarg.h>
#include "os.h"
#include "std.h"
#include "trd.h"
#include "err.h"
#include "mch.h"
#include "obj.h"
#include "run.h"
#include "voc.h"
#include "bif.h"
#include "dbg.h"
#include "sup.h"
#include "cmd.h"
#include "fio.h"
#include "prs.h"
#include "tok.h"
#include "emt.h"
#include "ply.h"
#include "oem.h"
#include "cmap.h"
#include "regex.h"


static void tddlogerr(void *ctx, char *fac, int err, int argc, erradef *argv);

/* printf-style formatter */
static void tddptf(const char *fmt, ...)
{
    char buf[256];
    va_list va;

    /* format the string */
    va_start(va, fmt);
    vsprintf(buf, fmt, va);
    va_end(va);

    /* print the formatted buffer */
    os_printz(buf);
}


static void tddusage_range(errcxdef *ec, int first_msg, int last_msg)
{
    int  i;
    char buf[128];
    
    for (i = first_msg ; i <= last_msg ; ++i)
    {
        errmsg(ec, buf, (uint)sizeof(buf), i);
        tddptf("%s\n", buf);
    }

    errsig(ec, ERR_USAGE);
}

static void tddusage(errcxdef *ctx)
{
    if (ctx->errcxappctx != 0 && ctx->errcxappctx->usage_app_name != 0)
    {
        char buf[128];
        char buf2[128];
        erradef argv[1];

        /* get the parameterized usage message */
        errmsg(ctx, buf, (uint)sizeof(buf), ERR_TRUSPARM);

        /* format in the application name */
        argv[0].errastr = (char *)ctx->errcxappctx->usage_app_name;;
        errfmt(buf2, (int)sizeof(buf2), buf, 1, argv);

        /* display it */
        tddptf("%s\n", buf2);

        /* display the rest of the messages */
        tddusage_range(ctx, ERR_TDBUS1+1, ERR_TDBUSL);
    }
    else
    {
        /* display the default messages */
        tddusage_range(ctx, ERR_TDBUS1, ERR_TDBUSL);
    }
}

static void tddusage_s(errcxdef *ctx)
{
    tddusage_range(ctx, ERR_TRSUS1, ERR_TRSUSL);
}


static void tddmain1(errcxdef *ec, int argc, char **argv, appctxdef *appctx,
                     char *save_ext)
{
    osfildef  *swapfp = (osfildef *)0;
    runcxdef   runctx;
    bifcxdef   bifctx;
    voccxdef   vocctx;
    void     (*bif[100])(struct bifcxdef *ctx, int argc);
    mcmcxdef  *mctx;
    mcmcx1def *globalctx;
    dbgcxdef   dbg;
    supcxdef   supctx;
    char      *swapname = 0;
    char       swapbuf[OSFNMAX];
    char     **argp;
    char      *arg;
    char      *infile;
    char       infile_abs[OSFNMAX];      /* fully-qualified input file name */
    char       infile_path[OSFNMAX];         /* absolute path to input file */
    ulong      swapsize = 0xffffffffL;        /* allow unlimited swap space */
    int        swapena = OS_DEFAULT_SWAP_ENABLED;      /* swapping enabled? */
    int        i;
    int        pause = FALSE;                 /* pause after finishing game */
    fiolcxdef  fiolctx;
    noreg int  loadopen = FALSE;
    char       inbuf[OSFNMAX];
    ulong      cachelimit = 0xffffffff;
    ushort     undosiz = TDD_UNDOSIZ;      /* default undo context size 16k */
    objucxdef *undoptr;
    uint       flags;                          /* flags read from game file */
    objnum     preinit;                   /* preinit function object number */
    prscxdef  *pctx;
    tokcxdef  *tc;
    emtcxdef  *ectx;
    uint       heapsiz = TDD_HEAPSIZ;
    uint       stksiz = TDD_STKSIZ;
    runsdef   *mystack;
    uchar     *myheap;
    uint       lclsiz = TDD_LCLSIZ;
    uint       poolsiz = TDD_POOLSIZ;
    int        safety_read, safety_write;      /* file safety level setting */
    char      *restore_file = 0;              /* saved game file to restore */
    char      *charmap = 0;                       /* character mapping file */
    int        charmap_none = FALSE;            /* use no character mapping */

    NOREG((&loadopen))
        
    /* initialize the output formatter */
    out_init();

    /* initialize lexical analysis context */
    tc = tokcxini(ec, (mcmcxdef *)0, supsctab);
    tc->tokcxdbg = &dbg;
    tc->tokcxsst = (ushort (*)(void *))prsxsst;
    tc->tokcxsad = (void (*)(void *, char *, ushort))prsxsad;
    tc->tokcxsend = (void (*)(void *))prsxsend;
    
    /* add current directory as first entry in search path */
    tokaddinc(tc, "", 0);

    /* set safety level 2 by default */
    safety_read = safety_write = 2;

    /* parse arguments */
    for (i = 1, argp = argv + 1 ; i < argc ; ++argp, ++i)
    {
        arg = *argp;
        if (*arg == '-')
        {
            switch(*(arg+1))
            {
            case 'c':
                if (!strcmp(arg+1, "ctab-"))
                {
                    /* do not use any character mapping table */
                    charmap_none = TRUE;
                }
                else if (!strcmp(arg+1, "ctab"))
                {
                    /* get the character mapping table */
                    charmap = cmdarg(ec, &argp, &i, argc, 4, tddusage);
                }
                else
                    tddusage(ec);
                break;
                
            case 'r':
                /* restore a game */
                restore_file = cmdarg(ec, &argp, &i, argc, 1, tddusage);
                break;
                
            case 's':
                {
                    char *p;

                    /* get the option */
                    p = cmdarg(ec, &argp, &i, argc, 1, tddusage);

                    /* if they're asking for help, display detailed usage */
                    if (*p == '?')
                        tddusage_s(ec);

                    /* get the safety level from the argument */
                    safety_read = *p - '0';
                    safety_write = (*(p+1) != '\0' ? *(p+1) - '0' :
                                    safety_read);

                    /* range-check the values */
                    if (safety_read < 0 || safety_read > 4
                        || safety_write < 0 || safety_write > 4)
                        tddusage_s(ec);

                    /* tell the host system about the settings */
                    if (appctx != 0 && appctx->set_io_safety_level != 0)
                        (*appctx->set_io_safety_level)
                            (appctx->io_safety_level_ctx,
                             safety_read, safety_write);
                }
                break;

            case 'm':
                switch(*(arg + 2))
                {
                case 's':
                    stksiz = atoi(cmdarg(ec, &argp, &i, argc, 2, tddusage));
                    break;
                    
                case 'h':
                    heapsiz = atoi(cmdarg(ec, &argp, &i, argc, 2, tddusage));
                    break;
                    
                case 'p':
                    poolsiz = atoi(cmdarg(ec, &argp, &i, argc, 2, tddusage));
                    break;
                    
                default:
                    cachelimit = atol(cmdarg(ec, &argp, &i, argc, 1,
                                             tddusage));
                    break;
                }
                break;
                
            case 't':
                /* swap file options:  -tf file, -ts size, -t- (no swap) */
                switch(*(arg+2))
                {
                case 'f':
                    swapname = cmdarg(ec, &argp, &i, argc, 2, tddusage);
                    break;
                    
                case 's':
                    swapsize = atol(cmdarg(ec, &argp, &i, argc, 2, tddusage));
                    break;
                    
                default:
                    swapena = cmdtog(ec, swapena, arg, 1, tddusage);
                    break;
                }
                break;
                
            case 'u':
                undosiz = atoi(cmdarg(ec, &argp, &i, argc, 1, tddusage));
                break;
                
            case 'i':
                {
                    char *path = cmdarg(ec, &argp, &i, argc, 1, tddusage);
                    tokaddinc(tc, path, (int)strlen(path));
                    break;
                }
                
            default:
                tddusage(ec);
            }
        }
        else break;
    }

    /* get input name argument */
    if (i == argc)
    {
        /* 
         *   no file was provided - check to see if the saved game file
         *   we're to restore specifies a game file; if that fails, see if
         *   the host system wants to provide a file 
         */
        infile = 0;
        if (restore_file != 0)
        {
            /* see if we can find the game name from the restore file */
            if (fiorso_getgame(restore_file, inbuf, sizeof(inbuf)))
            {
                /* got it - use this file */
                infile = inbuf;
            }
        }

        /* 
         *   if that didn't work, see if the host system can get us a file 
         */
        if (infile == 0 && appctx != 0 && appctx->get_game_name != 0
            && (*appctx->get_game_name)(appctx->get_game_name_ctx,
                                        inbuf, sizeof(inbuf)))
        {
            /* the host system provided a name - use it */
            infile = inbuf;
        }
        else
        {
            /* 
             *   we can't find an input file - give the usage message and
             *   give up 
             */
            tddusage(ec);
        }
    }
    else
    {
        /* use the last argument as the filename */
        infile = *argp;

        /* 
         *   make sure this is really the last argument - abort with the
         *   usage message if not 
         */
        if (i + 1 != argc)
            tddusage(ec);

        /* if the original name exists, use it; otherwise, try adding .GAM */
        if (osfacc(infile))
        {
            strcpy(inbuf, infile);
            os_defext(inbuf, "gam");
            infile = inbuf;
        }
    }

    /* open up the swap file */
    if (swapena && swapsize)
    {
        swapfp = os_create_tempfile(swapname, swapbuf);
        if (swapname == 0) swapname = swapbuf;
        if (swapfp == 0) errsig(ec, ERR_OPSWAP);
    }

    /* load the character map */
    if (charmap_none)
        cmap_override();
    else if (cmap_load(charmap))
        errsig(ec, ERR_INVCMAP);

    ERRBEGIN(ec)

    /* initialize cache manager context */
    globalctx = mcmini(cachelimit, 128, swapsize, swapfp, 0, ec);
    mctx = mcmcini(globalctx, 128, fioldobj, &fiolctx,
                   objrevert, (void *)0);
    mctx->mcmcxrvc = mctx;

    /* allocate and initialize parsing context */
    pctx = (prscxdef *)mchalo(ec, (sizeof(prscxdef) + poolsiz +
                                   lclsiz), "tcdmain");
    pctx->prscxerr = ec;
    pctx->prscxtok = tc;
    pctx->prscxmem = mctx;
    pctx->prscxprp = 0;
    pctx->prscxext = 0;
    pctx->prscxnsiz = pctx->prscxrrst = poolsiz;
    pctx->prscxnrst = pctx->prscxpool;
    pctx->prscxplcl = &pctx->prscxpool[poolsiz];
    pctx->prscxslcl = lclsiz;
    pctx->prscxflg = 0;
    pctx->prscxnode = &pctx->prscxpool[0];
    pctx->prscxgtab = (toktdef *)0;
    pctx->prscxvoc = &vocctx;
    pctx->prscxcpp = (char *)0;
    pctx->prscxcps = 0;
    pctx->prscxcpf = 0;
    pctx->prscxfsp = (uchar *)0;
    pctx->prscxfsf = 0;
    pctx->prscxfss = 0;
    pctx->prscxstab = (toktdef *)0;

    tc->tokcxscx = (void *)pctx;

    /* allocate a code generator context */
    ectx = (emtcxdef *)mchalo(ec, (sizeof(emtcxdef) +
                                   511*sizeof(emtldef)), "tcdmain");
    ectx->emtcxerr = ec;
    ectx->emtcxmem = pctx->prscxmem;
    ectx->emtcxptr = 0;   /* mcmalo(pctx->prscxmem, 1024, &ectx->emtcxobj); */
    ectx->emtcxlcnt = 512;
    ectx->emtcxfrob = MCMONINV;
    emtlini(ectx);
    
    pctx->prscxemt = ectx;

    /* set up an undo context */
    if (undosiz)
        undoptr = objuini(mctx, undosiz, vocdundo, vocdusz, &vocctx);
    else
        undoptr = (objucxdef *)0;

    /* set up vocabulary context */
    vocini(&vocctx, ec, mctx, &runctx, undoptr, 50, 50, 100);    
    
    /* allocate stack and heap */
    mystack = (runsdef *)mchalo(ec, (stksiz * sizeof(runsdef)),
                                "runtime stack");
    myheap = mchalo(ec, heapsiz, "runtime heap");

    /* get the absolute path for the input file */
    os_get_abs_filename(infile_abs, sizeof(infile_abs), infile);
    os_get_path_name(infile_path, sizeof(infile_path), infile_abs);

    /* set up execution context */
    runctx.runcxerr = ec;
    runctx.runcxmem = mctx;
    runctx.runcxstk = mystack;
    runctx.runcxstop = &mystack[stksiz];
    runctx.runcxsp = mystack;
    runctx.runcxbp = mystack;
    runctx.runcxheap = myheap;
    runctx.runcxhp = myheap;
    runctx.runcxhtop = &myheap[heapsiz];
    runctx.runcxundo = undoptr;
    runctx.runcxbcx = &bifctx;
    runctx.runcxbi = bif;
    runctx.runcxtio = (tiocxdef *)0;
    runctx.runcxdbg = &dbg;
    runctx.runcxvoc = &vocctx;
    runctx.runcxdmd = supcont;
    runctx.runcxdmc = &supctx;
    runctx.runcxext = 0;
    runctx.runcxgamename = infile;
    runctx.runcxgamepath = infile_path;

    /* set up setup context */
    supctx.supcxerr = ec;
    supctx.supcxmem = mctx;
    supctx.supcxtab = (tokthdef *)0;
    supctx.supcxbuf = (uchar *)0;
    supctx.supcxlen = 0;
    supctx.supcxvoc = &vocctx;
    supctx.supcxrun = &runctx;
    
    /* set up debug context */
    dbg.dbgcxtio = (tiocxdef *)0;
    dbg.dbgcxmem = mctx;
    dbg.dbgcxerr = ec;
    dbg.dbgcxtab = (tokthdef *)0;
    dbg.dbgcxfcn = 0;
    dbg.dbgcxdep = 0;
    dbg.dbgcxflg = DBGCXFSS + DBGCXFOK;    /* start off in single-step mode */
    dbg.dbgcxprs = pctx;
    dbg.dbgcxrun = &runctx;
    dbg.dbgcxlin = (lindef *)0;
    dbg.dbgcxnams = 2048;
    dbg.dbgcxnam = (char *)mchalo(ec, dbg.dbgcxnams, "bp names");
    dbg.dbgcxnamf = 0;
    dbg.dbgcxhstl = 4096;
    dbg.dbgcxhstf = 0;
    dbg.dbgcxui = 0;
    dbg.dbgcxhstp = (char *)mchalo(ec, dbg.dbgcxhstl, "history buf");
    
    memset(dbg.dbgcxbp, 0, sizeof(dbg.dbgcxbp));       /* clear breakpoints */
    memset(dbg.dbgcxwx, 0, sizeof(dbg.dbgcxwx));           /* clear watches */
    
    /* set up built-in function context */
    CLRSTRUCT(bifctx);
    bifctx.bifcxerr = ec;
    bifctx.bifcxrun = &runctx;
    bifctx.bifcxtio = (tiocxdef *)0;
    bifctx.bifcxrnd = 0;
    bifctx.bifcxrndset = FALSE;
    bifctx.bifcxappctx = appctx;
    bifctx.bifcxsafetyr = safety_read;
    bifctx.bifcxsafetyw = safety_write;
    bifctx.bifcxsavext = save_ext;
    
    /* initialize the regular expression parser context */
    re_init(&bifctx.bifcxregex, ec);

    ec->errcxlog = tddlogerr;
    ec->errcxlgc = &dbg;

    /* initialize debugger user interface */
    dbguini(&dbg, infile);
    
    /* read the game from the binary file */
    fiord(mctx, &vocctx, (struct tokcxdef *)0,
          infile, (char *)0, &fiolctx, &preinit, &flags,
          tc->tokcxinc, (uchar **)0, (uint *)0, (uint *)0, FALSE, appctx,
          argv[0]);
    loadopen = TRUE;

    /* use same case sensitivity as original compilation */
    if (flags & FIOFCASE)
        tc->tokcxflg |= TOKCXCASEFOLD;
    else
        tc->tokcxflg &= ~TOKCXCASEFOLD;

    /* make sure the game was compiled for debugging */
    if ((flags & (FIOFSYM + FIOFLIN + FIOFPRE)) !=
        (FIOFSYM + FIOFLIN + FIOFPRE))
        errsig(ec, ERR_NODBG);
    
    /* add the built-in functions, keywords, etc */
    suprsrv(&supctx, bif, &dbg.dbgcxtab->tokthsc,
            (int)(sizeof(bif)/sizeof(bif[0])),
            FALSE, (char *)0, tc->tokcxflg & TOKCXCASEFOLD);
    tc->tokcxstab = (toktdef *)dbg.dbgcxtab;
    
    /* do the second phase of debugger initialization */
    dbguini2(&dbg);

    /* set up status line hack */
    runistat(&vocctx, &runctx, (tiocxdef *)0);
    
    /* play the game, running preinit if necessary */
    plygo(&runctx, &vocctx, (tiocxdef *)0, preinit, restore_file);
    
    if (pause)
    {
        tddptf("[strike a key to exit]");
        os_waitc();
        tddptf("\n");
    }
    
    /* close load file */
    fiorcls(&fiolctx);
    loadopen = FALSE;

    /* close and delete swapfile, if one was opened */
    if (swapfp != 0)
    {
        osfcls(swapfp);
        swapfp = (osfildef *)0;
        osfdel_temp(swapname);
    }

    /* tell the debugger user interface that we're terminating */
    dbguterm(&dbg);

    ERRCLEAN(ec)
        /* close and delete swapfile, if one was opened */
        if (swapfp != 0)
        {
            osfcls(swapfp);
            swapfp = (osfildef *)0;
            osfdel_temp(swapname);
        }
        
        /* close the load file if one was opened */
        if (loadopen)
            fiorcls(&fiolctx);

        /* tell the debugger user interface that we're terminating */
        dbguterm(&dbg);

        /* delete the voc context */
        vocterm(&vocctx);

        /* delete the undo context */
        if (undoptr != 0)
            objuterm(undoptr);

        /* release the object cache structures */
        mcmcterm(mctx);
        mcmterm(globalctx);

    ERRENDCLN(ec)

    /* delete the voc context */
    vocterm(&vocctx);

    /* delete the undo context */
    if (undoptr != 0)
        objuterm(undoptr);

    /* release the object cache structures */
    mcmcterm(mctx);
    mcmterm(globalctx);
}

/* log an error */
static void tddlogerr(void *ctx, char *fac, int err, int argc, erradef *argv)
{
    char  buf[512];
    char  msg[512];

    errmsg(((dbgcxdef *)ctx)->dbgcxerr, msg, (uint)sizeof(msg), err);
    errfmt(buf, (int)sizeof(buf), msg, argc, argv);
    dbguerr((dbgcxdef *)ctx, err, buf);
}

/* log an error - used before debugger context is set up */
static void tddlogerr1(void *ctx0, char *fac, int err, int argc,
                       erradef *argv)
{
    errcxdef *ctx = (errcxdef *)ctx0;
    char      buf[512];
    char      msg[512];

    errmsg(ctx, msg, (uint)sizeof(msg), err);
    errfmt(buf, (int)sizeof(buf), msg, argc, argv);
    tddptf("%s-%d: %s\n", fac, err, buf);
}

/* main - called by os main after setting up arguments */
int tddmain(int argc, char **argv, appctxdef *appctx, char *save_ext)
{
    errcxdef  errctx;
    int       err;
    osfildef *fp;
    
    errctx.errcxlog = tddlogerr1;
    errctx.errcxlgc = &errctx;
    errctx.errcxfp  = (osfildef *)0;
    errctx.errcxofs = 0;
    errctx.errcxappctx = appctx;
    fp = oserrop(argv[0]);
    errini(&errctx, fp);

    /* copyright-date-string */
    tddptf("%s - A %s TADS %s Debugger.\n"
           "%sopyright (c) 1993, 2012 Michael J. Roberts\n",
           G_tads_oem_dbg_name, G_tads_oem_display_mode, TADS_RUNTIME_VERSION,
           G_tads_oem_copyright_prefix ? "TADS c" : "C");
    tddptf("%s\n", G_tads_oem_author);

    /* display any special port-specific startup message */
    tddptf(OS_TDB_STARTUP_MSG);
    
    ERRBEGIN(&errctx)
        tddmain1(&errctx, argc, argv, appctx, save_ext);
    ERRCATCH(&errctx, err)
        /* 
         *   log the error, unless it's usage (in which case we logged it
         *   already) or we're simply quitting the game 
         */
        if (err != ERR_USAGE && err != ERR_RUNQUIT)
            errclog(&errctx);

        /* close the error file */
        if (errctx.errcxfp != 0)
            osfcls(errctx.errcxfp);

        /* return failure unless we're simply quitting the game */
        return (err == ERR_RUNQUIT ? OSEXSUCC : OSEXFAIL);
    ERREND(&errctx)

    /* close the error file if we opened it */
    if (errctx.errcxfp != 0)
        osfcls(errctx.errcxfp);

    /* successful completion */
    return OSEXSUCC;
}


