#ifndef _RULES_H_
#define _RULES_H_
/*----------------------------------------------------------------------*\

  rules.h

  Header file for rules handler in Alan interpreter

\*----------------------------------------------------------------------*/

/* IMPORTS */
#include "acode.h"

/* TYPES */

typedef struct RuleEntry {   /* RULE TABLE */
  Abool run;            /* Is rule already run? */
  Aaddr exp;            /* Address to expression code */
  Aaddr stms;           /* Address to run */
} RuleEntry;


/* DATA */
extern RuleEntry *rules;      /* Rule table pointer */


/* FUNCTIONS */
extern void initRules(void);
extern void evaluateRules(void);

#endif
