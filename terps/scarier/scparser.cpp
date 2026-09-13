/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
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
 * o Some of the "finer" points of pattern matching in relation to "*"
 *   wildcards, and %text%, are unknown.
 *
 * o The inclusion of part or all of prefixes in %character% and %object%
 *   matching may be right; then again, it may not be.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "scarier.h"
#include "scprotos.h"
#include "scgamest.h"


/*
 * uip_strdup()
 *
 * Copy a std::string into a freshly scr_malloc'ed C string, so callers that
 * expect to scr_free() the result keep working unchanged.  The boundary between
 * std::string internals and the engine's char* contract.
 */
static scr_char *
uip_strdup (const std::string &string)
{
  scr_char *buffer = (scr_char *) scr_malloc (string.size () + 1);
  memcpy (buffer, string.c_str (), string.size () + 1);
  return buffer;
}


/* Assorted definitions and constants. */
static const scr_char NUL = '\0';
static const scr_char MINUS = '-';
static const scr_char PLUS = '+';
static const scr_char PERCENT = '%';
static const scr_char *const WHITESPACE = "\t\n\v\f\r ";

/* Pattern matching trace flag. */
static scr_bool uip_trace = FALSE;

/*
 * Antecedent article bookkeeping for the 4.0 pronoun echo.  Set by
 * uip_replace_pronouns() when a pronoun was substituted this turn (the
 * Runner leaves its antecedent alone for such commands), and by the library's
 * unknown-verb handler, whose antecedent is definite; both are consumed and
 * cleared by uip_assign_pronouns().  See uip_definite_form() below.
 */
static scr_bool uip_pronoun_used = FALSE;
static scr_bool uip_pending_definite = FALSE;

void
uip_note_definite_reference (void)
{
  uip_pending_definite = TRUE;
}

/* Enumeration of tokens.  TOK_NONE represents a non-occurring token. */
typedef enum
{
  TOK_NONE = 0,
  TOK_CHOICE, TOK_CHOICE_END, TOK_OPTIONAL, TOK_OPTIONAL_END,
  TOK_ALTERNATES_SEPARATOR,
  TOK_WILDCARD, TOK_WHITESPACE, TOK_WORD, TOK_VARIABLE,
  TOK_CHARACTER_REFERENCE, TOK_OBJECT_REFERENCE, TOK_NUMBER_REFERENCE,
  TOK_TEXT_REFERENCE, TOK_EOS
} scr_uip_tok_t;

/*
 * Small table tying token strings to tokens.  Anything not whitespace and
 * not caught by the table is a plain TOK_WORD.
 */
typedef struct
{
  const scr_char *const name;
  const scr_int length;
  const scr_uip_tok_t token;
} scr_uip_token_entry_t;

static const scr_uip_token_entry_t UIP_TOKENS[] = {
  {"[", 1, TOK_CHOICE}, {"]", 1, TOK_CHOICE_END},
  {"{", 1, TOK_OPTIONAL}, {"}", 1, TOK_OPTIONAL_END},
  {"/", 1, TOK_ALTERNATES_SEPARATOR},
  {"*", 1, TOK_WILDCARD},
  {"%character%", 11, TOK_CHARACTER_REFERENCE},
  {"%object%", 8, TOK_OBJECT_REFERENCE},
  {"%number%", 8, TOK_NUMBER_REFERENCE},
  {"%text%", 6, TOK_TEXT_REFERENCE},
  {NULL, 0, TOK_NONE}
};


/*
 * Tokenizer variables.  The temporary is used for keeping word token values.
 * For improved performance, we'll set it to indicate a static buffer if
 * short enough, otherwise it's allocated.
 */
static const scr_char *uip_pattern = NULL;
static scr_int uip_index = 0;
static const scr_char *uip_token_value;
enum { UIP_ALLOCATION_AVOIDANCE_SIZE = 128 };
static scr_char uip_static_temporary[UIP_ALLOCATION_AVOIDANCE_SIZE];
static scr_char *uip_temporary = NULL;

/*
 * uip_tokenize_start()
 * uip_tokenize_end()
 *
 * Start and wrap up pattern string tokenization.
 */
static void
uip_tokenize_start (const scr_char *pattern)
{
  static scr_bool initialized = FALSE;
  scr_int required;

  /* On first call only, verify the string lengths in the table. */
  if (!initialized)
    {
      const scr_uip_token_entry_t *entry;

      /* Compare table lengths with string lengths. */
      for (entry = UIP_TOKENS; entry->name; entry++)
        {
          if (entry->length != (scr_int) strlen (entry->name))
            {
              scr_fatal ("uip_tokenize_start:"
                        " table string length is wrong for \"%s\"\n",
                        entry->name);
            }
        }

      initialized = TRUE;
    }

  /* Save pattern, and restart index. */
  uip_pattern = pattern;
  uip_index = 0;

  /* Set up temporary; static if long enough, otherwise allocated. */
  required = strlen (pattern) + 1;
  uip_temporary = (required > UIP_ALLOCATION_AVOIDANCE_SIZE)
                  ? (decltype(+uip_static_temporary)) scr_malloc (required) : uip_static_temporary;
}

static void
uip_tokenize_end (void)
{
  /* Deallocate temporary if required, and clear pattern and index. */
  if (uip_temporary != uip_static_temporary)
    scr_free (uip_temporary);
  uip_temporary = NULL;
  uip_pattern = NULL;
  uip_index = 0;
}


/*
 * uip_next_token()
 *
 * Return the next token from the current pattern.
 */
static scr_uip_tok_t
uip_next_token (void)
{
  const scr_uip_token_entry_t *entry;
  scr_char close;
  assert (uip_pattern);

  /* Get next character, return EOS if at pattern end. */
  if (uip_pattern[uip_index] == NUL)
    {
      uip_token_value = NULL;
      return TOK_EOS;
    }

  /* If whitespace, skip it, then return a whitespace token. */
  if (scr_isspace (uip_pattern[uip_index]))
    {
      uip_index++;
      while (scr_isspace (uip_pattern[uip_index])
             && uip_pattern[uip_index] != NUL)
        uip_index++;
      uip_token_value = NULL;
      return TOK_WHITESPACE;
    }

  /* Search the table for matching strings. */
  for (entry = UIP_TOKENS; entry->name; entry++)
    {
      if (strncmp (uip_pattern + uip_index, entry->name, entry->length) == 0)
        break;
    }
  if (entry->name)
    {
      /* Advance over string, and return token. */
      uip_index += entry->length;
      uip_token_value = NULL;
      return entry->token;
    }

  /*
   * Search for a non-special variable reference.  This is apparently an
   * Adrift extension to the standard pattern match, allowing %user_var% to
   * be used in patterns.  If found, return a variable with the name as the
   * token value.  We can't interpolate the value into the string either
   * here or earlier, so we have to save the variable's name, and retrieve
   * it when we come to try the match.
   */
  if (sscanf (uip_pattern + uip_index, "%%%[^%]%c", uip_temporary, &close) == 2
      && close == PERCENT)
    {
      uip_index += strlen (uip_temporary) + 2;
      uip_token_value = uip_temporary;
      return TOK_VARIABLE;
    }

  /*
   * Return a word.  This is a contiguous run of non-pattern-special, non-
   * whitespace, non-percent characters
   */
  sscanf (uip_pattern + uip_index, "%[^][/{}*% \f\n\r\t\v]", uip_temporary);
  uip_token_value = uip_temporary;
  uip_index += strlen (uip_temporary);
  return TOK_WORD;
}


/*
 * uip_current_token_value()
 *
 * Return the token value of the current token.  It is an error to call
 * here if the current token is not a TOK_WORD or TOK_VARIABLE.
 */
static const scr_char *
uip_current_token_value (void)
{
  /* If the token value is NULL, the current token isn't a word. */
  if (!uip_token_value)
    {
      scr_fatal ("uip_current_token_value:"
                " attempt to take undefined token value\n");
    }

  /* Return value. */
  return uip_token_value;
}


/*
 * Parsed pattern tree node definition.   The tree is a left child, right
 * sibling representation, with token type, and word at nodes of type TOK_WORD.
 * NODE_UNUSED must be zero to ensure that the statically allocated array that
 * forms the node pool appears initially as containing only unused nodes.
 */
typedef enum
{
  NODE_UNUSED = 0,
  NODE_CHOICE, NODE_OPTIONAL, NODE_WILDCARD, NODE_WHITESPACE,
  NODE_HARD_WHITESPACE, NODE_JOIN,
  NODE_CHARACTER_REFERENCE, NODE_OBJECT_REFERENCE, NODE_TEXT_REFERENCE,
  NODE_NUMBER_REFERENCE, NODE_WORD, NODE_VARIABLE, NODE_LIST, NODE_EOS
} scr_pttype_t;
typedef struct scr_ptnode_s
{
  struct scr_ptnode_s *left_child;
  struct scr_ptnode_s *right_sibling;

  scr_pttype_t type;
  scr_char *word;
  scr_bool is_allocated;
} scr_ptnode_t;
typedef scr_ptnode_t *scr_ptnoderef_t;

/* Predictive parser lookahead token. */
static scr_uip_tok_t uip_parse_lookahead = TOK_NONE;

/* Parse error jump buffer. */
static jmp_buf uip_parse_error;

/*
 * Nesting depth of [...] and {...} groups currently being parsed.  A "/" is
 * an alternatives separator only inside a group; at depth zero run400 has no
 * notion of it at all and treats it as an ordinary literal character (see
 * uip_parse_list()).
 */
static scr_int uip_parse_group_depth = 0;

/* Parse tree for cleanup, and forward declaration of pattern list parser. */
static scr_ptnoderef_t uip_parse_tree = (scr_ptnoderef_t) NULL;
static void uip_parse_list (scr_ptnoderef_t list);

/*
 * Pool of statically allocated nodes, for faster allocations.  Nodes are
 * first allocated from here, then by straight malloc() if this pool is empty.
 * An average game's peak node allocation seems to be around 40-50 nodes, so
 * allowing 128 here should be plenty.
 */
enum { UIP_NODE_POOL_SIZE = 128 };
static scr_ptnode_t uip_node_pool[UIP_NODE_POOL_SIZE];
static scr_int uip_node_pool_cursor = 0;
static scr_int uip_node_pool_available = UIP_NODE_POOL_SIZE;

/*
 * Words held at nodes are usually short (15 chars covers 95% of English), so
 * to avoid a lot of small allocations we use a pool of short strings, used
 * first, then by straight malloc() should the pool empty.
 */
enum { UIP_WORD_POOL_SIZE = 64, UIP_SHORT_WORD_SIZE = 16 };
typedef struct
{
  scr_bool is_in_use;
  scr_char word[UIP_SHORT_WORD_SIZE];
} scr_ptshortword_t;
typedef scr_ptshortword_t *scr_ptshortwordref_t;
static scr_ptshortword_t uip_word_pool[UIP_WORD_POOL_SIZE];
static scr_int uip_word_pool_cursor = 0;
static scr_int uip_word_pool_available = UIP_WORD_POOL_SIZE;

/*
 * uip_parse_match()
 *
 * Match a token to the lookahead, then advance lookahead.
 */
static void
uip_parse_match (scr_uip_tok_t token)
{
  if (uip_parse_lookahead == token)
    uip_parse_lookahead = uip_next_token ();
  else
    {
      /* Syntax error. */
      scr_error ("uip_parse_match: syntax error, expected %ld, got %ld\n",
                (scr_int) uip_parse_lookahead, (scr_int) token);
      scr_longjmp (uip_parse_error, 1);
    }
}


/*
 * uip_new_word()
 *
 * Return a string containing a copy of the word.  Uses a ring allocator to
 * allocate initially from static storage, for performance.  If this is
 * exhausted, backs off to standard allocation.
 */
static scr_char *
uip_new_word (const scr_char *word)
{
  scr_int required;

  /*
   * Unless the pool is empty, search forwards from the next cursor position
   * until an unused slot is found, or until the index wraps to the cursor.
   */
  required = strlen (word) + 1;
  if (uip_word_pool_available > 0 && required <= UIP_SHORT_WORD_SIZE)
    {
      scr_int index_;
      scr_ptshortwordref_t shortword;

      index_ = (uip_word_pool_cursor + 1) % UIP_WORD_POOL_SIZE;
      while (index_ != uip_word_pool_cursor)
        {
          if (!uip_word_pool[index_].is_in_use)
            break;
          index_ = (index_ + 1) % UIP_WORD_POOL_SIZE;
        }

      if (uip_word_pool[index_].is_in_use)
        scr_fatal ("uip_new_word: no free slot found in the words pool\n");

      /* Use the slot and update the pool cursor and free count. */
      shortword = uip_word_pool + index_;
      strncpy (shortword->word, word, UIP_SHORT_WORD_SIZE);
      shortword->is_in_use = TRUE;

      uip_word_pool_cursor = index_;
      uip_word_pool_available--;

      /* Return the address of the copied string. */
      return shortword->word;
    }
  else
    {
      scr_char *word_copy;

      /* Fall back to less efficient allocations. */
      word_copy = (decltype(word_copy)) scr_malloc (required);
      strncpy (word_copy, word, required);
      return word_copy;
    }
}


/*
 * uip_free_word()
 *
 * If the word was allocated, free its memory; if not, find its short word
 * pool entry and return it to the pool.
 */
static void
uip_free_word (scr_char *word)
{
  const scr_char *first_in_pool, *last_in_pool;

  /* Obtain the range of valid addresses for words from the word pool. */
  first_in_pool = uip_word_pool[0].word;
  last_in_pool = uip_word_pool[UIP_WORD_POOL_SIZE - 1].word;

  /* If from the pool, mark the entry as no longer in use, otherwise free. */
  if (word >= first_in_pool && word <= last_in_pool)
    {
      scr_int index_;
      scr_ptshortwordref_t shortword;

      /*
       * Calculate the index to the word pool entry from which this short
       * word was allocated.
       */
      index_ = (word - first_in_pool) / sizeof (uip_word_pool[0]);
      shortword = uip_word_pool + index_;
      assert (shortword->word == word);

      shortword->is_in_use = FALSE;
      uip_word_pool_available++;
    }
  else
    scr_free (word);
}


/*
 * uip_new_node()
 *
 * Create a new node, populated with an initial type.  Uses a ring allocator
 * to allocate initially from static storage, for performance.  If this is
 * exhausted, backs off to standard allocation.
 */
static scr_ptnoderef_t
uip_new_node (scr_pttype_t type)
{
  scr_ptnoderef_t node;

  /*
   * Unless the pool is empty, search forwards from the next cursor position
   * until an unused slot is found, or until the index wraps to the cursor.
   */
  if (uip_node_pool_available > 0)
    {
      scr_int index_;

      index_ = (uip_node_pool_cursor + 1) % UIP_NODE_POOL_SIZE;
      while (index_ != uip_node_pool_cursor)
        {
          if (uip_node_pool[index_].type == NODE_UNUSED)
            break;
          index_ = (index_ + 1) % UIP_NODE_POOL_SIZE;
        }

      if (uip_node_pool[index_].type != NODE_UNUSED)
        scr_fatal ("uip_new_node: no free slot found in the nodes pool\n");

      /* Use the slot and update the pool cursor and free count. */
      node = uip_node_pool + index_;
      node->is_allocated = FALSE;

      uip_node_pool_cursor = index_;
      uip_node_pool_available--;
    }
  else
    {
      /* Fall back to less efficient allocations. */
      node = (decltype(node)) scr_malloc (sizeof (*node));
      node->is_allocated = TRUE;
    }

  /* Fill in the remaining fields and return the new node. */
  node->left_child = NULL;
  node->right_sibling = NULL;
  node->type = type;
  node->word = NULL;

  return node;
}


/*
 * uip_destroy_node()
 *
 * Destroy a node, and any allocated word memory.  If the node was allocated,
 * free its memory; if not, return it to the pool.
 */
static void
uip_destroy_node (scr_ptnoderef_t node)
{
  /* Free any word contained at this node. */
  if (node->word)
    uip_free_word (node->word);

  /*
   * If the node was allocated, poison memory and free it.  If it came from
   * the node pool, set it to unused and update the availability count for
   * the pool.
   */
  if (node->is_allocated)
    {
      memset (node, 0xaa, sizeof (*node));
      scr_free (node);
    }
  else
    {
      node->type = NODE_UNUSED;
      uip_node_pool_available++;
    }
}


/*
 * uip_parse_new_list()
 * uip_parse_alternatives()
 *
 * Parse a set of .../.../... alternatives for choices and optionals.  The
 * first function is a helper, returning a newly constructed parsed list.
 */
static scr_ptnoderef_t
uip_parse_new_list (void)
{
  scr_ptnoderef_t list;

  /* Create a new list node, parse into it, and return it. */
  list = uip_new_node (NODE_LIST);
  uip_parse_list (list);
  return list;
}

static void
uip_parse_alternatives (scr_ptnoderef_t node)
{
  scr_ptnoderef_t child;

  /* Parse initial alternative, then add other listed alternatives. */
  node->left_child = uip_parse_new_list ();
  child = node->left_child;
  while (uip_parse_lookahead == TOK_ALTERNATES_SEPARATOR)
    {
      uip_parse_match (TOK_ALTERNATES_SEPARATOR);
      child->right_sibling = uip_parse_new_list ();
      child = child->right_sibling;
    }
}


/*
 * uip_parse_element()
 *
 * Parse a single pattern element.
 */
static scr_ptnoderef_t
uip_parse_element (void)
{
  scr_ptnoderef_t node = (scr_ptnoderef_t) NULL;
  scr_uip_tok_t token;

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
      uip_parse_group_depth++;
      uip_parse_alternatives (node);
      uip_parse_group_depth--;
      uip_parse_match (TOK_CHOICE_END);
      break;

    case TOK_OPTIONAL:
      /* Parse a {...[/.../...]} optional element. */
      uip_parse_match (TOK_OPTIONAL);
      node = uip_new_node (NODE_OPTIONAL);
      uip_parse_group_depth++;
      uip_parse_alternatives (node);
      uip_parse_group_depth--;
      uip_parse_match (TOK_OPTIONAL_END);
      break;

    case TOK_ALTERNATES_SEPARATOR:
      {
        /*
         * A "/" outside any group.  Only reachable from uip_parse_list()
         * at depth zero, where it is a literal character rather than a
         * separator; make a word node for it.
         */
        scr_char *word;

        word = uip_new_word ("/");
        uip_parse_match (TOK_ALTERNATES_SEPARATOR);
        node = uip_new_node (NODE_WORD);
        node->word = word;
        break;
      }

    case TOK_WILDCARD:
    case TOK_CHARACTER_REFERENCE:
    case TOK_OBJECT_REFERENCE:
    case TOK_NUMBER_REFERENCE:
    case TOK_TEXT_REFERENCE:
      /* Parse %mumble% references and * wildcards. */
      token = uip_parse_lookahead;
      uip_parse_match (token);
      switch (token)
        {
        case TOK_WILDCARD:
          node = uip_new_node (NODE_WILDCARD);
          break;
        case TOK_CHARACTER_REFERENCE:
          node = uip_new_node (NODE_CHARACTER_REFERENCE);
          break;
        case TOK_OBJECT_REFERENCE:
          node = uip_new_node (NODE_OBJECT_REFERENCE);
          break;
        case TOK_NUMBER_REFERENCE:
          node = uip_new_node (NODE_NUMBER_REFERENCE);
          break;
        case TOK_TEXT_REFERENCE:
          node = uip_new_node (NODE_TEXT_REFERENCE);
          break;
        default:
          scr_fatal ("uip_parse_element: invalid token, %ld\n", (scr_int) token);
        }
      break;

    case TOK_WORD:
      {
        const scr_char *token_value;
        scr_char *word;

        /* Take a copy of the token's word value. */
        token_value = uip_current_token_value ();
        word = uip_new_word (token_value);

        /* Store details in a word node. */
        uip_parse_match (TOK_WORD);
        node = uip_new_node (NODE_WORD);
        node->word = word;
        break;
      }

    case TOK_VARIABLE:
      {
        const scr_char *token_value;
        scr_char *name;

        /* Take a copy of the token's variable name value. */
        token_value = uip_current_token_value ();
        name = uip_new_word (token_value);

        /* Store details in a variable node, overloading word. */
        uip_parse_match (TOK_VARIABLE);
        node = uip_new_node (NODE_VARIABLE);
        node->word = name;
        break;
      }

    default:
      /* Syntax error. */
      scr_error ("uip_parse_element: syntax error,"
                " unexpected token, %ld\n", (scr_int) uip_parse_lookahead);
      scr_longjmp (uip_parse_error, 1);
    }

  /* Return the newly created node. */
  assert (node);
  return node;
}


/*
 * uip_parse_list()
 *
 * Parse a list of pattern elements.
 */
static void
uip_parse_list (scr_ptnoderef_t list)
{
  scr_ptnoderef_t child, node;
  scr_bool literal_only = TRUE;

  /* Add elements until a list terminator token is encountered. */
  child = list;
  while (TRUE)
    {
      switch (uip_parse_lookahead)
        {
        case TOK_CHOICE_END:
        case TOK_OPTIONAL_END:
          /* Terminate list building and return. */
          return;

        case TOK_ALTERNATES_SEPARATOR:
          /*
           * Inside a group, this ends the current alternative.  Outside one
           * it is not a separator at all -- run400's matcher only ever looks
           * for "/" between [] or {} delimiters, so a bare "take/get/eat
           * stew" is a single literal that matches only itself.  Fall into
           * the default case, which makes a literal "/" word node.  (Before
           * this, the list was terminated here *without* a NODE_EOS, so the
           * pattern degenerated to "take" and prefix-matched anything.)
           */
          if (uip_parse_group_depth > 0)
            return;
          /* Fall through. */

        case TOK_EOS:
          /*
           * A space between the end of a plain literal pattern and the end
           * of the pattern is a space the input must have; see
           * uip_match_whitespace().  Groups and wildcards make it moot, so
           * only a pattern of nothing but words carries the mark.
           */
          if (literal_only && child != list
              && child->type == NODE_WHITESPACE)
            child->type = NODE_HARD_WHITESPACE;

          /* Place EOS at the appropriate link and return. */
          node = uip_new_node (NODE_EOS);
          if (child == list)
            child->left_child = node;
          else
            child->right_sibling = node;
          return;

        default:
          /* Add the next node at the appropriate link. */
          node = uip_parse_element ();
          if (node->type != NODE_WORD && node->type != NODE_WHITESPACE)
            literal_only = FALSE;
          if (child == list)
            {
              child->left_child = node;
              child = child->left_child;
            }
          else
            {
              /*
               * Make a special case of a choice or option next to another
               * choice or option.  In this case, add an (invented) join node,
               * which matches a space if one is there but is happy without
               * one, to ensure a match with suitable input.
               *
               * Both halves of that matter.  "[open/pull/push]{the}{wooden}
               * [door]" has no spaces in it at all, yet has to match "open
               * door" and "open the door" -- so a space between groups must be
               * allowed.  But ADRIFT authors also build single words out of
               * adjacent groups, with no separator intended: ImagiDroids has
               * "{move/run/walk/go/climb} {to/towards} {the} [d/out/in]{own}"
               * (d, down, out, in) and "[s]{outh}{ /-}[w]{est}", where the
               * explicit "{ /-}" for the space in "south west" is the proof
               * that adjacency alone does not imply one.  This node used to be
               * a plain NODE_WHITESPACE, which insisted on a space or a word
               * boundary, so "north" never matched "[n]{orth}" and the game's
               * own published walkthrough could not leave the first room.
               */
              if ((child->type == NODE_OPTIONAL || child->type == NODE_CHOICE)
                  && (node->type == NODE_OPTIONAL || node->type == NODE_CHOICE))
                {
                  scr_ptnoderef_t join;

                  /* Interpose invented optional whitespace. */
                  join = uip_new_node (NODE_JOIN);
                  child->right_sibling = join;
                  child = child->right_sibling;
                }

              child->right_sibling = node;
              child = child->right_sibling;
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
uip_destroy_tree (scr_ptnoderef_t node)
{
  if (node)
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
 * uip_debug_dump()
 *
 * Print out a pattern match tree.
 */
static void
uip_debug_dump_node (scr_ptnoderef_t node, scr_int depth)
{
  /* End recursion on null node. */
  if (node)
    {
      scr_int index_;

      scr_trace (" ");
      for (index_ = 0; index_ < depth; index_++)
        scr_trace ("  ");

      scr_trace ("%p", (void *) node);
      switch (node->type)
        {
        case NODE_CHOICE:
          scr_trace (", choice");
          break;
        case NODE_OPTIONAL:
          scr_trace (", optional");
          break;
        case NODE_WILDCARD:
          scr_trace (", wildcard");
          break;
        case NODE_WHITESPACE:
          scr_trace (", whitespace");
          break;
        case NODE_HARD_WHITESPACE:
          scr_trace (", hard whitespace");
          break;
        case NODE_JOIN:
          scr_trace (", join");
          break;
        case NODE_CHARACTER_REFERENCE:
          scr_trace (", character");
          break;
        case NODE_OBJECT_REFERENCE:
          scr_trace (", object");
          break;
        case NODE_TEXT_REFERENCE:
          scr_trace (", text");
          break;
        case NODE_NUMBER_REFERENCE:
          scr_trace (", number");
          break;
        case NODE_WORD:
          scr_trace (", word \"%s\"", node->word);
          break;
        case NODE_VARIABLE:
          scr_trace (", variable \"%s\"", node->word);
          break;
        case NODE_LIST:
          scr_trace (", list");
          break;
        case NODE_EOS:
          scr_trace (", <eos>");
          break;
        default:
          scr_trace (", unknown type %ld", (scr_int) node->type);
          break;
        }
      if (node->left_child)
        scr_trace (", left child %p", (void *) node->left_child);
      if (node->right_sibling)
        scr_trace (", right sibling %p", (void *) node->right_sibling);
      scr_trace ("\n");

      /* Recursively dump left child, then siblings. */
      uip_debug_dump_node (node->left_child, depth + 1);
      uip_debug_dump_node (node->right_sibling, depth);
    }
}

static void
uip_debug_dump (void)
{
  scr_trace ("UIParser: debug dump follows...\n");
  if (uip_parse_tree)
    {
      scr_trace ("uip_parse_tree = {\n");
      uip_debug_dump_node (uip_parse_tree, 0);
      scr_trace ("}\n");
    }
  else
    scr_trace ("uip_parse_tree = (nil)\n");
}


/* String matching variables. */
static const scr_char *uip_string = NULL;
static scr_int uip_posn = 0;
static scr_gameref_t uip_game = (scr_gameref_t) NULL;

/*
 * uip_match_start()
 * uip_match_end()
 *
 * Set up a string for matching to a pattern tree, and wrap up matching.
 */
static void
uip_match_start (const scr_char *string, scr_gameref_t game)
{
  /* Save string, and restart index. */
  uip_string = string;
  uip_posn = 0;

  /* Save the game we're working on. */
  uip_game = game;
}

static void
uip_match_end (void)
{
  /* Clear match target string, and variable set. */
  uip_string = NULL;
  uip_posn = 0;
  uip_game = NULL;
}


/*
 * uip_get_game()
 *
 * Safety wrapper to ensure module code sees a valid game when it requires
 * one.
 */
static scr_gameref_t
uip_get_game (void)
{
  assert (gs_is_game_valid (uip_game));
  return uip_game;
}


/* Forward declaration of low level node matcher. */
static scr_bool uip_match_node (scr_ptnoderef_t node);

/*
 * uip_match_eos()
 * uip_match_word()
 * uip_match_variable()
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
static scr_bool
uip_match_eos (void)
{
  /* Check that we hit the string's end. */
  return uip_string[uip_posn] == NUL;
}

static scr_bool
uip_match_word (scr_ptnoderef_t node)
{
  scr_int length;
  const scr_char *word;

  /* Get the word to match. */
  assert (node->word);
  word = node->word;

  /* Compare string text with this node's word, ignore case. */
  length = strlen (word);
  if (scr_strncasecmp (uip_string + uip_posn, word, length) == 0)
    {
      /* Word match, advance position and return. */
      uip_posn += length;
      return TRUE;
    }

  /* No match. */
  return FALSE;
}

static scr_bool
uip_match_variable (scr_ptnoderef_t node)
{
  const scr_gameref_t game = uip_get_game ();
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int type;
  scr_vartype_t vt_rvalue;
  const scr_char *name;

  /* Get the variable name to match, from overloaded word. */
  assert (node->word);
  name = node->word;

  /* Get the variable's value. */
  if (var_get (vars, name, &type, &vt_rvalue))
    {
      scr_int length;

      /* Compare the value against the current string position. */
      switch (type)
        {
          case VAR_INTEGER:
            {
              scr_char value[32];

              /* Compare numeric against the current string position. */
              snprintf (value, sizeof(value), "%ld", vt_rvalue.integer);
              length = strnlen (value, sizeof(value));
              if (strncmp (uip_string + uip_posn, value, length) == 0)
                {
                  /* Integer match, advance position and return. */
                  uip_posn += length;
                  return TRUE;
                }
              break;
            }

          case VAR_STRING:
            /* Compare string value against the current string position. */
            length = strlen (vt_rvalue.string);
            if (scr_strncasecmp (uip_string + uip_posn,
                                vt_rvalue.string, length) == 0)
              {
                /* String match, advance position and return. */
                uip_posn += length;
                return TRUE;
              }
            break;

          default:
            scr_fatal ("uip_match_variable: invalid variable type, %ld\n", type);
        }
    }

  /* No match, or no such variable. */
  return FALSE;
}

static scr_bool
uip_match_whitespace (scr_bool hard)
{
  /* If next character is space, read whitespace and return. */
  if (scr_isspace (uip_string[uip_posn]))
    {
      /* Space match, advance position and return. */
      while (uip_string[uip_posn] != NUL && scr_isspace (uip_string[uip_posn]))
        uip_posn++;
      return TRUE;
    }

  /*
   * A space the author left after the last WORD of a pattern is a space the
   * input has to have, and the player's line is trimmed, so such a pattern
   * matches nothing at all.  "Sommeril" has a task whose commands are "get
   * placemat " (trailing space), "take placemat", "get placemat page" and
   * three more; run400 answers `take placemat` with the task and `get
   * placemat` with the library's "Take what?", the placemat being unlisted
   * and so unreferenceable (measured 2026-09-05, Adrift_80.txt).  Without
   * this the end-of-string rule below matched the stray space and the task
   * claimed both.
   *
   * Only after a word: a trailing space after a [] or {} group is ignored,
   * and the corpus is emphatic about it -- "Woof"'s "[chase/hunt]
   * [bizet/Bizet/cat] " has to answer `chase cat`, and eight more rows
   * (the_cat_in_the_tree, skydiver, apokalupsis, wax_worx, vendetta, hub,
   * magicshow, house, valley) fail the same way if the space is made to
   * count there.  uip_parse_list() marks the ones that do count.
   */
  if (hard)
    return FALSE;

  /*
   * No match.  However, if we're trying to match space, this is a word
   * boundary.  So... even though we're not sitting on a space, if the string
   * prior character is whitespace, "double-match" the space.
   *
   * Also, match if we haven't yet matched any text.  In effect, this means
   * leading spaces on patterns will be ignored -- harmless, even useful,
   * and long since validated by the walkthrough corpus.
   */
  if (uip_posn == 0 || scr_isspace (uip_string[uip_posn - 1]))
    return TRUE;

  /*
   * And that's not all.  We also want to match whitespace if we're at the end
   * of a string (another word boundary).  This will permit patterns that end
   * in optional elements to succeed since options and wildcards always match,
   * even if to no text.
   */
  if (uip_string[uip_posn] == NUL)
    return TRUE;

  /* No match.  Really. */
  return FALSE;
}

/*
 * Optional whitespace, invented between two adjacent [] or {} groups.  Eat a
 * space if one is present, but never fail -- see uip_parse_list().
 */
static scr_bool
uip_match_join (void)
{
  while (uip_string[uip_posn] != NUL && scr_isspace (uip_string[uip_posn]))
    uip_posn++;

  return TRUE;
}

static scr_bool
uip_match_list (scr_ptnoderef_t node)
{
  scr_ptnoderef_t child;

  /*
   * If this list is empty, fail the match.  This special-case handling is
   * what catches constructed temporary lists for wildcard-like items that
   * don't actually encompass anything.
   */
  if (!node->left_child)
    return FALSE;

  /* Match everything listed sequentially. */
  for (child = node->left_child; child; child = child->right_sibling)
    {
      if (!uip_match_node (child))
        {
          /* No match. */
          return FALSE;
        }
    }

  /* Matched. */
  return TRUE;
}

static scr_bool
uip_match_alternatives (scr_ptnoderef_t node)
{
  scr_ptnoderef_t child;
  scr_int start_posn, extent;
  scr_bool matched;

  /* Note the start position for rewind between tries. */
  start_posn = uip_posn;

  /*
   * Try a match on each of the children, looking to see which one moves the
   * position on the furthest.  Match on this one.  This is a "maximal munch".
   */
  extent = uip_posn;
  matched = FALSE;
  for (child = node->left_child; child; child = child->right_sibling)
    {
      uip_posn = start_posn;
      if (uip_match_node (child))
        {
          /* Matched. */
          matched = TRUE;
          if (uip_posn > extent)
            extent = uip_posn;
        }
    }

  /* If matched, set position to extent; if not, back to start. */
  uip_posn = matched ? extent : start_posn;

  /* Return match status. */
  return matched;
}

static scr_bool
uip_match_choice (scr_ptnoderef_t node)
{
  /*
   * Return the result of matching alternatives.  The choice will therefore
   * fail if none of the alternatives match.
   */
  return uip_match_alternatives (node);
}

static scr_bool
uip_match_optional (scr_ptnoderef_t node)
{
  scr_int start_posn;
  scr_ptnoderef_t list;
  scr_bool matched;

  /* Note the start position for rewind on empty match. */
  start_posn = uip_posn;

  /*
   * Look ahead to see if we can match to nothing, and still have the main
   * pattern match.  If we can, we'll go with this.  It's a "minimal munch"-ish
   * strategy, but seems to be what Adrift does in this situation.
   */
  list = uip_new_node (NODE_LIST);
  list->left_child = node->right_sibling;

  /* Match on the temporary list. */
  matched = uip_match_node (list);

  /* Free the temporary list node. */
  uip_destroy_node (list);

  /*
   * If the temporary matched and consumed text, rewind position to match
   * nothing.  If it didn't, rewind anyway -- a failed look-ahead may still
   * have advanced the position, since uip_match_list() has no backtracking of
   * its own -- and then match alternatives to consume anything that may match
   * our options.
   *
   * The rewind before uip_match_alternatives() matters whenever an option is
   * a prefix of the word actually in the input.  Monsters (Release 2) has
   * "shine {the} [flashlight/light] {on} {the} {brainsucker} {brain}
   * {monster}": against "shine flashlight on the brainsucker" the look-ahead
   * from {brainsucker} lets {brain} eat the first five letters of
   * "brainsucker" (uip_match_word() is a prefix compare with no word-boundary
   * check), then fails on the trailing "sucker".  Without the rewind the
   * alternatives are tried at "sucker", {brainsucker} matches nothing, and
   * the whole pattern dies -- which lost the game its brainsucker task even
   * though the author's own published transcript shows the command working.
   */
  if (matched && uip_posn > start_posn)
    uip_posn = start_posn;
  else
    {
      uip_posn = start_posn;
      uip_match_alternatives (node);
    }

  /* Return TRUE no matter what. */
  return TRUE;
}

static scr_bool
uip_match_wildcard (scr_ptnoderef_t node)
{
  scr_int start_posn, limit, index_;
  scr_bool matched;
  scr_ptnoderef_t list;

  /*
   * At least one game uses patterns like "thing******...".  Why?  Who knows.
   * But if we're in a list of wildcards, and not the first, ignore the call;
   * only the final one needs handling.
   */
  if (node->right_sibling && node->right_sibling->type == NODE_WILDCARD)
    return TRUE;

  /* Note the start position for rewind on no match. */
  start_posn = uip_posn;

  /*
   * To make life a little easier, we'll match on the tree to the right of
   * this node by constructing a temporary list node, containing stuff to the
   * right of the wildcard, and then matching on that.
   */
  list = uip_new_node (NODE_LIST);
  list->left_child = node->right_sibling;

  /*
   * Repeatedly try to match the rest of the tree at successive character
   * positions, and stop if we succeed.  This is a "minimal munch", which may
   * or may not be the right thing to be doing here.
   *
   * When scanning forward, take care to include the NUL, needed to match
   * TOK_EOS.
   */
  matched = FALSE;
  limit = strlen (uip_string) + 1;
  for (index_ = uip_posn + 1; index_ < limit; index_++)
    {
      uip_posn = index_;
      if (uip_match_node (list))
        {
          /* Wildcard match at this point. */
          uip_posn = index_;
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
static scr_bool
uip_match_number (void)
{
  const scr_gameref_t game = uip_get_game ();
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int number;

  /* Attempt to read a number from input. */
  if (sscanf (uip_string + uip_posn, "%ld", &number) == 1)
    {
      /* Advance position over the number. */
      while (uip_string[uip_posn] == MINUS || uip_string[uip_posn] == PLUS)
        uip_posn++;
      while (scr_isdigit (uip_string[uip_posn]))
        uip_posn++;

      /* Set number reference in variables and return. */
      var_set_ref_number (vars, number);
      return TRUE;
    }

  /* No match. */
  return FALSE;
}

static scr_bool
uip_match_text (scr_ptnoderef_t node)
{
  const scr_gameref_t game = uip_get_game ();
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int start_posn, limit, index_;
  scr_bool matched;
  scr_ptnoderef_t list;

  /* Note the start position for rewind on no match. */
  start_posn = uip_posn;

  /*
   * As with wildcards, create a temporary list of the stuff to the right of
   * the reference node, and match on that.
   */
  list = uip_new_node (NODE_LIST);
  list->left_child = node->right_sibling;

  /*
   * Again, as with wildcards, repeatedly try to match the rest of the tree at
   * successive character positions, stopping if we succeed.
   */
  matched = FALSE;
  limit = strlen (uip_string) + 1;
  for (index_ = uip_posn + 1; index_ < limit; index_++)
    {
      uip_posn = index_;
      if (uip_match_node (list))
        {
          /* Text reference match at this point. */
          uip_posn = index_;
          matched = TRUE;
          break;
        }
    }

  /* Free the temporary list node. */
  uip_destroy_node (list);

  /* See if we found a match in the loop. */
  if (matched)
    {
      /* Found a match; create a string and save the text. */
      std::string string (uip_string + start_posn, uip_posn - start_posn);

      /*
       * Adrift seems to save referenced text as all-lowercase; we need to do
       * the same.
       */
      for (index_ = 0; string[index_] != NUL; index_++)
        string[index_] = scr_tolower (string[index_]);
      var_set_ref_text (vars, string.c_str ());

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
 * uip_skip_article()
 *
 * Skip over any "a"/"an"/"the"/"some" at the head of a string.  Helper for
 * %character% and %object% matchers.  Returns the revised string position.
 */
static scr_int
uip_skip_article (const scr_char *string, scr_int start)
{
  scr_int posn;

  /* Skip over articles. */
  posn = start;
  if (scr_compare_word (string + posn, "a", 1))
    posn += 1;
  else if (scr_compare_word (string + posn, "an", 2))
    posn += 2;
  else if (scr_compare_word (string + posn, "the", 3))
    posn += 3;
  else if (scr_compare_word (string + posn, "some", 4))
    posn += 4;

  /* Skip any whitespace, and return. */
  while (scr_isspace (string[posn]) && string[posn] != NUL)
    posn++;
  return posn;
}


/*
 * Strict %object% / %character% matching.
 *
 * MEASURED 2026-08-25 on p4BURN.taf under run400 (Adrift_12_p4burn.txt
 * and Adrift_13_p4burn.txt, every command echoed).  A task command pattern is matched
 * by the Runner at 458BBC: it takes the lowercased pattern, does a binary
 * Replace() of "%object%" with the object's Short -- or, in the loop just
 * below, one of its Aliases -- VERBATIM, and compares the result for exact
 * equality with the lowercased input.  Nothing else takes part: no Prefix,
 * no article, no partial name, and no case folding of the name itself.
 *
 *   task "PX %object%", object Short "coin"        px coin  -> runs
 *                                                  PX coin  -> runs
 *   task "pa %object%", object Short "Widget"      pa widget -> no
 *                                                  pa Widget -> no
 *   task "pa %object%", Short "brass key",         pa brass key       -> runs
 *                       Prefix "a small"           pa key             -> no
 *                                                  pa a brass key     -> no
 *                                                  pa the brass key   -> no
 *                                                  pa small brass key -> no
 *   task "pa %object%", Short "gem", Alias "jewel" pa gem   -> runs
 *                                                  pa jewel -> runs
 *                                                  pa a gem -> no
 *
 * So an object whose Short carries a capital letter can never bind a bare
 * %object% -- which is why The X-Files' task 24, `Burn %object%` over objects
 * named "Memo", "Coffee Mug" and "Gun Holster", never fires in the Runner
 * however the player phrases it.  Bisecting the game itself confirmed it:
 * lowering the verb and the Short together makes `burn memo` run and print
 * the CompleteText (Adrift_11_xfilesbisect.txt).
 *
 * %character% is NOT affected: its half of the same matcher (46918F) runs the
 * NPC Name and each Alias through LCase() before the Replace(), where the
 * object half (458BF4) substitutes the Short raw.  The asymmetry is the whole
 * bug, and it is one-sided -- ADRIFTMAS Party's `[kiss {the} %character%]`
 * over an NPC named "Mystery" runs in the Runner, and still runs here.
 *
 * MEASURED 2026-08-25 on p39CASE.taf under run390 (Adrift_1_p39case.txt, all
 * 19 commands echoed): 3.90 substitutes just as strictly, but it DOES fold
 * case.  The same cells, one Runner down --
 *
 *   task "pa %object%", Short "Widget"          pa widget -> runs
 *                                               pa Widget -> runs
 *   task "pa %object%", Short "brass key",      pa brass key       -> runs
 *                       Prefix "a small"        pa key             -> no
 *                                               pa a brass key     -> no
 *                                               pa the brass key   -> no
 *                                               pa small brass key -> no
 *   task "pa %object%", Short "gem",            pa gem   -> runs
 *                       Alias "jewel"           pa jewel -> runs
 *                                               pa a gem / pa the gem -> no
 *
 * -- so strict binding starts at 3.90, and only the case fold is lost at 4.0.
 * run390 does it in Form1.frm:13991ff, through c() and the seen byte at
 * .global_44, rewriting the task command in place.  Neither run370.exe nor
 * run380.exe contains the string "%object%" at all, so before 3.90 such a
 * pattern matches nothing whatever the player types, and the tolerant matcher
 * there is harmless.
 *
 * This is task-command matching only.  The library's own patterns and the
 * variable functions go through the Runner's noun resolver, which is
 * prefix- and case-tolerant, so the flag is set only around
 * run_match_task_commands().
 */
static scr_bool uip_strict_reference = FALSE;

/* Cleared for 3.90, which lower-cases the name it substitutes. */
static scr_bool uip_strict_case = FALSE;

/* Set for the duration of one uip_match_entity() call, because
 * uip_compare_candidate() cannot otherwise tell an NPC from an object. */
static scr_bool uip_strict_is_character = FALSE;

void
uip_set_strict_reference (scr_bool strict, scr_bool match_case)
{
  uip_strict_reference = strict;
  uip_strict_case = match_case;
}

/*
 * Whole-word containment for a trailing %object% (see uip_match_entity()).
 * Off by default; run_all_commands() turns it on for one last library pass
 * after every positional pass has declined, so that a verb whose pattern
 * binds in place ("take off %object%") is never pre-empted by a shorter one
 * ("take %object%") reaching past "off" to the object.
 */
static scr_bool uip_containment_enabled = FALSE;

void
uip_set_containment (scr_bool enabled)
{
  uip_containment_enabled = enabled;
}


/*
 * uip_compare_reference_strict()
 *
 * The strict comparator described above: the name must appear at the current
 * position exactly as authored -- 4.0 -- or bar its case -- 3.90 -- against a
 * lowercased view of the input, and must end on a word boundary.  Returns the
 * new position on match, else zero.
 */
static scr_int
uip_compare_reference_strict (const scr_char *name)
{
  scr_int wpos, posn;

  posn = uip_posn;
  for (wpos = 0; name[wpos] != NUL; wpos++, posn++)
    {
      const scr_char wanted = uip_strict_case
                              ? name[wpos] : scr_tolower (name[wpos]);

      if (wanted != scr_tolower (uip_string[posn]))
        return 0;
    }

  if (scr_isspace (uip_string[posn]) || uip_string[posn] == NUL)
    return posn;
  return 0;
}


/*
 * uip_compare_reference()
 *
 * Helper for %character% and %object% matchers.  Matches multiple words
 * if necessary, at the current position.  Returns zero if the string
 * didn't match, otherwise the length of the current position that matched
 * the words passed in (the new value of uip_posn on match).
 */
static scr_int
uip_compare_reference (const scr_char *words)
{
  scr_int wpos, posn;

  /* Skip articles and lead in space on words and string. */
  wpos = uip_skip_article (words, 0);
  posn = uip_skip_article (uip_string, uip_posn);

  /* Match characters from words with the string at position. */
  while (TRUE)
    {
      /* Any character mismatch means no words match. */
      if (scr_tolower (words[wpos]) != scr_tolower (uip_string[posn]))
        return 0;

      /* Move to next character in each. */
      wpos++;
      posn++;

      /*
       * If at space, advance over whitespace in words list.  Stop when we
       * hit the end of the words list -- unless the whitespace itself runs
       * to the end.
       *
       * Whitespace at the very end of a name is not forgiven: the real
       * Runner's c() matches the RAW stored Short/Alias with InStr and
       * requires the character after the match to be a space, comma or
       * end-of-input (run380.bas '429048; run390's c() LCases but keeps the
       * same shape; the 4.0 strict comparator above already refuses), so a
       * name authored with a trailing space can only match input holding
       * two consecutive spaces -- which scr_normalize_string() never
       * delivers.  Measured live 2026-08-31 on superliam.taf (3.80): object
       * Short "necko wafers " makes `take necko wafers` answer "Take
       * what?" in run380.exe, while the (task-matched) `eat necko wafers`
       * still works.
       */
      if (scr_isspace (words[wpos]) && words[wpos] != NUL)
        {
          while (scr_isspace (words[wpos]) && words[wpos] != NUL)
            wpos++;
          if (words[wpos] == NUL)
            return 0;
        }
      if (words[wpos] == NUL)
        break;

      /*
       * About to match another word, so advance over whitespace in the
       * current string too.
       */
      while (scr_isspace (uip_string[posn]) && uip_string[posn] != NUL)
        posn++;
    }

  /*
   * We reached the end of words.  If we're at the end of the match string, or
   * at spaces, we've matched.
   */
  if (scr_isspace (uip_string[posn]) || uip_string[posn] == NUL)
    return posn;

  /* More text after the match, so it's not quite a match. */
  return 0;
}


/*
 * Cached %object% and %character% match candidates.
 *
 * The reference matchers historically re-read every entity's Prefix, Name/
 * Short and Alias properties from the bundle and re-composed the "prefix
 * name" comparison string on every %object%/%character% match attempt --
 * per entity, per attempt, per input line -- which profiled as the largest
 * single share of parsing.  Those properties are fixed once the game is
 * loaded (the only runtime-mutable properties are the player's name and
 * gender globals, which the matchers never read), so build each game's
 * candidate list once and reuse it.
 *
 * Each candidate also precomputes the lead character (the first character
 * past any article, lowercased) of both its composed prefixed string and
 * its plain name.  uip_compare_reference() cannot match unless the input's
 * own lead character equals one of these, so the matchers use that as an
 * exact one-character rejection test before comparing in full.
 *
 * The cache tracks a single game (SCARE runs one at a time); gs_destroy()
 * calls uip_forget_game() so a later game reusing the same allocation
 * address cannot be mistaken for a cached one.
 */
typedef struct
{
  std::vector<std::string> forms;  /* every string this candidate answers to */
  const scr_char *plain;   /* interned name string from the bundle */
  std::string leads;       /* uip_lead_char() of each form, in form order */
} scr_uip_candidate_t;

typedef struct
{
  scr_uip_candidate_t name;
  std::vector<scr_uip_candidate_t> aliases;  /* empty aliases excluded */
} scr_uip_entity_t;

static const void *uip_cache_game = NULL;
static std::vector<scr_uip_entity_t> uip_cache_objects;
static std::vector<scr_uip_entity_t> uip_cache_npcs;

/*
 * uip_lead_char()
 *
 * Return the lowercased first character of a string past any leading
 * article, mirroring what uip_compare_reference() compares first.
 */
static scr_char
uip_lead_char (const scr_char *string)
{
  return scr_tolower (string[uip_skip_article (string, 0)]);
}

/*
 * uip_build_candidate()
 *
 * Fill in one match candidate from a prefix and a name.
 *
 * The forms are "prefix name", then that same string with the prefix's
 * leading words dropped one at a time, and finally the bare name.  Dropping
 * prefix words is what lets the real Runner answer to a partial prefix:
 * Monsters (Release 2) has Prefix "Sissy's four poster" on Short "bed", and
 * the author's own published transcript shows "examine the four poster bed"
 * returning the object's description.  Matching only the whole prefix or the
 * bare noun -- as this did before -- turned any such line into "I see no such
 * thing".  Only prefix words are droppable; the name itself is never cut
 * down, so a two-word Short still has to be given in full.
 */
static void
uip_build_candidate (scr_uip_candidate_t *candidate,
                     const scr_char *prefix, const scr_char *name)
{
  std::string composed;
  size_t word;

  /* Compose "prefix name" as the old per-call sprintf ("%s %s") did. */
  composed.assign (prefix);
  composed.append (1, ' ');
  composed.append (name);

  candidate->forms.clear ();
  candidate->forms.push_back (composed);

  /*
   * Add one form per remaining prefix word boundary.  Walk only as far as the
   * prefix extends -- past that we would be eating into the name.
   */
  for (word = 0; word < strlen (prefix); word++)
    {
      if (!scr_isspace (composed[word]) || scr_isspace (composed[word + 1]))
        continue;

      candidate->forms.push_back (composed.substr (word + 1));
    }

  /* The bare name, unless a wholly empty prefix already made it form 0. */
  if (candidate->forms.back () != name)
    candidate->forms.push_back (name);

  candidate->plain = name;

  candidate->leads.clear ();
  for (word = 0; word < candidate->forms.size (); word++)
    candidate->leads.append (1, uip_lead_char (candidate->forms[word].c_str ()));
}

/*
 * uip_build_entities()
 *
 * Build the candidate list for one entity class ("Objects" with "Short"
 * names, or "NPCs" with "Name" names), reading the same properties the
 * matchers read before candidates were cached.
 */
static void
uip_build_entities (std::vector<scr_uip_entity_t> &entities,
                    scr_gameref_t game, scr_int count,
                    const scr_char *class_key, const scr_char *name_key)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_int index_;

  entities.clear ();
  entities.resize (count);
  for (index_ = 0; index_ < count; index_++)
    {
      scr_uip_entity_t &entity = entities[index_];
      scr_vartype_t vt_key[4];
      const scr_char *prefix, *name;
      scr_int alias_count, alias;

      /* Get the entity's prefix and name. */
      vt_key[0].string = class_key;
      vt_key[1].integer = index_;
      vt_key[2].string = "Prefix";
      prefix = prop_get_string (bundle, "S<-sis", vt_key);
      vt_key[2].string = name_key;
      name = prop_get_string (bundle, "S<-sis", vt_key);
      uip_build_candidate (&entity.name, prefix, name);

      /* Add each non-empty alias (3.9 games introduce empty aliases). */
      vt_key[2].string = "Alias";
      alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
      for (alias = 0; alias < alias_count; alias++)
        {
          const scr_char *alias_name;

          vt_key[3].integer = alias;
          alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (scr_strempty (alias_name))
            continue;

          entity.aliases.push_back (scr_uip_candidate_t ());
          uip_build_candidate (&entity.aliases.back (), prefix, alias_name);
        }
    }
}

/*
 * uip_synchronize_cache()
 *
 * Ensure the candidate cache describes the given game.
 */
static void
uip_synchronize_cache (scr_gameref_t game)
{
  if (uip_cache_game == game)
    return;

  uip_build_entities (uip_cache_objects,
                      game, gs_object_count (game), "Objects", "Short");
  uip_build_entities (uip_cache_npcs,
                      game, gs_npc_count (game), "NPCs", "Name");
  uip_cache_game = game;
}

/*
 * uip_forget_game()
 *
 * Drop any candidate cache built for the given game.  Called from
 * gs_destroy() so a stale cache can never outlive its game.
 */
void
uip_forget_game (const void *game)
{
  if (uip_cache_game == game)
    {
      uip_cache_game = NULL;
      uip_cache_objects.clear ();
      uip_cache_npcs.clear ();
    }
}

/*
 * uip_compare_candidate()
 *
 * Attempt a reference match against each of a candidate's forms, longest
 * first (the fully prefixed name down to the bare one, which is the order
 * uip_build_candidate() stores them in, and which keeps the longest match
 * winning as the matchers always assumed).  Returns the extent of the match,
 * or zero if no match.
 */
static scr_int
uip_compare_candidate (const scr_uip_candidate_t &candidate)
{
  size_t form;

  /* 4.0 task commands substitute the bare name, and nothing else. */
  if (uip_strict_reference && !uip_strict_is_character)
    return uip_compare_reference_strict (candidate.plain);

  for (form = 0; form < candidate.forms.size (); form++)
    {
      scr_int extent = uip_compare_reference (candidate.forms[form].c_str ());

      if (extent > 0)
        return extent;
    }

  return 0;
}


/*
 * uip_nothing_follows()
 *
 * True when a reference is the last thing its pattern can match: no sibling
 * at all (it closes a group), or only the end-of-string marker.
 */
static scr_bool
uip_nothing_follows (scr_ptnoderef_t node)
{
  scr_ptnoderef_t next = node->right_sibling;

  /*
   * A trailing " *" counts as nothing: the wildcard matches the empty
   * string, so a containing reference that has consumed the whole line
   * still satisfies "pull %object% *".  Vardock Bates (4.00, run400
   * transcript 2026-08-29): `tirar de la palanca` -> synonym -> `pull de la
   * palanca` answers "You pull la palanca, but nothing happens." -- the
   * lever found by co() -- where we fell to "You pull, but nothing happens."
   */
  while (next && (next->type == NODE_WHITESPACE || next->type == NODE_WILDCARD))
    next = next->right_sibling;

  return !next || (next->type == NODE_EOS && !next->right_sibling);
}


/*
 * uip_contains_words()
 *
 * True if 'words' occurs, case-insensitively and on word boundaries, anywhere
 * in the input at or after the current position.
 */
static scr_bool
uip_contains_words (const scr_char *words)
{
  const size_t length = strlen (words);
  scr_int posn;

  if (length == 0)
    return FALSE;

  for (posn = uip_posn; uip_string[posn] != NUL; posn++)
    {
      if (posn > 0 && !scr_isspace (uip_string[posn - 1]))
        continue;
      if (scr_strncasecmp (uip_string + posn, words, length) != 0)
        continue;
      if (scr_isspace (uip_string[posn + length])
          || uip_string[posn + length] == NUL)
        return TRUE;
    }

  return FALSE;
}


/*
 * uip_match_remainder()
 *
 * Helper for %character% and %object% matchers.  Matches the remainder
 * of a pattern, to resolve the difference between, say, "table leg" and
 * "table".
 */
static scr_bool
uip_match_remainder (scr_ptnoderef_t node, scr_int extent)
{
  scr_ptnoderef_t list;
  scr_int start_posn;
  scr_bool matched;

  /*
   * If the reference is the last element of its list there is nothing left to
   * match, and the remainder is vacuously satisfied.  This happens whenever a
   * %character% or %object% closes a choice or optional group, as in the very
   * common idioms "[kiss {the} %character%]" and
   * "[smack/hit/punch/kick]{the}[%character%]".  Without this the temporary
   * list below is empty, uip_match_list() fails it by design, and the
   * reference can never match -- a divergence from the real Runner, which
   * matches both of those (verified against run400.exe on ADRIFTMAS Party).
   * At the top level the case does not arise: uip_parse_list() appends a
   * NODE_EOS there, so end-of-string is still enforced after the group.
   */
  if (!node->right_sibling)
    return TRUE;

  /* Note the start position, then advance to the given extent. */
  start_posn = uip_posn;
  uip_posn = extent;

  /*
   * Try to match everything after the node passed in, at this position in the
   * string.
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
 * uip_case_folds_name()
 *
 * The last thing the Runner's character resolver does before it answers
 * "yes, this command names that character".
 *
 * run400 Proc_21_40_45E99C picks the Name -- or, failing that, the LAST
 * matching Alias (loc_45E623..45E67D assigns without breaking) -- with the
 * case-insensitive whole-word test Proc_21_38_454CB0, lower-cases it into
 * var_98 at loc_45E6A6..45E6B2, and then returns
 *
 *     InStr(1, cmd, var_98, 0)            ' loc_45E743, 45E8B3, 45E938
 *
 * with compare mode 0 = vbBinaryCompare.  That final test is CASE-SENSITIVE
 * against the live command line, and it is a plain substring, the word
 * boundaries having already been settled by the selection above.
 *
 * Normally it can never fail: the whole typed line was lower-cased before
 * the parser ever saw it (run_player_input(), run400 loc_45C5DC), so a name
 * that matched case-insensitively matches case-sensitively too.  The one
 * way upper case gets back into a command is the game's own SYNONYM table,
 * which rewrites whole words AFTER the LCase and splices the author's
 * replacement text in verbatim.  A replacement that carries a capital
 * therefore makes that character permanently unreferenceable by any library
 * command -- only the author's own tasks, which match case-insensitively,
 * can still reach them.
 *
 * Measured 2026-09-07 on Bandera.taf (4.00), whose SYNONYM table maps
 * marife/Marife/marife' all to the capitalised "Marife'": run400 answers
 * `x marife`, `x Marife` and `x MARIFE` alike with the ALR'd "You see no
 * such thing." (Adrift_232_bandera.txt, Adrift_900_bandcase.txt), while
 * `hablar con marife` and `besar a marife` reach her tasks.  Repacking the
 * same game with the five replacements lower-cased makes `x marife` print
 * her description (Adrift_901_bandlc.txt) -- the capital is the whole cause.
 *
 * Objects are not affected: the Runner resolves them through co()
 * (Proc_21_39_46486C), which has no such trailing binary compare.
 */
static scr_bool
uip_case_folds_name_in (const scr_char *command, const scr_char *name)
{
  std::string wanted (name);

  for (auto &c : wanted)
    c = scr_tolower (c);

  return !wanted.empty ()
         && strstr (command, wanted.c_str ()) != NULL;
}

static scr_bool
uip_case_folds_name (const scr_char *name)
{
  return uip_case_folds_name_in (uip_string, name);
}


/*
 * uip_match_entity()
 * uip_match_character()
 * uip_match_object()
 *
 * Match a %character% or an %object% reference.  These search all of the NPC
 * or object names and aliases for possible matches, and set the game's
 * npc_references or object_references flag for any that match.  The final
 * one to match is also stored in variables.
 */
static scr_bool
uip_match_entity (scr_ptnoderef_t node, scr_bool is_character)
{
  const scr_gameref_t game = uip_get_game ();
  const scr_var_setref_t vars = gs_get_vars (game);
  scr_int entity_count, index, max_extent;
  scr_char input_lead;

  if (uip_trace)
    scr_trace ("UIParser: attempting to match %s\n",
               is_character ? "%character%" : "%object%");

  uip_strict_is_character = is_character;

  /* Clear all current references. */
  if (is_character)
    gs_clear_npc_references (game);
  else
    gs_clear_object_references (game);

  /* Ensure cached candidates for this game, and get the input's lead
     character -- a candidate whose own leads differ cannot match. */
  uip_synchronize_cache (game);
  input_lead = scr_tolower (uip_string[uip_skip_article (uip_string,
                                                         uip_posn)]);

  const std::vector<scr_uip_entity_t> &cache = is_character
                                               ? uip_cache_npcs
                                               : uip_cache_objects;
  std::vector<scr_bool> &references = is_character
                                      ? game->npc_references
                                      : game->object_references;

  /*
   * A trailing %object% in a library command is resolved the way the Runner's
   * co() resolves it -- run400 Proc_21_39_46486C, run380 c() @429048 -- by
   * whole-word containment: an object matches when its Short or any Alias
   * occurs as whole words anywhere in the typed command, not only at the
   * position the pattern has reached.  Measured on man_overboard (4.00) turn
   * 49: `x silver key`, with a "silver key" nowhere in the game but "a key"
   * in the room, examines the key; we answered "You see no such thing."
   * The containing match consumes the rest of the input.  Task commands
   * (strict, 4.0) and characters keep their positional matching, and so
   * does any reference with more pattern after it, where the extent decides
   * what the rest of the pattern sees.
   */
  const scr_bool contain = uip_containment_enabled && !is_character
                           && !uip_strict_reference
                           && uip_nothing_follows (node);
  const scr_int input_end = strlen (uip_string);

  /*
   * Pass 0 is the positional match.  Passes 1 and 2 are the containment
   * fallback, names first and then aliases, each taken only when the pass
   * before it found nothing: "unlock iron chest with golden key" must bind
   * the golden key alone even though every key answers to the alias "key"
   * (shadowpeak), and only `x silver key`, which nothing matches in place,
   * falls through to the key.
   */
  max_extent = 0;
  entity_count = cache.size ();
  for (scr_int pass = 0; pass < 3 && max_extent == 0; pass++)
    {
      if (pass > 0 && !contain)
        break;

  for (index = 0; index < entity_count; index++)
    {
      const scr_uip_entity_t &entity = cache[index];
      scr_int alias_count, alias, extent;

      /*
       * Compare the entity's name, then each of its aliases, both prefixed
       * and not.  Alias -1 stands for the name itself.
       */
      alias_count = entity.aliases.size ();
      for (alias = -1; alias < alias_count; alias++)
        {
          const scr_uip_candidate_t &candidate = alias < 0
                                                 ? entity.name
                                                 : entity.aliases[alias];

          if (pass == 1 && alias >= 0)
            break;
          if (pass == 2 && alias < 0)
            continue;

          if (uip_trace)
            scr_trace ("UIParser: trying %s%s\n",
                       alias < 0 ? "" : "alias ", candidate.plain);

          if (pass == 0)
            {
              if (!(uip_strict_reference && !is_character)
                  && candidate.leads.find (input_lead) == std::string::npos)
                continue;

              extent = uip_compare_candidate (candidate);
            }
          else
            extent = uip_contains_words (candidate.plain) ? input_end : 0;

          /*
           * A character has to survive the resolver's case-sensitive tail
           * test as well -- see uip_case_folds_name().  Task commands go
           * through a different Runner routine and are exempt.
           */
          if (extent > 0 && is_character && !uip_strict_reference
              && !uip_case_folds_name (candidate.plain))
            extent = 0;

          if (extent > 0 && uip_match_remainder (node, extent))
            {
              if (uip_trace)
                scr_trace ("UIParser: matched\n");

              /* Increase the maximum match extent if required. */
              max_extent = (extent > max_extent) ? extent : max_extent;

              /* Save match in variables and game. */
              if (is_character)
                var_set_ref_character (vars, index);
              else
                var_set_ref_object (vars, index);
              references[index] = TRUE;
            }
        }
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

static scr_bool
uip_match_character (scr_ptnoderef_t node)
{
  return uip_match_entity (node, TRUE);
}

static scr_bool
uip_match_object (scr_ptnoderef_t node)
{
  return uip_match_entity (node, FALSE);
}


/*
 * uip_match_node()
 *
 * Attempt to match the given node to the current match string/position.
 * Return TRUE, with position advanced, on match, FALSE on fail with the
 * position unchanged.
 */
static scr_bool
uip_match_node (scr_ptnoderef_t node)
{
  scr_bool match = FALSE;

  /* Match depending on node type. */
  switch (node->type)
    {
    case NODE_EOS:
      match = uip_match_eos ();
      break;
    case NODE_WORD:
      match = uip_match_word (node);
      break;
    case NODE_VARIABLE:
      match = uip_match_variable (node);
      break;
    case NODE_WHITESPACE:
      match = uip_match_whitespace (FALSE);
      break;
    case NODE_HARD_WHITESPACE:
      match = uip_match_whitespace (TRUE);
      break;
    case NODE_JOIN:
      match = uip_match_join ();
      break;
    case NODE_LIST:
      match = uip_match_list (node);
      break;
    case NODE_CHOICE:
      match = uip_match_choice (node);
      break;
    case NODE_OPTIONAL:
      match = uip_match_optional (node);
      break;
    case NODE_WILDCARD:
      match = uip_match_wildcard (node);
      break;
    case NODE_CHARACTER_REFERENCE:
      match = uip_match_character (node);
      break;
    case NODE_OBJECT_REFERENCE:
      match = uip_match_object (node);
      break;
    case NODE_NUMBER_REFERENCE:
      match = uip_match_number ();
      break;
    case NODE_TEXT_REFERENCE:
      match = uip_match_text (node);
      break;
    default:
      scr_fatal ("uip_match_node: invalid type, %ld\n", (scr_int) node->type);
    }

  return match;
}


/*
 * uip_cleanse_string()
 * uip_free_cleansed_string()
 *
 * Given a string, copy it to the given buffer, and trim leading and trailing
 * spaces from it.  If the string does not fit the buffer, malloc enough for
 * a string copy and use that instead.  The caller needs to free this
 * allocation if it happens (detectable by comparing the return value to the
 * buffer passed in), or call uip_free_cleansed_string.
 */
static scr_char *
uip_cleanse_string (const scr_char *original, scr_char *buffer, scr_int length,
                    scr_bool trim_trailing = TRUE)
{
  scr_int required;
  scr_char *string;

  /*
   * Use the supplied buffer if it is long enough, otherwise allocate, and
   * copy the string.
   */
  required = strlen (original) + 1;
  string = (required < length) ? buffer : (decltype(+buffer)) scr_malloc (required);
  strncpy (string, original, required);

  /*
   * Trim, and return the string.  A PATTERN keeps its trailing space: the
   * Runner treats one as a space the input must have, so a task command
   * spelled "get placemat " matches nothing (see uip_match_whitespace()).
   * Leading space is dropped from both -- uip_match_whitespace() ignores a
   * leading one in the pattern anyway.
   */
  if (trim_trailing)
    scr_trim_string (string);
  else
    {
      scr_char *const start = string;
      scr_int skip = 0;

      while (scr_isspace (start[skip]))
        skip++;
      if (skip > 0)
        memmove (start, start + skip, strlen (start + skip) + 1);
    }
  return string;
}

static scr_char *
uip_free_cleansed_string (scr_char *string, const scr_char *buffer)
{
  /* Free if the string was allocated by the function above. */
  if (string != buffer)
    scr_free (string);

  /* Always returns NULL, for the syntactic convenience of the caller. */
  return NULL;
}


/*
 * uip_debug_trace()
 *
 * Set pattern match tracing on/off.
 */
void
uip_debug_trace (scr_bool flag)
{
  uip_trace = flag;
}


/*
 * Cache of parsed pattern trees, keyed by the original pattern text.
 *
 * uip_match() historically built and destroyed a fresh parse tree on every
 * call, and with the whole library plus every task command template matched
 * against each input line, that tree churn profiles at a third of the whole
 * interpreter.  Patterns come from fixed library tables and from game
 * properties, so the set is small and stable: parse each distinct pattern
 * once and keep the tree (matching never mutates it).
 *
 * Cached trees are never destroyed or evicted.  uip_match() can re-enter
 * itself mid-match (%variable% matching can evaluate an "in_..." system
 * variable, which itself matches "%object%"), so eviction could free a tree
 * an outer call is still walking; instead, if the cache ever fills -- which
 * no sane game approaches -- further patterns just fall back to the old
 * parse-and-destroy path.  A pattern that fails to parse is cached as NULL
 * so it fails fast when retried.
 */
enum { UIP_TREE_CACHE_SIZE = 4096 };
static std::unordered_map<std::string, scr_ptnoderef_t> uip_tree_cache;

/*
 * uip_match()
 *
 * Match a string to a pattern, and return TRUE on match, FALSE otherwise.
 * For performance, this function uses a local buffer to try to avoid the
 * need to copy each of the pattern and match strings passed in, and reuses
 * parsed pattern trees through uip_tree_cache.
 */
scr_bool
uip_match (const scr_char *pattern, const scr_char *string, scr_gameref_t game)
{
  static scr_char *cleansed;  /* For setjmp safety. */
  scr_char buffer[UIP_ALLOCATION_AVOIDANCE_SIZE];
  scr_bool match, is_tree_cached;
  scr_ptnoderef_t tree;
  assert (pattern && string && game);

  /* Reuse any previously parsed tree for this pattern. */
  std::unordered_map<std::string, scr_ptnoderef_t>::const_iterator
    cached = uip_tree_cache.find (pattern);
  if (cached != uip_tree_cache.end ())
    {
      /* A cached NULL records a pattern that failed to parse. */
      if (!cached->second)
        return FALSE;

      tree = cached->second;
      is_tree_cached = TRUE;
      if (uip_trace)
        scr_trace ("UIParser: pattern \"%s\" (cached tree)\n", pattern);
    }
  else
    {
      /* Start tokenizer. */
      cleansed = uip_cleanse_string (pattern, buffer, sizeof (buffer), FALSE);
      if (uip_trace)
        scr_trace ("UIParser: pattern \"%s\"\n", cleansed);
      uip_tokenize_start (cleansed);

      /* Try parsing the pattern, and catch errors. */
      if (scr_setjmp (uip_parse_error) == 0)
        {
          /* Parse the pattern into a match tree. */
          uip_parse_lookahead = uip_next_token ();
          uip_parse_group_depth = 0;
          uip_parse_tree = uip_new_node (NODE_LIST);
          uip_parse_list (uip_parse_tree);
          uip_tokenize_end ();
          cleansed = uip_free_cleansed_string (cleansed, buffer);
        }
      else
        {
          /* Parse error -- clean up and fail. */
          uip_tokenize_end ();
          uip_destroy_tree (uip_parse_tree);
          uip_parse_tree = NULL;
          cleansed = uip_free_cleansed_string (cleansed, buffer);
          if (uip_tree_cache.size () < UIP_TREE_CACHE_SIZE)
            uip_tree_cache[pattern] = NULL;
          return FALSE;
        }

      /*
       * Detach the tree from the parser statics (so a re-entrant parse
       * cannot touch it) and cache it if there is room.
       */
      tree = uip_parse_tree;
      uip_parse_tree = NULL;
      is_tree_cached = uip_tree_cache.size () < UIP_TREE_CACHE_SIZE;
      if (is_tree_cached)
        uip_tree_cache[pattern] = tree;
    }

  /* Dump out the pattern tree if requested. */
  if (if_get_trace_flag (SCR_DUMP_PARSER_TREES))
    {
      uip_parse_tree = tree;
      uip_debug_dump ();
      uip_parse_tree = NULL;
    }

  /* Match the string to the pattern tree. */
  cleansed = uip_cleanse_string (string, buffer, sizeof (buffer));
  if (uip_trace)
    scr_trace ("UIParser: string \"%s\"\n", cleansed);
  uip_match_start (cleansed, game);
  match = uip_match_node (tree);

  /* Clean up matching, and free the pattern tree unless it is cached. */
  uip_match_end ();
  cleansed = uip_free_cleansed_string (cleansed, buffer);
  if (!is_tree_cached)
    uip_destroy_tree (tree);

  /* Return result of matching. */
  if (uip_trace)
    scr_trace ("UIParser: %s\n", match ? "MATCHED!" : "No match");
  return match;
}


/*
 * uip_replace_pronouns()
 *
 * Replaces pronouns by their respective object or NPC names, and returns the
 * resulting string to the caller, or NULL if no pronouns were replaced.  The
 * return string is malloc'ed, so the caller needs to remember to free it.
 */
scr_char *
uip_replace_pronouns (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  std::string buffer;
  scr_bool modified;
  const scr_char *current;
  scr_int offset;
  assert (string);

  if (uip_trace)
    scr_trace ("UIParser: pronoun search \"%s\"\n", string);

  /*
   * 'current' is the active string -- the input until a pronoun fires, then
   * the 'buffer' copy (basic copy-on-write).  'offset' is our position within
   * it; we work by offset because std::string edits may reallocate.
   */
  modified = FALSE;
  current = string;

  /* Search for pronouns until no more string remains. */
  offset = strspn (current, WHITESPACE);
  while (current[offset] != NUL)
    {
      scr_int object, npc, extent;
      const scr_char *prefix, *name, *echo;
      std::string definite;

      /* Initially, no object or NPC, no names, and a zero extent. */
      object = npc = -1;
      prefix = name = echo = NULL;
      extent = 0;

      /*
       * Search for pronouns where we have an assigned object or NPC.  We
       * can't be sure of getting plurality right, and it's not always
       * intuitive even in English -- is "a pair of scissors" an "it", or
       * a "them"?  Because of this, we just treat "it" and "them" equally
       * for now.
       */
      if (game->it_object != -1 && scr_compare_word (current + offset, "it", 2))
        {
          object = game->it_object;
          extent = 2;
        }
      else if (game->it_object != -1 && scr_compare_word (current + offset, "them", 4))
        {
          object = game->it_object;
          extent = 4;
        }
      else if (game->him_npc != -1 && scr_compare_word (current + offset, "him", 3))
        {
          npc = game->him_npc;
          extent = 3;
        }
      else if (game->him_npc != -1 && scr_compare_word (current + offset, "he", 2))
        {
          npc = game->him_npc;
          extent = 2;
        }
      else if (game->her_npc != -1 && scr_compare_word (current + offset, "her", 3))
        {
          npc = game->her_npc;
          extent = 3;
        }
      else if (game->her_npc != -1 && scr_compare_word (current + offset, "she", 3))
        {
          npc = game->her_npc;
          extent = 3;
        }
      else if (game->it_npc != -1 && scr_compare_word (current + offset, "it", 2))
        {
          npc = game->it_npc;
          extent = 2;
        }

      /*
       * Assign prefix and name to the full object or NPC name, if any.
       *
       * An object goes in as its whole noun phrase, prefix and all; a
       * character goes in as its bare Name.  That asymmetry is the Runner's,
       * and it holds in all four generations -- the two antecedents are
       * separate variables, seeded with "Absolutely nothing" / "nothing" for
       * the object and "Nobody" (4.0 adds "No male" / "No female") for the
       * character, and the character one is assigned the Name field alone:
       *
       *   run370  loc_438332   MemVar_4460B4 = var_164(0)         [Name]
       *   run370  loc_43B696   MemVar_4460AC = tense(Prefix) & " " & Short
       *   run400  loc_47F3B9   MemVar_494184 = var_140(0)         [Name]
       *
       * Field 0 of a character really is the Name, not a prefix: run390's
       * room lister builds "<(0)> is <(4)> <(8)>." at loc_459248 -- "Chloe is
       * a girl." -- and matches the typed word against (0) and (8) at
       * loc_4592B8.  We used to prepend the NPC's Prefix too, which turned
       * "ask him about pens" into "ask the Harold about pens" (wrecked, 3.80)
       * where the Runner writes "ask harold about pens".
       */
      if (object > -1)
        {
          prefix = prop_get_indexed_string (bundle, "Objects", object,
                                            "Prefix");
          name = prop_get_indexed_string (bundle, "Objects", object, "Short");

          /*
           * An empty Prefix is an "a" prefix: every Runner's .taf loader
           * substitutes the literal on the way in (run380 @4481B2, run370
           * @43F5DA, run400 loc_4900EC), so the antecedent the Runner builds
           * through Proc_21_31_448710 reads "a Cupboard".  Measured on
           * man_overboard (4.00): `open it` after `x cupboard` echoes "(a
           * Cupboard)", and likewise "(a wardrobe)", "(a pantry)", "(a
           * toolbox)".  parse_trim_object_names does the substitution now, so
           * the prefix read back here already carries it.
           */

          /*
           * 4.0 keeps the antecedent as a string composed by whichever
           * handler last set it, and the composer (run400 Proc_21_31_448710)
           * has two modes: mode 1 is Prefix & " " & Short as authored, mode 0
           * passes the prefix through tense (Proc_21_13_44F474: "a", "an" and
           * "some" become "the", anything else is left alone).  Examine
           * composes in mode 1 (loc_471749-471789), take/drop/open/close and
           * the "I don't understand what you want me to do with" reply in
           * mode 0 -- see uip_definite_form().  Measured on humbug (4.00,
           * Adrift_5.txt, 2026-08-29): `x plane` then `x it` echoes "(a paper
           * aeroplane)", `get plane` then `x it` "(the paper aeroplane)",
           * `open satchel` "(the satchel)", `throw shovel` (no such verb)
           * "(the shovel)"; "some gloves" and "an envelope" become "the
           * gloves" / "the envelope" after `get` (Adrift_4.txt).
           */
          if (game->it_definite
              && prop_get_taf_version (bundle) >= TAF_VERSION_400)
            {
              if (scr_compare_word (prefix, "a", 1))
                definite = std::string ("the") + (prefix + 1);
              else if (scr_compare_word (prefix, "an", 2))
                definite = std::string ("the") + (prefix + 2);
              else if (scr_compare_word (prefix, "some", 4))
                definite = std::string ("the") + (prefix + 4);
              else
                definite = prefix;
              prefix = definite.c_str ();
            }
        }
      else if (npc > -1)
        {
          prefix = "";
          name = prop_get_indexed_string (bundle, "NPCs", npc, "Name");
        }
      else if (scr_compare_word (current + offset, "it", 2)
               || scr_compare_word (current + offset, "them", 4))
        {
          /*
           * No antecedent at all for "it"/"them".  The Runner's object
           * antecedent is a pair of strings seeded at new game -- run400
           * Proc_19_4_45AA98 calls Proc_21_41_448C24 (flag 0, "nothing",
           * "Absolutely nothing") -- and never a "no reference" state, so
           * the pronoun is still replaced: "nothing" goes into the command
           * and "Absolutely nothing" is what the bracket echo prints.
           * Measured on ptgood_again (4.00) turns 5-6: `x it` with nothing
           * yet referenced answers "(Absolutely nothing)" then "You see no
           * such thing."; `g` repeats "(x it)", "(Absolutely nothing)".
           */
          prefix = "";
          name = "nothing";
          echo = "Absolutely nothing";
          extent = scr_compare_word (current + offset, "it", 2) ? 2 : 4;
        }
      else if (scr_compare_word (current + offset, "him", 3)
               || scr_compare_word (current + offset, "he", 2)
               || scr_compare_word (current + offset, "her", 3)
               || scr_compare_word (current + offset, "she", 3))
        {
          /*
           * The same thing for the character pronouns, and again the Runner
           * has no "no reference" state -- the register is a seeded string,
           * so him/he/her/she are rewritten even before anything has been
           * referred to.  3.9 and 4.0 keep a register per gender, seeded
           * "No male" and "No female" (run400 loc_45A7F9/45A800 in
           * Proc_19_4_45AA98, run390 loc_434969/434970 in clear(); assigned
           * the NPC's Name by gender byte at run400 loc_47F3B9/47F3D0 and
           * run390 loc_4592D7/4592EE, which is Scarier's him_npc/her_npc).
           * 3.7 and 3.8 have ONE character register for all four pronouns,
           * seeded "Nobody" (run370 loc_42398D MemVar_4460B4, read by every
           * branch of its() at 42CAFA/42CBC6/42CC9B/42CD67; run380
           * loc_4289F1).  Unlike the object register there is no second
           * lower-case copy: the echo and the text spliced into the command
           * are the same string, and the whole command is lower-cased after
           * the splice.
           *
           * Measured on showtime (4.00, Adrift_312_showtime.txt turn 59):
           * `get her hand` with no female yet referenced answers
           * "(No female)" and then runs the game's task, which survives
           * because its command is the wildcard `get * hand` and the
           * rewritten line is "get no female hand".
           */
          prefix = "";
          if (prop_get_taf_version (bundle) >= TAF_VERSION_390)
            name = (scr_compare_word (current + offset, "him", 3)
                    || scr_compare_word (current + offset, "he", 2))
                   ? "No male" : "No female";
          else
            name = "Nobody";
          echo = name;
          extent = scr_compare_word (current + offset, "he", 2) ? 2 : 3;
        }

      /*
       * If a pronoun was found, prefix and name indicate what to insert, and
       * extent shows how much of the buffer to replace with them.
       */
      if (prefix && name && extent > 0)
        {
          std::string replacement;

          uip_pronoun_used = TRUE;

          /*
           * If not yet copied, copy the input string into the buffer now and
           * switch 'current' to it.  The offset is unchanged since the buffer
           * starts as an exact copy; basic copy-on-write.
           */
          if (!modified)
            {
              buffer = string;
              modified = TRUE;
            }

          /* Build the replacement text: "<prefix> <name>", or just the name. */
          replacement.reserve (strlen (prefix) + 1 + strlen (name));
          if (prefix[0] != NUL)
            {
              replacement.append (prefix);
              replacement.push_back (' ');
            }
          replacement.append (name);

          /*
           * Every Runner echoes the antecedent in round brackets on a line of
           * its own before the command's response -- "(a trophy)".  4.0 does
           * it whenever Options -> Display & Media... -> "References in
           * brackets" is ticked, the reference setting Scarier models: run400
           * Proc_19_49_461F38 (him/he/her/she/it/them branches, e.g. "it" at
           * loc_461DA5) tests MemVar_4942BA then prints "(" & antecedent &
           * ")" & vbCrLf through Proc_21_19_47B568.  Measured on adrift_maze
           * (4.00) commands 24-25: "read it" answers "(a trophy)" then "It
           * has the following engraved on it."
           *
           * 3.9 is the same line behind the same setting: run390 Sub its()
           * @43D968 tests m_showbrackets (loc_43D6C2 for "it", 43D7BF "them",
           * 43D884 "one") and prints "(" & MemVar_46811C & ")" through
           * Proc_2_28_45CBD0; the menu item is read from ADRIFT\Runner
           * "showbrackets" at loc_44526F with default True (loc_44526B).
           * Measured on Archie's Birthday (3.90, run390, 2026-09-05): `x
           * camcorder` then `take it` answers "(a camcorder)" then "You take
           * a camcorder from the desk."; and on veteran (3.90, run390, same
           * day) `x bag`, `take it`, `open it` echo "(a bag)" both times --
           * 3.9's takes @455067 and drops @445BE6 compose the antecedent in
           * mode 1 (authored Prefix), so unlike 4.0 there is no "the" form,
           * which is why uip_definite_form() stays 4.00-only.  (An earlier
           * reading of run390 had
           * it keeping showbrackets only for the "ask about"/"talk about"
           * rewrite at loc_459036/459107 and echoing nothing -- wrong.)
           *
           * 3.7 and 3.8 print the same line UNGATED -- run370 Sub Form1.its
           * @2CA9C (loc_42CAFA ...) and run380 @326B4 have no Appearance menu
           * -- with the same antecedents: the NPC's Name, or tense(Prefix) &
           * " " & Short for an object, which is what 'replacement' holds.
           * From P-code only; no 3.7/3.8 replay has been measured for it.
           */
          pf_buffer_reference (gs_get_filter (game),
                               echo ? echo : replacement.c_str ());

          /*
           * Splice the replacement in for the matched extent, and lower-case
           * the whole line again -- the Runner assigns
           * `cmd = LCase(Left$ & antecedent & Right$)` after every splice
           * (run400 Proc_19_49_461F38 loc_461ACB..461AD7 for "him",
           * loc_461BA5..461BB1 for "he", and so on down the branches), so an
           * authored capital in the Name never survives into the parsed
           * command.  Without this, uip_case_folds_name() would refuse the
           * very character the pronoun just named: "ask him about pens" in
           * wrecked (3.80) becomes "ask harold about pens", not "ask Harold
           * about pens".
           */
          buffer.replace (offset, extent, replacement);
          for (auto &c : buffer)
            c = scr_tolower (c);
          current = buffer.c_str ();

          /* Adjust offset to skip over the replacement. */
          offset += replacement.size ();

          if (uip_trace)
            scr_trace ("Parser: pronoun \"%s\"\n", buffer.c_str ());
        }
      else
        {
          /* If no match, advance over the unmatched word. */
          offset += strcspn (current + offset, WHITESPACE);
        }

      /* Set offset to the next word start. */
      offset += strspn (current + offset, WHITESPACE);
    }

  /* Return the final string, or NULL if no pronoun replacements. */
  return modified ? uip_strdup (buffer) : NULL;
}


/*
 * uip_lowered()
 *
 * Lower-cased copy of a string, for the Runner's LCase() comparisons.
 */
static std::string
uip_lowered (const scr_char *string)
{
  std::string lowered (string);
  for (auto &c : lowered)
    c = scr_tolower (c);
  return lowered;
}

/*
 * uip_phrase_in()
 *
 * The Runner's c(): is the (lower-cased) phrase in the lower-cased command
 * as a whole word or words -- run400 Proc_21_38_454CB0, an InStr() loop that
 * accepts a hit only when what surrounds it is a space or the line's end.
 */
static scr_bool
uip_phrase_in (const std::string &lowered, const scr_char *phrase)
{
  std::string wanted = uip_lowered (phrase);
  if (wanted.empty ())
    return FALSE;

  for (std::string::size_type pos = lowered.find (wanted);
       pos != std::string::npos; pos = lowered.find (wanted, pos + 1))
    {
      const std::string::size_type end = pos + wanted.size ();
      const scr_bool left = pos == 0 || lowered[pos - 1] == ' ';
      const scr_bool right = end == lowered.size () || lowered[end] == ' ';
      if (left && right)
        return TRUE;
    }
  return FALSE;
}

/*
 * uip_npc_named()
 *
 * Does the command name this NPC?  run400 Proc_21_40_45E99C: c(LCase(Name))
 * and, only if that fails, c(LCase(alias)) for each alias -- the alias loop
 * assigns without breaking (loc_45E623..45E67D), so the LAST matching alias
 * is the one that survives, while a matching Name jumps the loop entirely
 * (loc_45E620).  There is no presence test -- an NPC two rooms away still
 * counts.
 *
 * Whichever string was chosen is then lower-cased and looked for in the
 * command with a case-SENSITIVE InStr; see uip_case_folds_name().
 */
static scr_bool
uip_npc_named (scr_gameref_t game, scr_int npc, const std::string &lowered,
               const scr_char *command)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_char *chosen = NULL;
  scr_vartype_t vt_key[4];
  scr_int alias_count, alias;

  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Name";
  chosen = prop_get_string (bundle, "S<-sis", vt_key);
  if (!uip_phrase_in (lowered, chosen))
    {
      chosen = NULL;

      vt_key[2].string = "Alias";
      alias_count = prop_get_child_count (bundle, "I<-sis", vt_key);
      for (alias = 0; alias < alias_count; alias++)
        {
          const scr_char *alias_name;

          vt_key[3].integer = alias;
          alias_name = prop_get_string (bundle, "S<-sisi", vt_key);
          if (!scr_strempty (alias_name) && uip_phrase_in (lowered, alias_name))
            chosen = alias_name;
        }
    }

  return chosen && uip_case_folds_name_in (command, chosen);
}

/*
 * uip_last_npc_name()
 *
 * The "character most recently named by a command" register -- run400
 * MemVar_494180, seeded "Nobody" at loc_45A7F5 (run370 MemVar_4460B4 the
 * same) -- as the Runner prints it.
 */
static const scr_char *
uip_last_npc_name (scr_gameref_t game, scr_int npc)
{
  scr_vartype_t vt_key[3];

  if (npc == -1)
    return "Nobody";

  vt_key[0].string = "NPCs";
  vt_key[1].integer = npc;
  vt_key[2].string = "Name";
  return prop_get_string (gs_get_bundle (game), "S<-sis", vt_key);
}

/*
 * uip_note_named_npcs()
 *
 * Update the last-named-character register from a command about to run.
 * run400 Proc_19_0_480674 loc_47F2C5..47F3A2 walks the NPCs in index order
 * and assigns Name for every one Proc_21_40_45E99C finds in the line, so
 * the highest-indexed match wins; it runs AFTER the routine's own "ask
 * about" rewrite, which is why the rewrite sees the previous command's
 * value (run_all_commands() keeps that by remembering the index it had
 * before this ran).
 *
 * The loop is unconditional, and characters() itself is reached at 48B56E
 * on EVERY line -- the task dispatch's early exits jump to 48B4E3, which is
 * still above it -- so a line a task answered names its characters too.
 * Measured 2026-09-05 (Adrift_79.txt, sommeril.taf): "ask about zzz" in the
 * gargoyle's street echoes "(Nobody)" even with GARGOYLE listed in the room
 * description, then the task-answered "give silver orb to gargoyle" makes
 * the next "ask about zzz" echo "(GARGOYLE)".
 */
void
uip_note_named_npcs (scr_gameref_t game, const scr_char *string)
{
  const std::string lowered = uip_lowered (string);
  scr_int index_;

  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (uip_npc_named (game, index_, lowered, string))
        game->last_npc = index_;
    }
}

/*
 * uip_line_names_npc()
 *
 * TRUE if the line names any character at all -- the same walk over
 * Proc_21_40_45E99C that run400 makes at loc_48B53C..48B569, whose result
 * (var_29C) gates the DontUnderstand text at 48B585.  See the note in
 * run_process_input_line().
 */
scr_bool
uip_line_names_npc (scr_gameref_t game, const scr_char *string)
{
  const std::string lowered = uip_lowered (string);
  scr_int index_;

  for (index_ = 0; index_ < gs_npc_count (game); index_++)
    {
      if (uip_npc_named (game, index_, lowered, string))
        return TRUE;
    }
  return FALSE;
}

/*
 * uip_rewrite_references()
 *
 * The two "References in brackets" rewrites that fill in a character the
 * player left out, each echoing what it assumed in round brackets on a line
 * of its own (Scarier models the box ticked):
 *
 *   give X [no character named, no "to"]  ->  "(to Bob)"  give X to bob
 *   ask about X / talk about X            ->  "(Bob)"     ask bob about X
 *
 * run400: the give rewrite is loc_48A98A..48AA38 in the input routine
 * Proc_19_61_48C0F0 (c("give"), a Proc_21_40_45E99C count of zero, Not
 * c("to")); the ask/talk rewrite opens the parser proper, Proc_19_0_480674
 * loc_47F134..47F2BA (Left(cmd, 10) = "ask about ", Left(cmd, 11) = "talk
 * about ").  Both take MemVar_494180 as it stands from the previous
 * command, print through Proc_21_19_47B568 behind MemVar_4942BA, and splice
 * LCase(Name) into the line.  run370 loc_43BED8/43BFC9 and loc_4380CF/
 * 438185 (run380 the same) do both with no gate -- 3.7 has no Appearance
 * menu.  run390 keeps only the ask/talk pair (loc_459036/459107, behind
 * m_showbrackets) and has no "(to " literal, so the give rewrite is gated
 * out there.  P-code only so far; vardock_bates turn 16 showed "(to
 * Vagabundo)" live but after a lost command.
 *
 * Returns a fresh string if anything was rewritten, else NULL.
 */
/*
 * uip_ask_echo_skip()
 *
 * The length of the "ask about " / "talk about " prefix this line carries,
 * or zero if it carries neither.
 */
static std::string::size_type
uip_ask_echo_skip (const std::string &lowered)
{
  if (lowered.compare (0, 10, "ask about ") == 0)
    return 10;
  if (lowered.compare (0, 11, "talk about ") == 0)
    return 11;
  return 0;
}


/*
 * uip_print_ask_echo()
 *
 * Print the "(<npc>)" the ask/talk rewrite echoes, and say whether it did.
 *
 * The echo comes out BEFORE task matching.  run400 prints it from inside the
 * parser proper, and a task that answers the line still prints after it --
 * measured 2026-09-05 on two opposite task shapes: `SPAM.taf` turn 11, whose
 * `* ingredients *` wildcard task answers `ask about ingredients` under a
 * bare "(Nobody)" (Adrift_77.txt), and `sommeril.taf` turn 36, whose LITERAL
 * task `ask about glass framed page` answers under "(GARGOYLE)"
 * (Adrift_78.txt).  The literal case is the interesting one: it proves the
 * echo is not suppressed by an earlier typed-command dispatch, and -- since
 * a pattern spelled `ask about glass framed page` cannot match the rewritten
 * `ask gargoyle about glass framed page` -- that the REWRITTEN string is the
 * library's alone.  Tasks go on matching the line the player typed.
 *
 * So only the echo is hoisted here; uip_rewrite_references() still splices
 * the name in, on the library path, where the give rewrite also stays (that
 * one lives at run400 loc_48A98A, after the typed-command dispatch at
 * 48A481, and a matched task does jump past it).
 */
scr_bool
uip_print_ask_echo (scr_gameref_t game, const scr_char *string)
{
  const std::string lowered = uip_lowered (string);

  if (uip_ask_echo_skip (lowered) == 0)
    return FALSE;

  pf_buffer_reference (gs_get_filter (game),
                       uip_last_npc_name (game, game->last_npc));
  return TRUE;
}


scr_char *
uip_rewrite_references (scr_gameref_t game, const scr_char *string,
                        scr_int prior_npc, scr_bool echo_printed)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_int version = prop_get_taf_version (bundle);
  const scr_char *name = uip_last_npc_name (game, prior_npc);
  const std::string lowered_name = uip_lowered (name);
  std::string command (string);
  std::string lowered = uip_lowered (string);
  scr_bool modified = FALSE;

  if ((version < TAF_VERSION_390 || version >= TAF_VERSION_400)
      && uip_phrase_in (lowered, "give") && !uip_phrase_in (lowered, "to"))
    {
      scr_int index_;
      scr_bool named = FALSE;

      for (index_ = 0; index_ < gs_npc_count (game) && !named; index_++)
        named = uip_npc_named (game, index_, lowered, command.c_str ());

      if (!named)
        {
          const std::string echo = std::string ("to ") + name;

          pf_buffer_reference (gs_get_filter (game), echo.c_str ());
          command += " to ";
          command += lowered_name;
          lowered = uip_lowered (command.c_str ());
          modified = TRUE;
        }
    }

  const std::string::size_type skip = uip_ask_echo_skip (lowered);
  if (skip > 0)
    {
      /* The echo is printed up front now; see uip_print_ask_echo(). */
      if (!echo_printed)
        pf_buffer_reference (gs_get_filter (game), name);
      command = "ask " + lowered_name + " about " + command.substr (skip);
      modified = TRUE;
    }

  if (modified && uip_trace)
    scr_trace ("Parser: reference rewrite \"%s\"\n", command.c_str ());

  return modified ? uip_strdup (command) : NULL;
}


/*
 * uip_definite_form()
 *
 * Decide whether the object antecedent that 'command' just assigned is held
 * in its definite ("the X") or indefinite (Prefix & " " & Short) form.  4.0
 * only; every earlier Runner is left on the authored prefix.
 *
 * The Runner's antecedent is a string, composed by whichever code last called
 * the setter Proc_21_41_448C24, in the composer mode that code chose:
 *
 *   co() (object resolution)   run390 twin co @43B69E: mode 0, definite, but
 *                              only in its found-in-the-room branch; a held
 *                              object resolves through the branch at 43B456
 *                              that never stores.  Measured: `look in dustbin`
 *                              then `x it` "(the dustbin)", `look in satchel`
 *                              (held) leaves "(a satchel)".
 *   examines @471749-471789    mode 1, indefinite: `x shovel` after `get
 *                              shovel` is back to "(a shovel)"; so is `x the
 *                              shovel`.
 *   takes? @462AAC-462AD9      mode 0 at entry, before the task pre-match,
 *                              which is why humbug's `get can`, intercepted by
 *                              its "get * can *" FailMessage task, still
 *                              leaves "(the can)"; a second `get shovel`
 *                              ("already have") stays "(the shovel)".  Taking
 *                              from a character goes another way: `Get
 *                              Document` from Grandad leaves "(a document)"
 *                              (Adrift_4.txt 1406).
 *   takes @47BFCF/@47C058      mode 1 on the refusal paths; a failed `get
 *                              shovel from satchel` left "(a shovel)".
 *   drop helper @465F5E        mode 0: `drop shovel` "(the shovel)"; drops'
 *                              refusal sites @46F77B/@46F81E are mode 1.
 *   openclose @47585A ...      mode 0 at all four sites, the lock/unlock
 *                              path (@476288-4762A1) included: "(the
 *                              satchel)" after `open satchel` and `close
 *                              satchel`; provenance's `unlock door` then
 *                              `open it` "(the trap door)" is from P-code.
 *   generaltasks @48A409       mode 0: `throw shovel` "(the shovel)".
 *   put                        never touches it: `put shovel in satchel`
 *                              (refused) and `put banana in dustbin` leave
 *                              whatever was there.
 *
 * A command that used the pronoun leaves the antecedent exactly as it was:
 * `drop it`, `x it`, `look at it`, `examine it` all echo the previous form
 * (Adrift_5.txt 321-349), so the caller skips assignment altogether for
 * those.  All measured on humbug, run400, 2026-08-29.
 */
enum uip_form_t
{ UIP_FORM_KEEP, UIP_FORM_INDEFINITE, UIP_FORM_DEFINITE };

static uip_form_t
uip_definite_form (scr_gameref_t game, const scr_char *command,
                   const scr_char *at, scr_int object)
{
  const scr_char *verb = command + strspn (command, WHITESPACE);
  const scr_int was_at = gs_object_position (game->temporary, object);
  const scr_bool was_held = was_at == OBJ_HELD_PLAYER
                            || was_at == OBJ_WORN_PLAYER;
  const scr_bool was_with_npc = was_at == OBJ_HELD_NPC
                                || was_at == OBJ_WORN_NPC;
  scr_bool has_from = FALSE;
  const scr_char *scan;

  if (uip_pending_definite)
    return UIP_FORM_DEFINITE;

  for (scan = verb; scan[0] != NUL; scan++)
    {
      if (scr_isspace (scan[0]) && scr_compare_word (scan + 1, "from", 4))
        {
          has_from = TRUE;
          break;
        }
    }

  /*
   * The object named after "from" is the source, not the antecedent, and
   * put's two objects are neither: `put banana in dustbin` left "(the
   * banana)", `put shovel in satchel` "(a shovel)", and the failed `get
   * shovel from satchel` "(a shovel)".
   */
  if (has_from && at > scan)
    return UIP_FORM_KEEP;
  if (scr_compare_word (verb, "put", 3) || scr_compare_word (verb, "insert", 6))
    return UIP_FORM_KEEP;

  if (scr_compare_word (verb, "x", 1)
      || scr_compare_word (verb, "ex", 2)
      || scr_compare_word (verb, "exam", 4)
      || scr_compare_word (verb, "examine", 7)
      || scr_compare_word (verb, "read", 4)
      || ((scr_compare_word (verb, "l", 1)
           || scr_compare_word (verb, "look", 4))
          && strstr (verb, " at ")))
    return UIP_FORM_INDEFINITE;

  if (scr_compare_word (verb, "get", 3)
      || scr_compare_word (verb, "take", 4)
      || scr_compare_word (verb, "pick", 4))
    {
      if (was_with_npc)
        return UIP_FORM_INDEFINITE;
      if (has_from)
        {
          const scr_int now_at = gs_object_position (game, object);
          return now_at == OBJ_HELD_PLAYER && !was_held
                 ? UIP_FORM_DEFINITE : UIP_FORM_INDEFINITE;
        }
      return UIP_FORM_DEFINITE;
    }

  if (scr_compare_word (verb, "drop", 4))
    return was_held ? UIP_FORM_DEFINITE : UIP_FORM_INDEFINITE;

  if (scr_compare_word (verb, "open", 4) || scr_compare_word (verb, "close", 5)
      || scr_compare_word (verb, "lock", 4)
      || scr_compare_word (verb, "unlock", 6))
    return UIP_FORM_DEFINITE;

  /* co() alone: definite for a room object, untouched for a held one. */
  return was_held ? UIP_FORM_KEEP : UIP_FORM_DEFINITE;
}


/*
 * uip_assign_pronouns()
 *
 * Search a player command for object and NPC names, and assign any found to
 * game pronouns.  The string is searched from front to back, assigning
 * pronouns for objects or NPC names as found.  Later ones will overwrite
 * earlier ones if there is more than one in the string.
 */
void
uip_assign_pronouns (scr_gameref_t game, const scr_char *string)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  const scr_var_setref_t vars = gs_get_vars (game);
  const scr_char *current;
  scr_int saved_ref_object, saved_ref_character;
  assert (string);

  if (uip_trace)
    scr_trace ("UIParser: pronoun assignment \"%s\"\n", string);

  /*
   * A 4.0 command that went through a pronoun leaves every antecedent as it
   * was -- see uip_definite_form() -- so there is nothing to assign.
   */
  if (uip_pronoun_used && prop_get_taf_version (bundle) >= TAF_VERSION_400)
    {
      uip_pronoun_used = FALSE;
      uip_pending_definite = FALSE;
      return;
    }
  uip_pronoun_used = FALSE;

  /* Save var references so we can restore them later. */
  saved_ref_object = var_get_ref_object (vars);
  saved_ref_character = var_get_ref_character (vars);

  /* Search for object and NPC names until no more string remains. */
  current = string + strspn (string, WHITESPACE);
  while (current[0] != NUL)
    {
      if (uip_match ("%object% *", current, game))
        {
          scr_int count, index_, object;

          /*
           * "Disambiguate" by rejecting objects that the player hasn't seen
           * or can't see.  If the reference is unique, assign to the 'it'
           * object pronoun.
           */
          count = 0;
          object = -1;
          for (index_ = 0; index_ < gs_object_count (game); index_++)
            {
              if (game->object_references[index_]
                  && (gs_object_seen (game, index_)
                      || prop_get_taf_version (bundle) < TAF_VERSION_390)
                  && obj_indirectly_in_room (game,
                                             index_, gs_playerroom (game)))
                {
                  count++;
                  object = index_;
                }
            }

          if (count == 1)
            {
              uip_form_t form = UIP_FORM_INDEFINITE;

              if (prop_get_taf_version (bundle) >= TAF_VERSION_400)
                form = uip_definite_form (game, string, current, object);

              if (form != UIP_FORM_KEEP)
                {
                  game->it_object = object;
                  game->it_definite = form == UIP_FORM_DEFINITE;
                  game->it_npc = -1;
                }

              if (uip_trace)
                {
                  scr_trace ("UIParser: object 'it/them' assigned %ld,"
                             " form %d\n", object, (int) form);
                }
            }
        }

      if (uip_match ("%character% *", current, game))
        {
          scr_int count, index_, npc;

          /* Do the same "disambiguation" as for objects above. */
          count = 0;
          npc = -1;
          for (index_ = 0; index_ < gs_npc_count (game); index_++)
            {
              if (game->npc_references[index_]
                  && gs_npc_seen (game, index_)
                  && npc_in_room (game, index_, gs_playerroom (game)))
                {
                  count++;
                  npc = index_;
                }
            }

          if (count == 1)
            {
              scr_int gender;

              /*
               * Version 3.8 games lack NPC gender information, so for this
               * case set "him"/"her" on each match, and never set "it"; this
               * matches the version 3.8 runner.  Version 3.7 has no gender
               * field either (its NPC record is version 3.8's), so it takes
               * the same treatment.
               */
              if (prop_get_taf_version (bundle) <= TAF_VERSION_380)
                {
                  game->him_npc = npc;
                  game->her_npc = npc;
                  game->it_npc = -1;

                  if (uip_trace)
                    {
                      scr_trace ("UIParser: 3.8 pronouns"
                                " 'him' and 'her' assigned %ld\n", npc);
                    }
                }
              else
                {
                  /* Find the NPC gender, so we know the pronoun to assign. */
                  gender = prop_get_indexed_integer (bundle, "NPCs",
                                                     npc, "Gender");

                  switch (gender)
                    {
                    case NPC_MALE:
                      game->him_npc = npc;
                      break;
                    case NPC_FEMALE:
                      game->her_npc = npc;
                      break;
                    case NPC_NEUTER:
                      game->it_npc = npc;
                      game->it_object = -1;
                      break;
                    default:
                      scr_error ("uip_assign_pronouns:"
                                " unknown gender, %ld\n", gender);
                    }

                  if (uip_trace)
                    scr_trace ("UIParser: NPC 'him/her/it' assigned %ld\n", npc);
                }
            }
        }

      /*
       * Advance the string position by a complete word.  This saves a lot
       * of time -- there's no point looking for an object or NPC name in
       * mid-word, and anyway it's not the right thing to do.
       */
      current += strcspn (current, WHITESPACE);
      current += strspn (current, WHITESPACE);
    }

  /* Restore variables references. */
  var_set_ref_object (vars, saved_ref_object);
  var_set_ref_character (vars, saved_ref_character);
  uip_pending_definite = FALSE;
}
