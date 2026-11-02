/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Module notes:
 *
 * o The tokenizer doesn't differentiate between "&", "&&", and "and", but "&"
 *   is context sensitive -- either a logical and for numerics, or concaten-
 *   ation for strings.  As a result, "&&" and "and" also work as string
 *   concatenators when used in string expressions.  It may not be to spec,
 *   but we'll call this a "feature".
 */

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "scvars.h"
#include "scexpr.h"
#include "sclocale.h"


/*
 * Bounds on the values stack and on factor nesting -- each nested '(' or
 * unary sign is one factor level, and each can leave a value pending on the
 * stack.  An expression past either is reported as a parse error, like any
 * other malformed expression, rather than ending the game.
 */
enum { MAX_NESTING_DEPTH = 256 };

/* Assorted definitions and constants. */
static const scr_char PERCENT = '%';
static const scr_char SINGLE_QUOTE = '\'';
static const scr_char DOUBLE_QUOTE = '"';


/*
 * Tokens.  Single character tokens are represented by their ascii value
 * (0-255), others by values above 255.  -1 represents a null token.  Because
 * '&' and '+' are context sensitive, the pseudo-token TOK_CONCATENATE
 * serves to indicate string concatenation -- it's never returned by the
 * tokenizer.
 */
enum
{ TOK_NONE = -1,
  TOK_ADD = '+', TOK_SUBTRACT = '-', TOK_MULTIPLY = '*', TOK_DIVIDE = '/',
  TOK_AND = '&', TOK_OR = '|',
  TOK_LPAREN = '(', TOK_RPAREN = ')', TOK_COMMA = ',', TOK_POWER = '^',
  TOK_EQUAL = '=', TOK_GREATER = '>', TOK_LESS = '<',

  TOK_IDENT = 256,
  TOK_INTEGER, TOK_STRING, TOK_VARIABLE, TOK_UMINUS, TOK_UPLUS,
  TOK_MOD, TOK_NOT_EQUAL, TOK_GREATER_EQ, TOK_LESS_EQ, TOK_IF,
  TOK_MIN, TOK_MAX, TOK_EITHER, TOK_RANDOM, TOK_INSTR, TOK_LEN, TOK_VAL,
  TOK_ABS, TOK_UPPER, TOK_LOWER, TOK_PROPER, TOK_RIGHT, TOK_LEFT, TOK_MID,
  TOK_STR, TOK_CONCATENATE,
  TOK_EOS
};


/*
 * Small tables tying multicharacter tokens strings to tokens.  At present,
 * the string lengths for names are not used.
 */
typedef struct
{
  const scr_char *const name;
  const scr_int length;
  const scr_int token;
} scr_expr_multichar_t;

static const scr_expr_multichar_t FUNCTION_TOKENS[] = {
  {"either", 6, TOK_EITHER},
  {"proper", 6, TOK_PROPER}, {"pcase", 5, TOK_PROPER}, {"instr", 5, TOK_INSTR},
  {"upper", 5, TOK_UPPER}, {"ucase", 5, TOK_UPPER},
  {"lower", 5, TOK_LOWER}, {"lcase", 5, TOK_LOWER},
  {"right", 5, TOK_RIGHT}, {"left", 4, TOK_LEFT},
  {"rand", 4, TOK_RANDOM}, {"max", 3, TOK_MAX}, {"min", 3, TOK_MIN},
  {"mod", 3, TOK_MOD}, {"abs", 3, TOK_ABS}, {"len", 3, TOK_LEN},
  {"val", 3, TOK_VAL}, {"and", 3, TOK_AND}, {"mid", 3, TOK_MID},
  {"str", 3, TOK_STR}, {"or", 2, TOK_OR}, {"if", 2, TOK_IF},
  {nullptr, 0, TOK_NONE}
};
static const scr_expr_multichar_t OPERATOR_TOKENS[] = {
  {"&&", 2, TOK_AND}, {"||", 2, TOK_OR},
  {"==", 2, TOK_EQUAL}, {"!=", 2, TOK_NOT_EQUAL},
  {"<>", 2, TOK_NOT_EQUAL}, {">=", 2, TOK_GREATER_EQ}, {"<=", 2, TOK_LESS_EQ},
  {nullptr, 0, TOK_NONE}
};


/*
 * expr_multichar_search()
 *
 * Multicharacter token table search, returns the matching token, or
 * TOK_NONE if no match.
 */
static scr_int
expr_multichar_search (const scr_char *name, const scr_expr_multichar_t *table)
{
  const scr_expr_multichar_t *entry;

  /* Scan the table for a case-independent full string match. */
  for (entry = table; entry->name; entry++)
    {
      if (scr_strcasecmp (name, entry->name) == 0)
        break;
    }

  /* Return the token matched, or TOK_NONE. */
  return entry->name ? entry->token : TOK_NONE;
}


/*
 * Evaluation values stack entry, uses a variable type so it can contain
 * both integers and strings, and flags strings for possible garbage
 * collection on parse errors.
 */
typedef struct
{
  scr_bool is_collectible;
  scr_vartype_t value;
} scr_stack_t;

/*
 * The expression evaluator's module state, in one instance (expr).  It is
 * grouped by the start/end pair that owns it; every group is scratch for
 * one expr_evaluate_expression() call, nothing is per game and nothing is
 * saved, so there is no reset hook:
 *
 *   tokenizer  the expression under scan, the cursor into it, the value and
 *              kind of the last token, and the temporary that holds token
 *              literals.  expr_tokenize_start()/_end(), through the
 *              expr_tokenize_guard RAII owner.
 *   eval       the values stack and its depth, and the variables set that
 *              %...% references resolve against.  expr_eval_start() resets
 *              the depth; expr_eval_result() or expr_eval_garbage_collect()
 *              empty the stack again.
 *   parse      the predictive parser's lookahead, the factor nesting count
 *              that bounds recursion, and the parse error jump buffer.
 *              Set at the top of each evaluation.
 */
struct scr_expr_state_s
{
  struct
  {
    const scr_char *expression = nullptr;
    scr_int index = 0;
    scr_vartype_t token_value;
    scr_char *temporary = nullptr;
    scr_int current_token = TOK_NONE;
  } tokenizer;

  struct
  {
    scr_stack_t stack[MAX_NESTING_DEPTH];
    scr_int stack_index = 0;
    scr_var_setref_t varset = nullptr;
  } eval;

  struct
  {
    jmp_buf error;
    scr_int lookahead = TOK_NONE;
    scr_int depth = 0;
  } parse;
};
typedef struct scr_expr_state_s scr_expr_state_t;

static scr_expr_state_t expr;


/*
 * expr_tokenize_start()
 * expr_tokenize_end()
 *
 * Start and wrap up expression string tokenization.
 */
static void
expr_tokenize_start (const scr_char *expression)
{
  static scr_bool initialized = FALSE;

  /* On first call only, verify the string lengths in the tables. */
  if (!initialized)
    {
      const scr_expr_multichar_t *entry;

      /* Compare table lengths with string lengths. */
      for (entry = FUNCTION_TOKENS; entry->name; entry++)
        {
          if (entry->length != (scr_int) strlen (entry->name))
            {
              scr_fatal ("expr_tokenize_start:"
                        " token string length is wrong for \"%s\"\n",
                        entry->name);
            }
        }

      for (entry = OPERATOR_TOKENS; entry->name; entry++)
        {
          if (entry->length != (scr_int) strlen (entry->name))
            {
              scr_fatal ("expr_tokenize_start:"
                        " operator string length is wrong for \"%s\"\n",
                        entry->name);
            }
        }

      initialized = TRUE;
    }

  /* Save expression, and restart index. */
  expr.tokenizer.expression = expression;
  expr.tokenizer.index = 0;

  /* Allocate a temporary token value/literals string. */
  assert (!expr.tokenizer.temporary);
  expr.tokenizer.temporary
    = (scr_char *) scr_malloc (strlen (expression) + 1);

  /* Reset last token to none. */
  expr.tokenizer.current_token = TOK_NONE;
}

static void
expr_tokenize_end (void)
{
  /* Deallocate temporary strings, clear expression. */
  scr_free (expr.tokenizer.temporary);
  expr.tokenizer.temporary = nullptr;
  expr.tokenizer.expression = nullptr;
  expr.tokenizer.index = 0;
  expr.tokenizer.current_token = TOK_NONE;
}


/*
 * expr_tokenize_guard
 *
 * RAII owner of a tokenization: expr_tokenize_start() on construction,
 * expr_tokenize_end() on destruction.  A scr_fatal_error thrown by var_get()
 * or an allocation mid-expression then still clears the tokenizer's
 * temporary, so the next expression in the same process does not trip the
 * assert in expr_tokenize_start().  Only multi-game processes ever saw that.
 */
struct expr_tokenize_guard
{
  explicit expr_tokenize_guard (const scr_char *expression)
  {
    expr_tokenize_start (expression);
  }
  ~expr_tokenize_guard ()
  {
    expr_tokenize_end ();
  }
  expr_tokenize_guard (const expr_tokenize_guard &) = delete;
  expr_tokenize_guard &operator= (const expr_tokenize_guard &) = delete;
};


/*
 * expr_next_token_unadjusted()
 * expr_next_token()
 *
 * Return the next token from the current expression.  The initial token may
 * be adjusted into a unary +/- depending on the value of the previous token.
 */
static scr_int
expr_next_token_unadjusted (scr_vartype_t *token_value)
{
  scr_int c;

  assert (expr.tokenizer.expression);

  /* Skip any and all leading whitespace. */
  do
    {
      c = expr.tokenizer.expression[expr.tokenizer.index++];
    }
  while (scr_isspace (c) && c != NUL);

  /* Return EOS if at expression end. */
  if (c == NUL)
    {
      expr.tokenizer.index--;
      return TOK_EOS;
    }

  /*
   * Identify and return numerics.  We deal only with unsigned numbers here;
   * the unary +/- tokens take care of any integer sign issues.
   */
  else if (scr_isdigit (c))
    {
      scr_int value;

      sscanf (expr.tokenizer.expression + expr.tokenizer.index - 1,
              "%ld", &value);

      while (scr_isdigit (c) && c != NUL)
        c = expr.tokenizer.expression[expr.tokenizer.index++];
      expr.tokenizer.index--;

      token_value->integer = value;
      return TOK_INTEGER;
    }

  /* Identify and return variable references. */
  else if (c == PERCENT)
    {
      scr_int index_;

      /* Copy variable name. */
      c = expr.tokenizer.expression[expr.tokenizer.index++];
      for (index_ = 0; c != PERCENT && c != NUL;)
        {
          expr.tokenizer.temporary[index_++] = c;
          c = expr.tokenizer.expression[expr.tokenizer.index++];
        }
      expr.tokenizer.temporary[index_++] = NUL;

      if (c == NUL)
        {
          scr_error ("expr_next_token_unadjusted:"
                    " warning: unterminated variable name\n");
          expr.tokenizer.index--;
        }

      /* Return a variable name. */
      token_value->string = expr.tokenizer.temporary;
      return TOK_VARIABLE;
    }

  /* Identify and return string literals. */
  else if (c == DOUBLE_QUOTE || c == SINGLE_QUOTE)
    {
      scr_int index_;
      scr_char quote;

      /* Copy maximal string literal. */
      quote = c;
      c = expr.tokenizer.expression[expr.tokenizer.index++];
      for (index_ = 0; c != quote && c != NUL;)
        {
          expr.tokenizer.temporary[index_++] = c;
          c = expr.tokenizer.expression[expr.tokenizer.index++];
        }
      expr.tokenizer.temporary[index_++] = NUL;

      if (c == NUL)
        {
          scr_error ("expr_next_token_unadjusted:"
                    " warning: unterminated string literal\n");
          expr.tokenizer.index--;
        }

      /* Return string literal. */
      token_value->string = expr.tokenizer.temporary;
      return TOK_STRING;
    }

  /* Identify ids and other multicharacter tokens. */
  else if (scr_isalpha (c))
    {
      scr_int index_, token;

      /*
       * Copy maximal alphabetical string.  While an ident would normally
       * be alpha followed by zero or more alnum, for Adrift purposes we
       * use only alpha -- all idents should really be "functions", and
       * in particular we want to see "mod7" as "mod" and 7 separately.
       */
      for (index_ = 0; scr_isalpha (c) && c != NUL;)
        {
          expr.tokenizer.temporary[index_++] = c;
          c = expr.tokenizer.expression[expr.tokenizer.index++];
        }
      expr.tokenizer.index--;
      expr.tokenizer.temporary[index_++] = NUL;

      /*
       * Check for a function name, and if known, return that, otherwise
       * return a bare id.
       */
      token = expr_multichar_search (expr.tokenizer.temporary, FUNCTION_TOKENS);
      if (token == TOK_NONE)
        {
          token_value->string = expr.tokenizer.temporary;
          return TOK_IDENT;
        }
      else
        return token;
    }

  /*
   * Last chance check for two-character (multichar) operators, and if none
   * then return a single-character token.
   */
  else
    {
      scr_char operator_[3];
      scr_int token;

      /*
       * Build a two-character string.  If we happen to be at the last
       * expression character, we'll pick up the expression NUL into
       * operator_[1], so no need to special case end of expression here.
       */
      operator_[0] = c;
      operator_[1] = expr.tokenizer.expression[expr.tokenizer.index];
      operator_[2] = NUL;

      /* Search for this two-character operator. */
      if (operator_[0] != NUL && operator_[1] != NUL)
        {
          token = expr_multichar_search (operator_, OPERATOR_TOKENS);
          if (token != TOK_NONE)
            {
              /* Matched, so advance expression index and return this token. */
              expr.tokenizer.index++;
              return token;
            }
        }

      /*
       * No match, or at last expression character; return a single character
       * token.
       */
      return c;
    }
}

static scr_int
expr_next_token (void)
{
  scr_int token;
  scr_vartype_t token_value;

  /*
   * Get the basic next token.  We may adjust it later for unary minus/plus
   * depending on what it is, and the prior token.
   */
  token_value.voidp = nullptr;
  token = expr_next_token_unadjusted (&token_value);

  /* Special handling for unary minus/plus signs. */
  if (token == TOK_SUBTRACT || token == TOK_ADD)
    {
      /*
       * Unary minus/plus if prior token was an operator or a comparison, left
       * parenthesis, or comma, or if there was no prior token.
       */
      switch (expr.tokenizer.current_token)
        {
        case TOK_MOD:
        case TOK_POWER:
        case TOK_ADD:
        case TOK_SUBTRACT:
        case TOK_MULTIPLY:
        case TOK_DIVIDE:
        case TOK_AND:
        case TOK_OR:
        case TOK_EQUAL:
        case TOK_GREATER:
        case TOK_LESS:
        case TOK_NOT_EQUAL:
        case TOK_GREATER_EQ:
        case TOK_LESS_EQ:
        case TOK_LPAREN:
        case TOK_COMMA:
        case TOK_NONE:
          token = (token == TOK_SUBTRACT) ? TOK_UMINUS : TOK_UPLUS;
          break;

        default:
          break;
        }
    }

  /* Set current token to the one just found, and return it. */
  expr.tokenizer.current_token = token;
  expr.tokenizer.token_value = token_value;
  return token;
}


/*
 * expr_current_token_value()
 *
 * Return the token value of the current token.  Undefined if the current
 * token is not numeric, an id, or a variable.
 */
static void
expr_current_token_value (scr_vartype_t *value)
{
  /* Quick check that the value is a valid one. */
  switch (expr.tokenizer.current_token)
    {
    case TOK_INTEGER:
    case TOK_STRING:
    case TOK_VARIABLE:
    case TOK_IDENT:
      break;

    default:
      scr_fatal ("expr_current_token_value:"
                " taking undefined token value, %ld\n",
                expr.tokenizer.current_token);
    }

  /* Return value. */
  *value = expr.tokenizer.token_value;
}


/*
 * expr_eval_start()
 *
 * Reset the evaluation stack to an empty state, and register the variables
 * set to use when referencing %...% variables.
 */
static void
expr_eval_start (scr_var_setref_t vars)
{
  expr.eval.stack_index = 0;
  expr.eval.varset = vars;
}


/*
 * expr_eval_garbage_collect()
 *
 * In case of parse error, empty out and free all collectible malloced
 * strings left in the evaluation array.
 */
static void
expr_eval_garbage_collect (void)
{
  scr_int index_;

  /*
   * Find and free all collectible strings still in the stack.  We have to
   * free through mutable string rather than const string.
   */
  for (index_ = 0; index_ < expr.eval.stack_index; index_++)
    {
      if (expr.eval.stack[index_].is_collectible)
        scr_free (expr.eval.stack[index_].value.mutable_string);
    }

  /* Reset the stack index, for clarity and neatness. */
  expr.eval.stack_index = 0;
}


/*
 * expr_eval_push_integer()
 * expr_eval_push_string()
 * expr_eval_push_alloced_string()
 *
 * Push a value onto the values stack.  Strings are malloc'ed and copied,
 * and the copy is placed onto the stack, unless _alloced_string() is used;
 * for this case, the input string is assumed to be already malloc'ed, and
 * the caller should not subsequently free the string.
 */
static void
expr_eval_push_integer (scr_int value)
{
  if (expr.eval.stack_index >= MAX_NESTING_DEPTH)
    {
      scr_error ("expr_eval_push_integer: stack overflow\n");
      scr_longjmp (expr.parse.error, 1);
    }

  expr.eval.stack[expr.eval.stack_index].is_collectible = FALSE;
  expr.eval.stack[expr.eval.stack_index++].value.integer = value;
}

static void
expr_eval_push_string (const scr_char *value)
{
  scr_char *value_copy;

  if (expr.eval.stack_index >= MAX_NESTING_DEPTH)
    {
      scr_error ("expr_eval_push_string: stack overflow\n");
      scr_longjmp (expr.parse.error, 1);
    }

  /* Push a copy of value. */
  value_copy = scr_strdup (value);
  expr.eval.stack[expr.eval.stack_index].is_collectible = TRUE;
  expr.eval.stack[expr.eval.stack_index++].value.mutable_string = value_copy;
}

static void
expr_eval_push_alloced_string (scr_char *value)
{
  if (expr.eval.stack_index >= MAX_NESTING_DEPTH)
    {
      scr_error ("expr_eval_push_alloced_string: stack overflow\n");
      scr_longjmp (expr.parse.error, 1);
    }

  expr.eval.stack[expr.eval.stack_index].is_collectible = TRUE;
  expr.eval.stack[expr.eval.stack_index++].value.mutable_string = value;
}


/*
 * expr_eval_pop_integer()
 * expr_eval_pop_string()
 *
 * Pop values off the values stack.  Returned strings are malloc'ed copies,
 * and the caller is responsible for freeing them.
 */
static scr_int
expr_eval_pop_integer (void)
{
  if (expr.eval.stack_index == 0)
    scr_fatal ("expr_eval_pop_integer: stack underflow\n");

  assert (!expr.eval.stack[expr.eval.stack_index - 1].is_collectible);
  return expr.eval.stack[--expr.eval.stack_index].value.integer;
}

static scr_char *
expr_eval_pop_string (void)
{
  if (expr.eval.stack_index == 0)
    scr_fatal ("expr_eval_pop_string: stack underflow\n");

  /* Returns mutable string rather than const string. */
  assert (expr.eval.stack[expr.eval.stack_index - 1].is_collectible);
  return expr.eval.stack[--expr.eval.stack_index].value.mutable_string;
}


/*
 * expr_eval_result()
 *
 * Return the top of the values stack as the expression result.
 */
static void
expr_eval_result (scr_vartype_t *vt_rvalue)
{
  if (expr.eval.stack_index != 1)
    scr_fatal ("expr_eval_result: values stack not completed\n");

  /* Clear down stack and return the top value. */
  expr.eval.stack_index = 0;
  *vt_rvalue = expr.eval.stack[0].value;
}


/*
 * expr_eval_add()
 * expr_eval_subtract()
 * expr_eval_multiply()
 * expr_eval_negate()
 *
 * Integer arithmetic that wraps on overflow, done in the unsigned domain
 * because signed overflow is undefined behaviour and game data can hold any
 * value.
 */
static scr_int
expr_eval_add (scr_int left, scr_int right)
{
  return (scr_int) ((scr_uint) left + (scr_uint) right);
}

static scr_int
expr_eval_subtract (scr_int left, scr_int right)
{
  return (scr_int) ((scr_uint) left - (scr_uint) right);
}

static scr_int
expr_eval_multiply (scr_int left, scr_int right)
{
  return (scr_int) ((scr_uint) left * (scr_uint) right);
}

static scr_int
expr_eval_negate (scr_int value)
{
  return (scr_int) (0 - (scr_uint) value);
}


/*
 * expr_eval_abs()
 *
 * Return the absolute value of the given scr_int.  Replacement for labs(),
 * avoids tying scr_int to long types too closely.
 */
static scr_int
expr_eval_abs (scr_int value)
{
  return value < 0 ? expr_eval_negate (value) : value;
}


/*
 * expr_eval_push_value()
 *
 * Push the value of an integer, string, or variable token.
 */
static void
expr_eval_push_value (scr_int token)
{
  scr_vartype_t token_value;

  switch (token)
    {
    case TOK_INTEGER:
      expr_current_token_value (&token_value);
      expr_eval_push_integer (token_value.integer);
      break;

    case TOK_STRING:
      expr_current_token_value (&token_value);
      expr_eval_push_string (token_value.string);
      break;

    case TOK_VARIABLE:
      {
        scr_vartype_t vt_rvalue;
        scr_int type;

        expr_current_token_value (&token_value);
        if (!var_get (expr.eval.varset, token_value.string, &type, &vt_rvalue))
          {
            scr_error ("expr_eval_action:"
                      " undefined variable, %s\n", token_value.string);
            scr_longjmp (expr.parse.error, 1);
          }
        switch (type)
          {
          case VAR_INTEGER:
            expr_eval_push_integer (vt_rvalue.integer);
            break;

          case VAR_STRING:
            expr_eval_push_string (vt_rvalue.string);
            break;

          default:
            scr_fatal ("expr_eval_action: bad variable type\n");
          }
        break;
      }

    default:
      scr_fatal ("expr_eval_action: bad token, %ld\n", token);
    }
}


/*
 * expr_eval_numeric_function()
 *
 * Evaluate a function returning numeric: if, max, min, either, instr, len,
 * and val.
 */
static void
expr_eval_numeric_function (scr_int token)
{
  switch (token)
    {
    case TOK_IF:
      {
        scr_int test, val1, val2;

        /* Pop the test and alternatives, and push back result. */
        val2 = expr_eval_pop_integer ();
        val1 = expr_eval_pop_integer ();
        test = expr_eval_pop_integer ();
        expr_eval_push_integer (test ? val1 : val2);
        break;
      }

    case TOK_MAX:
    case TOK_MIN:
      {
        scr_int argument_count, index_, result;

        /* Get argument count off the top of the stack. */
        argument_count = expr_eval_pop_integer ();
        assert (argument_count > 0);

        /* Find the max or min of these stacked values. */
        result = expr_eval_pop_integer ();
        for (index_ = 1; index_ < argument_count; index_++)
          {
            scr_int next;

            next = expr_eval_pop_integer ();
            switch (token)
              {
              case TOK_MAX:
                result = (next > result) ? next : result;
                break;

              case TOK_MIN:
                result = (next < result) ? next : result;
                break;

              default:
                scr_fatal ("expr_eval_action: bad token, %ld\n", token);
              }
          }

        /* Push back the result. */
        expr_eval_push_integer (result);
        break;
      }

    case TOK_EITHER:
      {
        scr_int argument_count, pick, index_;
        scr_int result = 0;

        /* Get argument count off the top of the stack. */
        argument_count = expr_eval_pop_integer ();
        assert (argument_count > 0);

        /*
         * Pick one of the top N items at random, then unstack all N and
         * push back the value of the one picked.
         */
        pick = scr_randomint (0, argument_count - 1);
        for (index_ = 0; index_ < argument_count; index_++)
          {
            scr_int val;

            val = expr_eval_pop_integer ();
            if (index_ == pick)
              result = val;
          }

        /* Push back the result. */
        expr_eval_push_integer (result);
        break;
      }

    case TOK_INSTR:
      {
        scr_char *val1, *val2, *search;
        scr_int result;

        /* Extract the two values to work on. */
        val2 = expr_eval_pop_string ();
        val1 = expr_eval_pop_string ();

        /*
         * Search for the second in the first.  The result is the character
         * position, starting at 1, or 0 if not found.  Then free the popped
         * strings, and push back the result.
         */
        search = (val1[0] != NUL) ? strstr (val1, val2) : nullptr;
        result = (!search) ? 0 : search - val1 + 1;
        scr_free (val1);
        scr_free (val2);
        expr_eval_push_integer (result);
        break;
      }

    case TOK_LEN:
      {
        scr_char *val;
        scr_int result;

        /* Pop the top string, and push back its length. */
        val = expr_eval_pop_string ();
        result = strlen (val);
        scr_free (val);
        expr_eval_push_integer (result);
        break;
      }

    case TOK_VAL:
      {
        scr_char *val;
        scr_int result = 0;

        /*
         * Extract the string at stack top, and try to convert, returning
         * zero if conversion fails.  Free the popped string, and push back
         * the result.
         */
        val = expr_eval_pop_string ();
        sscanf (val, "%ld", &result);
        scr_free (val);
        expr_eval_push_integer (result);
        break;
      }

    default:
      scr_fatal ("expr_eval_action: bad token, %ld\n", token);
    }
}


/*
 * expr_eval_numeric_operator()
 *
 * Evaluate a unary numeric operation, or any binary one but division, mod,
 * and power.
 */
static void
expr_eval_numeric_operator (scr_int token)
{
  switch (token)
    {
    case TOK_UMINUS:
      expr_eval_push_integer (expr_eval_negate (expr_eval_pop_integer ()));
      break;

    case TOK_UPLUS:
      break;

    case TOK_ABS:
      expr_eval_push_integer (expr_eval_abs (expr_eval_pop_integer ()));
      break;

    case TOK_ADD:
    case TOK_SUBTRACT:
    case TOK_MULTIPLY:
    case TOK_AND:
    case TOK_OR:
    case TOK_EQUAL:
    case TOK_GREATER:
    case TOK_LESS:
    case TOK_NOT_EQUAL:
    case TOK_GREATER_EQ:
    case TOK_LESS_EQ:
    case TOK_RANDOM:
      {
        scr_int val1, val2, result = 0;

        /* Extract the two values to work on. */
        val2 = expr_eval_pop_integer ();
        val1 = expr_eval_pop_integer ();

        /* Generate the result value. */
        switch (token)
          {
          case TOK_ADD:
            result = expr_eval_add (val1, val2);
            break;
          case TOK_SUBTRACT:
            result = expr_eval_subtract (val1, val2);
            break;
          case TOK_MULTIPLY:
            result = expr_eval_multiply (val1, val2);
            break;
          case TOK_AND:
            result = val1 && val2;
            break;
          case TOK_OR:
            result = val1 || val2;
            break;
          case TOK_EQUAL:
            result = val1 == val2;
            break;
          case TOK_GREATER:
            result = val1 > val2;
            break;
          case TOK_LESS:
            result = val1 < val2;
            break;
          case TOK_NOT_EQUAL:
            result = val1 != val2;
            break;
          case TOK_GREATER_EQ:
            result = val1 >= val2;
            break;
          case TOK_LESS_EQ:
            result = val1 <= val2;
            break;
          case TOK_RANDOM:
            result = scr_randomint (val1, val2);
            break;
          default:
            scr_fatal ("expr_eval_action: bad token, %ld\n", token);
          }

        /* Put result back at top of stack. */
        expr_eval_push_integer (result);
        break;
      }

    default:
      scr_fatal ("expr_eval_action: bad token, %ld\n", token);
    }
}


/*
 * expr_eval_division()
 *
 * Evaluate division or mod, handled separately since they're "eccentric".
 */
static void
expr_eval_division (scr_int token)
{
  switch (token)
    {
    case TOK_DIVIDE:
    case TOK_MOD:
      {
        scr_int val1, val2, result = 0;
        scr_uint x, y, quotient, remainder;

        /* Extract the two values to work on, complain about division by 0. */
        val2 = expr_eval_pop_integer ();
        val1 = expr_eval_pop_integer ();
        if (val2 == 0)
          {
            scr_error ("expr_eval_action: attempt to divide by zero\n");
            expr_eval_push_integer (result);
            break;
          }

        /*
         * ANSI/ISO C only defines integer division for positive values.
         * Negative values usually work consistently across platforms, but are
         * not guaranteed.  For maximum portability, then, here we'll work
         * carefully with positive integers only -- unsigned magnitudes, so
         * that the most negative value has one too.
         */
        x = val1 < 0 ? 0 - (scr_uint) val1 : (scr_uint) val1;
        y = val2 < 0 ? 0 - (scr_uint) val2 : (scr_uint) val2;
        quotient = x / y;
        remainder = x % y;

        /* Generate the result value. */
        switch (token)
          {
          case TOK_DIVIDE:
            /*
             * Adrift's division apparently works by dividing using floating
             * point, then applying (asymmetrical) rounding, so we have to do
             * the same here.
             */
            if ((val1 < 0) == (val2 < 0))
              result = (scr_int) (quotient
                                  + (remainder >= y - remainder ? 1 : 0));
            else
              result = expr_eval_negate ((scr_int) (quotient
                                  + (remainder > y - remainder ? 1 : 0)));
            break;

          case TOK_MOD:
            /*
             * Adrift also breaks numerical consistency by defining mod in a
             * conventional (non-rounded), way, so that A=(AdivB)*B+AmodB
             * does not hold.
             */
            result = (val1 < 0) ? expr_eval_negate ((scr_int) remainder)
                                : (scr_int) remainder;
            break;

          default:
            scr_fatal ("expr_eval_action: bad token, %ld\n", token);
          }

        /* Put result back at top of stack. */
        expr_eval_push_integer (result);
        break;
      }

    default:
      scr_fatal ("expr_eval_action: bad token, %ld\n", token);
    }
}


/*
 * expr_eval_power()
 *
 * Evaluate power, handled individually to avoid needing a maths library.
 */
static void
expr_eval_power (void)
{
  scr_int val1, val2, result;

  /* Extract the two values to work on. */
  val2 = expr_eval_pop_integer ();
  val1 = expr_eval_pop_integer ();

  /* Handle negative and zero power values first, as special cases. */
  if (val2 == 0)
    result = 1;
  else if (val2 < 0)
    {
      if (val1 == 0)
        {
          scr_error ("expr_eval_action: attempt to divide by zero\n");
          result = 0;
        }
      else if (val1 == 1)
        result = val1;
      else if (val1 == -1)
        result = (val2 & 1) ? val1 : -val1;
      else
        result = 0;
    }
  else
    {
      /* Raise to positive powers using the Russian Peasant algorithm. */
      while ((val2 & 1) == 0)
        {
          val1 = expr_eval_multiply (val1, val1);
          val2 >>= 1;
        }

      result = val1;
      val2 >>= 1;
      while (val2 > 0)
        {
          val1 = expr_eval_multiply (val1, val1);
          if (val2 & 1)
            result = expr_eval_multiply (result, val1);
          val2 >>= 1;
        }
    }

  /* Put result back at top of stack. */
  expr_eval_push_integer (result);
}


/*
 * expr_eval_string_function()
 *
 * Evaluate a function returning string: left, right, mid, and str.
 */
static void
expr_eval_string_function (scr_int token)
{
  switch (token)
    {
    case TOK_LEFT:
    case TOK_RIGHT:
      {
        scr_char *text;
        scr_int length;

        /*
         * Extract the text and length.  If length is longer than text, or
         * negative, do nothing.
         */
        length = expr_eval_pop_integer ();
        text = expr_eval_pop_string ();
        if (length < 0 || length >= (scr_int) strlen (text))
          {
            expr_eval_push_alloced_string (text);
            break;
          }

        /*
         * Take the left or right segment -- for left, the operation is a
         * simple truncation; for right, it's a memmove.
         */
        switch (token)
          {
          case TOK_LEFT:
            text[length] = NUL;
            break;

          case TOK_RIGHT:
            memmove (text, text + strlen (text) - length, length + 1);
            break;

          default:
            scr_fatal ("expr_eval_action: bad token, %ld\n", token);
          }

        /* Put result back at top of stack. */
        expr_eval_push_alloced_string (text);
        break;
      }

    case TOK_MID:
      {
        scr_char *text;
        scr_int length, start, limit;

        /*
         * Extract the text, start, and length, re-basing start from 1 to 0,
         * and calculate the limit on characters available for the move.
         */
        length = expr_eval_pop_integer ();
        start = expr_eval_pop_integer () - 1;
        text = expr_eval_pop_string ();
        limit = strlen (text);

        /*
         * Clamp ranges that roam outside the available text -- start less
         * than 0 to 0, and greater than len(text) to len(text), and length
         * less than 0 to 0, and off string end to string end.
         */
        if (start < 0)
          start = 0;
        else if (start > limit)
          start = limit;
        if (length < 0)
          length = 0;
        else if (length > limit - start)
          length = limit - start;

        /* Move substring, terminate, and put back at top of stack. */
        memmove (text, text + start, length + 1);
        text[length] = NUL;
        expr_eval_push_alloced_string (text);
        break;
      }

    case TOK_STR:
      {
        scr_int val;
        scr_char buffer[32];

        /*
         * Extract the value, convert it, and push back the resulting string.
         * The leading space on positive values matches the Runner.
         */
        val = expr_eval_pop_integer ();
        snprintf (buffer, sizeof (buffer), "% ld", val);
        expr_eval_push_string (buffer);
        break;
      }

    default:
      scr_fatal ("expr_eval_action: bad token, %ld\n", token);
    }
}


/*
 * expr_eval_string_operator()
 *
 * Evaluate a string operation: upper, lower, proper, and concatenation.
 */
static void
expr_eval_string_operator (scr_int token)
{
  switch (token)
    {
    case TOK_UPPER:
    case TOK_LOWER:
    case TOK_PROPER:
      {
        scr_char *text;
        scr_int index_;

        /* Extract the value to work on. */
        text = expr_eval_pop_string ();

        /* Convert the entire string in place -- it's malloc'ed. */
        for (index_ = 0; text[index_] != NUL; index_++)
          {
            switch (token)
              {
              case TOK_UPPER:
                text[index_] = scr_toupper (text[index_]);
                break;

              case TOK_LOWER:
                text[index_] = scr_tolower (text[index_]);
                break;

              case TOK_PROPER:
                if (index_ == 0 || scr_isspace (text[index_ - 1]))
                  text[index_] = scr_toupper (text[index_]);
                else
                  text[index_] = scr_tolower (text[index_]);
                break;

              default:
                scr_fatal ("expr_eval_action: bad token, %ld\n", token);
              }
          }

        /* Put result back at top of stack. */
        expr_eval_push_alloced_string (text);
        break;
      }

    case TOK_CONCATENATE:
      {
        scr_char *text1, *text2;

        /* Extract the two texts to work on. */
        text2 = expr_eval_pop_string ();
        text1 = expr_eval_pop_string ();

        /*
         * Resize text1 to be long enough for both, and concatenate, then
         * free text2, and push back the concatenation.
         */
        {
          size_t used = strlen (text1);
          size_t size = used + strlen (text2) + 1;

          text1 = (decltype(text1)) scr_realloc (text1, size);
          snprintf (text1 + used, size - used, "%s", text2);
        }
        scr_free (text2);
        expr_eval_push_alloced_string (text1);
        break;
      }

    default:
      scr_fatal ("expr_eval_action: bad token, %ld\n", token);
    }
}


/*
 * expr_eval_action()
 *
 * Evaluate the effect of a token into the values stack.
 */
static void
expr_eval_action (scr_int token)
{
  switch (token)
    {
    case TOK_INTEGER:
    case TOK_STRING:
    case TOK_VARIABLE:
      expr_eval_push_value (token);
      break;

    case TOK_IF:
    case TOK_MAX:
    case TOK_MIN:
    case TOK_EITHER:
    case TOK_INSTR:
    case TOK_LEN:
    case TOK_VAL:
      expr_eval_numeric_function (token);
      break;

    case TOK_UMINUS:
    case TOK_UPLUS:
    case TOK_ABS:
    case TOK_ADD:
    case TOK_SUBTRACT:
    case TOK_MULTIPLY:
    case TOK_AND:
    case TOK_OR:
    case TOK_EQUAL:
    case TOK_GREATER:
    case TOK_LESS:
    case TOK_NOT_EQUAL:
    case TOK_GREATER_EQ:
    case TOK_LESS_EQ:
    case TOK_RANDOM:
      expr_eval_numeric_operator (token);
      break;

    case TOK_DIVIDE:
    case TOK_MOD:
      expr_eval_division (token);
      break;

    case TOK_POWER:
      expr_eval_power ();
      break;

    case TOK_LEFT:
    case TOK_RIGHT:
    case TOK_MID:
    case TOK_STR:
      expr_eval_string_function (token);
      break;

    case TOK_UPPER:
    case TOK_LOWER:
    case TOK_PROPER:
    case TOK_CONCATENATE:
      expr_eval_string_operator (token);
      break;

    default:
      scr_fatal ("expr_eval_action: bad token, %ld\n", token);
    }
}


/* Forward declaration of factor parsers and string expression parser. */
static void expr_parse_numeric_factor (void);
static void expr_parse_string_factor (void);
static void expr_parse_string_expr (void);


/*
 * expr_parse_match()
 *
 * Match a token to the lookahead, then advance lookahead.
 */
static void
expr_parse_match (scr_int token)
{
  if (expr.parse.lookahead == token)
    expr.parse.lookahead = expr_next_token ();
  else
    {
      /* Syntax error. */
      scr_error ("expr_parse_match: syntax error,"
                " expected %ld, got %ld\n", token, expr.parse.lookahead);
      scr_longjmp (expr.parse.error, 1);
    }
}


/*
 * Numeric operator precedence table.  Operators are in order of precedence,
 * with the highest being a factor.  Each precedence entry permits several
 * listed tokens.  The end of the table (highest precedence) is marked by
 * a list with no operators (although in practice we need to put a TOK_NONE
 * in here since some C compilers won't accept { } as an empty initializer).
 */
typedef struct
{
  const scr_int token_count;
  const scr_int tokens[6];
} scr_precedence_entry_t;


/*
 * Adrift-like precedence table for the parser.  Exponentiation and modulus
 * operations seem to be implemented at the same level as addition and
 * subtraction, and boolean 'and' and 'or' have equal precedence.  This is
 * deliberately not the conventional (BASIC, C) table, where exponentiation
 * binds tightest, then multiplicative, additive, comparison, and boolean
 * combiners in that order; Adrift's evaluator does not do that.
 */
static const scr_precedence_entry_t PRECEDENCE_TABLE[] = {
  {2, {TOK_OR, TOK_AND}},
  {6, {TOK_EQUAL, TOK_NOT_EQUAL,
       TOK_GREATER, TOK_LESS, TOK_GREATER_EQ, TOK_LESS_EQ}},
  {4, {TOK_ADD, TOK_SUBTRACT, TOK_POWER, TOK_MOD}},
  {2, {TOK_MULTIPLY, TOK_DIVIDE}},
  {0, {TOK_NONE}}
};


/*
 * expr_parse_contains_token()
 *
 * Helper for expr_parse_numeric_element().  Search the token list for the
 * entry passed in, and return TRUE if it contains the given token.
 */
static int
expr_parse_contains_token (const scr_precedence_entry_t *entry, scr_int token)
{
  scr_bool is_matched;
  scr_int index_;

  /* Search the entry's token list for the token passed in. */
  is_matched = FALSE;
  for (index_ = 0; index_ < entry->token_count; index_++)
    {
      if (entry->tokens[index_] == token)
        {
          is_matched = TRUE;
          break;
        }
    }

  return is_matched;
}


/*
 * expr_parse_numeric_element()
 *
 * Parse numeric expression elements.  This function uses the precedence table
 * to match tokens, then decide whether, and how, to recurse into itself, or
 * whether to parse a highest-precedence factor.
 */
static void
expr_parse_numeric_element (scr_int precedence)
{
  const scr_precedence_entry_t *entry;

  /* See if the level passed in has listed tokens. */
  entry = PRECEDENCE_TABLE + precedence;
  if (entry->token_count == 0)
    {
      /* Precedence levels that hit the table end are factors. */
      expr_parse_numeric_factor ();
      return;
    }

  /*
   * Parse initial higher-precedence factor, then others that associate
   * with the given level.
   */
  expr_parse_numeric_element (precedence + 1);
  while (expr_parse_contains_token (entry, expr.parse.lookahead))
    {
      scr_int token;

      /* Note token and match, parse next level, then action this token. */
      token = expr.parse.lookahead;
      expr_parse_match (token);
      expr_parse_numeric_element (precedence + 1);
      expr_eval_action (token);
    }
}


/*
 * expr_parse_numeric_expr()
 *
 * Parse a complete numeric (sub-)expression.
 */
static void
expr_parse_numeric_expr (void)
{
  /* Call the parser of the lowest precedence operators. */
  expr_parse_numeric_element (0);
}


/*
 * expr_parse_variable_factor()
 *
 * Parse a variable as a numeric or string factor, rejecting one that is
 * undefined or of the other type.  `caller` and `mismatch` only word the
 * error trace.
 */
static void
expr_parse_variable_factor (const scr_char *caller, scr_int want,
                            const scr_char *mismatch)
{
  scr_vartype_t token_value, vt_rvalue;
  scr_int type;

  expr_current_token_value (&token_value);
  if (!var_get (expr.eval.varset, token_value.string, &type, &vt_rvalue))
    {
      scr_error ("%s: undefined variable, %s\n", caller, token_value.string);
      scr_longjmp (expr.parse.error, 1);
    }
  if (type != want)
    {
      scr_error ("%s: %s, %s\n", caller, mismatch, token_value.string);
      scr_longjmp (expr.parse.error, 1);
    }
  expr_eval_action (TOK_VARIABLE);
  expr_parse_match (TOK_VARIABLE);
}


/*
 * expr_parse_enter()
 *
 * Count a factor level on the way in, and fail the parse past the limit, so
 * that a run of '(' or unary signs cannot recurse without bound.  The
 * factor parsers count back down on the way out; a parse error skips that,
 * but every evaluation resets the count.
 */
static void
expr_parse_enter (void)
{
  if (++expr.parse.depth > MAX_NESTING_DEPTH)
    {
      scr_error ("expr_parse: expression nested too deeply\n");
      scr_longjmp (expr.parse.error, 1);
    }
}


/*
 * expr_parse_numeric_factor()
 *
 * Parse a numeric expression factor.
 */
static void
expr_parse_numeric_factor (void)
{
  expr_parse_enter ();

  /* Handle factors based on lookahead token. */
  switch (expr.parse.lookahead)
    {
      /* Handle straightforward factors first. */
    case TOK_LPAREN:
      expr_parse_match (TOK_LPAREN);
      expr_parse_numeric_expr ();
      expr_parse_match (TOK_RPAREN);
      break;

    case TOK_UMINUS:
      expr_parse_match (TOK_UMINUS);
      expr_parse_numeric_factor ();
      expr_eval_action (TOK_UMINUS);
      break;

    case TOK_UPLUS:
      expr_parse_match (TOK_UPLUS);
      expr_parse_numeric_factor ();
      break;

    case TOK_INTEGER:
      expr_eval_action (TOK_INTEGER);
      expr_parse_match (TOK_INTEGER);
      break;

    case TOK_VARIABLE:
      expr_parse_variable_factor ("expr_parse_numeric_factor", VAR_INTEGER,
                                  "string variable in numeric context");
      break;

      /* Handle functions as factors. */
    case TOK_ABS:
      /* Parse as "abs (val)". */
      expr_parse_match (TOK_ABS);
      expr_parse_match (TOK_LPAREN);
      expr_parse_numeric_expr ();
      expr_parse_match (TOK_RPAREN);
      expr_eval_action (TOK_ABS);
      break;

    case TOK_IF:
      /* Parse as "if (boolean, val1, val2)". */
      expr_parse_match (TOK_IF);
      expr_parse_match (TOK_LPAREN);
      expr_parse_numeric_expr ();
      expr_parse_match (TOK_COMMA);
      expr_parse_numeric_expr ();
      expr_parse_match (TOK_COMMA);
      expr_parse_numeric_expr ();
      expr_parse_match (TOK_RPAREN);
      expr_eval_action (TOK_IF);
      break;

    case TOK_RANDOM:
      /* Parse as "random (low, high)". */
      expr_parse_match (TOK_RANDOM);
      expr_parse_match (TOK_LPAREN);
      expr_parse_numeric_expr ();
      expr_parse_match (TOK_COMMA);
      expr_parse_numeric_expr ();
      expr_parse_match (TOK_RPAREN);
      expr_eval_action (TOK_RANDOM);
      break;

    case TOK_MAX:
    case TOK_MIN:
    case TOK_EITHER:
      /* Parse as "<func> (val1[,val2[,val3...]]])". */
      {
        scr_int token, argument_count;

        /* Match up the function name and opening parenthesis. */
        token = expr.parse.lookahead;
        expr_parse_match (token);
        expr_parse_match (TOK_LPAREN);

        /* Count variable number of arguments as they are stacked. */
        expr_parse_numeric_expr ();
        argument_count = 1;
        while (expr.parse.lookahead == TOK_COMMA)
          {
            expr_parse_match (TOK_COMMA);
            expr_parse_numeric_expr ();
            argument_count++;
          }
        expr_parse_match (TOK_RPAREN);

        /* Push additional value -- the count of arguments. */
        expr_eval_push_integer (argument_count);
        expr_eval_action (token);
        break;
      }

    case TOK_INSTR:
      /* Parse as "instr (val1, val2)". */
      expr_parse_match (TOK_INSTR);
      expr_parse_match (TOK_LPAREN);
      expr_parse_string_expr ();
      expr_parse_match (TOK_COMMA);
      expr_parse_string_expr ();
      expr_parse_match (TOK_RPAREN);
      expr_eval_action (TOK_INSTR);
      break;

    case TOK_LEN:
      /* Parse as "len (val)". */
      expr_parse_match (TOK_LEN);
      expr_parse_match (TOK_LPAREN);
      expr_parse_string_expr ();
      expr_parse_match (TOK_RPAREN);
      expr_eval_action (TOK_LEN);
      break;

    case TOK_VAL:
      /* Parse as "val (val)". */
      expr_parse_match (TOK_VAL);
      expr_parse_match (TOK_LPAREN);
      expr_parse_string_expr ();
      expr_parse_match (TOK_RPAREN);
      expr_eval_action (TOK_VAL);
      break;

    case TOK_IDENT:
      /* Unrecognized function-type token. */
      scr_error ("expr_parse_numeric_factor: syntax error, unknown ident\n");
      scr_longjmp (expr.parse.error, 1);

    default:
      /* Syntax error. */
      scr_error ("expr_parse_numeric_factor:"
                " syntax error, unexpected token, %ld\n", expr.parse.lookahead);
      scr_longjmp (expr.parse.error, 1);
    }

  expr.parse.depth--;
}


/*
 * expr_parse_string_expr()
 *
 * Parse a complete string (sub-)expression.
 */
static void
expr_parse_string_expr (void)
{
  /*
   * Parse a string factor, then all repeated concatenations.  Because the '+'
   * and '&' are context sensitive, we have to invent/translate them into the
   * otherwise unused TOK_CONCATENATE for evaluation.
   */
  expr_parse_string_factor ();
  while (expr.parse.lookahead == TOK_AND || expr.parse.lookahead == TOK_ADD)
    {
      expr_parse_match (expr.parse.lookahead);
      expr_parse_string_factor ();
      expr_eval_action (TOK_CONCATENATE);
    }
}


/*
 * expr_parse_string_factor()
 *
 * Parse a string expression factor.
 */
static void
expr_parse_string_factor (void)
{
  expr_parse_enter ();

  /* Handle factors based on lookahead token. */
  switch (expr.parse.lookahead)
    {
      /* Handle straightforward factors first. */
    case TOK_LPAREN:
      expr_parse_match (TOK_LPAREN);
      expr_parse_string_expr ();
      expr_parse_match (TOK_RPAREN);
      break;

    case TOK_STRING:
      expr_eval_action (TOK_STRING);
      expr_parse_match (TOK_STRING);
      break;

    case TOK_VARIABLE:
      expr_parse_variable_factor ("expr_parse_string_factor", VAR_STRING,
                                  "numeric variable in string context");
      break;

      /* Handle functions as factors. */
    case TOK_UPPER:
    case TOK_LOWER:
    case TOK_PROPER:
      /* Parse as "<func> (text)". */
      {
        scr_int token;

        token = expr.parse.lookahead;
        expr_parse_match (token);
        expr_parse_match (TOK_LPAREN);
        expr_parse_string_expr ();
        expr_parse_match (TOK_RPAREN);
        expr_eval_action (token);
        break;
      }

    case TOK_LEFT:
    case TOK_RIGHT:
      /* Parse as "<func> (text,length)". */
      {
        scr_int token;

        token = expr.parse.lookahead;
        expr_parse_match (token);
        expr_parse_match (TOK_LPAREN);
        expr_parse_string_expr ();
        expr_parse_match (TOK_COMMA);
        expr_parse_numeric_expr ();
        expr_parse_match (TOK_RPAREN);
        expr_eval_action (token);
        break;
      }

    case TOK_MID:
      /* Parse as "mid (text,start,length)". */
      expr_parse_match (TOK_MID);
      expr_parse_match (TOK_LPAREN);
      expr_parse_string_expr ();
      expr_parse_match (TOK_COMMA);
      expr_parse_numeric_expr ();
      expr_parse_match (TOK_COMMA);
      expr_parse_numeric_expr ();
      expr_parse_match (TOK_RPAREN);
      expr_eval_action (TOK_MID);
      break;

    case TOK_STR:
      /* Parse as "str (val)". */
      expr_parse_match (TOK_STR);
      expr_parse_match (TOK_LPAREN);
      expr_parse_numeric_expr ();
      expr_parse_match (TOK_RPAREN);
      expr_eval_action (TOK_STR);
      break;

    case TOK_IDENT:
      /* Unrecognized function-type token. */
      scr_error ("expr_parse_string_factor: syntax error, unknown ident\n");
      scr_longjmp (expr.parse.error, 1);

    default:
      /* Syntax error. */
      scr_error ("expr_parse_string_factor:"
                " syntax error, unexpected token, %ld\n", expr.parse.lookahead);
      scr_longjmp (expr.parse.error, 1);
    }

  expr.parse.depth--;
}


/*
 * expr_evaluate_expression()
 *
 * Parse a string expression into a runtime values stack.  Return the
 * value of the expression.
 */
static scr_bool
expr_evaluate_expression (const scr_char *expression, scr_var_setref_t vars,
                          scr_int assign_type, scr_vartype_t *vt_rvalue)
{
  assert (assign_type == VAR_INTEGER || assign_type == VAR_STRING);

  /*
   * Reset values stack and start the tokenizer; the guard ends it on any
   * exit from this function, a thrown scr_fatal_error included.
   */
  expr_eval_start (vars);
  const expr_tokenize_guard tokenizer (expression);

  /* Try parsing an expression, and catch errors. */
  expr.parse.depth = 0;
  if (scr_setjmp (expr.parse.error) == 0)
    {
      /* Parse an expression, and ensure it ends at string end. */
      expr.parse.lookahead = expr_next_token ();
      if (assign_type == VAR_STRING)
        expr_parse_string_expr ();
      else
        expr_parse_numeric_expr ();
      expr_parse_match (TOK_EOS);
    }
  else
    {
      /* Parse error -- collect garbage and fail. */
      expr_eval_garbage_collect ();
      return FALSE;
    }

  /* Return successfully with result. */
  expr_eval_result (vt_rvalue);
  return TRUE;
}


/*
 * expr_eval_numeric_expression()
 * expr_eval_string_expression()
 *
 * Public interfaces to expression evaluation.  Evaluate an expression, and
 * assign the result to either a numeric or a string.  For string expressions,
 * the return value is malloc'ed, and the caller is responsible for freeing
 * it.
 */
scr_bool
expr_eval_numeric_expression (const scr_char *expression,
                              scr_var_setref_t vars, scr_int *rvalue)
{
  scr_vartype_t vt_rvalue;
  scr_bool status;

  assert (expression && vars && rvalue);

  /* Evaluate numeric expression, and return value if valid. */
  status = expr_evaluate_expression (expression, vars, VAR_INTEGER, &vt_rvalue);
  if (status)
    *rvalue = vt_rvalue.integer;
  return status;
}

scr_bool
expr_eval_string_expression (const scr_char *expression,
                             scr_var_setref_t vars, scr_char **rvalue)
{
  scr_vartype_t vt_rvalue;
  scr_bool status;

  assert (expression && vars && rvalue);

  /* Evaluate string expression, and return value if valid. */
  status = expr_evaluate_expression (expression, vars, VAR_STRING, &vt_rvalue);
  if (status)
    *rvalue = vt_rvalue.mutable_string;
  return status;
}
