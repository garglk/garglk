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

#ifndef SCPARSER_INTERNAL_H
#define SCPARSER_INTERNAL_H

/*
 * Internal header of the pattern parser: what scparser.cpp and the four
 * files split out of it share.  They were one translation unit, so much here
 * is shared state rather than interface.
 *
 *   scparser.cpp          uip_match() and its cache of compiled patterns,
 *                         tracing, string cleansing
 *   scparser_pattern.cpp  tokenizer and tree builder, the node and word
 *                         pools, uip_compile_pattern()
 *   scparser_match.cpp    the per-node matchers and their state, the
 *                         matching options, the "*" wildcard matchers,
 *                         uip_match_pattern()
 *   scparser_entity.cpp   %object% and %character% matching and the
 *                         candidate cache
 *   scparser_pronoun.cpp  pronoun substitution, the antecedent registers,
 *                         named-NPC noting, the ask/talk rewrite
 */

/*
 * Module notes:
 *
 * o Pattern matching follows the Runners where it has been measured: the
 *   "*" wildcard and %text% rules are in uip_wildcard_match_400() /
 *   uip_wildcard_match_pre400() and uip_match_text(), and %character% /
 *   %object% matching, prefixes included, in uip_build_entities() and
 *   uip_match_entity().  Each carries its own measurement notes and the
 *   deliberate deviations from the Runner are marked as such.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"
#include "scprops.h"
#include "scvars.h"
#include "scprintf.h"
#include "scgamest_api.h"
#include "scparser.h"
#include "scnpcs.h"
#include "scobjcts.h"
#include "sclocale.h"
#include "scinterf.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const scr_char MINUS = '-';
static const scr_char PLUS = '+';
static const scr_char PERCENT = '%';


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
  NODE_HARD_WHITESPACE, NODE_DOUBLE_WHITESPACE, NODE_GROUP_DOUBLE_WHITESPACE,
  NODE_JOIN,
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


/*
 * A parsed pattern: its tree (NULL if the pattern failed to parse), and
 * whether uip_set_binary_input() can apply to it, which depends on the
 * pattern's text alone.  Matching never mutates either.
 */
struct scr_uip_pattern_s
{
  scr_ptnoderef_t tree;
  scr_bool binary_candidate;
};


/* Buffer size below which cleansed strings and tokens avoid malloc(). */
enum { UIP_ALLOCATION_AVOIDANCE_SIZE = 128 };


/* Enumeration of pattern tokens.  TOK_NONE represents a non-occurring token. */
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
 * Pool of statically allocated nodes, for faster allocations.  Nodes are
 * first allocated from here, then by straight malloc() if this pool is empty.
 * An average game's peak node allocation seems to be around 40-50 nodes, so
 * allowing 128 here should be plenty.
 */
enum { UIP_NODE_POOL_SIZE = 128 };

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


/*
 * A %object% or %character% match candidate: every string it answers to
 * (name, prefixed name, aliases), with a precomputed lead character per
 * form for cheap rejection.  See scparser_entity.cpp.
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


/*
 * The parser's module state, in one instance (uip, defined in scparser.cpp).
 * It is grouped by lifetime, which is also the autosave and re-entry story:
 *
 *   trace      the interface's parser trace flag (uip_debug_trace()), set
 *              before any game exists and kept across them.
 *   options    set by the runner through the uip_set_*() calls around a
 *              pass or a match, in pairs; read by the matchers.  Never
 *              saved.
 *   tokenizer  one pattern string being tokenized, between
 *              uip_tokenize_start() and uip_tokenize_end().
 *   parse      one uip_compile_pattern() call: the lookahead, the error
 *              jump, the group depth and the tree under construction.
 *   pools      the node and short-word pools.  Live as long as any compiled
 *              pattern does, which through scparser.cpp's pattern cache is
 *              the life of the process, so uip_state_reset() leaves them.
 *   match      one uip_match_pattern() call: the string, the cursor, the
 *              game, and the per-match snapshot of the options and the
 *              version-dependent punctuation rules.
 *   cache      the %object%/%character% candidates of one game, dropped by
 *              uip_forget_game() when that game is destroyed.
 *   pronoun    the line's staged antecedent, the registers as they stood
 *              before uip_note_named_npcs(), and the two flags a line that
 *              nothing answered leaves behind.  Those two outlast the line
 *              and are the only parser state in a Spatterlight autosave
 *              (uip_get_pronoun_flags() / uip_set_pronoun_flags()).
 *
 * uip_state_reset() puts the between-games groups (options, cache, pronoun)
 * back to their start; run_create() calls it.  The per-call groups are
 * cleared by their own start/end pairs.
 */
struct scr_uip_state_s
{
  scr_bool trace = FALSE;

  struct
  {
    scr_bool binary_input = FALSE;       /* uip_set_binary_input() */
    scr_bool task_commands = FALSE;      /* uip_set_task_commands() */
    scr_bool lenient_tasks = FALSE;      /* uip_set_lenient_tasks() */
    scr_bool strict_reference = FALSE;   /* uip_set_strict_reference() */
    scr_bool strict_case = FALSE;
    scr_bool containment = FALSE;        /* uip_set_containment() */
  } options;

  struct
  {
    const scr_char *pattern = nullptr;
    scr_int index = 0;
    const scr_char *token_value = nullptr;
    scr_char static_temporary[UIP_ALLOCATION_AVOIDANCE_SIZE] = {};
    scr_char *temporary = nullptr;
    scr_bool multi_space = FALSE;        /* see uip_next_token() */
    scr_bool after_group = FALSE;
  } tokenizer;

  struct
  {
    scr_uip_tok_t lookahead = TOK_NONE;
    jmp_buf error;
    scr_int group_depth = 0;
    scr_ptnoderef_t tree = nullptr;         /* for cleanup on a parse error */
  } parse;

  struct
  {
    scr_ptnode_t nodes[UIP_NODE_POOL_SIZE] = {};
    scr_int node_cursor = 0;
    scr_int node_available = UIP_NODE_POOL_SIZE;
    scr_ptshortword_t words[UIP_WORD_POOL_SIZE] = {};
    scr_int word_cursor = 0;
    scr_int word_available = UIP_WORD_POOL_SIZE;
  } pools;

  struct
  {
    const scr_char *string = nullptr;
    scr_int posn = 0;
    scr_gameref_t game = nullptr;
    scr_bool binary_active = FALSE;
    scr_bool comma_is_space = FALSE;
    const scr_char *word_end_punctuation = "";
    scr_bool last_group_empty = FALSE;
    scr_bool last_group_optional = FALSE;
    scr_bool strict_is_character = FALSE;  /* set by uip_match_entity() */
  } match;

  struct
  {
    const void *game = nullptr;
    std::vector<scr_uip_entity_t> objects;
    std::vector<scr_uip_entity_t> npcs;
  } cache;

  struct
  {
    scr_bool used = FALSE;
    scr_bool pending_definite = FALSE;
    scr_int antecedent_object = -1;
    scr_int antecedent_form = UIP_IT_INDEFINITE;
    scr_int antecedent_stage = -1;
    scr_int npc_before_noting = -1;
    scr_int him_before_noting = -1;
    scr_int her_before_noting = -1;
    scr_int it_before_noting = -1;
    scr_int it_object_before_noting = -1;
  } pronoun;
};
typedef struct scr_uip_state_s scr_uip_state_t;

extern scr_uip_state_t uip;


/* Shared across the files; each is defined in the file named. */

/* scparser.cpp */
extern scr_char *
uip_cleanse_string (const scr_char *original, scr_char *buffer, scr_int length,
                    scr_bool trim_trailing = TRUE);
extern scr_char *
uip_free_cleansed_string (scr_char *string, const scr_char *buffer);

/* scparser_pattern.cpp */
extern scr_ptnoderef_t uip_new_node (scr_pttype_t type);
extern void uip_debug_dump (scr_ptnoderef_t tree);

/* scparser_match.cpp */
extern scr_gameref_t uip_get_game (void);
extern scr_int uip_skip_article (const scr_char *string, scr_int start);
extern scr_bool uip_match_node (scr_ptnoderef_t node);

/* scparser_entity.cpp */
extern scr_bool
uip_case_folds_name_in (const scr_char *command, const scr_char *name);
extern scr_bool uip_match_character (scr_ptnoderef_t node);
extern scr_bool uip_match_object (scr_ptnoderef_t node);

#endif
