/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- the string-capable expression evaluator for
 * embedded `<# ... #>` expressions.
 *
 * ADRIFT text can embed expressions between `<#` and `#>` (the Adrift 5 runner
 * Global.ReplaceExpressions / EvaluateExpression -> clsVariable.SetToExpression).
 * This is also the evaluator for numeric variable assignments (a5run_action's
 * value path), reading back the leading integer of the result.  An embedded
 * `<# ... #>` expression instead yields a *string*: e.g.
 *
 *     <# IF(%Player%.Location.Exits.Count = 0, ".", ", only " + %Player%.Location.Exits.List + ".") #>
 *     <# IF(%direction% = "Up" OR %direction% = "Down", "", "to the ") #>
 *     <#100*%score%/%maxscore%#>
 *     <#if(%objects%.Count > 1, "are", "is")#>
 *
 * By the time this evaluator runs, every %reference%, %variable% and OO
 * property-expression in the body has already been substituted to a literal
 * (numbers stay bare; text is wrapped in double quotes -- the Adrift 5 runner's
 * bExpression quoting).  What remains is a self-contained expression over:
 *
 *   - numbers, "quoted strings", bare identifiers (treated as string literals);
 *   - arithmetic  + - * / mod ^   (with ADRIFT's precedence: * / bind tighter
 *     than + - mod ^, all left-associative; `+` is numeric add when both sides
 *     are numeric, otherwise string concatenation; `/` rounds away from zero);
 *   - comparisons = == <> != > < >= <=  (numeric or string; yield "1"/"0");
 *   - logic  AND OR  (also && ||);
 *   - functions  if min max abs upr lwr ppr len val str mid replace lft rgt ist
 *     either oneof rand urand  (the random ones draw via a5sexpr_rng_hook when
 *     the host wires it up -- see below; otherwise they fall back to the
 *     deterministic first operand / lower bound).
 *
 * This ports the token reducer of clsVariable.SetToExpression as a clean
 * recursive-descent evaluator that reproduces the same results for the
 * expressions ADRIFT 5 games actually ship.
 *
 * The returned char* is heap-allocated; the caller frees it.  Never NULL.
 */

#ifndef A5SEXPR_H
#define A5SEXPR_H

#include <string>

/* Evaluate a fully-substituted `<#...#>` expression body to its string value. */
char *a5_eval_sexpr (const char *expr);

/* True if `lid` (already lower-cased) names one of the functions apply_function
   understands (if/min/max/.../ucase/lcase/.../rand/urand).  Exposed so a
   caller can tell a bare function-call RHS ("UCASE(...)") apart from plain
   literal text before deciding whether to run it through the expression
   evaluator at all. */
bool a5sexpr_is_function (const std::string &lid);

/* RNG hook for the random functions (either/oneof/rand/urand).  Must return an
   integer in the inclusive range [lo, hi], mirroring the Adrift 5 runner's
   Global.Random(iMin, iMax).  The run harness sets this to a5rand_between so
   shipped <# OneOf(...) #> text picks draw from the same xoshiro stream as
   FrankenDrift; left NULL (a5text_dump, which links no RNG) the functions fall
   back to the deterministic first operand / lower bound. */
extern long (*a5sexpr_rng_hook) (long lo, long hi);

/* urand()'s no-repeat draw (the runner's clsVariable.NoRepeatRandom); the run harness
   sets this to a5rand_norepeat.  Left NULL, urand falls back to a plain
   a5sexpr_rng_hook draw. */
extern long (*a5sexpr_urand_hook) (long lo, long hi);

#endif /* A5SEXPR_H */
