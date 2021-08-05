#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/LERMSG.C,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1993 by Steve McAdams.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  lermsg.c - Library ERorr message handling routines
Function
  
Notes
  
Modified
  01/04/93 SMcAdams - Creation from TADS errmsg.c
*/


#include "os.h"
#include "lib.h"
#include "ler.h"
#include "ltk.h"


/*--------------------------------- lerini ---------------------------------*/
/*
 * lerini - allocate and initialize an error context.  Returns a
 * pointer to an initialized error context if successful, 0 otherwise.
 */
errcxdef *lerini()
{
  errcxdef *errcx;                                         /* error context */
  
  /* allocate an error context */
  if (!(errcx = ltk_suballoc(sizeof(errcxdef))))
  {
    /* failure */
    return((errcxdef *)0);
  }

  /* initialize the error context */
  errcx->errcxfp  = (osfildef *)0;                  /* no error file handle */
  errcx->errcxofs = 0;                      /* no offset in argument buffer */
  errcx->errcxlog = ltk_errlog;                    /* error logging routine */
  errcx->errcxlgc = errcx;                         /* error logging context */

  /* return the new context */
  return(errcx);
}


/*--------------------------------- lerfre ---------------------------------*/
/*
 * lerfre - FREe error context allocated by errini.
 */
void lerfre(errcx)
  errcxdef *errcx;                                 /* error context to free */
{
  /* free the context */
  ltk_subfree(errcx);
}


/*--------------------------------- errmsg ---------------------------------*/
/*
 * errmsg - format error message number 'err' into the given buffer.
 */
void errmsg(ctx, outbuf, outbufl, err)
  errcxdef *ctx;                                           /* error context */
  char     *outbuf;                                        /* output buffer */
  uint      outbufl;                                /* output buffer length */
  uint      err;                                      /* error msg # to get */
{
  sprintf(outbuf, "Error #%d occured.", err);
}


/*--------------------------------- errini ---------------------------------*/
/*
 * errini - initialize error system.
 */
void errini(ctx, arg0)
  errcxdef *ctx;
  char     *arg0;
{
    VARUSED(ctx);
    VARUSED(arg0);
}
