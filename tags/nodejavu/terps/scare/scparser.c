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
 * o Some of the "finer" points of pattern matching in relation to "*"
 *   wildcards, and %text%, are unknown.
 *
 * o The inclusion of part or all of prefixes in %character% and
 *   %object% matching may be right; then again, it may not be.
 */

#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <setjmp.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const sc_char	NUL			= '\0',
			MINUS			= '-',
			PLUS			= '+';

/* Pattern matching trace flag. */
static sc_bool		uip_trace		= FALSE;

/* Enumeration of tokens.  TOK_NONE represents a non-occurring token. */
typedef enum {
	TOK_NONE,
	TOK_CHOICE,		TOK_CHOICE_END,
	TOK_OPTIONAL,		TOK_OPTIONAL_END,
	TOK_ALT_SEPARATOR,
	TOK_WILDCARD,
	TOK_WHITESPACE,		TOK_WORD,
	TOK_CHARACTER_REF,	TOK_OBJECT_REF,
	TOK_NUMBER_REF,		TOK_TEXT_REF,
	TOK_EOS
} sc_uip_tok_t;

/*
 * Small table tying token strings to tokens.  Anything not whitespace, and
 * not caught by the table is a plain TOK_WORD.
 */
static const struct {
	const sc_char		*string;
	const sc_uip_tok_t	token;
} TOKENS[] = {
	{ "[",		TOK_CHOICE        },
	{ "]",		TOK_CHOICE_END    },
	{ "{",		TOK_OPTIONAL      },
	{ "}",		TOK_OPTIONAL_END  },
	{ "/",		TOK_ALT_SEPARATOR },
	{ "*",		TOK_WILDCARD      },
	{ "%character%",TOK_CHARACTER_REF },
	{ "%object%",	TOK_OBJECT_REF    },
	{ "%number%",	TOK_NUMBER_REF    },
	{ "%text%",	TOK_TEXT_REF      },
	{ NULL,		TOK_NONE          }
};


/* Tokenizer variables. */
static sc_char		*uip_pattern		= NULL;
static sc_uint16	uip_index		= 0;
static sc_char		*uip_token_value;
static sc_char		*uip_temporary		= NULL;

/*
 * uip_tokenize_start()
 * uip_tokenize_end()
 *
 * Start and wrap up pattern string tokenization.
 */
static void
uip_tokenize_start (sc_char *pattern)
{
	/* Save pattern, and restart index. */
	uip_pattern	= pattern;
	uip_index	= 0;

	/* Allocate temporary token value string. */
	if (uip_temporary == NULL)
		uip_temporary = sc_malloc (strlen (pattern) + 1);
}

static void
uip_tokenize_end (void)
{
	/* Deallocate temporary token value string, clear pattern. */
	if (uip_temporary != NULL)
	    {
		sc_free (uip_temporary);
		uip_temporary = NULL;
	    }
	uip_pattern	= NULL;
	uip_index	= 0;
}


/*
 * uip_next_token()
 *
 * Return the next token from the current pattern.
 */
static sc_uip_tok_t
uip_next_token (void)
{
	sc_int16	c;
	sc_uint32	tindex;
	assert (uip_pattern != NULL);

	/* Get next character, return EOS if at pattern end. */
	c = uip_pattern[uip_index];
	if (c == NUL)
	    {
		uip_token_value = NULL;
		return TOK_EOS;
	    }
	uip_index++;

	/* If whitespace, skip it, then return a whitespace token. */
	if (isspace (c))
	    {
		while (isspace (uip_pattern[uip_index])
				&& uip_pattern[uip_index] != NUL)
			uip_index++;
		uip_token_value = NULL;
		return TOK_WHITESPACE;
	    }

	/* Regress pattern pointer before table search. */
	assert (uip_index > 0);
	uip_index--;

	/* Search the table for matching strings. */
	for (tindex = 0; TOKENS[tindex].token != TOK_NONE; tindex++)
	    {
		const sc_char	*tstring;

		tstring = TOKENS[tindex].string;
		if (!strncmp (uip_pattern + uip_index,
					tstring, strlen (tstring)))
		    {
			/* Advance over string, and return token. */
			uip_index += strlen (tstring);
			uip_token_value = NULL;
			return TOKENS[tindex].token;
		    }
	    }

	/*
	 * Return a word.  This is a contiguous run of non-pattern-special,
	 * non-whitespace characters, specifically excluding '%' to work
	 * round occasionally "buggy" games that reference %user_var% in
	 * their patterns (only the four official references are allowed).
	 */
	sscanf (uip_pattern + uip_index,
				"%[^][/{}* \f\n\r\t\v]", uip_temporary);
	uip_token_value = uip_temporary;
	uip_index += strlen (uip_temporary);
	return TOK_WORD;
}


/*
 * uip_current_token_value()
 *
 * Return the token value of the current token.  It is an error to call
 * here if the current token is not a TOK_WORD.
 */
static sc_char *
uip_current_token_value (void)
{
	/* If the token value is NULL, the current token isn't a word. */
	if (uip_token_value == NULL)
	    {
		sc_fatal ("uip_current_token_value: attempt to take"
					" undefined token valued\n");
	    }

	/* Return value. */
	return uip_token_value;
}


/*
 * Parsed pattern tree node definition.   The tree is a left child,
 * right sibling representation, with token type, and word at nodes
 * of type TOK_WORD.
 */
typedef enum {
	NODE_CHOICE,		NODE_OPTIONAL,		NODE_WILDCARD,
	NODE_WHITESPACE,	NODE_CHARACTER_REF,	NODE_OBJECT_REF,
	NODE_TEXT_REF,		NODE_NUMBER_REF,	NODE_WORD,
	NODE_LIST,		NODE_EOS
} sc_pttype_t;
typedef struct sc_ptnode_s {
	struct sc_ptnode_s	*left_child;
	struct sc_ptnode_s	*right_sibling;

	sc_pttype_t		type;
	sc_char			*word;
} sc_ptnode_t;
typedef sc_ptnode_t		*sc_ptnoderef_t;

/* Predictive parser lookahead token. */
static sc_uip_tok_t	uip_parse_lookahead		= TOK_NONE;

/* Parse error jump buffer. */
static jmp_buf		uip_parse_error;

/* Forward declaration of pattern list parser. */
static sc_ptnoderef_t	uip_parse_list (void);

/*
 * uip_parse_match()
 *
 * Match a token to the lookahead, then advance lookahead.
 */
static void
uip_parse_match (sc_uip_tok_t token)
{
	if (uip_parse_lookahead == token)
		uip_parse_lookahead = uip_next_token ();
	else
	    {
		/* Syntax error. */
		sc_error ("uip_parse_match: syntax error,"
					" expected %d, got %d\n",
					uip_parse_lookahead, token);
		longjmp (uip_parse_error, 1);
	    }
}


/*
 * uip_new_node()
 *
 * Create a new node, populated with an initial type.
 */
static sc_ptnoderef_t
uip_new_node (sc_pttype_t type)
{
	sc_ptnoderef_t	node;

	/* Allocate the new node, and set it up. */
	node = sc_malloc (sizeof (*node));
	node->left_child	= NULL;
	node->right_sibling	= NULL;
	node->type		= type;
	node->word		= NULL;

	/* Return the new node. */
	return node;
}


/*
 * uip_destroy_node()
 *
 * Destroy a node, and any allocated word memory.
 */
static void
uip_destroy_node (sc_ptnoderef_t node)
{
	/* If the node contains a word, free it. */
	if (node->word != NULL)
		sc_free (node->word);

	/* Shred and destroy memory at this node. */
	memset (node, 0, sizeof (*node));
	sc_free (node);
}


/*
 * uip_parse_alternatives()
 *
 * Parse a set of .../.../... alternatives for choices and optionals.
 */
static void
uip_parse_alternatives (sc_ptnoderef_t node)
{
	sc_ptnoderef_t	cnode;

	/* Parse initial alternative, then add other listed alternatives. */
	node->left_child = uip_parse_list ();
	cnode = node->left_child;
	while (TRUE)
	    {
		switch (uip_parse_lookahead)
		    {
		    case TOK_ALT_SEPARATOR:
			uip_parse_match (TOK_ALT_SEPARATOR);
			cnode->right_sibling = uip_parse_list ();
			cnode = cnode->right_sibling;
			continue;

		    default:
			return;
		    }
	    }
}


/*
 * uip_parse_element()
 *
 * Parse a single pattern element.
 */
static sc_ptnoderef_t
uip_parse_element (void)
{
	sc_ptnoderef_t	node = NULL;
	sc_uip_tok_t	token;

	/* Handle pattern element based on lookahead token. */
	switch (uip_parse_lookahead)
	    {
	    case TOK_WHITESPACE:
		uip_parse_match (TOK_WHITESPACE);
		node = uip_new_node (NODE_WHITESPACE);
		break;

	    case TOK_CHOICE:
		/* Parse a [...[/.../...]] choice. */
		uip_parse_match (TOK_CHOICE);
		node = uip_new_node (NODE_CHOICE);
		uip_parse_alternatives (node);
		uip_parse_match (TOK_CHOICE_END);
		break;

	    case TOK_OPTIONAL:
		/* Parse a {...[/.../...]} optional element. */
		uip_parse_match (TOK_OPTIONAL);
		node = uip_new_node (NODE_OPTIONAL);
		uip_parse_alternatives (node);
		uip_parse_match (TOK_OPTIONAL_END);
		break;

	    case TOK_WILDCARD:
	    case TOK_CHARACTER_REF:
	    case TOK_OBJECT_REF:
	    case TOK_NUMBER_REF:
	    case TOK_TEXT_REF:
		/* Parse %mumble% references and * wildcards. */
		token = uip_parse_lookahead;
		uip_parse_match (token);
		switch (token)
		    {
		    case TOK_WILDCARD:
			node = uip_new_node (NODE_WILDCARD);		break;
		    case TOK_CHARACTER_REF:
			node = uip_new_node (NODE_CHARACTER_REF);	break;
		    case TOK_OBJECT_REF:
			node = uip_new_node (NODE_OBJECT_REF);		break;
		    case TOK_NUMBER_REF:
			node = uip_new_node (NODE_NUMBER_REF);		break;
		    case TOK_TEXT_REF:
			node = uip_new_node (NODE_TEXT_REF);		break;
		    default:
			sc_fatal ("uip_parse_element:"
					" invalid token, %d\n", token);
		    }
		break;

	    case TOK_WORD:
		{
		sc_char		*word, *word_copy;

		/* Take a copy of the token's word value. */
		word = uip_current_token_value ();
		word_copy = sc_malloc (strlen (word) + 1);
		strcpy (word_copy, word);

		/* Store details in a word node. */
		uip_parse_match (TOK_WORD);
		node = uip_new_node (NODE_WORD);
		node->word = word_copy;
		break;
		}

	    default:
		/* Syntax error. */
		sc_error ("uip_parse_element: syntax error,"
						" unexpected token, %d\n",
						uip_parse_lookahead);
		longjmp (uip_parse_error, 1);
	    }

	/* Return the newly created node. */
	assert (node != NULL);
	return node;
}


/*
 * uip_parse_list()
 *
 * Parse a list of pattern elements.
 */
static sc_ptnoderef_t
uip_parse_list (void)
{
	sc_ptnoderef_t	node, cnode, lnode;

	/* Create the new list node. */
	node = uip_new_node (NODE_LIST);

	/* Add elements until a list terminator token is encountered. */
	cnode = node;
	while (TRUE)
	    {
		switch (uip_parse_lookahead)
		    {
		    case TOK_CHOICE_END:
		    case TOK_OPTIONAL_END:
		    case TOK_ALT_SEPARATOR:
			/* Terminate list building and return. */
			return node;

		    case TOK_EOS:
			/* Place EOS at the appropriate link and return. */
			lnode = uip_new_node (NODE_EOS);
			if (cnode == node)
				cnode->left_child = lnode;
			else
				cnode->right_sibling = lnode;
			return node;

		    default:
			/* Add the next node at the appropriate link. */
			lnode = uip_parse_element ();
			if (cnode == node)
			    {
				cnode->left_child = lnode;
				cnode = cnode->left_child;
			    }
			else
			    {
				/*
				 * Make a special case of a choice or option
				 * next to another choice or option.  In this
				 * case, add an (invented) whitespace node,
				 * to ensure a match with suitable input.
				 */
				if ((cnode->type == NODE_OPTIONAL
						|| cnode->type == NODE_CHOICE)
					&& (lnode->type == NODE_OPTIONAL
						|| lnode->type == NODE_CHOICE))
				    {
					sc_ptnoderef_t	tnode;

					/* Interpose invented whitespace. */
					tnode = uip_new_node (NODE_WHITESPACE);
					cnode->right_sibling = tnode;
					cnode = cnode->right_sibling;
				    }

				cnode->right_sibling = lnode;
				cnode = cnode->right_sibling;
			    }
			continue;
		    }
	    }
}


/*
 * uip_destroy_tree()
 *
 * Free and destroy a parsed pattern tree.
 */
static void
uip_destroy_tree (sc_ptnoderef_t node)
{
	if (node != NULL)
	   {
		/* Recursively destroy siblings, then left child. */
		uip_destroy_tree (node->right_sibling);
		uip_destroy_tree (node->left_child);

		/* Destroy the node itself. */
		uip_destroy_node (node);
	   }
}


/*
 * uip_debug_dump_node()
 *
 * Print out pattern match tree.
 */
static void
uip_debug_dump_node (sc_ptnoderef_t node)
{
	/* End recursion on null node. */
	if (node != NULL)
	   {
		sc_trace ("Node %p", (void *) node);
		switch (node->type)
		    {
		    case NODE_CHOICE:		sc_trace (", CHOICE");	break;
		    case NODE_OPTIONAL:		sc_trace (", OPTIONAL");break;
		    case NODE_WILDCARD:		sc_trace (", WILDCARD");break;
		    case NODE_WHITESPACE:	sc_trace (", WHITESPACE");
									break;
		    case NODE_CHARACTER_REF:	sc_trace (", CHARACTER");
									break;
		    case NODE_OBJECT_REF:	sc_trace (", OBJECT");	break;
		    case NODE_TEXT_REF:		sc_trace (", TEXT");	break;
		    case NODE_NUMBER_REF:	sc_trace (", NUMBER");	break;
		    case NODE_WORD:
			sc_trace (", WORD, word \"%s\"", node->word);	break;
		    case NODE_LIST:		sc_trace (", LIST");	break;
		    case NODE_EOS:		sc_trace (", <EOS>");	break;
		    default:
			sc_trace (", type %d", node->type);		break;
		    }
		if (node->left_child != NULL)
			sc_trace (", V %p", (void *) node->left_child);
		if (node->right_sibling != NULL)
			sc_trace (", > %p", (void *) node->right_sibling);
		sc_trace ("\n");

		/* Recursively dump left child, then siblings. */
		uip_debug_dump_node (node->left_child);
		uip_debug_dump_node (node->right_sibling);
	   }
}


/* String matching variables. */
static sc_char			*uip_string		= NULL;
static sc_uint16		uip_posn		= 0;
static sc_gamestateref_t	uip_gamestate		= NULL;

/*
 * uip_match_start()
 * uip_match_end()
 *
 * Set up a string for matching to a pattern tree, and wrap up matching.
 */
static void
uip_match_start (sc_char *string, sc_gamestateref_t gamestate)
{
	/* Save string, and restart index. */
	uip_string	= string;
	uip_posn	= 0;

	/* Save the gamestate we're working on. */
	uip_gamestate= gamestate;
}

static void
uip_match_end (void)
{
	/* Clear match target string, and variable set. */
	uip_string	= NULL;
	uip_posn	= 0;
	uip_gamestate	= NULL;
}


/* Forward declaration of low level node matcher. */
static sc_bool	uip_match_node (sc_ptnoderef_t node);

/*
 * uip_match_eos()
 * uip_match_word()
 * uip_match_whitespace()
 * uip_match_list()
 * uip_match_alternatives()
 * uip_match_choice()
 * uip_match_optional()
 * uip_match_wildcard()
 *
 * Text element and list/choice element match functions.  Return TRUE, and
 * advance position if necessary, on match, FALSE on no match, with position
 * unchanged.
 */
static sc_bool
uip_match_eos (void)
{
	/* Check that we hit the string's end. */
	return uip_string[uip_posn] == NUL;
}

static sc_bool
uip_match_word (sc_ptnoderef_t node)
{
	sc_uint16	length;
	sc_char		*word;

	/* Get the word to match. */
	assert (node->word != NULL);
	word = node->word;

	/* Compare string text with this node's word, ignore case. */
	length = strlen (word);
	if (!sc_strncasecmp (uip_string + uip_posn, word, length))
	    {
		/* Word match, advance position and return. */
		uip_posn += length;
		return TRUE;
	    }

	/* No match. */
	return FALSE;
}

static sc_bool
uip_match_whitespace (void)
{
	/* If next character is space, read whitespace and return. */
	if (isspace (uip_string[uip_posn]))
	    {
		/* Space match, advance position and return. */
		while (uip_string[uip_posn] != NUL
				&& isspace (uip_string[uip_posn]))
			uip_posn++;
		return TRUE;
	    }

	/*
	 * No match.  However, if we're trying to match space, this
	 * is a word boundary.  So... even though we're not sitting
	 * on a space, if the string prior character is whitespace,
	 * "double-match" the space.
	 *
	 * Also, match if we haven't yet matched any text.  In effect,
	 * this means leading spaces on patterns will be ignored.
	 * TODO Is this what we want to happen?  It seems harmless,
	 * even useful.
	 */
	if (uip_posn == 0
			|| isspace (uip_string[uip_posn - 1]))
		return TRUE;

	/*
	 * And that's not all.  We also want to match whitespace if
	 * we're at the end of a string (another word boundary).
	 * This will permit patterns that end in optional elements to
	 * succeed since options and wildcards always match, even
	 * if to no text.
	 */
	if (uip_string[uip_posn] == NUL)
		return TRUE;

	/* No match.  Really. */
	return FALSE;
}

static sc_bool
uip_match_list (sc_ptnoderef_t node)
{
	sc_ptnoderef_t	cnode;

	/* Match everything listed sequentially. */
	for (cnode = node->left_child; cnode != NULL;
				cnode = cnode->right_sibling)
	    {
		if (!uip_match_node (cnode))
		    {
			/* No match. */
			return FALSE;
		    }
	    }

	/* Matched. */
	return TRUE;
}

static sc_bool
uip_match_alternatives (sc_ptnoderef_t node)
{
	sc_ptnoderef_t	cnode;
	sc_uint16	start_posn, extent;
	sc_bool		matched;

	/* Note the start position for rewind between tries. */
	start_posn = uip_posn;

	/*
	 * Try a match on each of the children, looking to see which one
	 * moves the position on the furthest.  Match on this one.  This
	 * is a "maximal munch".
	 */
	extent = uip_posn;
	matched = FALSE;
	for (cnode = node->left_child; cnode != NULL;
				cnode = cnode->right_sibling)
	    {
		uip_posn = start_posn;
		if (uip_match_node (cnode))
		    {
			/* Matched. */
			matched = TRUE;
			if (uip_posn > extent)
				extent = uip_posn;
		    }
	    }

	/* If matched, set position to extent; if not, back to start. */
	if (matched)
		uip_posn = extent;
	else
		uip_posn = start_posn;

	/* Return match status. */
	return matched;
}

static sc_bool
uip_match_choice (sc_ptnoderef_t node)
{
	/*
	 * Return the result of matching alternatives.  The choice will
	 * therefore fail if none of the alternatives match.
	 */
	return uip_match_alternatives (node);
}

static sc_bool
uip_match_optional (sc_ptnoderef_t node)
{
	sc_uint16	start_posn;
	sc_ptnoderef_t	list;
	sc_bool		matched;

	/* Note the start position for rewind on empty match. */
	start_posn = uip_posn;

	/*
	 * Look ahead to see if we can match to nothing, and still have
	 * the main pattern match.  If we can, we'll go with this.  It's
	 * a "minimal munch"-ish strategy, but seems to be what Adrift
	 * does in this situation.
	 */
	list = uip_new_node (NODE_LIST);
	list->left_child = node->right_sibling;

	/* Match on the temporary list. */
	matched = uip_match_node (list);

	/* Free the temporary list node. */
	uip_destroy_node (list);

	/*
	 * If the temporary matched, rewind position to match nothing.
	 * If it didn't, match alternatives to consume anything that
	 * may match our options.
	 */
	if (matched)
		uip_posn = start_posn;
	else
		uip_match_alternatives (node);

	/* Return TRUE no matter what. */
	return TRUE;
}

static sc_bool
uip_match_wildcard (sc_ptnoderef_t node)
{
	sc_uint16	start_posn;
	sc_uint16	index;
	sc_bool		matched;
	sc_ptnoderef_t	list;

	/*
	 * At least one game uses patterns like "thing******...".  Why?
	 * Who knows.  But if we're in a list of wildcards, and not the
	 * first, ignore the call; only the final one needs handling.
	 */
	if (node->right_sibling != NULL
			&& node->right_sibling->type == NODE_WILDCARD)
		return TRUE;

	/* Note the start position for rewind on no match. */
	start_posn = uip_posn;

	/*
	 * To make life a little easier, we'll match on the tree to
	 * the right of this node by constructing a temporary list node,
	 * containing stuff to the right of the wildcard, and then
	 * matching on that.
	 */
	list = uip_new_node (NODE_LIST);
	list->left_child = node->right_sibling;

	/*
	 * Repeatedly try to match the rest of the tree at successive
	 * character positions, and stop if we succeed.  This is a
	 * "minimal munch", which may or may not be the right thing to
	 * be doing here.
	 *
	 * When scanning forward, take care to include the NUL, needed
	 * to match TOK_EOS.
	 */
	matched = FALSE;
	for (index = uip_posn + 1;
			index < strlen (uip_string) + 1; index++)
	    {
		uip_posn = index;
		if (uip_match_node (list))
		    {
			/* Wildcard match at this point. */
			uip_posn = index;
			matched = TRUE;
			break;
		    }
	    }

	/* Free the temporary list node. */
	uip_destroy_node (list);

	/* If we didn't match in the loop, restore position. */
	if (!matched)
		uip_posn = start_posn;

	/* Return TRUE whether we matched text or not. */
	return TRUE;
}


/*
 * uip_match_number()
 * uip_match_text()
 *
 * Attempt to match a number, or a word, from the string.
 */
static sc_bool
uip_match_number (void)
{
	sc_var_setref_t		vars;
	sc_int32		number;
	assert (uip_gamestate != NULL);

	vars	= uip_gamestate->vars;

	/* Attempt to read a number from input. */
	if (sscanf (uip_string + uip_posn, "%ld", &number) == 1)
	    {
		/* Advance position over the number. */
		while (uip_string[uip_posn] == MINUS
				|| uip_string[uip_posn] == PLUS)
			uip_posn++;
		while (isdigit (uip_string[uip_posn]))
			uip_posn++;

		/* Set number reference in variables and return. */
		var_set_ref_number (vars, number);
		return TRUE;
	    }

	/* No match. */
	return FALSE;
}

static sc_bool
uip_match_text (sc_ptnoderef_t node)
{
	sc_var_setref_t		vars;
	sc_uint16		start_posn, index;
	sc_bool			matched;
	sc_ptnoderef_t		list;
	assert (uip_gamestate != NULL);

	vars	= uip_gamestate->vars;

	/* Note the start position for rewind on no match. */
	start_posn = uip_posn;

	/*
	 * As with wildcards, create a temporary list of the stuff to the
	 * right of the reference node, and match on that.
	 */
	list = uip_new_node (NODE_LIST);
	list->left_child = node->right_sibling;

	/*
	 * Again, as with wildcards, repeatedly try to match the rest of the
	 * tree at successive character positions, stopping if we succeed.
	 */
	matched = FALSE;
	for (index = uip_posn + 1;
			index < strlen (uip_string) + 1; index++)
	    {
		uip_posn = index;
		if (uip_match_node (list))
		    {
			/* Text reference match at this point. */
			uip_posn = index;
			matched = TRUE;
			break;
		    }
	    }

	/* Free the temporary list node. */
	uip_destroy_node (list);

	/* See if we found a match in the loop. */
	if (matched)
	    {
		sc_char		*text;

		/* Found a match; create a string and save the text. */
		text = sc_malloc (uip_posn - start_posn + 1);
		memcpy (text, uip_string + start_posn,
						uip_posn - start_posn);
		text[uip_posn - start_posn] = NUL;
		var_set_ref_text (vars, text);
		sc_free (text);

		/* Return TRUE since we matched text. */
		return TRUE;
	    }
	else
	    {
		/* We didn't match in the loop; restore position. */
		uip_posn = start_posn;

		/* Return FALSE on no match. */
		return FALSE;
	    }
}


/*
 * uip_compare_pronoun()
 *
 * Match occurrences of the pronoun handed in at the current position, and
 * return zero if no match, otherwise the string position.
 */
static sc_uint16
uip_compare_pronoun (const sc_char *pronoun)
{
	sc_uint16	posn, len;

	len = strlen (pronoun);
	posn = uip_posn;

	/* If pronoun found, return position advanced by length. */
	if (!sc_strncasecmp (uip_string + posn, pronoun, len)
				&& (uip_string[posn + len] == NUL
					|| isspace (uip_string[posn + len])))
		return posn + len;

	/* Not found -- return 0. */
	return 0;
}


/*
 * uip_match_character_pronoun()
 *
 * Match "it", "him", or "her" as a pronoun for a %character% reference.
 */
static sc_bool
uip_match_character_pronoun (void)
{
	sc_var_setref_t		vars	= uip_gamestate->vars;
	sc_int32		npc;
	sc_uint16		extent;

	/* Check for "it", if not then "him", and finally "her". */
	npc = -1; extent = uip_posn;
	if (uip_gamestate->it_npc != -1)
	    {
		extent = uip_compare_pronoun ("it");
		if (extent > 0)
			npc = uip_gamestate->it_npc;
	    }
	if (npc == -1 && uip_gamestate->him_npc != -1)
	    {
		extent = uip_compare_pronoun ("him");
		if (extent > 0)
			npc = uip_gamestate->him_npc;
	    }
	if (npc == -1 && uip_gamestate->her_npc != -1)
	    {
		extent = uip_compare_pronoun ("her");
		if (extent > 0)
			npc = uip_gamestate->her_npc;
	    }
	if (npc == -1)
		return FALSE;

	/* Set reference in variables and in gamestate. */
	if (uip_trace)
		sc_trace ("UIParser:   %%character%% pronoun match\n");
	var_set_ref_character (vars, npc);

	uip_gamestate->npc_references[npc]	= TRUE;
	uip_gamestate->is_npc_pronoun		= TRUE;

	/* Advance position and return. */
	uip_posn = extent;
	return TRUE;
}


/*
 * uip_match_object_pronoun()
 *
 * Match "it" or "them" as a pronoun for a %object% reference.
 */
static sc_bool
uip_match_object_pronoun (void)
{
	sc_var_setref_t		vars	= uip_gamestate->vars;
	sc_int32		object;
	sc_uint16		extent;

	/*
	 * Check for "it", if not then "them".  TODO Is "them" a valid
	 * thing to check for?  It seems useful...
	 */
	object = -1; extent = uip_posn;
	if (uip_gamestate->it_object != -1)
	    {
		extent = uip_compare_pronoun ("it");
		if (extent > 0)
			object = uip_gamestate->it_object;
	    }
	if (object == -1 && uip_gamestate->it_object != -1)
	    {
		extent = uip_compare_pronoun ("them");
		if (extent > 0)
			object = uip_gamestate->it_object;
	    }
	if (object == -1)
		return FALSE;

	/* Set reference in variables and in gamestate. */
	if (uip_trace)
		sc_trace ("UIParser:   %%object%% pronoun match\n");
	var_set_ref_object (vars, object);

	uip_gamestate->object_references[object]	= TRUE;
	uip_gamestate->is_object_pronoun		= TRUE;

	/* Advance position and return. */
	uip_posn = extent;
	return TRUE;
}


/*
 * uip_skip_article()
 *
 * Skip over any "a"/"an"/"the"/"some" at the head of a string.  Helper for
 * %character% and %object% matchers.  Returns the revised string index.
 */
static sc_uint16
uip_skip_article (const sc_char *string, sc_uint16 posn)
{
	sc_uint16	skip;

	/* Skip over articles. */
	skip = posn;
	if (!sc_strncasecmp (string + posn, "a", 1)
				&& (string[posn + 1] == NUL
					|| isspace (string[posn + 1])))
		skip += 1;
	else if (!sc_strncasecmp (string + posn, "an", 2)
				&& (string[posn + 2] == NUL
					|| isspace (string[posn + 2])))
		skip += 2;
	else if (!sc_strncasecmp (string + posn, "the", 3)
				&& (string[posn + 3] == NUL
					|| isspace (string[posn + 3])))
		skip += 3;
	else if (!sc_strncasecmp (string + posn, "some", 4)
				&& (string[posn + 4] == NUL
					|| isspace (string[posn + 4])))
		skip += 4;

	/* Skip any whitespace, and return. */
	while (isspace (string[skip]) && string[skip] != NUL)
		skip++;
	return skip;
}


/*
 * uip_compare_reference()
 *
 * Helper for %character% and %object% matchers.  Matches multiple words
 * if necessary, at the current position.  Returns zero if the string
 * didn't match, otherwise the length of the current position that matched
 * the words passed in (the new value of uip_posn on match).
 */
static sc_uint16
uip_compare_reference (const sc_char *words)
{
	sc_uint16	wpos, posn;

	/* Skip articles and lead in space on words and string. */
	wpos = uip_skip_article (words, 0);
	posn = uip_skip_article (uip_string, uip_posn);

	/* Match characters from words with the string at position. */
	while (TRUE)
	    {
		/* Any character mismatch means no words match. */
		if (tolower (words[wpos]) != tolower (uip_string[posn]))
			return 0;

		/* Move to next character in each. */
		wpos++; posn++;

		/*
		 * If at space, advance over whitespace in words list.  Stop
		 * when we hit the end of the words list.
		 */
		while (isspace (words[wpos]) && words[wpos] != NUL)
			wpos++;
		if (words[wpos] == NUL)
			break;

		/*
		 * About to match another word, so advance over whitespace
		 * in the current string too.
		 */
		while (isspace (uip_string[posn])
					&& uip_string[posn] != NUL)
			posn++;
	    }

	/*
	 * We reached the end of words.  If we're at the end of the match
	 * string, or at spaces, we've matched.
	 */
	if (isspace (uip_string[posn])
				|| uip_string[posn] == NUL)
		return posn;

	/* More text after the match, so it's not quite a match. */
	return 0;
}


/*
 * uip_match_remainder()
 *
 * Helper for %character% and %object% matchers.  Matches the remainder
 * of a pattern, to resolve the difference between, say, "table leg" and
 * "table".
 */
static sc_bool
uip_match_remainder (sc_ptnoderef_t node, sc_uint16 extent)
{
	sc_ptnoderef_t	list;
	sc_uint16	start_posn;
	sc_bool		matched;

	/* Note the start position, then advance to the given extent. */
	start_posn = uip_posn;
	uip_posn = extent;

	/*
	 * Try to match everything after the node passed in, at this
	 * position in the string.
	 */
	list = uip_new_node (NODE_LIST);
	list->left_child = node->right_sibling;

	/* Match on the temporary list. */
	matched = uip_match_node (list);

	/* Free the temporary list node, and restore position. */
	uip_destroy_node (list);
	uip_posn = start_posn;

	/* Return TRUE if the pattern remainder matched. */
	return matched;
}


/*
 * uip_match_character()
 *
 * Match a %character% reference.  This function searches all NPC names and
 * aliases for possible matches, and sets the gamestate npc_references flag
 * for any that match.  The final one to match is also stored in variables.
 */
static sc_bool
uip_match_character (sc_ptnoderef_t node)
{
	sc_prop_setref_t	bundle;
	sc_var_setref_t		vars;
	sc_int32		npc;
	sc_uint16		max_extent;
	assert (uip_gamestate != NULL);

	bundle	= uip_gamestate->bundle;
	vars	= uip_gamestate->vars;
	if (uip_trace)
		sc_trace ("UIParser:   attempting to match %%character%%\n");

	/* Clear all current character references. */
	uip_gamestate->is_npc_pronoun = FALSE;
	for (npc = 0; npc < uip_gamestate->npc_count; npc++)
		uip_gamestate->npc_references[npc] = FALSE;

	/*
	 * If there are pronoun references, try to match it/him/her.  If
	 * any found, consider all matched and return TRUE.
	 */
	if (uip_match_character_pronoun ())
		return TRUE;

	/* Iterate characters, looking for a name or alias match. */
	max_extent = 0;
	for (npc = 0; npc < uip_gamestate->npc_count; npc++)
	    {
		sc_vartype_t	vt_key[4];
		sc_char		*prefix, *name, *string;
		sc_vartype_t	vt_rvalue;
		sc_int32	alias_count, alias;
		sc_uint16	extent;

		/* Get the NPC's prefix and name. */
		vt_key[0].string = "NPCs";
		vt_key[1].integer = npc;
		vt_key[2].string = "Prefix";
		prefix = prop_get_string (bundle, "S<-sis", vt_key);
		vt_key[2].string = "Name";
		name = prop_get_string (bundle, "S<-sis", vt_key);
		if (uip_trace)
			sc_trace ("UIParser:     trying %s\n", name);

		/* Build a prefix_name string. */
		string = sc_malloc (strlen (prefix) + strlen (name) + 2);
		strcpy (string, prefix);
		strcat (string, " ");
		strcat (string, name);

		/* Check this, and just the name, against the input. */
		extent = uip_compare_reference (string);
		if (extent == 0)
			extent = uip_compare_reference (name);
		if (extent > 0
			&& uip_match_remainder (node, extent))
		    {
			if (uip_trace)
				sc_trace ("UIParser:     matched\n");

			/* Note match extent. */
			if (extent > max_extent)
				max_extent = extent;

			/* Save match in variables and gamestate. */
			var_set_ref_character (vars, npc);
			uip_gamestate->npc_references[npc] = TRUE;
		    }
		sc_free (string);

		/* Now compare against all NPC aliases. */
		vt_key[2].string = "Alias";
		alias_count = prop_get (bundle, "I<-sis", &vt_rvalue, vt_key)
						? vt_rvalue.integer : 0;
		for (alias = 0; alias < alias_count; alias++)
		    {
			sc_char		*alias_name;

			/*
			 * Get the NPC alias.  Version 3.9 games introduce
			 * empty aliases, so check here.
			 */
			vt_key[3].integer = alias;
			alias_name = prop_get_string (bundle,
							"S<-sisi", vt_key);
			if (sc_strempty (alias_name))
				continue;
			if (uip_trace)
			    {
				sc_trace ("UIParser:     trying"
						" alias %s\n", alias_name);
			    }

			/* Build a prefix_alias string. */
			string = sc_malloc (strlen (prefix)
						+ strlen (alias_name) + 2);
			strcpy (string, prefix);
			strcat (string, " ");
			strcat (string, alias_name);

			/* Check this, and just the alias, against the input. */
			extent = uip_compare_reference (string);
			if (extent == 0)
				extent = uip_compare_reference (alias_name);
			if (extent > 0
				&& uip_match_remainder (node, extent))
			    {
				if (uip_trace)
					sc_trace ("UIParser:     matched\n");

				/* Note match extent. */
				if (extent > max_extent)
					max_extent = extent;

				/* Save match in variables and gamestate. */
				var_set_ref_character (vars, npc);
				uip_gamestate->npc_references[npc] = TRUE;
			    }
			sc_free (string);
		    }
	    }

	/* On match, advance position and return successfully. */
	if (max_extent > 0)
	    {
		uip_posn = max_extent;
		return TRUE;
	    }

	/* No match. */
	return FALSE;
}


/*
 * uip_match_object()
 *
 * Match an %object% reference.  This function searches all object names and
 * aliases for possible matches, and sets the gamestate object_references flag
 * for any that match.  The final one to match is also stored in variables.
 */
static sc_bool
uip_match_object (sc_ptnoderef_t node)
{
	sc_prop_setref_t	bundle;
	sc_var_setref_t		vars;
	sc_int32		object;
	sc_uint16		max_extent;
	assert (uip_gamestate != NULL);

	bundle	= uip_gamestate->bundle;
	vars	= uip_gamestate->vars;
	if (uip_trace)
		sc_trace ("UIParser:   attempting to match %%object%%\n");

	/* Clear all current object references. */
	uip_gamestate->is_object_pronoun = FALSE;
	for (object = 0; object < uip_gamestate->object_count; object++)
		uip_gamestate->object_references[object] = FALSE;

	/*
	 * If there are pronoun references, try to match "it".  If found,
	 * consider all matched and return TRUE.
	 */
	if (uip_match_object_pronoun ())
		return TRUE;

	/* Iterate objects, looking for a name or alias match. */
	max_extent = 0;
	for (object = 0; object < uip_gamestate->object_count; object++)
	    {
		sc_vartype_t	vt_key[4];
		sc_char		*prefix, *name, *string;
		sc_vartype_t	vt_rvalue;
		sc_int32	alias_count, alias;
		sc_uint16	extent;

		/* Get the object's prefix and name. */
		vt_key[0].string = "Objects";
		vt_key[1].integer = object;
		vt_key[2].string = "Prefix";
		prefix = prop_get_string (bundle, "S<-sis", vt_key);
		vt_key[2].string = "Short";
		name = prop_get_string (bundle, "S<-sis", vt_key);
		if (uip_trace)
			sc_trace ("UIParser:     trying %s\n", name);

		/* Build a prefix_name string. */
		string = sc_malloc (strlen (prefix) + strlen (name) + 2);
		strcpy (string, prefix);
		strcat (string, " ");
		strcat (string, name);

		/* Check this, and just the name, against the input. */
		extent = uip_compare_reference (string);
		if (extent == 0)
			extent = uip_compare_reference (name);
		if (extent > 0
			&& uip_match_remainder (node, extent))
		    {
			if (uip_trace)
				sc_trace ("UIParser:     matched\n");

			/* Note match extent. */
			if (extent > max_extent)
				max_extent = extent;

			/* Save match in variables and gamestate. */
			var_set_ref_object (vars, object);
			uip_gamestate->object_references[object] = TRUE;
		    }
		sc_free (string);

		/* Now compare against all object aliases. */
		vt_key[2].string = "Alias";
		alias_count = prop_get (bundle, "I<-sis", &vt_rvalue, vt_key)
						? vt_rvalue.integer : 0;
		for (alias = 0; alias < alias_count; alias++)
		    {
			sc_char		*alias_name;

			/*
			 * Get the object alias.  Version 3.9 games introduce
			 * empty aliases, so check here.
			 */
			vt_key[3].integer = alias;
			alias_name = prop_get_string (bundle,
							"S<-sisi", vt_key);
			if (sc_strempty (alias_name))
				continue;
			if (uip_trace)
			    {
				sc_trace ("UIParser:     trying"
						" alias %s\n", alias_name);
			    }

			/* Build a prefix_alias string. */
			string = sc_malloc (strlen (prefix)
						+ strlen (alias_name) + 2);
			strcpy (string, prefix);
			strcat (string, " ");
			strcat (string, alias_name);

			/* Check this, and alias, against the input word. */
			extent = uip_compare_reference (string);
			if (extent == 0)
				extent = uip_compare_reference (alias_name);
			if (extent > 0
				&& uip_match_remainder (node, extent))
			    {
				if (uip_trace)
					sc_trace ("UIParser:     matched\n");

				/* Note match extent. */
				if (extent > max_extent)
					max_extent = extent;

				/* Save match in variables and gamestate. */
				var_set_ref_object (vars, object);
				uip_gamestate->object_references[object] = TRUE;
			    }
			sc_free (string);
		    }
	    }

	/* On match, advance position and return successfully. */
	if (max_extent > 0)
	    {
		uip_posn = max_extent;
		return TRUE;
	    }

	/* No match. */
	return FALSE;
}


/*
 * uip_match_node()
 *
 * Attempt to match the given node to the current match string/position.
 * Return TRUE, with position advanced, on match, FALSE on fail with the
 * position unchanged.
 */
static sc_bool
uip_match_node (sc_ptnoderef_t node)
{
	sc_bool		match = FALSE;

	/* Match depending on node type. */
	switch (node->type)
	    {
	    case NODE_EOS:
		match = uip_match_eos ();				break;
	    case NODE_WORD:
		match = uip_match_word (node);				break;
	    case NODE_WHITESPACE:
		match = uip_match_whitespace ();			break;
	    case NODE_LIST:
		match = uip_match_list (node);				break;
	    case NODE_CHOICE:
		match = uip_match_choice (node);			break;
	    case NODE_OPTIONAL:
		match = uip_match_optional (node);			break;
	    case NODE_WILDCARD:
		match = uip_match_wildcard (node);			break;
	    case NODE_CHARACTER_REF:
		match = uip_match_character (node);			break;
	    case NODE_OBJECT_REF:
		match = uip_match_object (node);			break;
	    case NODE_NUMBER_REF:
		match = uip_match_number ();				break;
	    case NODE_TEXT_REF:
		match = uip_match_text (node);				break;
	    default:
		sc_fatal ("uip_match_node: invalid type, %d\n", node->type);
	    }

	return match;
}


/*
 * uip_trim()
 *
 * Return a copy of a string, with leading and trailing space removed.
 */
static sc_char *
uip_trim (const sc_char *string)
{
	sc_uint16	start, end;
	sc_char		*trimmed;

	/*
	 * Deal with the case of empty strings separately.  In particular,
	 * zero-length strings won't work right with the loops below.
	 */
	if (sc_strempty (string))
	    {
		trimmed = sc_malloc (1);
		trimmed[0] = NUL;
		return trimmed;
	    }

	/* Find the start and the end of string text. */
	assert (strlen (string) != 0);
	for (start = 0; string[start] != NUL && isspace (string[start]); )
		start++;
	for (end = strlen (string) - 1; end > start && isspace (string[end]); )
		end--;

	/* Malloc and copy the string.  Return the copy. */
	trimmed = sc_malloc (end - start + 2);
	memcpy (trimmed, string + start, end - start + 1);
	trimmed[end - start + 1] = NUL;
	return trimmed;
}


/*
 * uip_debug_trace()
 *
 * Set pattern match tracing on/off.
 */
void
uip_debug_trace (sc_bool flag)
{
	uip_trace = flag;
}


/*
 * uip_match()
 *
 * Match a string to a pattern, and return TRUE on match, FALSE otherwise.
 */
sc_bool
uip_match (const sc_char *pattern, const sc_char *string,
		sc_gamestateref_t gamestate)
{
	static sc_char		*t_pattern;		/* For setjmp safety. */
	static sc_ptnoderef_t	tree;			/* For setjmp safety. */
	assert (pattern != NULL && string != NULL && gamestate != NULL);

	/* Start tokenizer. */
	t_pattern = uip_trim (pattern);
	if (uip_trace)
		sc_trace ("UIParser: pattern \"%s\"\n", t_pattern);
	uip_tokenize_start (t_pattern);

	/* Set up error handling jump buffer. */
	if (setjmp (uip_parse_error) == 0)
	    {
		sc_char		*t_string;
		sc_bool		match;

		/* Parse the pattern into a match tree. */
		uip_parse_lookahead = uip_next_token ();
		tree = uip_parse_list ();
		uip_tokenize_end ();
		sc_free (t_pattern);

		/* Match the string to the pattern tree. */
		t_string = uip_trim (string);
		if (uip_trace)
			sc_trace ("UIParser:  string \"%s\"\n", t_string);
		uip_match_start (t_string, gamestate);
		match = uip_match_node (tree);
		uip_match_end ();
		sc_free (t_string);

		/* Finished with the parsed pattern tree. */
		uip_destroy_tree (tree);

		/* Return result of matching. */
		if (uip_trace)
			sc_trace ("UIParser: %s\n",
					match ? "MATCHED!" : "No match");
		return match;
	    }

	/* Parse error -- clean up and fail. */
	uip_tokenize_end ();
	sc_free (t_pattern);
	uip_destroy_tree (tree);
	return FALSE;
}
