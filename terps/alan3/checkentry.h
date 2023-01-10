#ifndef _CHECKENTRY_H_
#define _CHECKENTRY_H_
/*----------------------------------------------------------------------*\

  CheckEntries

\*----------------------------------------------------------------------*/

#include "types.h"
#include "acode.h"


/* CONSTANTS */
#ifndef EXECUTE_CHECK_BODY_ON_FAIL
#define EXECUTE_CHECK_BODY_ON_FAIL true
#define DONT_EXECUTE_CHECK_BODY_ON_FAIL false
#endif


/* TYPES */
typedef struct CheckEntry { /* CHECK TABLE */
  Aaddr exp;            /* ACODE address to expression code */
  Aaddr stms;           /* ACODE address to statement code */
} CheckEntry;


/* DATA */
typedef CheckEntry CheckEntryArray[];


/* FUNCTIONS */
extern bool checksFailed(Aaddr adr, bool execute);

#endif
