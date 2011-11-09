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
#include "compatibility.h"

#ifdef HAVE_GLK
#include "glkio.h"
#endif


/* PUBLIC DATA */
RuleEntry *rules;         /* Rule table pointer */


/* PRIVATE DATA: */
static bool *rulesLastEval;     /* Table for last evaluation of the rules */

/*======================================================================*/
void initRules() {
    int ruleCount;

    rules = (RuleEntry *) pointerTo(header->ruleTableAddress);

    for (ruleCount = 0; !isEndOfArray(&rules[ruleCount]); ruleCount++);
    rulesLastEval = allocate(ruleCount*sizeof(bool));
}


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



/*----------------------------------------------------------------------*/
static void evaluateRulesPreBeta2(void)
{
    bool change = TRUE;
    int i;

    for (i = 1; !isEndOfArray(&rules[i-1]); i++)
	rules[i-1].run = FALSE;

    while (change) {
	change = FALSE;
	for (i = 1; !isEndOfArray(&rules[i-1]); i++)
	    if (!rules[i-1].run) {
		traceRuleEvaluation(i);
		if (evaluate(rules[i-1].exp)) {
		    change = TRUE;
		    rules[i-1].run = TRUE;
		    traceRuleExecution(i);
		    interpret(rules[i-1].stms);
		} else if (sectionTraceOption && !singleStepOption)
		    printf(":>\n");
	    }
    }
}


/*----------------------------------------------------------------------*/
static void evaluateRulesBeta2Onwards(void)
{
    bool change = TRUE;
    int i;

    for (i = 1; !isEndOfArray(&rules[i-1]); i++)
	rules[i-1].run = FALSE;

    current.location = NOWHERE;
    current.actor = 0;

    while (change) {
	change = FALSE;
	for (i = 1; !isEndOfArray(&rules[i-1]); i++)
	    if (!rules[i-1].run) {
                bool triggered = evaluate(rules[i-1].exp);
		traceRuleEvaluation(i);
		if (triggered) {
                    if (rulesLastEval[i-1] == false) {
                        change = TRUE;
                        rules[i-1].run = TRUE;
                        traceRuleExecution(i);
                        interpret(rules[i-1].stms);
                    }
                    rulesLastEval[i-1] = triggered;
		} else {
                    rulesLastEval[i-1] = false;
                    if (sectionTraceOption && !singleStepOption)
                        printf(":>\n");
                }
	    }
    }
}


/*=======================================================================*/
void evaluateRules(void) {
    if (isPreBeta2(header->version))
        evaluateRulesPreBeta2();
    else
        evaluateRulesBeta2Onwards();
}
