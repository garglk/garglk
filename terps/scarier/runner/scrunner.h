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

#ifndef SCRUNNER_H
#define SCRUNNER_H

/*
 * Internal header of the runner, shared by scrunner.cpp, scrun_split.cpp,
 * scrun_respell.cpp, scrun_match.cpp, scrun_match_run.cpp,
 * scrun_match_refusal.cpp, scrun_tables.cpp, scrun_dispatch.cpp,
 * scrun_input.cpp, and by the dump tools in tools/ (scrun_census.cpp,
 * scdump.cpp).  Nothing here is part of the interface the rest of Scarier
 * uses; that stays in core/scrunner_api.h.
 */

#include <string>
#include <vector>

#include "core/scarier.h"
#include "core/scprotos.h"
#include "core/scgamest.h"

/* Assorted definitions and constants. */
enum { LINE_BUFFER_SIZE = 256 };
static const scr_char SPECIAL_PATTERN = '#';
static const scr_char WILDCARD_PATTERN = '*';


/* State shared across the runner files, each defined in the file named. */

#ifdef SCARIER_DUMP_TOOLS
/* tools/scrun_census.cpp */
extern scr_bool run_census_requested (scr_gameref_t game);
/* scrun_match.cpp */
extern scr_bool run_census_matches_task (scr_gameref_t game, scr_int task,
                                         const scr_char *string);
#endif

/* scrun_match_run.cpp */
#ifdef SCARIER_DUMP_TOOLS
extern scr_bool run_probe_strict;
extern scr_bool run_strict_tasks_requested (const scr_char *string);
#endif
/* scrun_match.cpp */
extern scr_bool run_rerun_skips_tasks;
extern scr_bool run_rerun_exact_spaces;
extern scr_bool run_put_class_only;
/* scrun_match_run.cpp */
extern scr_bool run_matcher_second_pass;
/* scrun_match_refusal.cpp */
extern scr_bool run_repeat_assist;

/* scrun_tables.cpp */
extern scr_bool run_priority_deferred;
extern scr_bool run_priority_refused;
extern scr_bool run_put_clause_loop_active;

/*
 * scr_ref_number_guard
 *
 * Save and put back the game's referenced-number state across a library
 * command table walk.
 *
 * Three library rows carry a %number% wildcard -- "wait 5", "redo 3",
 * "hist 3" (the wait row is written twice, abbreviation-gated) -- and
 * uip_match_number() sets the referenced number as a side effect of matching
 * one, exactly as it does for a game task's own %number%.  But those
 * commands are Scarier's, not the Runner's: run400 has no "wait N" at all,
 * and its referenced number (MemVar_49420C) has just two writers,
 * numintext/numintext2, reachable only from the wildcard expansion helper
 * mdlSpreadTheLoad.Proc_19_36_45F268 and only when the pattern being
 * expanded really contains %number%.  So in the Runner a game with no number
 * wildcard anywhere can never see a referenced number other than the initial
 * zero, and a "$number = N" restriction (Type 4, Var1 0 -- run400
 * loc_4817BF..loc_4817F8) can never pass.
 *
 * Leaving the leak in makes those meta commands part of the game: Sandy.taf's
 * win task tests the referenced number against 2 where the author meant the
 * variable "mom", so `wait 2` followed by `look in toilet` won a game that is
 * unwinnable in the Runner.  Restoring around the whole walk -- not inside the
 * handlers -- also covers a row that matches and then declines, such as
 * "redo 99" falling through to the %text% row.
 *
 * Only the number is guarded.  The referenced object, character and text are
 * bound by library rows the Runner does have, and the text they later expand
 * into is meant to see them.
 */
class scr_ref_number_guard
{
public:
  explicit scr_ref_number_guard (scr_gameref_t game)
    : vars_ (gs_get_vars (game)),
      number_ (var_get_ref_number (vars_)),
      is_referenced_ (var_is_number_referenced (vars_))
  {
  }

  ~scr_ref_number_guard ()
  {
    var_restore_ref_number (vars_, number_, is_referenced_);
  }

  scr_ref_number_guard (const scr_ref_number_guard &) = delete;
  scr_ref_number_guard &operator= (const scr_ref_number_guard &) = delete;

private:
  const scr_var_setref_t vars_;
  const scr_int number_;
  const scr_bool is_referenced_;
};


/* scrun_dispatch.cpp */
#ifdef SCARIER_DUMP_TOOLS
extern std::string run_trace_last_input;
#endif
extern const scr_char *run_dispatch_input;

/*
 * run_dispatch_input_guard
 *
 * Points run_dispatch_input at a line of a pass's own making for the scope
 * of the guard, and puts back the line it found there when the scope ends,
 * by return or by throw alike.
 */
struct run_dispatch_input_guard
{
  explicit run_dispatch_input_guard (const scr_char *line)
    : saved (run_dispatch_input) { run_dispatch_input = line; }
  ~run_dispatch_input_guard () { run_dispatch_input = saved; }
  run_dispatch_input_guard (const run_dispatch_input_guard &) = delete;
  run_dispatch_input_guard &operator= (const run_dispatch_input_guard &)
      = delete;

private:
  const scr_char *const saved;
};
extern std::vector<std::string> run_goto_steps;
extern size_t run_goto_next;
extern std::string run_goto_rest;
extern std::string run_goto_arrival;
extern scr_bool run_goto_arrival_due;
extern std::vector<scr_bool> run_tasks_ran_this_command;
extern scr_bool run_repeat_found_400;
extern std::string run_co_pending_input;
extern scr_bool run_co_task_claimed;
extern scr_char run_prior_element[LINE_BUFFER_SIZE];
extern std::string run_previous_typed_line;
extern std::string run_typed_line;
extern std::string run_temporary_text;
extern std::string run_undo_text;



/* Structure used to associate a pattern with a handler function. */
typedef struct scr_commands_s
{
  const scr_char *const command;
  scr_bool (*const handler) (scr_gameref_t game);
} scr_commands_t;

typedef scr_commands_t *scr_commandsref_t;

/*
 * scr_ref_entity_guard
 *
 * The referenced object and character, restored on scope exit.  For the
 * speculative table probes (run_is_put_command() and friends), which ask
 * "would this row match?" without running anything: the answer must not
 * leave a binding behind.  3.9 and 4.0 forget both references at the top of
 * every command (see run_player_input()), so a probe's leftover is exactly
 * what the Runner never has.  Professor
 * (runner_probes/professor.run400.profmail.txt, turn 21): the put-row probe
 * bound the mailbox from `get mail from mailbox on-a rope` and task 7's
 * state restriction on the referenced object passed on it, where run400
 * fails the restriction silently and takes the mail.
 */
class scr_ref_entity_guard
{
public:
  explicit scr_ref_entity_guard (scr_gameref_t game)
    : vars_ (gs_get_vars (game)),
      object_ (var_get_ref_object (vars_)),
      character_ (var_get_ref_character (vars_))
  {
  }

  ~scr_ref_entity_guard ()
  {
    var_set_ref_object (vars_, object_);
    var_set_ref_character (vars_, character_);
  }

  scr_ref_entity_guard (const scr_ref_entity_guard &) = delete;
  scr_ref_entity_guard &operator= (const scr_ref_entity_guard &) = delete;

private:
  const scr_var_setref_t vars_;
  const scr_int object_;
  const scr_int character_;
};

enum {
  RUN_GOTO_NONE, RUN_GOTO_EXAMINE, RUN_GOTO_TAKE, RUN_GOTO_DROP,
  RUN_GOTO_KEEP, RUN_GOTO_OPEN, RUN_GOTO_BELOW
};

enum {
  RUN_BATTLE_WEAR = 1 << 0, RUN_BATTLE_REMOVE = 1 << 1,
  RUN_BATTLE_TAKE = 1 << 2, RUN_BATTLE_DROP = 1 << 3,
  RUN_BATTLE_SIT = 1 << 4, RUN_BATTLE_OPEN = 1 << 5,
  RUN_BATTLE_EXAMINE = 1 << 6, RUN_BATTLE_SCORE = 1 << 7,
  RUN_BATTLE_GIVE = 1 << 8, RUN_BATTLE_WHERE = 1 << 9,
  RUN_BATTLE_GOTO = 1 << 10, RUN_BATTLE_WAIT = 1 << 11,
  RUN_BATTLE_TALK = 1 << 12, RUN_BATTLE_ASK = 1 << 13,
  RUN_BATTLE_PLAIN = 1 << 14
};

enum run_refusal_pass_t
{
  REFUSAL_PASS_PRE = 0,
  REFUSAL_PASS_MID,
  REFUSAL_PASS_POST,
  REFUSAL_PASS_PROBE
};


/* Functions shared across the runner files, each defined in the file named. */

/* scrun_split.cpp */
extern scr_bool run_counts_line_elements (scr_gameref_t game);
extern scr_int run_find_split_pre400 (scr_int version, const scr_char *line,
                                      scr_int *tail, scr_bool comma_splits);
extern std::string run_empty_then_head_390 (const scr_char *line);
extern scr_int run_find_split_400 (scr_gameref_t game, const scr_char *line,
                                   scr_int *sep_length);
extern scr_int run_get_version (const scr_prop_setref_t bundle);
extern void run_squeeze_spaces (const scr_char *string, scr_char *buffer);
extern scr_int run_instr (const scr_char *line, const scr_char *word);

/* scrun_respell.cpp */
extern scr_int run_goto_line_class (scr_gameref_t game, const scr_char *line);
extern scr_bool run_goto_strip_head (scr_gameref_t game, const scr_char *line,
                                     std::string &rest);
extern scr_bool run_goto_after (scr_gameref_t game, const scr_char *typed,
                                scr_int goto_class, size_t mark,
                                const std::vector<scr_int> &places,
                                scr_bool status);
extern scr_int run_battle_line_class (scr_gameref_t game, const scr_char *line);
extern std::vector<scr_int> run_battle_places (scr_gameref_t game);
extern const scr_char *
run_battle_kind_word (scr_int version, const scr_char *line, scr_int kind);
extern std::string run_battle_respell (scr_gameref_t game,
                                       const scr_char *line, scr_int kind,
                                       const scr_char *head);
extern scr_bool run_battle_line (scr_gameref_t game, const scr_char *typed,
                                 scr_int kinds);
extern scr_bool run_two_verb_line_400 (scr_gameref_t game,
                                       const scr_char *line,
                                       std::string &hoisted);
extern scr_bool run_hoist_verb_line (scr_gameref_t game,
                                     const scr_char *string,
                                     std::string &hoisted);
extern scr_bool run_standard_commands (scr_gameref_t game,
                                       const scr_char *string);

/* scrun_match.cpp */
extern const std::vector<const scr_char *> &
run_task_command_patterns (scr_gameref_t game, scr_int task, scr_bool forwards);

/* scrun_match_run.cpp */
extern void run_restriction_cache_task_pick (scr_gameref_t game,
                                             const scr_char *string);
extern scr_bool run_game_commands_common (scr_gameref_t game,
                                          const scr_char *string,
                                          scr_bool include_restrictions,
                                          scr_bool is_library,
                                          scr_bool exclude_silent_literal);
extern scr_bool
run_game_commands_in_parser_context (scr_gameref_t game,
                                     const scr_char *string,
                                     scr_bool include_restrictions,
                                     scr_bool exclude_silent_literal);
extern scr_bool
run_defer_loud_tasks_to_movement (scr_gameref_t game, const scr_char *string);

/* scrun_match_refusal.cpp */
extern scr_int run_spent_task_390 (scr_gameref_t game, const scr_char *string,
                                   const scr_char **message);
extern scr_bool run_spent_survivor_390 (scr_gameref_t game,
                                        const scr_char *string);
extern void run_spent_claim_390 (scr_gameref_t game, const scr_char *message);
extern scr_bool run_task_refusal (scr_gameref_t game, const scr_char *string,
                                  run_refusal_pass_t pass);
extern scr_bool run_put_take_400 (scr_gameref_t game, const scr_char *string);
extern void run_takes_second_pass_370 (scr_gameref_t game,
                                       const scr_char *string,
                                       const scr_char *task_string,
                                       size_t task_mark);

/* scrun_tables.cpp */
extern scr_commands_t EXAMINE_ABOVE_REFUSAL_390_COMMANDS[];
extern scr_bool run_priority_commands (scr_gameref_t game,
                                       const scr_char *string);
extern scr_bool run_is_put_command (scr_gameref_t game,
                                    const scr_char *string);
extern scr_bool run_is_put_command_400 (scr_gameref_t game,
                                        const scr_char *string,
                                        scr_bool *contained);
extern scr_bool run_is_inventory_command (scr_gameref_t game,
                                          const scr_char *string);
extern scr_bool run_repeat_survivor_400 (scr_gameref_t game,
                                         const scr_char *string);
extern scr_bool run_try_command_table (scr_commandsref_t command,
                                       scr_gameref_t game,
                                       const scr_char *string);
extern scr_bool run_movement_succeeds (scr_gameref_t game,
                                       const scr_char *string);
extern scr_bool run_standard_verb_commands (scr_gameref_t game,
                                            const scr_char *string);
extern scr_bool
run_standard_give_npc_commands (scr_gameref_t game, const scr_char *string);
extern scr_bool
run_standard_fallback_commands (scr_gameref_t game, const scr_char *string);
extern scr_bool run_therest_arm_at (scr_int version, const scr_char *word);
extern scr_bool run_therest_pre400 (scr_gameref_t game, const scr_char *string);
extern scr_bool run_therest_absent_370 (scr_gameref_t game,
                                        const scr_char *string);
extern scr_bool run_line_for_anywhere (scr_gameref_t game,
                                       const scr_char *line);
extern scr_bool run_score_anywhere (scr_gameref_t game,
                                    const scr_char *string);
extern scr_bool run_wait_anywhere (scr_gameref_t game, const scr_char *string);

/* scrun_dispatch.cpp */
extern scr_bool run_all_commands (scr_gameref_t game, const scr_char *string);
extern void run_finish_goto_walk (void);
extern void run_cancel_goto_walk (void);

/* scrun_input.cpp */
extern scr_char run_line_element[LINE_BUFFER_SIZE];
extern scr_bool run_separator_task_line (scr_gameref_t game,
                                         const scr_char *line);
extern scr_bool run_comma_splits_pre390 (scr_gameref_t game,
                                         const scr_char *line);
extern scr_char *run_element_filtered (scr_gameref_t game);
extern scr_bool run_player_input (scr_gameref_t game);

#endif
