#ifndef _RULES_H_
#define _RULES_H_
/*----------------------------------------------------------------------*\

  rules.h

  Header file for rules handler in Alan interpreter

\*----------------------------------------------------------------------*/

/* IMPORTS */
#include "acode.h"


/* TYPES */


/* DATA */
extern RuleEntry *rules;      /* Rule table pointer */
extern bool anyRuleRun;		  /* Did any rule run? */


/* FUNCTIONS */
extern void initRules(Aaddr rulesTableAddress);
extern void resetAndEvaluateRules(RuleEntry rules[], char *version);
extern void resetRules(void);
extern void evaluateRules(RuleEntry rules[]);

#endif
