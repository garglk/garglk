#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/LER.C,v 1.3 1999/07/11 00:46:29 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  ler.c - library error handling functions
Function
  Functions for error signalling and recovery
Notes
  None
Modified
  12/30/92 MJRoberts     - created from TADS err.c
  10/02/91 MJRoberts     - creation
*/

#include <string.h>
#include "os.h"
#include "ler.h"

/* format an error message, sprintf-style, using an erradef array */
int errfmt(char *outbuf, int outbufl, char *fmt, int argc, erradef *argv)
{
    int    outlen = 0;
    int    argi   = 0;
    int    len;
    char   buf[20];
    char  *p;
    char   fmtchar;

    while (*fmt != '\0' && outbufl > 1)
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
                len = 0;
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
                
            case 'u':
                sprintf(buf, "%u", argv[argi].erraint);
                len = strlen(buf);
                p = buf;
                break;
                
            case 's':
                p = argv[argi].errastr;
                len = strlen(p);
                break;
                
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
        if (len != 0)
        {
            if (outbufl >= len)
            {
                memcpy(outbuf, p, (size_t)len);
                outbufl -= len;
                outbuf += len;
            }
            else if (outbufl > 1)
            {
                memcpy(outbuf, p, (size_t)outbufl - 1);
                outbufl = 1;
            }
            outlen += len;
        }
        ++fmt;
    }

    /* add a null terminator */
    if (outbufl != 0)
        *outbuf++ = '\0';

    /* return the length */
    return outlen;
}

#if defined(DEBUG) && !defined(ERR_NO_MACRO)
void errjmp(jmp_buf buf, int e)
{
    longjmp(buf, e);
}
#endif /* DEBUG */



#ifdef ERR_NO_MACRO

/* base error signal function */
void errsign(errcxdef *ctx, int e, char *facility)
{
    strncpy(ctx->errcxptr->errfac, facility, ERRFACMAX);
    ctx->errcxptr->errfac[ERRFACMAX] = '\0';
    ctx->errcxofs = 0;
    longjmp(ctx->errcxptr->errbuf, e);
}

/* signal an error with no arguments */
void errsigf(errcxdef *ctx, char *facility, int e)
{
    errargc(ctx, 0);
    errsign(ctx, e, facility);
}

/* enter a string argument */
char *errstr(errcxdef *ctx, const char *str, int len)
{
    char *ret = &ctx->errcxbuf[ctx->errcxofs];
    
    memcpy(ret, str, (size_t)len);
    ret[len] = '\0';
    ctx->errcxofs += len + 1;
    return(ret);
}

/* resignal current error */
void errrse1(errcxdef *ctx, errdef *fr)
{
    errargc(ctx, fr->erraac);
    memcpy(ctx->errcxptr->erraav, fr->erraav,
           (size_t)(fr->erraac * sizeof(erradef)));
    errsign(ctx, fr->errcode, fr->errfac);
}

/* log an error: base function */
void errlogn(errcxdef *ctx, int err, char *facility)
{
    ctx->errcxofs = 0;
    (*ctx->errcxlog)(ctx->errcxlgc, facility, err, ctx->errcxptr->erraac,
                     ctx->errcxptr->erraav);
}

/* log an error with no arguments */
void errlogf(errcxdef *ctx, char *facility, int err)
{
    errargc(ctx, 0);
    errlogn(ctx, err, facility);
}


#endif /* ERR_NO_MACRO */

