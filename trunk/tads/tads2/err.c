#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/ERR.C,v 1.2 1999/05/17 02:52:11 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  err.c - error handling functions
Function
  Functions for error signalling and recovery
Notes
  None
Modified
  10/02/91 MJRoberts     - creation
*/

#include <string.h>
#include "os.h"
#include "err.h"
#include "tok.h"

/* format an error message, sprintf-style, using an erradef array */
int errfmt(outbuf, outbufl, fmt, argc, argv)
char    *outbuf;
int      outbufl;
char    *fmt;
int      argc;
erradef *argv;
{
    int    outlen = 0;
    int    argi   = 0;
    int    len;
    char   buf[20];
    char  *p;
    char   fmtchar;

    while (*fmt)
    {
        switch(*fmt)
        {
        case '\\':
            ++fmt;
            len = 1;
            switch(*fmt)
            {
            case '\0':
                --fmt;
                break;
            case '\n':
                p = "\n";
                break;
            case '\t':
                p = "\t";
                break;
            default:
                p = fmt;
                break;
            }
            break;
            
        case '%':
            ++fmt;
            fmtchar = *fmt;
            if (argi >= argc) fmtchar = 1;          /* too many - ignore it */
            switch(fmtchar)
            {
            case '\0':
                --fmt;
                break;
                
            case '%':
                p = "%";
                len = 1;
                break;
                
            case 'd':
                sprintf(buf, "%d", argv[argi].erraint);
                len = strlen(buf);
                p = buf;
                break;
                
            case 's':
                p = argv[argi].errastr;
                len = strlen(p);
                break;
                
            case 't':
                {
                    int i;
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
                        if (toklist[i].tokid == argv[argi].erraint)
                        {
                            p = toklist[i].toknam;
                            break;
                        }
                    }
                    if (!toklist[i].toknam)
                        p = "<unknown token>";
                    len = strlen(p);
                    break;
                }
                
            default:
                p = "";
                len = 0;
                --argi;
                break;
            }
            ++argi;
            break;
            
        default:
            p = fmt;
            len = 1;
            break;
        }

        /* copy output that was set up above */
        if (len)
        {
            if (outbufl >= len)
            {
                memcpy(outbuf, p, (size_t)len);
                outbufl -= len;
                outbuf += len;
            }
            else if (outbufl)
            {
                memcpy(outbuf, p, (size_t)outbufl);
                outbufl = 0;
            }
            outlen += len;
        }
        ++fmt;
    }
    
    if (outbufl) *outbuf++ = '\0';
    return(outlen);
}

#ifdef DEBUG
void errjmp(buf, e)
jmp_buf buf;
int     e;
{
    longjmp(buf, e);
}
#endif /* DEBUG */



#ifdef ERR_NO_MACRO

/* base error signal function */
void errsign(ctx, e)
errcxdef *ctx;
int       e;
{
    ctx->errcxofs = 0;
    longjmp(ctx->errcxptr->errbuf, e);
}

/* signal an error with no arguments */
void errsig(ctx, e)
errcxdef *ctx;
int       e;
{
    errargc(ctx, 0);
    errsign(ctx, e);
}

/* enter a string argument */
char *errstr(ctx, str, len)
errcxdef *ctx;
char     *str;
int       len;
{
    char *ret = &ctx->errcxbuf[ctx->errcxofs];
    
    memcpy(ret, str, (size_t)len);
    ret[len] = '\0';
    ctx->errcxofs += len + 1;
    return(ret);
}

/* resignal current error */
void errrse1(ctx, fr)
errcxdef *ctx;
errdef   *fr;
{
    errargc(ctx, fr->erraac);
    memcpy(ctx->errcxptr->erraav, fr->erraav,
           (size_t)(fr->erraac * sizeof(erradef)));
    errsign(ctx, fr->errcode);
}

/* log an error: base function */
void errlogn(ctx, err)
errcxdef *ctx;
int       err;
{
    ctx->errcxofs = 0;
    (*ctx->errcxlog)(ctx->errcxlgc, err, ctx->errcxptr->erraac,
                     ctx->errcxptr->erraav);
}

/* log an error with no arguments */
void errlog(ctx, err)
errcxdef *ctx;
int       err;
{
    errargc(ctx, 0);
    errlogn(ctx, err);
}


#endif /* ERR_NO_MACRO */

