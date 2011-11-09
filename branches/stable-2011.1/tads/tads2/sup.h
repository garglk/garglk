/*
$Header: d:/cvsroot/tads/TADS2/SUP.H,v 1.2 1999/05/17 02:52:13 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  sup.h - definitions for post-compilation setup
Function
  does post-compilation setup, such as setting up contents lists
Notes
  none
Modified
  03/28/92 MJRoberts     - creation
*/

#ifndef SUP_INCLUDED
#define SUP_INCLUDED

#ifndef TOK_INCLUDED
#include "tok.h"
#endif
#ifndef MCM_INCLUDED
#include "mcm.h"
#endif
#ifndef OBJ_INCLUDED
#include "obj.h"
#endif
#ifndef PRP_INCLUDED
#include "prp.h"
#endif
#ifndef VOC_INCLUDED
#include "voc.h"
#endif

/* setup context */
struct supcxdef
{
    errcxdef *supcxerr;
    mcmcxdef *supcxmem;                    /* memory manager client context */
    voccxdef *supcxvoc;                   /* player command parsing context */
    tokthdef *supcxtab;                           /* top-level symbol table */
    runcxdef *supcxrun;                                /* execution context */
    uchar    *supcxbuf;                        /* space for building a list */
    ushort    supcxlen;                                   /* size of buffer */
};
typedef struct supcxdef supcxdef;

/* set up contents list for one object for demand-on-load */
void supcont(void *ctx, objnum obj, prpnum prp);

/* set up inherited vocabulary (called before executing game) */
void supivoc(supcxdef *ctx);

/* find required objects/functions */
void supfind(errcxdef *ctx, tokthdef *tab, voccxdef *voc,
             objnum *preinit, int warnlevel, int casefold);

/* set up reserved words */
void suprsrv(supcxdef *sup, void (*bif[])(struct bifcxdef *, int),
             toktdef *tab, int fncntmax, int v1compat, char *new_do,
             int casefold);

/* set up built-in functions without symbol table (for run-time) */
void supbif(supcxdef *sup, void (*bif[])(struct bifcxdef *, int),
            int bifsiz);

/* log an undefined-object error */
void sup_log_undefobj(mcmcxdef *mctx, errcxdef *ec, int err,
                      char *sym_name, int sym_name_len, objnum objn);

/* set up inherited vocabulary for a particular object */
void supivoc1(supcxdef *sup, voccxdef *ctx, vocidef *v, objnum target,
              int inh_from_obj, int flags);

/* get name of an object out of symbol table */
void supgnam(char *buf, tokthdef *tab, objnum objn);

/* table of built-in functions */
typedef struct supbidef supbidef;
struct supbidef
{
    char  *supbinam;                                    /* name of function */
    void (*supbifn)(struct bifcxdef *, int);           /* C routine to call */
};

/* external definition for special token table */
extern tokldef supsctab[];

#endif /* SUP_INCLUDED */
