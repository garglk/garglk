/* vi: set ts=8:
 *
 * Copyright (C) 2003-2005  Simon Baldwin and Mark J. Tilford
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 */

/*
 * Module notes:
 *
 * o The tokenizer doesn't differentiate between "&", "&&", and "and",
 *   but "&" is context sensitive -- either a logical and for numerics,
 *   or concatenation for strings.  As a result, "&&" and "and" also
 *   work as string concatenators when used in string expressions.  It
 *   may not be to spec, but we'll call this a "feature".
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <limits.h>
#include <setjmp.h>

#include "scare.h"
#include "scprotos.h"


/* Assorted definitions and constants. */
enum {			MAX_NESTING_DEPTH	= 32 };
static const sc_char	NUL			= '\0',
			PERCENT			= '%',
			SINGLE_QUOTE		= '\'',
			DOUBLE_QUOTE		= '"';

/* Enumerated variable types. */
enum {			VAR_INTEGER		= 'I',
			VAR_STRING		= 'S' };


/*
 * Tokens.  Single character tokens are represented by their ASCII value
 * (0-255), others by values above 255.  -1 represents a null token.  Because
 * '&' and '+' are context sensitive, the pseudo-token TOK_CONCATENATE
 * serves to indicate string concatenation -- it's never returned by the
 * tokenizer.
 */
typedef enum {
	TOK_NONE	= -1,
	TOK_ADD		= '+',	TOK_SUBTRACT	= '-',
	TOK_MULTIPLY	= '*',	TOK_DIVIDE	= '/',
	TOK_AND		= '&',	TOK_OR		= '|',
	TOK_LPAREN	= '(',	TOK_RPAREN	= ')',
	TOK_COMMA	= ',',	TOK_POWER	= '^',
	TOK_EQUAL	= '=',	TOK_GREATER	= '>',	TOK_LESS	= '<',
	TOK_IDENT	= 256,

	TOK_INTEGER,		TOK_STRING,		TOK_VARIABLE,
	TOK_UMINUS,		TOK_UPLUS,
	TOK_MOD,
	TOK_NOT_EQUAL,		TOK_GREATER_EQ,		TOK_LESS_EQ,
	TOK_IF,
	TOK_MIN,		TOK_MAX,
	TOK_EITHER,		TOK_RANDOM,
	TOK_INSTR,		TOK_LEN,		TOK_VAL,
	TOK_ABS,
	TOK_UPPER,		TOK_LOWER,		TOK_PROPER,
	TOK_RIGHT,		TOK_LEFT,		TOK_MID,
	TOK_STR,		TOK_CONCATENATE,
	TOK_EOS
} sc_expr_tok_t;

/* Small tables tying multicharacter tokens strings to tokens. */
typedef struct {
	const sc_char		*string;
	const sc_expr_tok_t	token;
} sc_expr_multichar_t;
static const sc_expr_multichar_t	FUNCTION_TOKENS[] = {
	{ "either",  TOK_EITHER     },
	{ "proper",  TOK_PROPER     },	{ "pcase",   TOK_PROPER     },
	{ "instr",   TOK_INSTR      },
	{ "upper",   TOK_UPPER      },	{ "ucase",   TOK_UPPER      },
	{ "lower",   TOK_LOWER      },	{ "lcase",   TOK_LOWER      },
	{ "right",   TOK_RIGHT      },	{ "left",    TOK_LEFT       },
	{ "rand",    TOK_RANDOM     },
	{ "max",     TOK_MAX        },	{ "min",     TOK_MIN        },
	{ "mod",     TOK_MOD        },	{ "abs",     TOK_ABS        },
	{ "len",     TOK_LEN        },	{ "val",     TOK_VAL        },
	{ "and",     TOK_AND        },
	{ "mid",     TOK_MID        },	{ "str",     TOK_STR        },
	{ "or",	     TOK_OR         },
	{ "if",	     TOK_IF         },
	{ NULL,	     TOK_NONE       }
};
static const sc_expr_multichar_t	OPERATOR_TOKENS[] = {
	{ "&&",	     TOK_AND        },	{ "||",	     TOK_OR         },
	{ "==",	     TOK_EQUAL      },	{ "!=",	     TOK_NOT_EQUAL  },
	{ "<>",	     TOK_NOT_EQUAL  },
	{ ">=",	     TOK_GREATER_EQ },
	{ "<=",	     TOK_LESS_EQ    },
	{ NULL,	     TOK_NONE       }
};


/*
 * expr_multichar_search()
 *
 * Multicharacter token table search, returns the matching token, or
 * TOK_NONE if no match.
 */
static sc_expr_tok_t
expr_multichar_search (const char *string, const sc_expr_multichar_t *table)
{
	sc_uint16	index;
	sc_expr_tok_t	token;

	/* Scan the table for a case-independent full string match. */
	token = TOK_NONE;
	for (index = 0; table[index].token != TOK_NONE; index++)
	    {
		if (!sc_strcasecmp (string, table[index].string))
		    {
			token = table[index].token;
			break;
		    }
	    }

	/* Return the token matched, or TOK_NONE. */
	return token;
}


/* Tokenizer variables. */
static sc_char		*expr_expression	= NULL;
static sc_uint32	expr_index		= 0;
static sc_vartype_t	expr_token_value;
static sc_char		*expr_temporary		= NULL;
static sc_expr_tok_t	expr_current_token	= TOK_NONE;

/*
 * expr_tokenize_start()
 * expr_tokenize_end()
 *
 * Start and wrap up expression string tokenization.
 */
static void
expr_tokenize_start (sc_char *expression)
{
	/* Save expression, and restart index. */
	expr_expression		= expression;
	expr_index		= 0;

	/* Allocate a temporary token value/literals string. */
	assert (expr_temporary == NULL);
	expr_temporary = sc_malloc (strlen (expression) + 1);

	/* Reset last token to none. */
	expr_current_token	= TOK_NONE;
}

static void
expr_tokenize_end (void)
{
	/* Deallocate temporary strings, clear expression. */
	if (expr_temporary != NULL)
	    {
		sc_free (expr_temporary);
		expr_temporary = NULL;
	    }
	expr_expression		= NULL;
	expr_index		= 0;
	expr_current_token	= TOK_NONE;
}


/*
 * expr_next_token()
 *
 * Return the next token from the current expression.
 */
static sc_expr_tok_t
expr_next_token (void)
{
	sc_int16	c;
	sc_expr_tok_t	next_token;
	assert (expr_expression != NULL);

	/* Find and return the next token. */
	c = NUL; next_token = TOK_NONE;
	while (TRUE)
	    {
		/* Get next character, return EOS if at expression end. */
		c = expr_expression[expr_index];
		if (c == NUL)
		    {
			next_token = TOK_EOS;
			break;
		    }
		expr_index++;

		/* Skip whitespace. */
		if (isspace (c))
			continue;

		/* Identify and return numerics. */
		else if (isdigit (c))
		    {
			sscanf (&expr_expression[expr_index - 1],
					"%ld", &expr_token_value.integer);

			while (isdigit (c) && c != NUL)
				c = expr_expression[expr_index++];
			expr_index--;

			next_token = TOK_INTEGER;
			break;
		    }

		/* Identify and return variable references. */
		else if (c == PERCENT)
		    {
			sc_uint32	index;

			/* Copy variable name. */
			c = expr_expression[expr_index++];
			for (index = 0; c != PERCENT && c != NUL; )
			    {
				expr_temporary[index++] = c;
				c = expr_expression[expr_index++];
			    }
			expr_temporary[index++] = NUL;

			if (c == NUL)
			    {
				sc_error ("expr_next_token: warning:"
					" unterminated variable name\n");
				expr_index--;
			    }

			/* Return a variable name. */
			expr_token_value.string = expr_temporary;
			next_token = TOK_VARIABLE;
			break;
		    }

		/* Identify and return string literals. */
		else if (c == DOUBLE_QUOTE || c == SINGLE_QUOTE)
		    {
			sc_uint32	tindex;
			sc_char		quote;

			/* Copy maximal string literal. */
			quote = c;
			c = expr_expression[expr_index++];
			for (tindex = 0; c != quote && c != NUL; )
			    {
				expr_temporary[tindex++] = c;
				c = expr_expression[expr_index++];
			    }
			expr_temporary[tindex++] = NUL;

			/* Return string literal. */
			expr_token_value.string = expr_temporary;
			next_token = TOK_STRING;
			break;
		    }

		/* Identify ids and other multicharacter tokens. */
		else if (isalpha (c))
		    {
			sc_uint32	index;

			/* Copy maximal alphanumeric. */
			for (index = 0; isalnum (c) && c != NUL; )
			    {
				expr_temporary[index++] = c;
				c = expr_expression[expr_index++];
			    }
			expr_index--;
			expr_temporary[index++] = NUL;

			/*
			 * Check for a function name, and if known, return
			 * that, otherwise return a bare id.
			 */
			next_token = expr_multichar_search (expr_temporary,
							FUNCTION_TOKENS);
			if (next_token == TOK_NONE)
			    {
				expr_token_value.string = expr_temporary;
				next_token = TOK_IDENT;
			    }
			break;
		    }

		/*
		 * Last chance check for two-character (multichar) operators,
		 * and if none then return a single-character token.
		 */
		else
		    {
			sc_char		operator[3];

			/*
			 * Build a two-character string.  If we happen to be
			 * at the last expression character, we'll pick up
			 * the expression NULL into operator[2], so no need
			 * to handle end of expression specially here.
			 */
			operator[0] = c;
			operator[1] = expr_expression[expr_index];
			operator[2] = NUL;

			/* Search for this two-character operator. */
			if (strlen (operator) == 2)
			    {
				next_token = expr_multichar_search
							(operator,
							OPERATOR_TOKENS);
				if (next_token != TOK_NONE)
				    {
					/*
					 * Matched, so advance expression
					 * index and return this token.
					 */
					expr_index++;
					break;
				    }
			    }

			/*
			 * No match, or at last expression character; return
			 * a single-character token.
			 */
			next_token = c;
			break;
		    }
	    }

	/* Special handling for unary minus/plus signs. */
	if (next_token == TOK_SUBTRACT || next_token == TOK_ADD)
	    {
		/*
		 * Unary minus/plus if prior token was an operator or a
		 * comparison, left parenthesis, or comma, or if there was
		 * no prior token.
		 */
		switch (expr_current_token)
		    {
		    case TOK_MOD:	case TOK_POWER:
		    case TOK_ADD:	case TOK_SUBTRACT:
		    case TOK_MULTIPLY:	case TOK_DIVIDE:
		    case TOK_AND:	case TOK_OR:
		    case TOK_EQUAL:	case TOK_GREATER:
		    case TOK_LESS:	case TOK_NOT_EQUAL:
		    case TOK_GREATER_EQ:case TOK_LESS_EQ:
		    case TOK_LPAREN:	case TOK_COMMA:
		    case TOK_NONE:
			next_token = (next_token == TOK_SUBTRACT)
						? TOK_UMINUS : TOK_UPLUS;
			break;

		    default:
			break;
		    }
	    }

	/* Set current token to the one just found, and return it. */
	expr_current_token = next_token;
	return next_token;
}


/*
 * expr_current_token_value()
 *
 * Return the token value of the current token.  Undefined if the current
 * token is not numeric, an id, or a variable.
 */
static void
expr_current_token_value (sc_vartype_t *value)
{
	/* Quick check that the value is a valid one. */
	switch (expr_current_token)
	    {
	    case TOK_INTEGER:		case TOK_STRING:
	    case TOK_VARIABLE:
	    case TOK_IDENT:
		break;

	    default:
		sc_fatal ("expr_current_token_value: taking"
				" undefined token value, %d\n",
						expr_current_token);
	    }

	/* Return value. */
	*value = expr_token_value;
}


/*
 * Evaluation values stack, uses a variable type so it can contain both
 * integers and strings, and flags strings for possible garbage collection
 * on parse errors.
 */
typedef struct {
	sc_bool		is_collectible;
	sc_vartype_t	value;
} sc_stack_t;
static sc_stack_t	expr_eval_stack[MAX_NESTING_DEPTH];
static sc_uint16	expr_eval_stack_index	= 0;

/* Variables set to reference for %...% values. */
static sc_var_setref_t	expr_varset		= NULL;

/*
 * expr_eval_start()
 *
 * Reset the evaluation stack to an empty state, and register the variables
 * set to use when referencing %...% variables.
 */
static void
expr_eval_start (sc_var_setref_t vars)
{
	expr_eval_stack_index	= 0;
	expr_varset		= vars;
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
	sc_uint16	index;

	/* Find and free all collectible strings still in the stack. */
	for (index = 0; index < expr_eval_stack_index; index++)
	    {
		if (expr_eval_stack[index].is_collectible)
			sc_free (expr_eval_stack[index].value.string);
	    }

	/* Reset the stack index, for clarity and neatness. */
	expr_eval_stack_index = 0;
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
expr_eval_push_integer (sc_int32 value)
{
	/* Check for stack overflow. */
	if (expr_eval_stack_index >= MAX_NESTING_DEPTH)
		sc_fatal ("expr_eval_push_integer: stack overflow\n");

	/* Push value. */
	expr_eval_stack[expr_eval_stack_index].is_collectible = FALSE;
	expr_eval_stack[expr_eval_stack_index++].value.integer = value;
}

static void
expr_eval_push_string (sc_char *value)
{
	sc_char		*value_copy;

	/* Check for stack overflow. */
	if (expr_eval_stack_index >= MAX_NESTING_DEPTH)
		sc_fatal ("expr_eval_push_string: stack overflow\n");

	/* Push a copy of value. */
	value_copy = sc_malloc (strlen (value) + 1);
	strcpy (value_copy, value);
	expr_eval_stack[expr_eval_stack_index].is_collectible = TRUE;
	expr_eval_stack[expr_eval_stack_index++].value.string = value_copy;
}

static void
expr_eval_push_alloced_string (sc_char *value)
{
	/* Check for stack overflow. */
	if (expr_eval_stack_index >= MAX_NESTING_DEPTH)
		sc_fatal ("expr_eval_push_alloced_string: stack overflow\n");

	/* Push value. */
	expr_eval_stack[expr_eval_stack_index].is_collectible = TRUE;
	expr_eval_stack[expr_eval_stack_index++].value.string = value;
}


/*
 * expr_eval_pop_integer()
 * expr_eval_pop_string()
 *
 * Pop values off the values stack.  Returned strings are malloc'ed copies,
 * and the caller is responsible for freeing them.
 */
static sc_int32
expr_eval_pop_integer (void)
{
	/* Check for stack underflow. */
	if (expr_eval_stack_index == 0)
		sc_fatal ("expr_eval_pop_integer: stack underflow\n");

	/* Pop value. */
	assert (!expr_eval_stack[expr_eval_stack_index - 1].is_collectible);
	return expr_eval_stack[--expr_eval_stack_index].value.integer;
}

static sc_char *
expr_eval_pop_string (void)
{
	/* Check for stack underflow. */
	if (expr_eval_stack_index == 0)
		sc_fatal ("expr_eval_pop_string: stack underflow\n");

	/* Pop value. */
	assert (expr_eval_stack[expr_eval_stack_index - 1].is_collectible);
	return expr_eval_stack[--expr_eval_stack_index].value.string;
}


/*
 * expr_eval_result()
 *
 * Return the top of the values stack as the expression result.
 */
static void
expr_eval_result (sc_vartype_t *vt_rvalue)
{
	if (expr_eval_stack_index != 1)
		sc_fatal ("expr_eval_result: values stack not completed\n");

	/* Clear down stack and return the top value. */
	expr_eval_stack_index = 0;
	*vt_rvalue = expr_eval_stack[0].value;
}


/* Parse error jump buffer. */
static jmp_buf		expr_parse_error;

/*
 * expr_eval_action()
 *
 * Evaluate the effect of a token into the values stack.
 */
static void
expr_eval_action (sc_expr_tok_t token)
{
	sc_vartype_t	token_value;

	switch (token)
	    {
	    /* Handle tokens representing stack pushes. */
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
		sc_vartype_t	vt_rvalue;
		sc_char		type;

		expr_current_token_value (&token_value);
		if (!var_get (expr_varset, token_value.string,
						&type, &vt_rvalue))
		    {
			sc_error ("expr_eval_action: undefined variable, %s\n",
							token_value.string);
			longjmp (expr_parse_error, 1);
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
			sc_fatal ("expr_eval_action: bad variable type\n");
		    }
		break;
		}

	    /* Handle tokens representing functions returning numeric. */
	    case TOK_IF:
		{
		sc_int32	test, val1, val2;

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
		sc_int16	argument_count, index;
		sc_int32	result;

		/* Get argument count off the top of the stack. */
		argument_count = expr_eval_pop_integer ();

		/* Find the max or min of these stacked values. */
		result = expr_eval_pop_integer ();
		for (index = 1; index < argument_count; index++)
		    {
			sc_int32	next;

			next = expr_eval_pop_integer ();
			switch (token)
			    {
			    case TOK_MAX:
				if (next > result)
					result = next;
				break;

			    case TOK_MIN:
				if (next < result)
					result = next;
				break;

			    default:
				sc_fatal ("expr_eval_action:"
						" bad token, %d\n", token);
			    }
		    }

		/* Push back the result. */
		expr_eval_push_integer (result);
		break;
		}

	    case TOK_EITHER:
		{
		sc_int16	argument_count, pick, index;
		sc_int32	result = 0;

		/* Get argument count off the top of the stack. */
		argument_count = expr_eval_pop_integer ();

		/*
		 * Pick one of the top N items at random, then unstack
		 * all N, retaining the value of the one picked.
		 */
		pick = sc_rand () % argument_count;
		for (index = 0; index < argument_count; index++)
		    {
			sc_int32	val;

			val = expr_eval_pop_integer ();
			if (index == pick)
				result = val;
		    }

		/* Push back the result. */
		expr_eval_push_integer (result);
		break;
		}

	    case TOK_INSTR:
		{
		sc_char		*val1, *val2, *search;
		sc_int32	result;

		/* Extract the two values to work on. */
		val2 = expr_eval_pop_string ();
		val1 = expr_eval_pop_string ();

		/*
		 * Search for the second in the first.  The result is the
		 * character position, starting at 1, or 0 if not found.
		 * Then free the popped strings, and push back the result.
		 */
		search = strstr (val1, val2);
		result = (search == NULL) ? 0 : search - val1 + 1;
		sc_free (val1); sc_free (val2);
		expr_eval_push_integer (result);
		break;
		}

	    case TOK_LEN:
		{
		sc_char		*val;
		sc_int32	result;

		/* Pop the top string, and push back its length. */
		val = expr_eval_pop_string ();
		result = strlen (val);
		sc_free (val);
		expr_eval_push_integer (result);
		break;
		}

	    case TOK_VAL:
		{
		sc_char		*val;
		sc_int32	result = 0;

		/*
		 * Extract the string at stack top, and try to convert,
		 * returning zero if conversion fails.  Free the popped
		 * string, and push back the result.
		 */
		val = expr_eval_pop_string ();
		sscanf (val, "%ld", &result);
		sc_free (val);
		expr_eval_push_integer (result);
		break;
		}

	    /* Handle tokens representing unary numeric operations. */
	    case TOK_UMINUS:
		expr_eval_push_integer (-(expr_eval_pop_integer ()));
		break;

	    case TOK_UPLUS:
		break;

	    case TOK_ABS:
		expr_eval_push_integer (labs (expr_eval_pop_integer ()));
		break;

	    /* Handle tokens representing most binary numeric operations. */
	    case TOK_MOD: case TOK_ADD: case TOK_SUBTRACT: case TOK_MULTIPLY:
	    case TOK_DIVIDE: case TOK_AND: case TOK_OR: case TOK_EQUAL:
	    case TOK_GREATER: case TOK_LESS: case TOK_NOT_EQUAL:
	    case TOK_GREATER_EQ: case TOK_LESS_EQ: case TOK_RANDOM:
		{
		sc_int32	val1, val2, result = 0;

		/* Extract the two values to work on. */
		val2 = expr_eval_pop_integer ();
		val1 = expr_eval_pop_integer ();

		/* Generate the result value. */
		switch (token)
		    {
		    case TOK_MOD:	result = val1 %  val2;	break;
		    case TOK_ADD:	result = val1 +  val2;	break;
		    case TOK_SUBTRACT:	result = val1 -  val2;	break;
		    case TOK_MULTIPLY:	result = val1 *  val2;	break;
		    case TOK_DIVIDE:	result = val1 /  val2;	break;
		    case TOK_AND:	result = val1 && val2;	break;
		    case TOK_OR:	result = val1 || val2;	break;
		    case TOK_EQUAL:	result = val1 == val2;	break;
		    case TOK_GREATER:	result = val1 >  val2;	break;
		    case TOK_LESS:	result = val1 <  val2;	break;
		    case TOK_NOT_EQUAL:	result = val1 != val2;	break;
		    case TOK_GREATER_EQ:result = val1 >= val2;	break;
		    case TOK_LESS_EQ:	result = val1 <= val2;	break;
		    case TOK_RANDOM:	result = sc_randomint (val1, val2);
								break;
		    default:
			sc_fatal ("expr_eval_action: bad token, %d\n", token);
		    }

		/* Put result back at top of stack. */
		expr_eval_push_integer (result);
		break;
		}

	    /* Handle power individually, to avoid needing a maths library. */
	    case TOK_POWER:
		{
		sc_int32	val1, val2, result = 1;

		/* Extract the two values to work on. */
		val2 = expr_eval_pop_integer ();
		val1 = expr_eval_pop_integer ();

		/* Fake and somewhat inefficient integer pow() function. */
		if (val2 > 0)
		    {
			for (; val2 > 0; val2--)
				result *= val1;
		    }
		else if (val2 < 0)
		    {
			if (val1 == 0)
				result = INT_MIN;
			else
			    {
				for (; val2 < 0; val2++)
					result /= val1;
			    }
		    }

		/* Put result back at top of stack. */
		expr_eval_push_integer (result);
		break;
		}

	    /* Handle tokens representing functions returning string. */
	    case TOK_LEFT:
	    case TOK_RIGHT:
		{
		sc_char		*text;
		sc_int32	length;

		/*
		 * Extract the text and length.  If length is longer than
		 * text, or -ve, do nothing.
		 */
		length = expr_eval_pop_integer ();
		text = expr_eval_pop_string ();
		if (length < 0 || length >= (sc_int32) strlen (text))
		    {
			expr_eval_push_alloced_string (text);
			break;
		    }

		/*
		 * Take the left or right segment -- for left, the operation
		 * is a simple truncation; for right, it's a memmove.
		 */
		switch (token)
		    {
		    case TOK_LEFT:
			text[length] = NUL;
			break;

		    case TOK_RIGHT:
			memmove (text, text + strlen (text) - length,
								length + 1);
			break;

		    default:
			sc_fatal ("expr_eval_action: bad token, %d\n", token);
		    }

		/* Put result back at top of stack. */
		expr_eval_push_alloced_string (text);
		break;
		}

	    case TOK_MID:
		{
		sc_char		*text;
		sc_int32	start, length;

		/* Extract the text, start, and length. */
		length = expr_eval_pop_integer ();
		start = expr_eval_pop_integer ();
		text = expr_eval_pop_string ();

		/*
		 * Dispense with the empty text case first; it's not
		 * handled by code below, which assumes some text.
		 */
		if (text[0] == NUL)
		    {
			expr_eval_push_alloced_string (text);
			break;
		    }

		/*
		 * It's unclear what to do with start and length values
		 * that roam outside the text.  For now, I'm going with
		 * what Chipmunk BASIC for Linux does -- clamp start less
		 * than 1 to 1, and greater than len(text) to len(text),
		 * and length less than 1 to 1, and off string end to
		 * string end.
		 */
		if (start < 1)
			start = 1;
		else if (start > (sc_int32) strlen (text))
			start = strlen (text);
		if (length < 1)
			length = 1;
		else if (length > (sc_int32) strlen (text) - start + 1)
			length = strlen (text) - start + 1;

		/* Move substring, terminate, and return. */
		memmove (text, text + start - 1, length);
		text[length] = NUL;
		expr_eval_push_alloced_string (text);
		break;
		}

	    case TOK_STR:
		{
		sc_int32	val;
		sc_char		buffer[16];

		/*
		 * Extract the value, convert it, and push back the resulting
		 * string.
		 */
		val = expr_eval_pop_integer ();
		sprintf (buffer, "%ld", val);
		expr_eval_push_string (buffer);
		break;
		}


	    /* Handle tokens representing unary string operations. */
	    case TOK_UPPER:
	    case TOK_LOWER:
	    case TOK_PROPER:
		{
		sc_char		*text;
		sc_uint16	index;

		/* Extract the value to work on. */
		text = expr_eval_pop_string ();

		/* Convert the entire string in place -- it's malloc'ed. */
		for (index = 0; text[index] != NUL; index++)
		    {
			switch (token)
			    {
			    case TOK_UPPER:
				text[index] = toupper (text[index]);
				break;

			    case TOK_LOWER:
				text[index] = tolower (text[index]);
				break;

			    case TOK_PROPER:
				if (index == 0 || isspace (text[index - 1]))
					text[index] = toupper (text[index]);
				else
					text[index] = tolower (text[index]);
				break;

			    default:
				sc_fatal ("expr_eval_action:"
						" bad token, %d\n", token);
			    }
		    }

		/* Put result back at top of stack. */
		expr_eval_push_alloced_string (text);
		break;
		}

	    /* Handle token representing binary string operation. */
	    case TOK_CONCATENATE:
		{
		sc_char		*text1, *text2;

		/* Extract the two textues to work on. */
		text2 = expr_eval_pop_string ();
		text1 = expr_eval_pop_string ();

		/*
		 * Resize text1 to be long enough for both, and concatenate,
		 * then free text2, and push back the concatenation.
		 */
		text1 = sc_realloc (text1, strlen (text1) + strlen (text2) + 1);
		strcat (text1, text2);
		sc_free (text2);
		expr_eval_push_alloced_string (text1);
		break;
		}

	    default:
		sc_fatal ("expr_eval_action: bad token, %d\n", token);
	    }
}


/* Predictive parser lookahead token. */
static sc_expr_tok_t	expr_parse_lookahead		= TOK_NONE;

/* Forward declaration of factor parsers and string expression parser. */
static void		expr_parse_numeric_factor (void);
static void		expr_parse_string_factor (void);
static void		expr_parse_string_expr (void);

/*
 * expr_parse_match()
 *
 * Match a token to the lookahead, then advance lookahead.
 */
static void
expr_parse_match (sc_expr_tok_t token)
{
	if (expr_parse_lookahead == token)
		expr_parse_lookahead = expr_next_token ();
	else
	    {
		/* Syntax error. */
		sc_error ("expr_parse_match: syntax error,"
					" expected %d, got %d\n",
					expr_parse_lookahead, token);
		longjmp (expr_parse_error, 1);
	    }
}


/*
 * Numeric operator precedence table.  There are eight levels of precedence,
 * with the highest being a factor.  Each precedence entry permits up to four
 * listed tokens.  The end of the table (highest precedence) is marked by
 * a list with no entries (although in practice we need to put a TOK_NONE
 * in here since some C compilers won't accept { } as an empty initializer).
 */
typedef struct {
	const sc_uint16		entries;
	const sc_expr_tok_t	tokens[4];
} sc_precedence_entry_t;
static const sc_precedence_entry_t	PRECEDENCE_TABLE[] = {
	{ 1, { TOK_OR							}},
	{ 1, { TOK_AND							}},
	{ 2, { TOK_EQUAL,    TOK_NOT_EQUAL				}},
	{ 4, { TOK_GREATER,  TOK_LESS,     TOK_GREATER_EQ,  TOK_LESS_EQ	}},
	{ 2, { TOK_ADD,      TOK_SUBTRACT				}},
	{ 3, { TOK_MULTIPLY, TOK_DIVIDE,   TOK_MOD			}},
	{ 1, { TOK_POWER						}},
	{ 0, { TOK_NONE							}}
};


/*
 * expr_parse_numeric_element()
 *
 * Parse numeric expression elements.  This function uses the precedence
 * table to match tokens, then decide whether, and how, to recurse into
 * itself, or whether to parse a highest-precedence factor.
 */
static void
expr_parse_numeric_element (sc_uint16 precedence)
{
	sc_expr_tok_t			token;
	const sc_precedence_entry_t	*table_entry;

	/* See if the level passed in has listed tokens. */
	table_entry = PRECEDENCE_TABLE + precedence;
	if (table_entry->entries != 0)
	    {
		/*
		 * Parse initial higher-precedence factor, then others
		 * that associate with the given level.
		 */
		expr_parse_numeric_element (precedence + 1);
		while (TRUE)
		    {
			sc_bool		match;
			sc_uint16	index;

			/*
			 * Search the table list for tokens at this level.
			 * Return if none.
			 */
			match = FALSE;
			for (index = 0;
				index < table_entry->entries && !match;
				index++)
			    {
				if (expr_parse_lookahead ==
						table_entry->tokens[index])
					match = TRUE;
			    }
			if (!match)
				return;

			/* Note token, match, parse next level, then action. */
			token = expr_parse_lookahead;
			expr_parse_match (token);
			expr_parse_numeric_element (precedence + 1);
			expr_eval_action (token);
		    }
	    }
	else
	    {
		/* Precedence levels that hit the table end are factors. */
		expr_parse_numeric_factor ();
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
 * expr_parse_numeric_factor()
 *
 * Parse a numeric expression factor.
 */
static void
expr_parse_numeric_factor (void)
{
	/* Handle factors based on lookahead token. */
	switch (expr_parse_lookahead)
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
		{
		sc_expr_tok_t	token;

		token = expr_parse_lookahead;
		expr_eval_action (token);
		expr_parse_match (token);
		break;
		}

	    case TOK_VARIABLE:
		{
		sc_vartype_t	token_value, vt_rvalue;
		sc_char		type;

		expr_current_token_value (&token_value);
		if (!var_get (expr_varset, token_value.string,
						&type, &vt_rvalue))
		    {
			sc_error ("expr_parse_numeric_factor:"
						" undefined variable, %s\n",
							token_value.string);
			longjmp (expr_parse_error, 1);
		    }
		if (type != VAR_INTEGER)
		    {
			sc_error ("expr_parse_numeric_factor:"
				" string variable in numeric context, %s\n",
							token_value.string);
			longjmp (expr_parse_error, 1);
		    }
		expr_eval_action (TOK_VARIABLE);
		expr_parse_match (TOK_VARIABLE);
		break;
		}

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

	    case TOK_MAX: case TOK_MIN: case TOK_EITHER:
		/* Parse as "<func> (val1[,val2[,val3...]]])". */
		{
		sc_expr_tok_t	token;
		sc_int16	argument_count;

		/* Match up the function name and opening parenthesis. */
		token = expr_parse_lookahead;
		expr_parse_match (token);
		expr_parse_match (TOK_LPAREN);

		/* Count variable number of arguments as they are stacked. */
		expr_parse_numeric_expr ();
		argument_count = 1;
		while (expr_parse_lookahead == TOK_COMMA)
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
		sc_error ("expr_parse_numeric_factor: syntax error,"
						" unknown ident\n");
		longjmp (expr_parse_error, 1);

	    default:
		/* Syntax error. */
		sc_error ("expr_parse_numeric_factor: syntax error,"
						" unexpected token, %d\n",
						expr_parse_lookahead);
		longjmp (expr_parse_error, 1);
	    }
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
	 * Parse a string factor, then all repeated concatenations.  Because
	 * the '+' and '&' are context sensitive, we have to invent/translate
	 * them into the otherwise unused TOK_CONCATENATE for evaluation.
	 */
	expr_parse_string_factor ();
	while (TRUE)
	    {
		switch (expr_parse_lookahead)
		    {
		    case TOK_AND:
		    case TOK_ADD:
			expr_parse_match (expr_parse_lookahead);
			expr_parse_string_factor ();
			expr_eval_action (TOK_CONCATENATE);
			continue;

		    default:
			return;
		    }
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
	/* Handle factors based on lookahead token. */
	switch (expr_parse_lookahead)
	    {
	    /* Handle straightforward factors first. */
	    case TOK_LPAREN:
		expr_parse_match (TOK_LPAREN);
		expr_parse_string_expr ();
		expr_parse_match (TOK_RPAREN);
		break;

	    case TOK_STRING:
		{
		sc_expr_tok_t	token;

		token = expr_parse_lookahead;
		expr_eval_action (token);
		expr_parse_match (token);
		break;
		}

	    case TOK_VARIABLE:
		{
		sc_vartype_t	token_value, vt_rvalue;
		sc_char		type;

		expr_current_token_value (&token_value);
		if (!var_get (expr_varset, token_value.string,
						&type, &vt_rvalue))
		    {
			sc_error ("expr_parse_string_factor:"
						" undefined variable, %s\n",
							token_value.string);
			longjmp (expr_parse_error, 1);
		    }
		if (type != VAR_STRING)
		    {
			sc_error ("expr_parse_string_factor:"
				" numeric variable in string context, %s\n",
							token_value.string);
			longjmp (expr_parse_error, 1);
		    }
		expr_eval_action (TOK_VARIABLE);
		expr_parse_match (TOK_VARIABLE);
		break;
		}

	    /* Handle functions as factors. */
	    case TOK_UPPER: case TOK_LOWER: case TOK_PROPER:
		/* Parse as "<func> (text)". */
		{
		sc_expr_tok_t	token;

		token = expr_parse_lookahead;
		expr_parse_match (token);
		expr_parse_match (TOK_LPAREN);
		expr_parse_string_expr ();
		expr_parse_match (TOK_RPAREN);
		expr_eval_action (token);
		break;
		}

	    case TOK_LEFT: case TOK_RIGHT:
		/* Parse as "<func> (text,length)". */
		{
		sc_expr_tok_t	token;

		token = expr_parse_lookahead;
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
		sc_error ("expr_parse_string_factor: syntax error,"
						" unknown ident\n");
		longjmp (expr_parse_error, 1);

	    default:
		/* Syntax error. */
		sc_error ("expr_parse_string_factor: syntax error,"
						" unexpected token, %d\n",
						expr_parse_lookahead);
		longjmp (expr_parse_error, 1);
	    }
}


/*
 * expr_evaluate_expression()
 *
 * Parse a string expression into a runtime values stack.  Return the
 * value of the expression.
 */
static sc_bool
expr_evaluate_expression (sc_char *expression, sc_var_setref_t vars,
		sc_char assign_type, sc_vartype_t *vt_rvalue)
{
	assert (assign_type == VAR_INTEGER || assign_type == VAR_STRING);

	/* Reset values stack and start tokenizer. */
	expr_eval_start (vars);
	expr_tokenize_start (expression);

	/* Set up error handling jump buffer. */
	if (setjmp (expr_parse_error) == 0)
	    {
		/* Parse an expression, and ensure it ends at string end. */
		expr_parse_lookahead = expr_next_token ();
		if (assign_type == VAR_STRING)
			expr_parse_string_expr ();
		else
			expr_parse_numeric_expr ();
		expr_parse_match (TOK_EOS);

		/* Clean up tokenizer and return successfully with result. */
		expr_tokenize_end ();
		expr_eval_result (vt_rvalue);
		return TRUE;
	    }

	/* Parse error -- clean up tokenizer, collect garbage, and fail. */
	expr_tokenize_end ();
	expr_eval_garbage_collect ();
	return FALSE;
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
sc_bool
expr_eval_numeric_expression (sc_char *expression, sc_var_setref_t vars,
		sc_int32 *rvalue)
{
	sc_vartype_t	vt_rvalue;
	sc_bool		status;
	assert (expression != NULL && vars != NULL && rvalue != NULL);

	/* Evaluate numeric expression, and return value if valid. */
	status = expr_evaluate_expression
				(expression, vars, VAR_INTEGER, &vt_rvalue);
	if (status)
		*rvalue = vt_rvalue.integer;
	return status;
}

sc_bool
expr_eval_string_expression (sc_char *expression, sc_var_setref_t vars,
		sc_char **rvalue)
{
	sc_vartype_t	vt_rvalue;
	sc_bool		status;
	assert (expression != NULL && vars != NULL && rvalue != NULL);

	/* Evaluate string expression, and return value if valid. */
	status = expr_evaluate_expression
				(expression, vars, VAR_STRING, &vt_rvalue);
	if (status)
		*rvalue = vt_rvalue.string;
	return status;
}
