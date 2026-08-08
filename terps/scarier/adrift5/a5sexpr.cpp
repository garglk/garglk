/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- the string-capable `<#...#>` expression
 * evaluator.  See a5sexpr.h for the grammar and rationale.
 *
 * Implementation: tokenise once, then a recursive-descent reducer over the
 * tokens.  Each parse step yields a Val (a string plus, when applicable, a
 * numeric interpretation), mirroring clsVariable's "expr" tokens whose .Value is
 * a string that math/comparison operators re-interpret via Val().
 */

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "a5sexpr.h"
#include "a5util.h"

/* Wired to a5rand_between by the run harness; NULL in RNG-less builds. */
long (*a5sexpr_rng_hook) (long lo, long hi) = NULL;
/* Wired to a5rand_norepeat (the runner NoRepeatRandom) by the run harness; urand()
   falls back to a plain a5sexpr_rng_hook draw when unset. */
long (*a5sexpr_urand_hook) (long lo, long hi) = NULL;

namespace {

/* --------------------------------------------------------------- values */

struct Val {
  std::string s;        /* the string form (always populated)            */
  bool isnum;           /* true when this was produced as a number       */
  double n;             /* the numeric value when isnum                  */
  bool is_test = false; /* produced by a comparison ("test" token) --
                           drives AND's logical-vs-concat split          */
};

/* Format a double the way ADRIFT's CStr/.ToString does for our cases: as a
   plain integer when whole (the games' arithmetic stays integral after the
   away-from-zero division rounding), else a compact decimal. */
std::string
fmt_num (double d)
{
  char buf[40];
  /* Match .NET Double.ToString: NaN -> "NaN", +/-Infinity -> "∞" (U+221E).
     Six Silver Bullets divides score by a missing %maxscore% (== 0), so its
     score line is "(NaN%)"; reproducing that keeps us byte-faithful. */
  if (isnan (d))
    return "NaN";
  if (isinf (d))
    return d < 0 ? "-\xe2\x88\x9e" : "\xe2\x88\x9e";
  /* .NET Core formats negative zero (e.g. 0 / -96 -> -0.0, Lost Coastlines'
     score percentage) as "-0"; the (long long) cast would drop the sign. */
  if (d == 0.0 && signbit (d))
    return "-0";
  if (d == floor (d) && fabs (d) < 9.0e15)
    snprintf (buf, sizeof buf, "%lld", (long long) d);
  else
    snprintf (buf, sizeof buf, "%g", d);
  return buf;
}

Val
mk_num (double d)
{
  Val v;
  v.isnum = true;
  v.n = d;
  v.s = fmt_num (d);
  return v;
}

Val
mk_str (const std::string &s)
{
  Val v;
  v.isnum = false;
  v.n = 0;
  v.s = s;
  return v;
}

/* Does the string look like a number (VB IsNumeric, simplified)?  Leading and
   trailing spaces allowed, optional sign, digits with an optional decimal. */
bool
is_numeric_str (const std::string &s)
{
  size_t i = 0, n = s.size ();
  while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
  if (i < n && (s[i] == '+' || s[i] == '-')) i++;
  bool digit = false, dot = false;
  for (; i < n; i++)
    {
      if (isdigit ((unsigned char) s[i])) { digit = true; continue; }
      if (s[i] == '.' && !dot) { dot = true; continue; }
      if (s[i] == ' ' || s[i] == '\t') break;
      return false;
    }
  while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
  return digit && i == n;
}

/* VB Val(): the leading numeric prefix of a string, else 0.

   clsVariable keeps every reduced token as a STRING, so a division by zero --
   which Math.Round(x / 0) leaves as Double.NaN -- is stored as the literal text
   "NaN", and any later Val() of it is 0, not NaN.  (Beginner's Cave leans on
   this: its to-hit formula divides the armour-penalty sum by an unarmoured
   NPC's RequiredEx sum, i.e. 0/0, so every enemy attack goes through the
   NaN path.)  strtod would happily read "NaN"/"inf" back as those values, so
   answer 0 for them here. */
double
val_of (const Val &v)
{
  if (v.isnum)
    return (isnan (v.n) || isinf (v.n)) ? 0.0 : v.n;
  const char *p = v.s.c_str ();
  char *end;
  double d = strtod (p, &end);
  if (isnan (d) || isinf (d))
    return 0.0;
  return (end == p) ? 0.0 : d;
}

/* --------------------------------------------------------------- tokens */

enum TokKind { T_NUM, T_STR, T_IDENT, T_OP, T_END };

struct Tok {
  TokKind kind;
  std::string text;     /* operator name, identifier, or string content   */
  double num;
};

std::vector<Tok>
tokenise (const char *expr)
{
  std::vector<Tok> out;
  const char *p = expr ? expr : "";
  while (*p)
    {
      if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') { p++; continue; }

      /* quoted string ("..." or '...') */
      if (*p == '"' || *p == '\'')
        {
          char quote = *p++;
          std::string s;
          while (*p && *p != quote) s += *p++;
          if (*p == quote) p++;
          Tok t; t.kind = T_STR; t.text = s; t.num = 0; out.push_back (t);
          continue;
        }

      /* number */
      if (isdigit ((unsigned char) *p))
        {
          char *end;
          double d = strtod (p, &end);
          Tok t; t.kind = T_NUM; t.num = d;
          t.text.assign (p, (size_t) (end - p));
          out.push_back (t);
          p = end;
          continue;
        }

      /* operators */
      Tok t; t.kind = T_OP; t.num = 0;
      switch (*p)
        {
        case '+': case '-': case '*': case '/': case '^':
        case '(': case ')': case ',':
          t.text = std::string (1, *p); p++; out.push_back (t); continue;
        case '=':
          t.text = "EQ"; p += (p[1] == '=') ? 2 : 1; out.push_back (t); continue;
        case '<':
          if (p[1] == '>') { t.text = "NE"; p += 2; }
          else if (p[1] == '=') { t.text = "LE"; p += 2; }
          else { t.text = "LT"; p += 1; }
          out.push_back (t); continue;
        case '>':
          if (p[1] == '=') { t.text = "GE"; p += 2; }
          else { t.text = "GT"; p += 1; }
          out.push_back (t); continue;
        case '!':
          if (p[1] == '=') { t.text = "NE"; p += 2; out.push_back (t); }
          else p++;
          continue;
        case '|':
          t.text = "OR"; p += (p[1] == '|') ? 2 : 1; out.push_back (t); continue;
        case '&':
          t.text = "AND"; p += (p[1] == '&') ? 2 : 1; out.push_back (t); continue;
        default: break;
        }

      /* identifier (function name, AND/OR/mod keyword, or a bare string) */
      if (isalpha ((unsigned char) *p))
        {
          const char *s = p;
          while (isalnum ((unsigned char) *p) || *p == '_') p++;
          std::string id (s, (size_t) (p - s));
          std::string lid = lower (id);
          if (lid == "and") { Tok o; o.kind = T_OP; o.text = "AND"; o.num = 0; out.push_back (o); }
          else if (lid == "or") { Tok o; o.kind = T_OP; o.text = "OR"; o.num = 0; out.push_back (o); }
          else if (lid == "mod") { Tok o; o.kind = T_OP; o.text = "mod"; o.num = 0; out.push_back (o); }
          else { Tok o; o.kind = T_IDENT; o.text = id; o.num = 0; out.push_back (o); }
          continue;
        }

      /* anything else: skip one byte */
      p++;
    }
  Tok end; end.kind = T_END; end.num = 0; out.push_back (end);
  return out;
}

/* --------------------------------------------------------------- parser */

struct Parser {
  std::vector<Tok> toks;
  size_t pos;
  const Tok &cur () const { return toks[pos]; }
  bool is_op (const char *o) const
    { return toks[pos].kind == T_OP && toks[pos].text == o; }
  void adv () { if (toks[pos].kind != T_END) pos++; }
};

Val parse_logic (Parser &p);

bool
is_function (const std::string &lid)
{
  static const char *fns[] = {
    "if", "min", "max", "abs", "upr", "lwr", "ppr", "len", "val", "str",
    "mid", "replace", "lft", "rgt", "ist", "either", "oneof", "rand", "urand",
    "ucase", "lcase", "upper", "lower", "proper", "pcase", "left", "right",
    "instr", "len", NULL
  };
  for (int i = 0; fns[i]; i++)
    if (lid == fns[i]) return true;
  return false;
}

Val
apply_function (const std::string &lid, std::vector<Val> &a)
{
  size_t n = a.size ();
  /* normalise aliases */
  std::string f = lid;
  if (f == "ucase" || f == "upper") f = "upr";
  else if (f == "lcase" || f == "lower") f = "lwr";
  else if (f == "pcase" || f == "proper") f = "ppr";
  else if (f == "left") f = "lft";
  else if (f == "right") f = "rgt";
  else if (f == "instr") f = "ist";

  if (f == "if" && n >= 3)
    return (val_of (a[0]) > 0) ? a[1] : a[2];
  if (f == "min" && n >= 2)
    return mk_num (val_of (a[0]) < val_of (a[1]) ? val_of (a[0]) : val_of (a[1]));
  if (f == "max" && n >= 2)
    return mk_num (val_of (a[0]) > val_of (a[1]) ? val_of (a[0]) : val_of (a[1]));
  if (f == "abs" && n >= 1)
    return mk_num (fabs (val_of (a[0])));
  if (f == "upr" && n >= 1)
    { std::string s = a[0].s; for (char &c : s) c = (char) toupper ((unsigned char) c); return mk_str (s); }
  if (f == "lwr" && n >= 1)
    return mk_str (lower (a[0].s));
  if (f == "ppr" && n >= 1)
    return mk_str (to_proper (a[0].s));
  if (f == "len" && n >= 1)
    return mk_num ((double) a[0].s.size ());
  if (f == "val" && n >= 1)
    return mk_num ((double) (long) val_of (a[0]));
  if (f == "str" && n >= 1)
    return mk_str (a[0].s);
  if (f == "mid" && n >= 3)
    { long start = (long) val_of (a[1]), cnt = (long) val_of (a[2]);
      if (start < 1) start = 1;
      std::string s = a[0].s;
      if ((size_t) (start - 1) >= s.size ()) return mk_str ("");
      return mk_str (s.substr (start - 1, cnt < 0 ? std::string::npos : (size_t) cnt)); }
  if (f == "replace" && n >= 3)
    { std::string s = a[0].s, from = a[1].s, to = a[2].s;
      if (!from.empty ())
        { size_t i = 0; while ((i = s.find (from, i)) != std::string::npos)
            { s.replace (i, from.size (), to); i += to.size (); } }
      return mk_str (s); }
  if (f == "lft" && n >= 2)
    { long k = (long) val_of (a[1]); if (k < 0) k = 0;
      return mk_str (a[0].s.substr (0, (size_t) k)); }
  if (f == "rgt" && n >= 2)
    { long k = (long) val_of (a[1]); if (k < 0) k = 0;
      std::string s = a[0].s;
      return mk_str ((size_t) k >= s.size () ? s : s.substr (s.size () - (size_t) k)); }
  if (f == "ist" && n >= 2)
    { size_t i = a[0].s.find (a[1].s); return mk_num (i == std::string::npos ? 0 : (double) (i + 1)); }
  /* either/oneof/rand/urand draw via the host RNG hook (a5rand_between, wired by
     the run harness) so shipped <# OneOf(...) #> picks match frankendrift's --
     e.g. Stone of Wisdom's troll-approach lines.  Without the hook (a5text_dump)
     they fall back to the deterministic first operand / lower bound. */
  if (f == "either" && n >= 2)
    /* clsVariable.SetToExpression "either": Random(1)=1 -> left, else right. */
    return (a5sexpr_rng_hook && a5sexpr_rng_hook (0, 1) == 1) ? a[0] : a[1];
  if (f == "oneof" && n >= 1)
    { long idx = a5sexpr_rng_hook ? a5sexpr_rng_hook (0, (long) n - 1) : 0;
      if (idx < 0) idx = 0; if ((size_t) idx >= n) idx = (long) n - 1;
      return a[idx]; }
  if ((f == "rand" || f == "urand") && n >= 2)
    {
      /* rand(min,max) -> Random(min,max); urand(min,max) -> NoRepeatRandom
         (a per-range shuffled pool consumed without repeats -- The Salvage's
         planet-name assignment urand(1,33)). */
      long lo = (long) val_of (a[0]), hi = (long) val_of (a[1]);
      if (f == "urand" && a5sexpr_urand_hook)
        return mk_num ((double) a5sexpr_urand_hook (lo, hi));
      return mk_num ((double) (a5sexpr_rng_hook ? a5sexpr_rng_hook (lo, hi) : lo));
    }
  if ((f == "rand" || f == "urand") && n >= 1)
    {
      /* clsVariable.SetToExpression single-arg rand: Random(0, value) -- inclusive
         0..value (e.g. RAND(8) draws 0..8), NOT the bare literal. */
      long hi = (long) val_of (a[0]);
      if (f == "urand" && a5sexpr_urand_hook)
        return mk_num ((double) a5sexpr_urand_hook (0, hi));
      return mk_num ((double) (a5sexpr_rng_hook ? a5sexpr_rng_hook (0, hi) : hi));
    }

  /* unknown / wrong arity: empty string */
  return mk_str ("");
}

Val
parse_atom (Parser &p)
{
  const Tok &t = p.cur ();
  if (t.kind == T_NUM) { Val v = mk_num (t.num); p.adv (); return v; }
  if (t.kind == T_STR) { Val v = mk_str (t.text); p.adv (); return v; }
  if (p.is_op ("("))
    { p.adv (); Val v = parse_logic (p); if (p.is_op (")")) p.adv (); return v; }
  if (t.kind == T_IDENT)
    {
      std::string id = t.text, lid = lower (id);
      /* a function call: name '(' args ')' */
      if (is_function (lid) && p.toks[p.pos + 1].kind == T_OP
          && p.toks[p.pos + 1].text == "(")
        {
          p.adv ();          /* name */
          p.adv ();          /* '('  */
          std::vector<Val> args;
          if (!p.is_op (")"))
            {
              args.push_back (parse_logic (p));
              while (p.is_op (",")) { p.adv (); args.push_back (parse_logic (p)); }
            }
          if (p.is_op (")")) p.adv ();
          return apply_function (lid, args);
        }
      /* a bare identifier is a string literal (clsVariable's "vlu" fallback) */
      p.adv ();
      return mk_str (id);
    }
  /* unexpected token */
  p.adv ();
  return mk_str ("");
}

Val
parse_unary (Parser &p)
{
  if (p.is_op ("-")) { p.adv (); return mk_num (-val_of (parse_unary (p))); }
  if (p.is_op ("+")) { p.adv (); return mk_num (val_of (parse_unary (p))); }
  return parse_atom (p);
}

/* * and / -- the tightest binary tier (clsVariable run 1). */
Val
parse_mul (Parser &p)
{
  Val left = parse_unary (p);
  for (;;)
    {
      if (p.is_op ("*")) { p.adv (); left = mk_num (val_of (left) * val_of (parse_unary (p))); }
      else if (p.is_op ("/"))
        { p.adv (); double r = val_of (parse_unary (p));
          double q = val_of (left) / r;       /* IEEE: x/0 -> +/-inf, 0/0 -> NaN */
          double rounded = (isnan (q) || isinf (q)) ? q
                         : (q >= 0 ? floor (q + 0.5) : ceil (q - 0.5));
          /* Math.Round(AwayFromZero) preserves -0.0 (0 / -96 -> -0); our
             floor(q+0.5) would yield +0.0, so restore the sign of zero. */
          if (rounded == 0.0)
            rounded = copysign (0.0, q);
          left = mk_num (rounded); }
      else break;
    }
  return left;
}

/* + - mod ^ -- all one tier, left-associative (clsVariable run 2).  `+` is a
   numeric add when both operands are numeric, else string concatenation. */
Val
parse_add (Parser &p)
{
  Val left = parse_mul (p);
  for (;;)
    {
      if (p.is_op ("+"))
        {
          p.adv ();
          Val right = parse_mul (p);
          /* IsNumeric() on the token's STRING form, exactly as clsVariable
             tests it -- so a "NaN" (or "∞") operand is NOT numeric and this
             falls through to concatenation: `50 + NaN - 15` reduces to
             "50NaN" and then Val("50NaN") - 15 == 35.  That quirk is load-
             bearing in Beginner's Cave, whose combat rolls all divide by an
             unarmoured NPC's zero RequiredEx sum. */
          bool ln = is_numeric_str (left.s);
          bool rn = is_numeric_str (right.s);
          if (ln && rn) left = mk_num (val_of (left) + val_of (right));
          else left = mk_str (left.s + right.s);
        }
      else if (p.is_op ("-")) { p.adv (); left = mk_num (val_of (left) - val_of (parse_mul (p))); }
      else if (p.is_op ("mod"))
        { p.adv (); long r = (long) val_of (parse_mul (p));
          left = mk_num (r == 0 ? 0.0 : (double) ((long) val_of (left) % r)); }
      else if (p.is_op ("^")) { p.adv (); left = mk_num (pow (val_of (left), val_of (parse_mul (p)))); }
      else break;
    }
  return left;
}

/* comparison (clsVariable run 3): non-associative, yields a 1/0 test value. */
Val
parse_cmp (Parser &p)
{
  Val left = parse_add (p);
  const std::string &o = p.cur ().text;
  if (p.cur ().kind == T_OP
      && (o == "EQ" || o == "NE" || o == "GT" || o == "LT" || o == "GE" || o == "LE"))
    {
      std::string op = o;
      p.adv ();
      Val right = parse_add (p);
      bool both_num = is_numeric_str (left.s) && is_numeric_str (right.s);
      int res = 0;
      if (op == "EQ")
        res = both_num ? (val_of (left) == val_of (right)) : (left.s == right.s);
      else if (op == "NE")
        res = both_num ? (val_of (left) != val_of (right)) : (left.s != right.s);
      /* Ordered comparisons follow clsVariable too: numeric when both operands
         are numbers, otherwise a lexical string comparison (val_of returns 0
         for any non-numeric string, so comparing two strings numerically would
         collapse to 0-vs-0 and be wrong). */
      else if (op == "GT") res = both_num ? (val_of (left) > val_of (right))  : (left.s > right.s);
      else if (op == "LT") res = both_num ? (val_of (left) < val_of (right))  : (left.s < right.s);
      else if (op == "GE") res = both_num ? (val_of (left) >= val_of (right)) : (left.s >= right.s);
      else if (op == "LE") res = both_num ? (val_of (left) <= val_of (right)) : (left.s <= right.s);
      Val t = mk_num (res ? 1.0 : 0.0);
      t.is_test = true;                  /* clsVariable "test" token */
      return t;
    }
  return left;
}

/* AND / OR (loosest): logical combination of test values. */
Val
parse_logic (Parser &p)
{
  Val left = parse_cmp (p);
  for (;;)
    {
      if (p.is_op ("AND"))
        { p.adv (); Val r = parse_cmp (p);
          /* clsVariable: `test AND test` (comparison results) is logical
             (vb:833); `expr AND expr` (plain values) CONCATENATES (vb:919) --
             VB's `&`.  Magnetic Moon's `Lid.ReadText & %text%` is
             `"" & "mike"` -> "mike". */
          if (!left.is_test && !r.is_test)
            left = mk_str (left.s + r.s);
          else
            { left = mk_num ((val_of (left) > 0 && val_of (r) > 0) ? 1.0 : 0.0);
              left.is_test = true; } }
      else if (p.is_op ("OR"))
        { p.adv (); Val r = parse_cmp (p);
          left = mk_num ((val_of (left) > 0 || val_of (r) > 0) ? 1.0 : 0.0); }
      else break;
    }
  return left;
}

} /* namespace */

char *
a5_eval_sexpr (const char *expr)
{
  Parser p;
  p.toks = tokenise (expr);
  p.pos = 0;
  Val v = parse_logic (p);
  return strdup (v.s.c_str ());
}

bool
a5sexpr_is_function (const std::string &lid)
{
  return is_function (lid);
}
