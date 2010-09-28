/*----------------------------------------------------------------------*\

  rules.c

  Rule handling unit of Alan interpreter module, ARUN.

  \*----------------------------------------------------------------------*/
#include "rules.h"


/* IMPORTS */
#include "lists.h"
#include "inter.h"
#include "debug.h"
#include "current.h"
#include "options.h"

#ifdef HAVE_GLK
#include "glkio.h"
#endif


/* PUBLIC DATA */
RuleEntry *ruls;         /* Rule table pointer */


/*----------------------------------------------------------------------*/
static void traceRule(int i, char *what, char *tail) {
    printf("\n<RULE %d", i);
    if (current.location != 0) {
	printf(" (at ");
	traceSay(current.location);
    } else
	printf(" (nowhere");
    printf("[%d]), %s%s", current.location, what, tail);
}


/*----------------------------------------------------------------------*/
static void traceRuleEvaluation(int i) {
    if (sectionTraceOption) {
	if (!singleStepOption) {
	    traceRule(i, "Evaluating", "");
	} else {
	    traceRule(i, "Evaluating", ":>\n");
	}
    }
}


/*----------------------------------------------------------------------*/
static void traceRuleExecution(int i) {
    if (sectionTraceOption) {
	if (!singleStepOption)
	    printf(", Executing:>\n");
	else {
	    traceRule(i, "Executing:>\n", "");
	}
    }
}



/*=======================================================================*/
void rules(void)
{
    Bool change = TRUE;
    int i;

    for (i = 1; !isEndOfArray(&ruls[i-1]); i++)
	ruls[i-1].run = FALSE;

    while (change) {
	change = FALSE;
	for (i = 1; !isEndOfArray(&ruls[i-1]); i++)
	    if (!ruls[i-1].run) {
		traceRuleEvaluation(i);
		if (evaluate(ruls[i-1].exp)) {
		    change = TRUE;
		    ruls[i-1].run = TRUE;
		    traceRuleExecution(i);
		    interpret(ruls[i-1].stms);
		} else if (sectionTraceOption && !singleStepOption)
		    printf(":>\n");
	    }
    }
}
