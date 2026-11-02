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
 * Prototypes for scparser*.cpp: pattern matching, pronouns and the
 * antecedent bookkeeping.
 */

#ifndef SCARIER_PARSER_H
#define SCARIER_PARSER_H

#include "sctypes.h"

/* Pattern matching functions. */
typedef struct scr_uip_pattern_s *scr_uip_patternref_t;
extern scr_bool uip_match (const scr_char *pattern,
                          const scr_char *string, scr_gameref_t game);
extern scr_uip_patternref_t uip_compile_pattern (const scr_char *pattern);
extern void uip_destroy_pattern (scr_uip_patternref_t compiled);
extern scr_bool uip_match_pattern (scr_uip_patternref_t compiled,
                                   const scr_char *string,
                                   scr_gameref_t game);
extern void uip_set_strict_reference (scr_bool strict, scr_bool match_case);
extern void uip_set_containment (scr_bool enabled);
extern void uip_set_lenient_tasks (scr_bool lenient);
extern void uip_set_binary_input (scr_bool binary);
extern void uip_set_task_commands (scr_bool task_commands);
extern scr_bool uip_get_lenient_tasks (void);
extern scr_bool uip_wildcard_match_400 (const scr_char *pattern,
                                        const scr_char *string);
extern scr_bool uip_wildcard_match_pre400 (const scr_char *pattern,
                                           const scr_char *string,
                                           scr_bool pad);
extern scr_char *uip_replace_pronouns (scr_gameref_t game,
                                       const scr_char *string);
extern void uip_assign_pronouns (scr_gameref_t game, const scr_char *string);
extern void uip_get_pronoun_flags (scr_bool *used, scr_bool *pending_definite);
extern void uip_set_pronoun_flags (scr_bool used, scr_bool pending_definite);
extern void uip_note_definite_reference (void);

/*
 * The three forms a 4.0 object antecedent is held in (scr_game_t it_form):
 * Prefix & " " & Short as authored, the tense()'d "the X", and the bare Short
 * co()'s park arm stores (run400 464788).  Below 4.0 only the first two occur.
 */
enum { UIP_IT_INDEFINITE = 0, UIP_IT_DEFINITE = 1, UIP_IT_BARE = 2 };

/* Stages of a 4.0 line's antecedent writes; see uip_note_antecedent_400(). */
enum { UIP_STAGE_SCORER = 0, UIP_STAGE_CO = 1, UIP_STAGE_HANDLER = 2 };
extern void uip_begin_antecedent_400 (void);
extern void uip_note_antecedent_400 (scr_int object, scr_int form,
                                     scr_int stage);
extern scr_bool uip_pronoun_was_used (void);
extern void uip_commit_antecedent_400 (scr_gameref_t game);
extern scr_char *uip_rewrite_references (scr_gameref_t game,
                                         const scr_char *string,
                                         scr_int prior_npc,
                                         scr_bool echo_printed);
extern scr_bool uip_print_ask_echo (scr_gameref_t game, const scr_char *string);
extern void uip_note_named_npcs (scr_gameref_t game, const scr_char *string);
extern void uip_renote_named_npcs (scr_gameref_t game,
                                   const scr_char *cut_line,
                                   scr_bool walked);
extern scr_bool uip_line_names_npc (scr_gameref_t game,
                                    const scr_char *string);
extern void uip_forget_game (const void *game);
extern void uip_state_reset (void);
extern void uip_debug_trace (scr_bool flag);

/*
 * The matcher's options are ambient: one is set over a whole task loop or
 * command table, and read many calls down, by the matches that loop makes.
 * These guards hold one for a scope and put it back however the scope is
 * left.
 *
 * scr_task_commands_guard marks the matches in its lifetime as task
 * commands, which keeps the pre-3.9 comma boundary to the library's
 * patterns; see uip_match_whitespace().  scr_containment_guard and
 * scr_binary_input_guard set theirs to the value given.  All three clear
 * the option on the way out, whatever it was on the way in: the scopes that
 * nest (a put line's containment over the command tables' own) have always
 * ended with the inner one.
 *
 * scr_lenient_tasks_guard turns lenient task matching on or off -- for one
 * run_all_commands() line, or for a strict peek inside one -- and puts back
 * what was there before.
 */
class scr_task_commands_guard
{
public:
  scr_task_commands_guard () { uip_set_task_commands (TRUE); }
  ~scr_task_commands_guard () { uip_set_task_commands (FALSE); }

  scr_task_commands_guard (const scr_task_commands_guard &) = delete;
  scr_task_commands_guard &
  operator= (const scr_task_commands_guard &) = delete;
};

class scr_containment_guard
{
public:
  explicit scr_containment_guard (scr_bool enabled)
  {
    uip_set_containment (enabled);
  }

  ~scr_containment_guard () { uip_set_containment (FALSE); }

  scr_containment_guard (const scr_containment_guard &) = delete;
  scr_containment_guard &operator= (const scr_containment_guard &) = delete;
};

class scr_binary_input_guard
{
public:
  explicit scr_binary_input_guard (scr_bool binary)
  {
    uip_set_binary_input (binary);
  }

  ~scr_binary_input_guard () { uip_set_binary_input (FALSE); }

  scr_binary_input_guard (const scr_binary_input_guard &) = delete;
  scr_binary_input_guard &
  operator= (const scr_binary_input_guard &) = delete;
};

class scr_lenient_tasks_guard
{
public:
  explicit scr_lenient_tasks_guard (scr_bool lenient)
    : saved_ (uip_get_lenient_tasks ())
  {
    uip_set_lenient_tasks (lenient);
  }

  ~scr_lenient_tasks_guard () { uip_set_lenient_tasks (saved_); }

  scr_lenient_tasks_guard (const scr_lenient_tasks_guard &) = delete;
  scr_lenient_tasks_guard &
  operator= (const scr_lenient_tasks_guard &) = delete;

private:
  const scr_bool saved_;
};

#endif
