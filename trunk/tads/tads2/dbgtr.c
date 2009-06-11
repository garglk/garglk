/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  dbgtr.c - Debugging functions for run-time
Function
  Provides dummy entrypoints for various debugger functions for run-time.
Notes
  Eliminates a number of time- and space-consuming functions from TR.
  Also defines a couple of TOKTH entrypoints since there will be no
  need for a symbol table when debugging is not enabled.
Modified
  12/18/92 MJRoberts     - creation
*/

#include <stdlib.h>

#include "os.h"
#include "std.h"
#include "tok.h"
#include "dbg.h"

/* indicate that the debugger is not present */
int dbgpresent()
{
    return FALSE;
}

static void dummy_add(toktdef *tab, char *nam, int namel, int typ,
                      int val, int hash) {}
static int  dummy_sea(toktdef *tab, char *nam, int namel, int hash,
                      toksdef *ret) { return(0); }
static void dummy_set(toktdef *tab, toksdef *sym) {}
static void dummy_each(toktdef *tab, void (*fn)(void *, toksdef *),
                       void *fnctx) {}
uint tokhsh(char *nam) { return(0); }

/* dummy symbol table entrypoints */
void tokthini(errcxdef *ec, mcmcxdef *mctx, toktdef *symtab1)
{
    tokthdef *symtab = (tokthdef *)symtab1;      /* convert to correct type */

    CLRSTRUCT(*symtab);
    symtab1->toktfadd = dummy_add;
    symtab1->toktfsea = dummy_sea;
    symtab1->toktfset = dummy_set;
    symtab1->toktfeach = dummy_each;
    symtab1->tokterr = ec;
    symtab->tokthmem = mctx;
}

/* dummy debugger entrypoints */
void dbgent(dbgcxdef *ctx, struct runsdef *bp, objnum self, objnum target,
            prpnum prop, int binum, int argc)
{
}

void dbglv(dbgcxdef *ctx, int exittype)
{
}

int  dbgnam(dbgcxdef *ctx, char *outbuf, int typ, int val)
{
    memcpy(outbuf, "<NO SYMBOL TABLE>", (size_t)17);
    return(17);
}

void dbgds(dbgcxdef *ctx)
{
    VARUSED(ctx);
}

int dbgu_err_resume(dbgcxdef *ctx)
{
    VARUSED(ctx);
    return FALSE;
}

void dbguquitting(dbgcxdef *ctx)
{
    VARUSED(ctx);
}

/*
void dbglget() {}
void dbgclin() {}
void dbgstktr() {}
*/

