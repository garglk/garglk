#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/TCD.C,v 1.4 1999/07/11 00:46:30 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2000 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tcd.c - tads2 compiler driver
Function
  compiles a game and writes to a binary file
Notes
  This is essentially for the command line interface; main() calls
  tcdmain(), which compiles the game and writes to a binary file.
Modified
  04/04/98 CNebel        - Use new headers.
  12/16/92 MJRoberts     - add TADS/Graphic extensions
  04/02/92 MJRoberts     - creation
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include "os.h"
#include "std.h"
#include "lin.h"
#include "linf.h"
#include "err.h"
#include "tok.h"
#include "mch.h"
#include "mcm.h"
#include "prs.h"
#include "emt.h"
#include "opc.h"
#include "run.h"
#include "voc.h"
#include "bif.h"
#include "dbg.h"
#include "sup.h"
#include "cmd.h"
#include "fio.h"
#include "oem.h"
#include "cmap.h"
#include "tcg.h"

/* this must be included after os.h, because of memory size #defines */
#include "tcd.h"

/* debugger not present */
int dbgpresent()
{
    return FALSE;
}

/* dummy debugger functions */
void trchid() {}
void trcsho() {}

int dbgu_err_resume(dbgcxdef *ctx)
{
    VARUSED(ctx);
    return FALSE;
}

void dbguquitting(dbgcxdef *ctx)
{
    VARUSED(ctx);
}

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

struct runsdef *dbgfrfind(dbgcxdef *ctx, objnum frobj, uint frofs)
{
    VARUSED(frobj);
    VARUSED(frofs);
    errsig(ctx->dbgcxerr, ERR_INACTFR);
    return 0;
}

void dbgss(struct dbgcxdef *ctx, uint ofs, int instr, int err,
           uchar *noreg *instrp)
{
    VARUSED(ctx);
    VARUSED(ofs);
    VARUSED(instr);
    VARUSED(err);
    return;
}

int dbgstart(struct dbgcxdef *ctx)
{
    VARUSED(ctx);
    return(TRUE);
}

/* dummy functions to satisfy built-ins that can't be used by preinit */

void vocinialo(voccxdef *ctx, vocddef **what, int cnt)
{
    VARUSED(ctx);
    VARUSED(what);
    VARUSED(cnt);
}

void vocremfd(voccxdef *ctx, vocddef *what, objnum func, prpnum prop,
              runsdef *val, int err)
{
    VARUSED(ctx);
    VARUSED(what);
    VARUSED(func);
    VARUSED(prop);
    VARUSED(val);
    VARUSED(err);
}

void vocsetfd(voccxdef *ctx, vocddef *what, objnum func, prpnum prop,
              uint tm, runsdef *val, int err)
{
    VARUSED(ctx);
    VARUSED(what);
    VARUSED(func);
    VARUSED(prop);
    VARUSED(tm);
    VARUSED(val);
    VARUSED(err);
}

void vocturn(voccxdef *ctx, int cnt, int do_fuses)
{
    VARUSED(ctx);
    VARUSED(cnt);
    VARUSED(do_fuses);
}

void vocdmnclr(voccxdef *ctx)
{
    VARUSED(ctx);
}

int voclistlen(vocoldef *lst)
{
    VARUSED(lst);
    return 0;
}

void exedaem(voccxdef *ctx)
{
    VARUSED(ctx);
}

int exefuse(voccxdef *ctx, int do_run)
{
    VARUSED(ctx);
    VARUSED(do_run);
    return FALSE;
}

int execmd_recurs(voccxdef *ctx, objnum actor, objnum verb,
                  objnum dobj, objnum prep, objnum iobj,
                  int validate_dobj, int validate_iobj)
{
    VARUSED(ctx);
    VARUSED(actor);
    VARUSED(verb);
    VARUSED(dobj);
    VARUSED(prep);
    VARUSED(iobj);
    return 0;
}

void voc_parse_tok(voccxdef *ctx) { VARUSED(ctx); }
void voc_parse_types(voccxdef *ctx) { VARUSED(ctx); }
void voc_parse_dict_lookup(voccxdef *ctx) { VARUSED(ctx); }
void voc_parse_np(voccxdef *ctx) { VARUSED(ctx); }
void voc_parse_disambig(voccxdef *ctx) { VARUSED(ctx); }
void voc_parse_replace_cmd(voccxdef *ctx) { VARUSED(ctx); }
void voc_push_objlist(voccxdef *ctx, objnum objlist[], int cnt)
{
    VARUSED(ctx);
    VARUSED(objlist);
    VARUSED(cnt);
}

/* more stub items for run-time */
FILE *scrfp;
int scrquiet;

char *qasgets(char *buf, int bufl)
{
    VARUSED(buf);
    return((char *)0);
}

void qasclose(void)
{
}

void vocdusave_newobj(voccxdef *ctx, objnum objn) {}
void vocdusave_delobj(voccxdef *ctx, objnum objn) {}
void vocdusave_addwrd(voccxdef *ctx, objnum objn, prpnum typ, int flags,
                      char *wrd) {}
void vocdusave_delwrd(voccxdef *ctx, objnum objn, prpnum typ, int flags,
                      char *wrd) {}
void vocdusave_me(voccxdef *ctx, objnum old_me) {}

/* 
 *   dummy runstat - we have no status line when running preinit under the
 *   compiler, so this doesn't need to do anything 
 */
void runstat(void)
{
}

/* printf-style formatting */
static void tcdptf(const char *fmt, ...)
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

static void tcduspt(errcxdef *ec, int first, int last)
{
    int  i;
    char buf[128];
    
    for (i = first ; i <= last ; ++i)
    {
        errmsg(ec, buf, (uint)sizeof(buf), i);
        tcdptf("%s\n", buf);
    }
}

static void tcdtogusage(errcxdef *ec)
{
    tcduspt(ec, ERR_TCTGUS1, ERR_TCTGUSL);
}

static void tcdusage(errcxdef *ec)
{
    tcduspt(ec, ERR_TCUS1, ERR_TCUSL);
    tcdtogusage(ec);
    errsig(ec, ERR_USAGE);
}

static void tcdzusage(errcxdef *ec)
{
    tcduspt(ec, ERR_TCZUS1, ERR_TCZUSL);
    tcdtogusage(ec);
    errsig(ec, ERR_USAGE);
}

static void tcd1usage(errcxdef *ec)
{
    tcduspt(ec, ERR_TC1US1, ERR_TC1USL);
    tcdtogusage(ec);
    errsig(ec, ERR_USAGE);
}

static void tcdmusage(errcxdef *ec)
{
    tcduspt(ec, ERR_TCMUS1, ERR_TCMUSL);
    errsig(ec, ERR_USAGE);
}

static void tcdvusage(errcxdef *ec)
{
    tcduspt(ec, ERR_TCVUS1, ERR_TCVUSL);
    errsig(ec, ERR_USAGE);
}

/* context for tcdchkundef */
typedef struct tcdchkctx
{
    mcmcxdef *mem;
    errcxdef *ec;
    int       cnt;
} tcdchkctx;

/*
 *   Callback to check for undefined objects 
 */
static void tcdchkundef(void *ctx0, toksdef *t)
{
    tcdchkctx *ctx = (tcdchkctx *)ctx0;
    int        err;
    
    switch(t->tokstyp)
    {
    case TOKSTFWDOBJ:
        /* undefined function */
        err = ERR_UNDEFO;
        goto log_the_error;
        
    case TOKSTFWDFN:
        /* undefined object */
        err = ERR_UNDEFF;

    log_the_error:
        /* display the error */
        sup_log_undefobj(ctx->mem, ctx->ec, err,
                         t->toksnam, (int)t->tokslen, (objnum)t->toksval);

        /* count the symbol */
        ++(ctx->cnt);
        break;

    default:
        /* ignore defined objects */
        break;
    }
}


/*
 *   Error callback context 
 */
struct tcderrdef
{
    FILE     *tcderrfil;                              /* error logging file */
    tokcxdef *tcderrtok;                           /* token parsing context */
    int       tcderrlvl;                                   /* warning level */
    int       tcderrcnt;        /* number of non-warning errors encountered */
    int       tcdwrncnt;                  /* number of warnings encountered */
    int       tcdfatal;                       /* a fatal error has occurred */
    char      tcdwrn_AMBIGBIN;   /* warn on ERR_AMBIGBIN (-v-abin hides it) */
};
typedef struct tcderrdef tcderrdef;

/*
 *   Version string information.  This information is used by both the
 *   pre-defined #define's that are set up by the compiler and by the
 *   version string display (keeping them common increases the likelihood
 *   that they'll remain in sync).  
 */
static char vsn_major[] = "2";
static char vsn_minor[] = "5";
static char vsn_maint[] = "17";


/*
 *   Default memory sizes, if previously defined 
 */
#ifndef TCD_SETTINGS_DEFINED
# define TCD_POOLSIZ  (6 * 1024)
# define TCD_LCLSIZ   2048
# define TCD_HEAPSIZ  4096
# define TCD_STKSIZ   50
# define TCD_LABSIZ   1024
#endif

/* TRUE if pause after compile/run */
static int ex_pause = FALSE;

/* compiler main */
static void tcdmain1(errcxdef *ec, int argc, char *argv[], tcderrdef *errcbcx,
                     char *save_ext)
{
    tokcxdef  *tc;
    prscxdef  *pctx;
    osfildef  *swapfp = (osfildef *)0;
    emtcxdef  *ectx;
    tokthdef   symtab;                               /* hashed symbol table */
    toktldef   labtab;                           /* table for 'goto' labels */
    runcxdef   runctx;
    bifcxdef   bifctx;
    uchar     *labmem;
    mcmon      evalobj;
    voccxdef   vocctx;
    void     (*bif[100])(struct bifcxdef *, int);
    mcmcxdef  *mctx = 0;
    mcmcx1def *globalctx;
    objnum     preinit;
    dbgcxdef   dbg;
    supcxdef   supctx;
    int        err;
    char      *swapname = 0;
    char       swapbuf[OSFNMAX];
    char     **argp;
    char      *arg;
    char      *outfile = (char *)0;                            /* .gam file */
    char       outfile_abs[OSFNMAX];      /* fully-qualified .gam file name */
    char       outfile_path[OSFNMAX];         /* absolute path to .gam file */
    char      *infile;
    ulong      swapsize = 0xffffffffL;        /* allow unlimited swap space */
    int        swapena = OS_DEFAULT_SWAP_ENABLED;      /* swapping enabled? */
    linfdef   *noreg linf = 0;                    /* input file line source */
    int        i;
    char       inbuf[OSFNMAX];
    char       outbuf[OSFNMAX];
    ulong      cachelimit = 0xffffffffL;
    int        argchecking = TRUE;          /* enable run-time arg checking */
    int        v1kw = FALSE;               /* v1 keyword compatibility mode */
    int        v1else = FALSE;   /* v1 'else' compat - ignore ';' after '}' */
    int        stats = FALSE;                  /* show statistics when done */
    int        symdeb = FALSE;          /* generate source-level debug info */
    int        symdeb2 = FALSE;        /* new-style source-level debug info */
    int        genoutput = TRUE;      /* FALSE if output file is suppressed */
    uint       poolsiz = TCD_POOLSIZ;  /* default space for parse node pool */
    uint       lclsiz = TCD_LCLSIZ;    /* default space for local variables */
    uint       heapsiz = TCD_HEAPSIZ;
    uint       stksiz = TCD_STKSIZ;
    uint       labsiz = TCD_LABSIZ;
    char      *binout = (char *)0;                /* precompiled header out */
    char      *binin = (char *)0;                  /* precompiled header in */
    fiolcxdef  fiolctx;                                     /* load context */
    noreg int  loadopen = FALSE;
    uint       inflags;
    char      *do_kw = (char *)0;           /* replacement keyword for "do" */
    int        warnlevel = 0;                              /* warning level */
    char      *filever = "*";            /* .GAM file compatibility setting */
    int        c_mode = FALSE;                     /* use C-style operators */
    char      *sym;
    int        symlen;
    char      *expan;
    int        explen;
    static char vsn_major_nm[] = "__TADS_VERSION_MAJOR";
    static char vsn_minor_nm[] = "__TADS_VERSION_MINOR";
    static char sys_name[] = "__TADS_SYSTEM_NAME";
    static char oem_name[] = "__TADS_OEM_NAME";
    static char dbg_name[] = "__DEBUG";
    static char date_name[] = "__DATE__";
    static char time_name[] = "__TIME__";
    static char line_name[] = "__LINE__";
    static char file_name[] = "__FILE__";
    static char si_sysinfo[] = "__SYSINFO_SYSINFO";
    static char si_version[] = "__SYSINFO_VERSION";
    static char si_osname[] = "__SYSINFO_OS_NAME";
    static char si_html[] = "__SYSINFO_HTML";
    static char si_jpeg[] = "__SYSINFO_JPEG";
    static char si_png[] = "__SYSINFO_PNG";
    static char si_wav[] = "__SYSINFO_WAV";
    static char si_midi[] = "__SYSINFO_MIDI";
    static char si_wav_midi_ovl[] = "__SYSINFO_WAV_MIDI_OVL";
    static char si_wav_ovl[] = "__SYSINFO_WAV_OVL";
    static char si_pref_images[] = "__SYSINFO_PREF_IMAGES";
    static char si_pref_sounds[] = "__SYSINFO_PREF_SOUNDS";
    static char si_pref_music[] = "__SYSINFO_PREF_MUSIC";
    static char si_pref_links[] = "__SYSINFO_PREF_LINKS";
    static char si_mpeg[] = "__SYSINFO_MPEG_AUDIO";
    static char si_mpeg1[] = "__SYSINFO_MPEG_AUDIO_1";
    static char si_mpeg2[] = "__SYSINFO_MPEG_AUDIO_2";
    static char si_mpeg3[] = "__SYSINFO_MPEG_AUDIO_3";
    static char si_htmlmode[] = "__SYSINFO_HTML_MODE";
    static char si_links_http[] = "__SYSINFO_LINKS_HTTP";
    static char si_links_ftp[] = "__SYSINFO_LINKS_FTP";
    static char si_links_news[] = "__SYSINFO_LINKS_NEWS";
    static char si_links_mailto[] = "__SYSINFO_LINKS_MAILTO";
    static char si_links_telnet[] = "__SYSINFO_LINKS_TELNET";
    static char si_png_trans[] = "__SYSINFO_PNG_TRANS";
    static char si_png_alpha[] = "__SYSINFO_PNG_ALPHA";
    static char si_ogg[] = "__SYSINFO_OGG";
    static char si_mng[] = "__SYSINFO_MNG";
    static char si_mng_trans[] = "__SYSINFO_MNG_TRANS";
    static char si_mng_alpha[] = "__SYSINFO_MNG_ALPHA";
    static char si_txtcol[] = "__SYSINFO_TEXT_COLORS";
    static char si_txthi[] = "__SYSINFO_TEXT_HILITE";
    static char si_banners[] = "__SYSINFO_BANNERS";
    static char si_interp_class[] = "__SYSINFO_INTERP_CLASS";
    static char si_ic_text[] = "__SYSINFO_ICLASS_TEXT";
    static char si_ic_textgui[] = "__SYSINFO_ICLASS_TEXTGUI";
    static char si_ic_html[] = "__SYSINFO_ICLASS_HTML";
    static char si_audio_fade[] = "__SYSINFO_AUDIO_FADE";
    static char si_audio_crossfade[] = "__SYSINFO_AUDIO_CROSSFADE";
    static char si_af_wav[] = "__SYSINFO_AUDIOFADE_WAV";
    static char si_af_mpeg[] = "__SYSINFO_AUDIOFADE_MPEG";
    static char si_af_ogg[] = "__SYSINFO_AUDIOFADE_OGG";
    static char si_af_midi[] = "__SYSINFO_AUDIOFADE_MIDI";
    time_t     timer;
    struct tm *tblock;
    char      *datetime;
    char       datebuf[15];
    ulong      totsize;
    uchar     *myheap;
    runsdef   *mystack;
    char      *strfile = 0;                  /* name of string capture file */
    char      *charmap = 0;                       /* character mapping file */
    int        checked_undefs = FALSE;     /* checked for undefined symbols */
    
    NOREG((&loadopen))
    NOREG((&linf))

    /* initialize the output formatter */
    out_init();

    /* presume no exit pause */
    ex_pause = FALSE;

    /* initialize lexical analysis context */
    tc = tokcxini(ec, (mcmcxdef *)0, supsctab);
    tc->tokcxdbg = &dbg;
    ec->errcxlgc = errcbcx;
    errcbcx->tcderrfil = 0;                    /* no error capture file yet */
    errcbcx->tcderrtok = tc;   /* to allow error logger to figure file/line */
    errcbcx->tcderrlvl = 0;                 /* presume a warning level of 0 */

    /* add current directory as first entry in include path */
    tokaddinc(tc, "", 0);

    /* parse arguments */
    for (i = 1, argp = argv + 1 ; i < argc ; ++argp, ++i)
    {
        arg = *argp;
        if (*arg == '-')
        {
            switch(*(arg+1))
            {
            case 'D':
                /* get the symbol */
                sym = cmdarg(ec, &argp, &i, argc, 1, tcdusage);

                /* look to see if there's a '=' specifying the expansion */
                for (expan = sym ; *expan && *expan != '=' ; ++expan) ;

                /* provide a default expansion of "1" if none was provided */
                if (*expan == '\0')
                {
                    expan = "1";
                    symlen = strlen(sym);
                }
                else
                {
                    symlen = expan - sym;
                    ++expan;
                }
                explen = strlen(expan);

                /* add the symbol */
                tok_add_define_cvtcase(tc, sym, symlen, expan, explen);
                break;
                
            case 'U':
                /* get the symbol and undefine it */
                sym = cmdarg(ec, &argp, &i, argc, 1, tcdusage);
                symlen = strlen(sym);
                tok_del_define(tc, sym, symlen);
                break;
                
            case 'C':
                c_mode = cmdtog(ec, c_mode, arg, 1, tcdusage);
                break;
                
            case 'c':
                {
                    int case_sensitive;

                    if (strlen(arg+1) >= 4
                        && !memcmp(arg+1, "case", (size_t)4))
                    {
                        case_sensitive =
                            cmdtog(ec, (tc->tokcxflg & TOKCXCASEFOLD) == 0,
                                   arg, 4, tcdusage);
                        if (case_sensitive)
                            tc->tokcxflg &= ~TOKCXCASEFOLD;
                        else
                            tc->tokcxflg |= TOKCXCASEFOLD;
                    }
                    else if (!strcmp(arg+1, "ctab"))
                    {
                        /* get the character mapping table */
                        charmap = cmdarg(ec, &argp, &i, argc, 4, tcdusage);
                    }
                    else
                        tcdusage(ec);
                    break;
                }
                
            case 'e':                                 /* error capture file */
                {
                    char *errfname;
                    errfname = cmdarg(ec, &argp, &i, argc, 1, tcdusage);
                    if (!(errcbcx->tcderrfil = fopen(errfname, "w")))
                        errsig(ec, ERR_ERRFIL);
                    break;
                }
                
            case 'd':
                if (arg[2] == 's')
                {
                    if (arg[3] == '2')
                    {
                        /* new-style - turn on symdeb2 as well as symdeb */
                        symdeb2 = cmdtog(ec, symdeb, arg, 3, tcdusage);
                        symdeb = symdeb2;
                    }
                    else
                    {
                        /* old-style - turn on only symdeb */
                        symdeb = cmdtog(ec, symdeb, arg, 2, tcdusage);
                    }
                }
                else
                    tcdusage(ec);
                break;
                
            case 'm':
                switch(*(arg+2))
                {
                case '?':
                    tcdmusage(ec);
                    
                case 'g':
                    labsiz = atoi(cmdarg(ec, &argp, &i, argc, 2, tcdusage));
                    break;
                    
                case 'p':
                    poolsiz = atoi(cmdarg(ec, &argp, &i, argc, 2, tcdusage));
                    break;
                    
                case 'l':
                    lclsiz = atoi(cmdarg(ec, &argp, &i, argc, 2, tcdusage));
                    break;

                case 's':
                    stksiz = atoi(cmdarg(ec, &argp, &i, argc, 2, tcdusage));
                    break;
                    
                case 'h':
                    heapsiz = atoi(cmdarg(ec, &argp, &i, argc, 2, tcdusage));
                    break;
                    
                default:
                    cachelimit = atol(cmdarg(ec, &argp, &i, argc,
                                             1, tcdusage));
                    break;
                }
                break;
                
            case 'o':
                if (arg[2] == '-')
                    genoutput = FALSE;
                else
                    outfile = cmdarg(ec, &argp, &i, argc, 1, tcdusage);
                break;
                
            case 's':
                stats = cmdtog(ec, stats, arg, 1, tcdusage);
                break;
                
            case 't':
                /* swap file options:  -tf file, -ts size, -t- (no swap) */
                switch(*(arg+2))
                {
                case 'f':
                    swapname = cmdarg(ec, &argp, &i, argc, 2, tcdusage);
                    break;
                    
                case 's':
                    swapsize = atol(cmdarg(ec, &argp, &i, argc, 2, tcdusage));
                    break;
                    
                default:
                    swapena = cmdtog(ec, swapena, arg, 1, tcdusage);
                    break;
                }
                break;
                
            case 'i':
                {
                    char *path = cmdarg(ec, &argp, &i, argc, 1, tcdusage);
                    tokaddinc(tc, path, (int)strlen(path));
                    break;
                }
                
            case 'l':
                binin = cmdarg(ec, &argp, &i, argc, 1, tcdusage);
                break;
                
            case 'p':
                ex_pause = cmdtog(ec, ex_pause, arg, 1, tcdusage);
                break;
                
            case 'w':
                binout = cmdarg(ec, &argp, &i, argc, 1, tcdusage);
                break;

            case 'f':
                switch(arg[2])
                {
                case 'v':                                   /* file version */
                    filever = cmdarg(ec, &argp, &i, argc, 2, tcdusage);
                    if (filever[1] != '\0' ||
                        (*filever != 'a' && *filever != 'b' &&
                         *filever != 'c' && *filever != '*'))
                        tcdusage(ec);
                    break;

                default:
                    tcdusage(ec);
                }
                break;

            case 'F':
                switch(arg[2])
                {
                case 's':
                    strfile = cmdarg(ec, &argp, &i, argc, 2, tcdusage);
                    break;

                default:
                    tcdusage(ec);
                }
                break;

            case '1':
                switch(arg[2])
                {
                case '?':
                    tcd1usage(ec);
                    
                case 'k':
                    v1kw = cmdtog(ec, v1kw, arg, 2, tcdusage);
                    break;
                    
                case 'e':
                    v1else = cmdtog(ec, v1else, arg, 2, tcdusage);
                    break;
                    
                case 'a':
                    argchecking = cmdtog(ec, argchecking, arg, 2, tcdusage);
                    break;
                    
                case 'd':
                    do_kw = cmdarg(ec, &argp, &i, argc, 2, tcdusage);
                    break;
                    
                case '\0':
                    /* no suboption - invert all v1 options */
                    argchecking = !argchecking;
                    v1else = !v1else;
                    v1kw = !v1kw;
                    break;
                    
                default:
                    tcdusage(ec);
                }
                break;

            case 'v':                          /* verbosity (warning) level */
                {
                    char *p;

                    /* get the argument, and see what we have */
                    p = cmdarg(ec, &argp, &i, argc, 1, tcdusage);
                    if (isdigit((uchar)*p))
                    {
                        /* it's a numeric warning verbosity level */
                        warnlevel = atoi(p);
                        errcbcx->tcderrlvl = warnlevel;
                    }
                    else if (*p == '-' || *p == '+')
                    {
                        if (!strcmp(p+1, "abin"))
                            errcbcx->tcdwrn_AMBIGBIN = (*p == '+');
                        else
                            tcdvusage(ec);
                    }
                    else if (*p == '?')
                        tcdvusage(ec);
                    else
                        tcdusage(ec);
                }
                break;
                
            case 'Z':
                switch(arg[2])
                {
                case '?':
                    tcdzusage(ec);
                    
                case 'a':
                    argchecking = cmdtog(ec, argchecking, arg, 2, tcdusage);
                    break;
                    
                default:
                    tcdusage(ec);
                }
                break;
                
            default:
                tcdusage(ec);
            }
        }
        else break;
    }

    /* load the character map */
    if (cmap_load(charmap))
        errsig(ec, ERR_INVCMAP);

    /*
     *   Add built-in #define's for the system ID.  We add a symbol named
     *   TADS_SYSTEM_NAME defined to an appropriate single-quoted string
     *   for the system, and we also add a #define of the same name as the
     *   expansion of TADS_SYSTEM_NAME and give it the value 1, for
     *   testing in #ifdef's.  
     */
    {
        char buf[40];

        sprintf(buf, "'%s'", OS_SYSTEM_NAME);
        tok_add_define_cvtcase(tc, sys_name, (int)sizeof(sys_name) - 1,
                               buf, (int)strlen(buf));
    }
    tok_add_define_cvtcase(tc, OS_SYSTEM_NAME, (int)strlen(OS_SYSTEM_NAME),
                           "1", 1);

    /* add built-in #define's for major and minor version numbers */
    tok_add_define_cvtcase(tc, vsn_major_nm, (int)sizeof(vsn_major_nm) - 1,
                           vsn_major, (int)sizeof(vsn_major) - 1);
    tok_add_define_cvtcase(tc, vsn_minor_nm, (int)sizeof(vsn_minor_nm) - 1,
                           vsn_minor, (int)sizeof(vsn_minor) - 1);

    /* add built-in #define for OEM name */
    {
        char buf[256];

        sprintf(buf, "'%s'", TADS_OEM_NAME);
        tok_add_define_cvtcase(tc, oem_name, (int)sizeof(oem_name) - 1,
                               buf, (int)strlen(buf));
    }

    /* get the current time and date */
    timer = time(NULL);
    tblock = localtime(&timer);
    datetime = asctime(tblock);

    /* build the __DATE__ definition, of the form 'Jan 01 1994' */
    datebuf[0] = '\'';
    memcpy(datebuf + 1, datetime + 4, (size_t)7);
    memcpy(datebuf + 8, datetime + 20, (size_t)4);
    datebuf[12] = '\'';
    tok_add_define_cvtcase(tc, date_name, (int)sizeof(date_name) - 1,
                           datebuf, 13);

    /* build the __TIME__ definition, of the form '01:23:45' */
    memcpy(datebuf + 1, datetime + 11, (size_t)8);
    datebuf[9] = '\'';
    tok_add_define_cvtcase(tc, time_name, (int)sizeof(time_name)-1,
                           datebuf, 10);

    /*
     *   add placeholders for __FILE__ and __LINE__ -- the actual
     *   definitions of these will be filled in dynamically whenever they
     *   are needed 
     */
    tok_add_define_cvtcase(tc, file_name, (int)sizeof(file_name) - 1,
                           inbuf, OSFNMAX);
    tok_add_define_cvtcase(tc, line_name, (int)sizeof(line_name) - 1,
                           inbuf, 40);

    /*
     *   add the SYSINFO codes 
     */
    tok_add_define_num_cvtcase(tc, si_sysinfo, (int)sizeof(si_sysinfo) - 1,
                               SYSINFO_SYSINFO);
    tok_add_define_num_cvtcase(tc, si_version, (int)sizeof(si_version) - 1,
                               SYSINFO_VERSION);
    tok_add_define_num_cvtcase(tc, si_osname, (int)sizeof(si_osname) - 1,
                               SYSINFO_OS_NAME);
    tok_add_define_num_cvtcase(tc, si_html, (int)sizeof(si_html) - 1,
                               SYSINFO_HTML);
    tok_add_define_num_cvtcase(tc, si_jpeg, (int)sizeof(si_jpeg) - 1,
                               SYSINFO_JPEG);
    tok_add_define_num_cvtcase(tc, si_png, (int)sizeof(si_png) - 1,
                               SYSINFO_PNG);
    tok_add_define_num_cvtcase(tc, si_wav, (int)sizeof(si_wav) - 1,
                               SYSINFO_WAV);
    tok_add_define_num_cvtcase(tc, si_midi, (int)sizeof(si_midi) - 1,
                               SYSINFO_MIDI);
    tok_add_define_num_cvtcase(tc, si_wav_midi_ovl,
                               (int)sizeof(si_wav_midi_ovl) - 1,
                               SYSINFO_WAV_MIDI_OVL);
    tok_add_define_num_cvtcase(tc, si_wav_ovl, (int)sizeof(si_wav_ovl) - 1,
                               SYSINFO_WAV_OVL);
    tok_add_define_num_cvtcase(tc, si_pref_images,
                               (int)sizeof(si_pref_images) - 1,
                               SYSINFO_PREF_IMAGES);
    tok_add_define_num_cvtcase(tc, si_pref_sounds,
                               (int)sizeof(si_pref_sounds) - 1,
                               SYSINFO_PREF_SOUNDS);
    tok_add_define_num_cvtcase(tc, si_pref_music,
                               (int)sizeof(si_pref_music) - 1,
                               SYSINFO_PREF_MUSIC);
    tok_add_define_num_cvtcase(tc, si_pref_links,
                               (int)sizeof(si_pref_links) - 1,
                               SYSINFO_PREF_LINKS);
    tok_add_define_num_cvtcase(tc, si_mpeg, (int)sizeof(si_mpeg) - 1,
                               SYSINFO_MPEG);
    tok_add_define_num_cvtcase(tc, si_mpeg1, (int)sizeof(si_mpeg1) - 1,
                               SYSINFO_MPEG1);
    tok_add_define_num_cvtcase(tc, si_mpeg2, (int)sizeof(si_mpeg2) - 1,
                               SYSINFO_MPEG2);
    tok_add_define_num_cvtcase(tc, si_mpeg3, (int)sizeof(si_mpeg3) - 1,
                               SYSINFO_MPEG3);
    tok_add_define_num_cvtcase(tc, si_htmlmode, (int)sizeof(si_htmlmode) - 1,
                               SYSINFO_HTML_MODE);
    tok_add_define_num_cvtcase(tc, si_links_http,
                               (int)sizeof(si_links_http) - 1,
                               SYSINFO_LINKS_HTTP);
    tok_add_define_num_cvtcase(tc, si_links_ftp,
                               (int)sizeof(si_links_ftp) - 1,
                               SYSINFO_LINKS_FTP);
    tok_add_define_num_cvtcase(tc, si_links_news,
                               (int)sizeof(si_links_news) - 1,
                               SYSINFO_LINKS_NEWS);
    tok_add_define_num_cvtcase(tc, si_links_mailto,
                               (int)sizeof(si_links_mailto) - 1,
                               SYSINFO_LINKS_MAILTO);
    tok_add_define_num_cvtcase(tc, si_links_telnet,
                               (int)sizeof(si_links_telnet) - 1,
                               SYSINFO_LINKS_TELNET);
    tok_add_define_num_cvtcase(tc, si_png_trans,
                               (int)sizeof(si_png_trans) - 1,
                               SYSINFO_PNG_TRANS);
    tok_add_define_num_cvtcase(tc, si_png_alpha,
                               (int)sizeof(si_png_alpha) - 1,
                               SYSINFO_PNG_ALPHA);
    tok_add_define_num_cvtcase(tc, si_ogg, (int)sizeof(si_ogg) - 1,
                               SYSINFO_OGG);
    tok_add_define_num_cvtcase(tc, si_mng, (int)sizeof(si_mng) - 1,
                               SYSINFO_MNG);
    tok_add_define_num_cvtcase(tc, si_mng_trans,
                               (int)sizeof(si_mng_trans) - 1,
                               SYSINFO_MNG_TRANS);
    tok_add_define_num_cvtcase(tc, si_mng_alpha,
                               (int)sizeof(si_mng_alpha) - 1,
                               SYSINFO_MNG_ALPHA);
    tok_add_define_num_cvtcase(tc, si_txthi, (int)sizeof(si_txthi) - 1,
                               SYSINFO_TEXT_HILITE);
    tok_add_define_num_cvtcase(tc, si_txtcol, (int)sizeof(si_txtcol) - 1,
                               SYSINFO_TEXT_COLORS);
    tok_add_define_num_cvtcase(tc, si_banners, (int)sizeof(si_banners) - 1,
                               SYSINFO_BANNERS);
    tok_add_define_num_cvtcase(tc, si_interp_class,
                               (int)sizeof(si_interp_class) - 1,
                               SYSINFO_INTERP_CLASS);
    tok_add_define_num_cvtcase(tc, si_ic_text,
                               (int)sizeof(si_ic_text) - 1,
                               SYSINFO_ICLASS_TEXT);
    tok_add_define_num_cvtcase(tc, si_ic_textgui,
                               (int)sizeof(si_ic_textgui) - 1,
                               SYSINFO_ICLASS_TEXTGUI);
    tok_add_define_num_cvtcase(tc, si_ic_html,
                               (int)sizeof(si_ic_html) - 1,
                               SYSINFO_ICLASS_HTML);
    tok_add_define_num_cvtcase(tc, si_audio_fade,
                               (int)sizeof(si_audio_fade) - 1,
                               SYSINFO_AUDIO_FADE);
    tok_add_define_num_cvtcase(tc, si_audio_crossfade,
                               (int)sizeof(si_audio_crossfade) - 1,
                               SYSINFO_AUDIO_CROSSFADE);
    tok_add_define_num_cvtcase(tc, si_af_wav, (int)sizeof(si_af_wav) - 1,
                               SYSINFO_AUDIOFADE_WAV);
    tok_add_define_num_cvtcase(tc, si_af_mpeg, (int)sizeof(si_af_mpeg) - 1,
                               SYSINFO_AUDIOFADE_MPEG);
    tok_add_define_num_cvtcase(tc, si_af_ogg, (int)sizeof(si_af_ogg) - 1,
                               SYSINFO_AUDIOFADE_OGG);
    tok_add_define_num_cvtcase(tc, si_af_midi, (int)sizeof(si_af_midi) - 1,
                               SYSINFO_AUDIOFADE_MIDI);

    /* turn on the __DEBUG symbol if debugging is on */
    if (symdeb)
        tok_add_define_cvtcase(tc, dbg_name,
                               (int)(sizeof(dbg_name) - 1), "1", 1);

    /* get input name argument, and make sure it's the last argument */
    if (i == argc) tcdusage(ec);
    infile = *argp;
    if (i + 1 != argc) tcdusage(ec);
    
    /* add default .T extension to input file */
    strcpy(inbuf, infile);
    os_defext(inbuf, "t");

    /*
     *   if no output file is specified, use input file minus extension
     *   plus GAM extension; otherwise, use specified output file 
     */
    if (outfile)
    {
        /* use their name exactly as-is */
        strcpy(outbuf, outfile);

        /* if it's exactly the same as the input file, apply an extension */
        if (!strcmp(outfile, infile))
        {
            os_remext(outbuf);
            os_addext(outbuf, "gam");
        }
    }
    else
    {
        /* get the input name minus the current extension */
        strcpy(outbuf, infile);
        os_remext(outbuf);

        /* add .GAM extension */
        os_addext(outbuf, "gam");
    }
    
    /* set input/output file pointers to refer to generated filenames */
    infile = inbuf;
    outfile = outbuf;

    /* echo information to error logging file if present */
    if (errcbcx->tcderrfil)
    {
        time_t t;

        t = time(NULL);
        fprintf(errcbcx->tcderrfil, "TADS Compilation of %s\n%s\n",
                infile, ctime(&t));
    }
    
    /* open up the swap file */
    if (swapena && swapsize)
    {
        swapfp = os_create_tempfile(swapname, swapbuf);
        if (swapname == 0) swapname = swapbuf;
        if (swapfp == 0) errsig(ec, ERR_OPSWAP);
    }

    ERRBEGIN(ec)

    /* initialize cache manager context */
    globalctx = mcmini(cachelimit, 128, swapsize, swapfp, 0, ec);
    mctx = mcmcini(globalctx, 128, fioldobj, &fiolctx,
                   (void (*)(void *, mcmon))objrevert, (void *)0);
    mctx->mcmcxrvc = mctx;

    tc->tokcxmem = mctx;

    /* allocate and initialize parsing context */

    totsize = sizeof(prscxdef) + (long)poolsiz + (long)lclsiz;
    if (totsize != (size_t)totsize)
        errsig1(ec, ERR_PRSCXSIZ, ERRTINT, (int)(65535 - sizeof(prscxdef)));
    pctx = (prscxdef *)mchalo(ec, (size_t)totsize, "tcdmain");
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
    pctx->prscxgtab = (toktdef *)&labtab;
    pctx->prscxvoc = &vocctx;
    pctx->prscxcpp = (char *)0;
    pctx->prscxcps = 0;
    pctx->prscxcpf = 0;
    pctx->prscxspp = (char *)0;
    pctx->prscxspf = 0;
    pctx->prscxsps = 0;
    pctx->prscxfsp = (uchar *)0;
    pctx->prscxfsf = 0;
    pctx->prscxfss = 0;
    pctx->prscxextc = 0;

    /* open the strings file, if one was specified */
    if (strfile != 0)
    {
        /* open the file */
        pctx->prscxstrfile = osfopwt(strfile, OSFTTEXT);

        /* complain if we couldn't open it */
        if (pctx->prscxstrfile == 0)
            errsig(ec, ERR_OPNSTRFIL);
    }
    else
        pctx->prscxstrfile = 0;
    
    /* set up flags */
    if (argchecking) pctx->prscxflg |= PRSCXFARC;
    if (symdeb) pctx->prscxflg |= (PRSCXFLIN | PRSCXFLCL);
    if (symdeb2) pctx->prscxflg |= (PRSCXFLIN2 | PRSCXFLCL);
    if (v1else) pctx->prscxflg |= PRSCXFV1E;
    if (c_mode) tc->tokcxflg |= TOKCXFCMODE;
    if (symdeb2) tc->tokcxflg |= TOKCXFLIN2;
    if (*filever != '*' && *filever < 'c') pctx->prscxflg |= PRSCXFTPL1;
    
    /* set up vocabulary context */
    vocini(&vocctx, ec, mctx, &runctx, (objucxdef *)0, 50, 50, 100);
    vocctx.voccxflg |= VOCCXFVWARN;

    /* allocate stack and heap */
    totsize = (ulong)stksiz * (ulong)sizeof(runsdef);
    if (totsize != (size_t)totsize)
        errsig1(ec, ERR_STKSIZE, ERRTINT, (uint)(65535/sizeof(runsdef)));
    mystack = (runsdef *)mchalo(ec, (size_t)totsize, "runtime stack");
    myheap = mchalo(ec, (size_t)heapsiz, "runtime heap");

    /* set up linear symbol table for 'goto' labels */
    labmem = mchalo(ec, (size_t)labsiz, "main1");
    toktlini(ec, &labtab, labmem, labsiz);
    labtab.toktlsc.toktnxt = (toktdef *)&symtab;       /* 2nd to last table */

    /* set up to read from input file */
    tc->tokcxsst = (ushort (*)(void *))prsxsst;
    tc->tokcxsad = (void (*)(void *, char *, ushort))prsxsad;
    tc->tokcxsend = (void (*)(void *))prsxsend;
    tc->tokcxscx = (void *)pctx;

    /* set up debug context */
    dbg.dbgcxtio = (tiocxdef *)0;
    dbg.dbgcxtab = &symtab;
    dbg.dbgcxmem = mctx;
    dbg.dbgcxerr = ec;
    dbg.dbgcxfcn = 0;
    dbg.dbgcxdep = 0;
    dbg.dbgcxfid = 0;                  /* start file serial numbers at zero */
    dbg.dbgcxflg = 0;
    
    vocctx.voccxrun = &runctx;
    runctx.runcxdbg = &dbg;
    dbg.dbgcxtab = &symtab;

    /* initialize a file line source for the input file */
    if (!(linf = linfini(mctx, ec, infile, (int)strlen(infile),
                         (tokpdef *)0, TRUE, symdeb2)))
        errsig(ec, ERR_OPNINP);

    linf->linflin.linpar = (lindef *)0;
    dbg.dbgcxlin = &linf->linflin;    /* source file is start of line chain */
    tc->tokcxhdr = (linfdef *)&linf->linflin;

    /* set up debug line source chain with just this record */
    tc->tokcxlin = &linf->linflin;
    linf->linflin.linid = dbg.dbgcxfid++;
    linf->linflin.linnxt = (lindef *)0;
    
    /* set up a hashed symbol table */
    tokthini(ec, mctx, (toktdef *)&symtab);
    pctx->prscxstab = (toktdef *)&symtab;               /* add symbols here */
/*was:  tc->tokcxstab = (toktdef *)&labtab; */
    tc->tokcxstab = (toktdef *)&symtab;          /* search for symbols here */
    
    /* allocate a code generator context */
    ectx = (emtcxdef *)mchalo(ec,
                              (sizeof(emtcxdef) + 511*sizeof(emtldef)),
                              "tcdmain");
    ectx->emtcxerr = ec;
    ectx->emtcxmem = pctx->prscxmem;
    ectx->emtcxptr = 0;   /* mcmalo(pctx->prscxmem, 1024, &ectx->emtcxobj); */
    ectx->emtcxlcnt = 512;
    ectx->emtcxfrob = MCMONINV;          /* not using debugger frame locals */
    emtlini(ectx);

    /* add code generator context to parser context */
    pctx->prscxemt = ectx;
    
    /* get the absolute path for the .gam file */
    os_get_abs_filename(outfile_abs, sizeof(outfile_abs), outfile);
    os_get_path_name(outfile_path, sizeof(outfile_path), outfile_abs);

    /* set up execution context */
    runctx.runcxerr = ec;
    runctx.runcxmem = pctx->prscxmem;
    runctx.runcxstk = mystack;
    runctx.runcxstop = &mystack[stksiz];
    runctx.runcxsp = mystack;
    runctx.runcxbp = mystack;
    runctx.runcxheap = myheap;
    runctx.runcxhp = myheap;
    runctx.runcxhtop = &myheap[heapsiz];
    runctx.runcxundo = (objucxdef *)0;
    runctx.runcxbcx = &bifctx;
    runctx.runcxbi = bif;
    runctx.runcxtio = (tiocxdef *)0;
    runctx.runcxdbg = &dbg;
    runctx.runcxvoc = &vocctx;
    runctx.runcxdmd = (void (*)(void *, objnum, prpnum))supcont;
    runctx.runcxdmc = &supctx;
    runctx.runcxgamename = outfile;
    runctx.runcxgamepath = outfile_path;
    
    /* set up setup context */
    supctx.supcxerr = ec;
    supctx.supcxmem = mctx;
    supctx.supcxtab = &symtab;
    supctx.supcxbuf = (uchar *)0;
    supctx.supcxlen = 0;
    supctx.supcxvoc = &vocctx;
    supctx.supcxrun = &runctx;
    
    /* set up built-in function context */
    CLRSTRUCT(bifctx);
    bifctx.bifcxerr = ec;
    bifctx.bifcxrun = &runctx;
    bifctx.bifcxtio = (tiocxdef *)0;
    bifctx.bifcxsavext = save_ext;
    
    /* allocate object for expression code generation */
    mcmalo(pctx->prscxmem, (ushort)1024, &evalobj);
    mcmunlck(pctx->prscxmem, evalobj);
    
    /* add vocabulary properties */
    pctx->prscxprp = PRP_LASTRSV + 1;
    
    /* add the built-in functions, keywords, etc */
    suprsrv(&supctx, bif, (toktdef *)&symtab,
            (int)(sizeof(bif)/sizeof(bif[0])), v1kw, do_kw,
            tc->tokcxflg & TOKCXCASEFOLD);

    /* load pre-compiled header file if one was specified */
    if (binin)
    {
        dbg.dbgcxfid--;                        /* reset the line id counter */

        fiord(mctx, &vocctx, tc, binin, (char *)0, &fiolctx, &preinit,
              &inflags, tc->tokcxinc,
              &pctx->prscxfsp, &pctx->prscxfsf, &pctx->prscxprp, FALSE, 0,
              argv[0]);
        tc->tokcxhdr = (linfdef *)dbg.dbgcxlin;
        pctx->prscxfss = pctx->prscxfsf;
        pctx->prscxcpp = vocctx.voccxcpp;
        pctx->prscxcps =
        pctx->prscxcpf = vocctx.voccxcpl;
        pctx->prscxspp = vocctx.voccxspp;
        pctx->prscxsps =
        pctx->prscxspf = vocctx.voccxspl;
        loadopen = TRUE;
        
        linf->linflin.linid = dbg.dbgcxfid++; /* get new line id for source */

        /* use same case sensitivity as original compilation */
        if (inflags & FIOFCASE)
            tc->tokcxflg |= TOKCXCASEFOLD;
        else
            tc->tokcxflg &= ~TOKCXCASEFOLD;

        /*
         *   run through the arguments again, and remove any preprocessor
         *   symbols loaded from the file that have been explicitly
         *   undefined with -U in this run 
         */
        for (i = 1, argp = argv + 1 ; i < argc ; ++argp, ++i)
        {
            arg = *argp;
            if (*arg == '-')
            {
                switch(*(arg+1))
                {
                case 'U':
                    /* get the symbol and undefine it */
                    sym = cmdarg(ec, &argp, &i, argc, 1, tcdusage);
                    symlen = strlen(sym);
                    tok_del_define(tc, sym, symlen);
                    break;

                default:
                    /* ignore all other arguments on this pass */
                    break;
                }
            }
        }
                
    }

    /* get the first token */
    prsrstn(pctx);
    toknext(tc);

    /* parse the entire file, stopping when we encounter EOF */
parse_loop:
    ERRBEGIN(ec)
    for ( ;; )
    {
        /* reset emit settings */
        ectx->emtcxofs = 0;

        /* generate progress report */
        os_progress(((linfdef *)tc->tokcxlin)->linfnam,
                    ((linfdef *)tc->tokcxlin)->linfnum);

        /* cooperatively multitask if necessary, and check for interrupt */
        if (os_yield())
            errsig(ec, ERR_USRINT);

        /* stop on a fatal error */
        if (errcbcx->tcdfatal)
            break;

        /* stop on end of file */
        if (tc->tokcxcur.toktyp == TOKTEOF)
            break;

        /* parse the next object or function definition */
        prscode(pctx, TRUE);
    }
    ERRCATCH(ec, err)
        if (err >= 100 && err < 450)
        {
            int brace_cnt;
            int paren_cnt;
            
            /*
             *   This is some kind of syntax error, which means we can
             *   keep going.  Log the error, throw away tokens up to the
             *   next semicolon, then try to start again.  
             */
            errclog(ec);
            for (brace_cnt = paren_cnt = 0 ;; )
            {
                switch(tc->tokcxcur.toktyp)
                {
                case TOKTSEM:
                    /* if within braces, keep going */
                    if (brace_cnt || paren_cnt)
                        break;

                    /* FALLTHROUGH */
                case TOKTEOF:
                    toknext(tc);
                    goto parse_loop;

                case TOKTLBRACE:
                    /* count the brace, and clear the paren count */
                    ++brace_cnt;
                    paren_cnt = 0;
                    break;

                case TOKTRBRACE:
                    /* uncount the brace, and clear the paren count */
                    paren_cnt = 0;
                    if (brace_cnt)
                        --brace_cnt;
                    break;

                case TOKTLPAR:
                    ++paren_cnt;
                    break;

                case TOKTRPAR:
                    --paren_cnt;
                    break;
                }

                /* skip the token */
                toknext(tc);
            }
        }
        
        /* can't deal with this error - resignal it */
        errrse(ec);
    ERREND(ec)
            
    /* set up vocab context with format string information */
    vocctx.voccxcpp = pctx->prscxcpp;
    vocctx.voccxcpl = pctx->prscxcpf;
        
    /* likewise the special word information */
    vocctx.voccxspp = pctx->prscxspp;
    vocctx.voccxspl = pctx->prscxspf;

    if (!binout && !errcbcx->tcdfatal)
    {
        tcdchkctx cbctx;

        /* apply TADS/Graphic external resources */
        tcgcomp(ec, pctx, infile);
    
        /* tell output subsystem about format strings */
        tiosetfmt((tiocxdef *)0, &runctx, pctx->prscxfsp,
                  (uint)pctx->prscxfsf);
    
        /* get required object definitions if not precompiling headers */
        supfind(ec, &symtab, &vocctx, &preinit, warnlevel,
                tc->tokcxflg & TOKCXCASEFOLD);

        /* 
         *   inherit vocabulary - do this before calling preinit, to ensure
         *   that preinit sees the same inherited location and vocabulary
         *   data that would be seen during normal execution 
         */
        supivoc(&supctx);

        /* check for undefined objects before running preinit */
        cbctx.mem = mctx;
        cbctx.ec = ec;
        cbctx.cnt = 0;
        toktheach(&symtab.tokthsc, tcdchkundef, &cbctx);
        checked_undefs = TRUE;

        /* 
         *   run preinit function if it's defined, and we're not generating
         *   debug output, and there have been no errors so far
         */
        if (preinit != MCMONINV && !symdeb
            && errcbcx->tcderrcnt == 0 && errcbcx->tcdfatal == 0)
            runfn(&runctx, preinit, 0);
    }
        
    if (binout && errcbcx->tcderrcnt == 0)
    {
        fiowrt(mctx, &vocctx, tc, &symtab, pctx->prscxfsp,
               (uint)pctx->prscxfsf,
               binout, FIOFSYM + FIOFLIN + FIOFPRE + FIOFBIN
                       + (tc->tokcxflg & TOKCXCASEFOLD ? FIOFCASE : 0),
               (objnum)MCMONINV, pctx->prscxextc, pctx->prscxprp, filever);
    }

    /* write game to output if desired (and not precompiling headers) */
    if (genoutput && !binout
        && errcbcx->tcderrcnt == 0 && errcbcx->tcdfatal == 0)
    {
        fiowrt(mctx, &vocctx, tc,
               &symtab, pctx->prscxfsp, (uint)pctx->prscxfsf,
               outfile,
               (FIOFFAST
                + (symdeb ? (FIOFSYM + FIOFLIN + FIOFPRE
                             + ((tc->tokcxflg & TOKCXCASEFOLD)
                                ? FIOFCASE : 0)
                            )
                          : FIOFCRYPT
                  )
                + (symdeb2 ? FIOFLIN2 : 0)
               ),
               preinit, pctx->prscxextc, (uint)0, filever);
    }
    else if (!binout && !checked_undefs)
    {
        tcdchkctx cbctx;
        
        /* we're not writing the file, but still check for undefined objects */
        cbctx.mem = mctx;
        cbctx.ec = ec;
        cbctx.cnt = 0;
        toktheach(&symtab.tokthsc, tcdchkundef, &cbctx);
    }

    /* show statistics if desired */
    if (stats)
    {
        int        count;
        int        i;
        int        j;
        vocdef   **vhsh;
        vocdef    *voc;
        vocidef ***vpg;
        vocidef  **v;
        
        IF_DEBUG(extern ulong mchtotmem;)
            
        tcdptf("\n* * * Statistics * * *\n");
        tcdptf("Virtual Object Cache size:  %lu\n", mcmcsiz(mctx));
        tcdptf("Global symbol table size:   %lu\n",
               ((ulong)symtab.tokthpcnt * (ulong)TOKTHSIZE));
        IF_DEBUG(tcdptf("Total heap memory:          %lu\n", mchtotmem);)
            
        /* count vocabulary words, eliminating duplicates */
        count = 0;
        for (i = 0, vhsh = vocctx.voccxhsh ; i < VOCHASHSIZ ; ++i, ++vhsh)
        {
            for (voc = *vhsh ; voc ; voc = voc->vocnxt)
            {
                /* check for duplicates; don't count if found one */
                ++count;
#if 0
/* no need to check for duplicates with new vocwdef system */
                {
                    vocdef    *voc2;
                    for (voc2 = voc->vocnxt ; voc2 ; voc2 = voc2->vocnxt)
                    {
                        if (voc->voclen == voc2->voclen
                            && voc->vocln2 == voc2->vocln2
                            && !memcmp(voc->voctxt, voc2->voctxt,
                                       (size_t)(voc->voclen + voc->vocln2)))
                        {
                            --count;
                            break;
                        }
                    }
                }
#endif /* NEVER */

            }
        }
        tcdptf("Vocabulary words:           %d\n", count);
        
        /* count objects, using inheritance records */
        count = 0;
        for (vpg = vocctx.voccxinh, i = 0 ; i < VOCINHMAX ; ++vpg, ++i)
        {
            if (!*vpg) continue;
            for (v = *vpg, j = 0 ; j < 256 ; ++v, ++j)
            {
                if (*v) ++count;
            }
        }
        tcdptf("Objects:                    %d\n", count);
    }

    /* close and delete swapfile, if one was opened */
    if (swapfp)
    {
        osfcls(swapfp);
        swapfp = (osfildef *)0;
        osfdel_temp(swapname);
    }
    
    /* close load file if one was open */
    if (loadopen)
        fiorcls(&fiolctx);

    /* close error echo file */
    if (errcbcx->tcderrfil)
    {
        fclose(errcbcx->tcderrfil);
        errcbcx->tcderrfil = 0;
    }

    /* close the string file */
    if (pctx->prscxstrfile != 0)
    {
        osfcls(pctx->prscxstrfile);
        pctx->prscxstrfile = 0;
    }

    /* close the input line source */
    if (linf != 0)
        linfcls(&linf->linflin);

    ERRCATCH(ec, err)
        /* if out of memory, describe current cache condition */
        if (err == ERR_NOMEM)
        {
            tcdptf("*** Note for -m option:\n");
            tcdptf("*** Current cache size is %lu\n", mcmcsiz(mctx));
        }
        
        /* close and delete swapfile, if one was opened */
        if (swapfp)
        {
            osfcls(swapfp);
            swapfp = (osfildef *)0;
            osfdel_temp(swapname);
        }
        
        /* close load file if one was opened */
        if (loadopen)
            fiorcls(&fiolctx);

        /* close the input line source */
        if (linf != 0)
            linfcls(&linf->linflin);

        /* resignal the error */
        errrse(ec);
    ERREND(ec)
}

/* log an error */
static void tcdlogerr(void *ectx0, char *fac, int err, int argc,
                      erradef *argv)
{
    tcderrdef *ectx = (tcderrdef *)ectx0;
    char       buf[256];
    char       msg[256];
    tokcxdef  *ctx = ectx->tcderrtok;
    FILE      *fp;

    /* ignore certain errors if the warning level is low */
    switch(err)
    {
    case ERR_INCRPT:
        if (ectx->tcderrlvl < 1) return;
        break;
    }

    if (!ctx)
    {
        tcdptf("%s-%d: error message not available\n", buf, fac, err);
        return;
    }

    /* figure out what kind of error we have, and increment the counter */
    switch(err)
    {
    case ERR_AMBIGBIN:
        /* check for suppression, and ignore the error if suppressed */
        if (!ectx->tcdwrn_AMBIGBIN) return;
        goto do_warning;
        
    case ERR_TRUNC:
    case ERR_INCRPT:
    case ERR_WEQASI:
    case ERR_STREND:
    case ERR_RPLSPEC:
    case ERR_VOCREVB:
    case ERR_LOCNOBJ:
    case ERR_CNTNLST:
    case ERR_WRNONF:
    case ERR_GNOFIL:
    case ERR_PUNDEF:
    case ERR_PIA:
    do_warning:
        /* warning - increment warning count */
        ++(ectx->tcdwrncnt);
        break;

    case ERR_NOMEM:
    case ERR_FSEEK:
    case ERR_FREAD:
    case ERR_FWRITE:
    case ERR_NOPAGE:
    case ERR_SWAPBIG:
    case ERR_SWAPPG:
    case ERR_CLIFULL:
    case ERR_NOMEM1:
    case ERR_NOMEM2:
    case ERR_NOLCLSY:
    case ERR_MANYSYM:
    case ERR_NONODE:
    case ERR_VOCSTK:
    case ERR_MANYDBG:
    case ERR_VOCMNPG:
    case ERR_NOMEMLC:
    case ERR_NOMEMAR:
        /* flag the fatal error, and increment the error count */
        ectx->tcdfatal = TRUE;
        ++(ectx->tcderrcnt);
        break;

    default:
        /* error - increment error count */
        ++(ectx->tcderrcnt);
        break;
    }

    /*
     *   certain special errors include the line position in the argument
     *   vector itself 
     */
    switch(err)
    {
    case ERR_UNDFOBJ:
    case ERR_UNDEFO:
    case ERR_UNDEFF:
        /* these errors include the position as the second argument */
        strcpy(buf, argv[1].errastr);
        break;

    default:
        /* for other errors, get the location from the line source */
        if (ctx->tokcxlin != 0)
            linppos(ctx->tokcxlin, buf, (uint)sizeof(buf));
        else
            buf[0] = '\0';
    }

    fp = ectx->tcderrfil;
    
    tcdptf("%serror %s-%d: ", buf, fac, err);
    if (fp) fprintf(fp, "%serror %s-%d: ", buf, fac, err);
    
    errmsg(ctx->tokcxerr, msg, (uint)sizeof(msg), err);
    errfmt(buf, (int)sizeof(buf), msg, argc, argv);
    tcdptf("%s\n", buf);
    if (fp) fprintf(fp, "%s\n", buf);
    
#ifdef OS_ERRLINE
    if ((err >= ERR_INVTOK && err < ERR_VOCINUS)
        || err == ERR_UNDFOBJ || err == ERR_UNDEFO || err == ERR_UNDEFF)
    {
        char *p;
        int   len;

        switch(err)
        {
        case ERR_UNDFOBJ:
        case ERR_UNDEFO:
        case ERR_UNDEFF:
            p = argv[1].errastr;
            p += strlen(p) + 1;
            len = strlen(p);
            break;
            
        default:
            p = ctx->tokcxlin->linbuf;
            len = ctx->tokcxlin->linlen;
            break;
        }

        if (len + 1 > sizeof(buf)) len = sizeof(buf) - 1;
        memcpy(buf, p, (size_t)len);
        buf[len] = '\0';
        tcdptf("%s\n\n", buf);
        if (fp) fprintf(fp, "%s\n\n", buf);
    }
#endif /* OS_ERRLINE */
}

/* pause before exiting, if desired */
static void maybe_pause(void)
{
    /* pause if desired */
    if (ex_pause)
    {
        tcdptf("[done with compilation - press a key to continue]");
        os_waitc();
        tcdptf("\n");
    }
}

/* main - called by os main after setting up arguments */
int tcdmain(int argc, char **argv, char *save_ext)
{
    errcxdef   errctx;
    int        err;
    osfildef  *fp;
    extern     char tcgname[];
    tcderrdef  errcbcx;                           /* error callback context */
    char       vsnbuf[128];

    /* initialize the error structure */
    CLRSTRUCT(errcbcx);
    errcbcx.tcdwrn_AMBIGBIN = TRUE;                   /* -v+abin by default */

    errctx.errcxlog = tcdlogerr;
    errctx.errcxlgc = (void *)0;
    errctx.errcxofs = 0;
    errctx.errcxfp  = (osfildef *)0;
    errctx.errcxappctx = 0;
    fp = oserrop(argv[0]);
    errini(&errctx, fp);

    /* copyright-date-string */
    sprintf(vsnbuf, "%s v%s.%s.%s  %s\n", tcgname,
            vsn_major, vsn_minor, vsn_maint,
            "Copyright (c) 1993, 2012 Michael J. Roberts");
    tcdptf(vsnbuf);
    sprintf(vsnbuf, "TADS for %s [%s] patchlevel %s.%s\n",
            OS_SYSTEM_LDESC, OS_SYSTEM_NAME,
            TADS_OEM_VERSION, OS_SYSTEM_PATCHSUBLVL);
    tcdptf(vsnbuf);
    tcdptf("%s maintains this port.\n", TADS_OEM_NAME);
    
    ERRBEGIN(&errctx)
        tcdmain1(&errctx, argc, argv, &errcbcx, save_ext);
    ERRCATCH(&errctx, err)
        if (err != ERR_USAGE)
            errclog(&errctx);
    
        if (errctx.errcxfp) osfcls(errctx.errcxfp);
        if (errcbcx.tcderrfil) fclose(errcbcx.tcderrfil);
        /* os_expause(); */
        maybe_pause();
        return(OSEXFAIL);
    ERREND(&errctx)

    /* close message file */
    if (errctx.errcxfp) osfcls(errctx.errcxfp);

    /* close error echo file if any */
    if (errcbcx.tcderrfil) fclose(errcbcx.tcderrfil);

    /* os_expause(); */
    maybe_pause();
    return(errcbcx.tcderrcnt == 0 ? OSEXSUCC : OSEXFAIL);
}

