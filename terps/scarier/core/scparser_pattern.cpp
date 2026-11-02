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
 * Pattern compilation: the tokenizer and the predictive parser that turn a
 * pattern string ("[get/take] {the} %object%") into a tree of nodes, the node
 * and word pools, and uip_compile_pattern().  See scparser_internal.h.
 */

#include "scparser_internal.h"


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
  {nullptr, 0, TOK_NONE}
};


/*
 * The tokenizer's state is uip.tokenizer.  The temporary is used for keeping
 * word token values.  For improved performance, we'll set it to indicate a
 * static buffer if short enough, otherwise it's allocated.
 */


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
  uip.tokenizer.pattern = pattern;
  uip.tokenizer.index = 0;

  /* Set up temporary; static if the pattern is short enough, else allocated. */
  required = strlen (pattern) + 1;
  uip.tokenizer.temporary
    = (required > UIP_ALLOCATION_AVOIDANCE_SIZE)
      ? (decltype(+uip.tokenizer.static_temporary)) scr_malloc (required)
      : uip.tokenizer.static_temporary;
}

static void
uip_tokenize_end (void)
{
  /* Deallocate temporary if required, and clear pattern and index. */
  if (uip.tokenizer.temporary != uip.tokenizer.static_temporary)
    scr_free (uip.tokenizer.temporary);
  uip.tokenizer.temporary = nullptr;
  uip.tokenizer.pattern = nullptr;
  uip.tokenizer.index = 0;
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
  auto &tok = uip.tokenizer;
  assert (tok.pattern);

  /* Get next character, return EOS if at pattern end. */
  if (tok.pattern[tok.index] == NUL)
    {
      tok.token_value = nullptr;
      return TOK_EOS;
    }

  /*
   * If whitespace, skip it, then return a whitespace token.  A run of two
   * or more is remembered in uip.tokenizer.multi_space; see
   * uip_parse_element().
   */
  if (scr_isspace (tok.pattern[tok.index]))
    {
      scr_char before = tok.index > 0 ? tok.pattern[tok.index - 1] : NUL;

      tok.index++;
      tok.multi_space = scr_isspace (tok.pattern[tok.index])
                        && tok.pattern[tok.index] != NUL;
      while (scr_isspace (tok.pattern[tok.index])
             && tok.pattern[tok.index] != NUL)
        tok.index++;

      /*
       * A run right after a [..] or {..} group is not simply literal.
       * run400's NewParse (45D940) expands a group by splicing an
       * alternative onto the untouched remainder and recursing, and the
       * recursion Trim()s both sides: with the group OMITTED the remainder
       * starts with the run, so Trim() swallows it and the rest matches as
       * usual; with an alternative INCLUDED the remainder is "alt  ..." and
       * its literal prefix, two spaces and all, is compared against the
       * collapsed line and fails.  So "A View to a Home" answers `open box`
       * from "[open] {the}  {metal} [box]" and "Monsters" `shine flashlight
       * on brainsucker` from "... {brain}  {monster}" (both groups before
       * the run omitted), while "christmas present 1.0" refuses `amber sit
       * on the washing machine` for task 21 "Amber sit on {the}  {standard}
       * washing machine" and falls through to the library's "You sit down
       * on the standard washing machine." (runner_transcripts/
       * christmaspresent.txt T53, 2026-09-26): {the} is included, and "the
       * " with two spaces never matches "the w".  uip_parse_element() gives
       * such a run its own node, and uip_match_group_double_whitespace()
       * asks whether the group it follows matched empty.  A run between a
       * word and a group is an ordinary literal stretch (the prefix up to
       * the first '[' / '{' keeps its spaces, 45D7A1-45D83E), so it stays a
       * plain double -- read from the P-code, no golden reaches that cell.
       * A trailing run is dropped by the top-level Trim() and stays lenient.
       */
      tok.after_group = FALSE;
      if (before == '}' || before == ']')
        {
          if (tok.pattern[tok.index] == NUL)
            tok.multi_space = FALSE;
          else
            tok.after_group = tok.multi_space;
        }
      tok.token_value = nullptr;
      return TOK_WHITESPACE;
    }

  /* Search the table for matching strings. */
  for (entry = UIP_TOKENS; entry->name; entry++)
    {
      if (strncmp (tok.pattern + tok.index, entry->name, entry->length) == 0)
        break;
    }
  if (entry->name)
    {
      /* Advance over string, and return token. */
      tok.index += entry->length;
      tok.token_value = nullptr;
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
  if (sscanf (tok.pattern + tok.index, "%%%[^%]%c", tok.temporary, &close) == 2
      && close == PERCENT)
    {
      tok.index += strlen (tok.temporary) + 2;
      tok.token_value = tok.temporary;
      return TOK_VARIABLE;
    }

  /*
   * Return a word.  This is a contiguous run of non-pattern-special, non-
   * whitespace, non-percent characters
   */
  if (sscanf (tok.pattern + tok.index, "%[^][/{}*% \f\n\r\t\v]",
              tok.temporary) != 1)
    {
      /*
       * A '%' that opens no %var% (a trailing "50%", an unclosed "%foo").
       * The scan above matched nothing and left the previous token in the
       * buffer, so take the '%' as a one-character word instead.
       */
      tok.temporary[0] = tok.pattern[tok.index];
      tok.temporary[1] = NUL;
    }
  tok.token_value = tok.temporary;
  tok.index += strlen (tok.temporary);
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
  if (!uip.tokenizer.token_value)
    {
      scr_fatal ("uip_current_token_value:"
                " attempt to take undefined token value\n");
    }

  /* Return value. */
  return uip.tokenizer.token_value;
}


/*
 * The tree builder's state is uip.parse: the predictive parser's lookahead
 * token, the parse error jump buffer, the tree under construction (for
 * cleanup), and the nesting depth of [...] and {...} groups currently being
 * parsed.  A "/" is an alternatives separator only inside a group; at depth
 * zero run400 has no notion of it at all and treats it as an ordinary
 * literal character (see uip_parse_list()).
 */


/*
 * The deepest nesting a pattern may have.  Each level is a handful of stack
 * frames in the parser and again in the matcher, and a damaged game can open
 * tens of thousands of groups in one command; authored patterns nest two or
 * three deep.
 */
enum { UIP_MAX_GROUP_DEPTH = 64 };

/* Forward declaration of pattern list parser. */
static void uip_parse_list (scr_ptnoderef_t list);


/*
 * uip_parse_match()
 *
 * Match a token to the lookahead, then advance lookahead.
 */
static void
uip_parse_match (scr_uip_tok_t token)
{
  if (uip.parse.lookahead == token)
    uip.parse.lookahead = uip_next_token ();
  else
    {
      /* Syntax error. */
      scr_error ("uip_parse_match: syntax error, expected %ld, got %ld\n",
                (scr_int) token, (scr_int) uip.parse.lookahead);
      scr_longjmp (uip.parse.error, 1);
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
  if (uip.pools.word_available > 0 && required <= UIP_SHORT_WORD_SIZE)
    {
      scr_int index_;
      scr_ptshortwordref_t shortword;

      index_ = (uip.pools.word_cursor + 1) % UIP_WORD_POOL_SIZE;
      while (index_ != uip.pools.word_cursor)
        {
          if (!uip.pools.words[index_].is_in_use)
            break;
          index_ = (index_ + 1) % UIP_WORD_POOL_SIZE;
        }

      if (uip.pools.words[index_].is_in_use)
        scr_fatal ("uip_new_word: no free slot found in the words pool\n");

      /* Use the slot and update the pool cursor and free count. */
      shortword = uip.pools.words + index_;
      strncpy (shortword->word, word, UIP_SHORT_WORD_SIZE);
      shortword->is_in_use = TRUE;

      uip.pools.word_cursor = index_;
      uip.pools.word_available--;

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
  first_in_pool = uip.pools.words[0].word;
  last_in_pool = uip.pools.words[UIP_WORD_POOL_SIZE - 1].word;

  /* If from the pool, mark the entry as no longer in use, otherwise free. */
  if (word >= first_in_pool && word <= last_in_pool)
    {
      scr_int index_;
      scr_ptshortwordref_t shortword;

      /*
       * Calculate the index to the word pool entry from which this short
       * word was allocated.
       */
      index_ = (word - first_in_pool) / sizeof (uip.pools.words[0]);
      shortword = uip.pools.words + index_;
      assert (shortword->word == word);

      shortword->is_in_use = FALSE;
      uip.pools.word_available++;
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
scr_ptnoderef_t
uip_new_node (scr_pttype_t type)
{
  scr_ptnoderef_t node;

  /*
   * Unless the pool is empty, search forwards from the next cursor position
   * until an unused slot is found, or until the index wraps to the cursor.
   */
  if (uip.pools.node_available > 0)
    {
      scr_int index_;

      index_ = (uip.pools.node_cursor + 1) % UIP_NODE_POOL_SIZE;
      while (index_ != uip.pools.node_cursor)
        {
          if (uip.pools.nodes[index_].type == NODE_UNUSED)
            break;
          index_ = (index_ + 1) % UIP_NODE_POOL_SIZE;
        }

      if (uip.pools.nodes[index_].type != NODE_UNUSED)
        scr_fatal ("uip_new_node: no free slot found in the nodes pool\n");

      /* Use the slot and update the pool cursor and free count. */
      node = uip.pools.nodes + index_;
      node->is_allocated = FALSE;

      uip.pools.node_cursor = index_;
      uip.pools.node_available--;
    }
  else
    {
      /* Fall back to less efficient allocations. */
      node = (decltype(node)) scr_malloc (sizeof (*node));
      node->is_allocated = TRUE;
    }

  /* Fill in the remaining fields and return the new node. */
  node->left_child = nullptr;
  node->right_sibling = nullptr;
  node->type = type;
  node->word = nullptr;

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
      uip.pools.node_available++;
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
  while (uip.parse.lookahead == TOK_ALTERNATES_SEPARATOR)
    {
      uip_parse_match (TOK_ALTERNATES_SEPARATOR);
      child->right_sibling = uip_parse_new_list ();
      child = child->right_sibling;
    }
}


/*
 * uip_parse_enter_group()
 *
 * Note one more level of group nesting, and refuse a pattern that nests too
 * deeply to parse on the stack.
 */
static void
uip_parse_enter_group (void)
{
  if (uip.parse.group_depth >= UIP_MAX_GROUP_DEPTH)
    {
      scr_error ("uip_parse_enter_group: groups nested too deeply\n");
      scr_longjmp (uip.parse.error, 1);
    }
  uip.parse.group_depth++;
}


/*
 * uip_parse_element()
 *
 * Parse a single pattern element.
 */
static scr_ptnoderef_t
uip_parse_element (void)
{
  scr_ptnoderef_t node = (scr_ptnoderef_t) nullptr;
  scr_uip_tok_t token;

  /* Handle pattern element based on lookahead token. */
  switch (uip.parse.lookahead)
    {
    case TOK_WHITESPACE:
      {
        /*
         * Two or more spaces in a row are matched literally by every
         * Runner: the 4.0 matcher (run400 NewParse 45D940) compares the
         * literal stretch of a pattern with Left()/Right() and "=" after
         * Trim(pattern) -- only the TYPED line has its "  " collapsed to
         * " " -- and 3.7-3.9 checkwild (run390 4346A8) compares literally
         * with nothing collapsed at all.  So a pattern with a double space
         * inside it never matches a normally typed line.  "Ticket to No
         * Where" (4.00) has task 347 "ask  *girl* about *" -- the girl's
         * CharTask, meant to run from her walk -- and run400 answers `ask
         * young girl about grantby` from her conversation topic ("Wie
         * bitte? Grantby?", runner_probes/ticket.run400.txt, 2026-09-12);
         * Scarier ran the task instead, and the extra random(0,12) draw put
         * the whole rest of the transcript out of step.  The multi-space flag
         * belongs to the lookahead token, so read it before advancing.
         */
        scr_bool is_double = uip.tokenizer.multi_space;
        scr_bool after_group = uip.tokenizer.after_group;

        uip_parse_match (TOK_WHITESPACE);
        node = uip_new_node (!is_double ? NODE_WHITESPACE
                             : after_group ? NODE_GROUP_DOUBLE_WHITESPACE
                                           : NODE_DOUBLE_WHITESPACE);
        break;
      }

    case TOK_CHOICE:
      /* Parse a [...[/.../...]] choice. */
      uip_parse_match (TOK_CHOICE);
      node = uip_new_node (NODE_CHOICE);
      uip_parse_enter_group ();
      uip_parse_alternatives (node);
      uip.parse.group_depth--;
      uip_parse_match (TOK_CHOICE_END);
      break;

    case TOK_OPTIONAL:
      /* Parse a {...[/.../...]} optional element. */
      uip_parse_match (TOK_OPTIONAL);
      node = uip_new_node (NODE_OPTIONAL);
      uip_parse_enter_group ();
      uip_parse_alternatives (node);
      uip.parse.group_depth--;
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
      token = uip.parse.lookahead;
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
          scr_fatal ("uip_parse_element: invalid token, %ld\n",
                     (scr_int) token);
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
                " unexpected token, %ld\n", (scr_int) uip.parse.lookahead);
      scr_longjmp (uip.parse.error, 1);
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
  scr_bool literal_only = TRUE, last_was_object = FALSE;

  /* Add elements until a list terminator token is encountered. */
  child = list;
  while (TRUE)
    {
      switch (uip.parse.lookahead)
        {
        case TOK_CHOICE_END:
        case TOK_OPTIONAL_END:
          /* Terminate list building and return. */
          return;

        case TOK_EOS:
          /*
           * A space between the end of a plain literal pattern and the end
           * of the pattern is a space the input must have; see
           * uip_match_whitespace().  So is one after a closing %object%:
           * p4BEYOND's `polish %object% ` matches nothing in run400, `polish
           * orb` being the object catch-all (make_beyondprobe.py,
           * runner_probes/beyond.run400.txt, 2026-09-26), as in all three
           * older Runners.  Not after a group or a wildcard: House's task 190
           * `*get *cathy* ` answers `get cathy`
           * (runner_transcripts/house.txt:146).
           */
          if ((literal_only || last_was_object) && child != list
              && child->type == NODE_WHITESPACE)
            child->type = NODE_HARD_WHITESPACE;

          /* Place EOS at the appropriate link and return. */
          node = uip_new_node (NODE_EOS);
          if (child == list)
            child->left_child = node;
          else
            child->right_sibling = node;
          return;

        case TOK_ALTERNATES_SEPARATOR:
          /*
           * Inside a group, this ends the current alternative.  Outside one
           * it is not a separator at all -- run400's matcher only ever looks
           * for "/" between [] or {} delimiters, so a bare "take/get/eat
           * stew" is a single literal that matches only itself.  Fall into
           * the default case, which makes a literal "/" word node.  (Before
           * this, the list was terminated here *without* a NODE_EOS, so the
           * pattern degenerated to "take" and prefix-matched anything; and
           * later this case sat above TOK_EOS and fell into THAT, so it
           * ended the pattern at the first "/" and "n/north/go north"
           * matched a bare `n` -- hotelconfuego's task 102 swallowed the
           * move out of the Cabaret, where run400 walks north, T39 of
           * runner_transcripts/hotelconfuego.txt.)
           */
          if (uip.parse.group_depth > 0)
            return;
          /* FALLTHROUGH */

        default:
          /* Add the next node at the appropriate link. */
          node = uip_parse_element ();
          if (node->type != NODE_WORD && node->type != NODE_WHITESPACE
              && node->type != NODE_DOUBLE_WHITESPACE)
            literal_only = FALSE;
          if (node->type != NODE_WHITESPACE
              && node->type != NODE_DOUBLE_WHITESPACE
              && node->type != NODE_GROUP_DOUBLE_WHITESPACE)
            last_was_object = node->type == NODE_OBJECT_REFERENCE;
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
        case NODE_DOUBLE_WHITESPACE:
          scr_trace (", double whitespace");
          break;
        case NODE_GROUP_DOUBLE_WHITESPACE:
          scr_trace (", double whitespace after group");
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

void
uip_debug_dump (scr_ptnoderef_t tree)
{
  scr_trace ("UIParser: debug dump follows...\n");
  if (tree)
    {
      scr_trace ("uip.parse.tree = {\n");
      uip_debug_dump_node (tree, 0);
      scr_trace ("}\n");
    }
  else
    scr_trace ("uip.parse.tree = (nil)\n");
}


/*
 * uip_compile_pattern()
 * uip_destroy_pattern()
 *
 * Parse a pattern for uip_match_pattern(), and free one.  A pattern that
 * fails to parse still compiles, to a pattern that matches nothing.  The
 * runner keeps one per task command for the life of the game, so the task
 * matcher neither parses nor looks one up per attempt.
 */
scr_uip_patternref_t
uip_compile_pattern (const scr_char *pattern)
{
  scr_uip_patternref_t compiled;
  scr_char *cleansed;
  scr_char buffer[UIP_ALLOCATION_AVOIDANCE_SIZE];
  assert (pattern);

  compiled = new scr_uip_pattern_s ();
  compiled->binary_candidate = strpbrk (pattern, "*[{")
                               && !strchr (pattern, '%');

  /* Start tokenizer. */
  cleansed = uip_cleanse_string (pattern, buffer, sizeof (buffer), FALSE);
  if (uip.trace)
    scr_trace ("UIParser: pattern \"%s\"\n", cleansed);
  uip_tokenize_start (cleansed);

  /* Try parsing the pattern, and catch errors. */
  if (scr_setjmp (uip.parse.error) == 0)
    {
      /* Parse the pattern into a match tree. */
      uip.parse.lookahead = uip_next_token ();
      uip.parse.group_depth = 0;
      uip.parse.tree = uip_new_node (NODE_LIST);
      uip_parse_list (uip.parse.tree);
      uip_tokenize_end ();
    }
  else
    {
      /* Parse error -- clean up and fail. */
      uip_tokenize_end ();
      uip_destroy_tree (uip.parse.tree);
      uip.parse.tree = nullptr;
    }
  uip_free_cleansed_string (cleansed, buffer);

  /*
   * Detach the tree from the parser statics, so a re-entrant parse cannot
   * touch it.
   */
  compiled->tree = uip.parse.tree;
  uip.parse.tree = nullptr;
  return compiled;
}

void
uip_destroy_pattern (scr_uip_patternref_t compiled)
{
  if (compiled)
    {
      if (compiled->tree)
        uip_destroy_tree (compiled->tree);
      delete compiled;
    }
}
