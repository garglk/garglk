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

#ifndef SCLIBRAR_INTERNAL_H
#define SCLIBRAR_INTERNAL_H

/*
 * Internal header of the library's core: what the eleven core topics
 * (sclibrar_disambig.cpp and the two files split out of it, _examine,
 * _dispatch, _take, _takefrom, _drop, _open, _put, _battle, _refuse and
 * _verbobj) share among themselves.  They were once fragments of a single
 * translation unit, so much here is helper detail rather than interface;
 * the peripheral topic files use sclibrar.h alone.
 *
 * Module notes:
 *
 * o Ensure module messages precisely match the real Runner ones.  This
 *   matters for ALRs.
 *
 * o Capacity checks on the player and on containers are implemented, but
 *   may not be right.
 */

#include "sclibrar.h"

/*
 * The disambiguation files' module state: every file-scope variable of
 * sclibrar_disambig.cpp and the two files split out of it, grouped by
 * lifetime.  One instance, `disambig`, defined in sclibrar_disambig.cpp.
 * It is declared here and not in sclibrar_disambig_internal.h because the
 * other core files write a few of the per-line and per-element flags.
 *
 *   which      whether this game keeps the Runner's "Which <term>." form;
 *              decided once per bundle by lib_which_runner_form(), dropped
 *              by lib_co_400_reset().
 *   long_names the multi-word names lib_mask_long_names() masks, objects
 *              then characters; collected once per bundle, dropped by
 *              lib_forget_bundle() before the runner destroys it.
 *   line       per typed line, set by lib_co_note_line_top(): the 3.7
 *              "a handler settled this line's object" flag, and the
 *              snapshot of which objects fitted the line's co() mode.
 *   resolve_parent_400
 *              the container mode 4 admits from; see
 *              lib_resolve_admit_parent().  Set and put back to -1 around
 *              one call by the take-from handler.
 *   question   the open 4.0 question: generaltasks' MemVar_494234 and the
 *              bookkeeping of spending it.  Lives from the prompt to the
 *              line that answers or drops it.
 *   element    what this 4.0 line element did, cleared for each one by
 *              lib_co_400_begin_line().
 *   offered    the list the last 4.0 prompt offered (MemVar_4941F4); kept
 *              across a line boundary only while `flagged` is set.
 *
 * Saved in a Spatterlight autosave, through lib_co_400_get_question() /
 * lib_co_400_set_question(): question.pending, .term, .command and
 * .candidates, and the offered list when the next line will keep it.  The
 * rest is per-line scratch or rebuilds from the bundle.  lib_co_400_reset()
 * clears the three 4.0 groups and `which` for a new run; see
 * sclibrar_disambig_400.cpp for what each 4.0 member models.
 */
enum { LIB_LONG_NAME_CATEGORIES = 2 };

struct scr_disambig_state_s
{
  struct
  {
    scr_prop_setref_t bundle = nullptr;
    scr_bool runner_form = FALSE;
  } which;

  struct
  {
    scr_prop_setref_t bundle = nullptr;
    std::vector<std::string> names[LIB_LONG_NAME_CATEGORIES];
  } long_names;

  struct
  {
    scr_bool prompt_370_blocked = FALSE;
    scr_int top_mode = 0;
    std::vector<scr_bool> top_fits;
  } line;

  scr_int resolve_parent_400 = -1;

  struct
  {
    scr_bool pending = FALSE;
    scr_bool refused = FALSE;
    scr_bool was_pending = FALSE;
    std::string term;
    std::string command;
    std::vector<scr_int> candidates;
    scr_bool spend = FALSE;
  } question;

  struct
  {
    scr_bool named_raised = FALSE;
    scr_bool therest_split = FALSE;
  } element;

  struct
  {
    std::vector<scr_int> list;
    scr_bool seen = FALSE;
    scr_bool flagged = FALSE;
  } offered;
};
typedef struct scr_disambig_state_s scr_disambig_state_t;

extern scr_disambig_state_t disambig;

/* sclibrar_disambig.cpp, _disambig_scan, _disambig_400 */
extern scr_bool
lib_print_npc_not_here_pre390 (scr_gameref_t game, scr_int npc);
extern scr_bool lib_co_term_shadowed (scr_gameref_t game, const scr_char *line,
                                      scr_int object, const scr_char *term);
extern const scr_char *lib_co_lastword (const scr_char *string);
extern scr_bool lib_co_prefix_excluded (scr_gameref_t game,
                                        const scr_char *line, scr_int object);
extern scr_bool lib_co_names_prefix (scr_gameref_t game, const scr_char *line,
                                     scr_int object);
extern scr_bool lib_co_object_answers_to (scr_gameref_t game, scr_int object,
                                          const scr_char *term);
extern scr_bool lib_co_candidate (scr_gameref_t game, scr_int object,
                                  scr_int room);
extern scr_bool lib_co_mode_fits (scr_gameref_t game, scr_int object,
                                  scr_int mode);
extern void lib_co_400_raise (scr_gameref_t game, const scr_char *term,
                              const std::vector<scr_int> &objects);
extern void lib_co_400_raise_named (scr_gameref_t game, const scr_char *term,
                                    const std::vector<scr_int> &objects);
extern scr_bool lib_with_split_crowd_400 (scr_gameref_t game, scr_bool examine,
                                          std::vector<scr_int> *crowd,
                                          scr_int *pending,
                                          scr_int *head_object);
extern scr_bool
lib_co_400_raise_for_with_tail (scr_gameref_t game, scr_int pending,
                                const std::vector<scr_int> &crowd);
extern scr_int lib_co_400_prefix_contest (scr_gameref_t game,
                                          const scr_char *word,
                                          const scr_char *input, scr_int room);
extern void lib_co_400_walk_step (scr_gameref_t game, scr_int object,
                                  const scr_char *input, scr_int *me,
                                  std::vector<scr_int> *list,
                                  scr_bool *list_ok);
extern scr_bool
lib_co_400_raise_for_short_tie (scr_gameref_t game,
                                const std::vector<scr_int> &tied);
extern scr_int lib_npc_400_prefix_score (scr_gameref_t game, scr_int npc,
                                         const scr_char *input);
extern scr_bool
lib_npc_400_find_namesakes (scr_gameref_t game, std::string *term_out,
                            std::vector<scr_int> *namesakes_out);
extern scr_bool lib_npc_400_raise_for_line (scr_gameref_t game);
extern scr_int
lib_disambiguate_object_common (scr_gameref_t game, const scr_char *verb,
                                scr_bool (*resolver) (scr_gameref_t, scr_int,
                                                      scr_int),
                                scr_int resolver_arg, scr_bool *is_ambiguous);
extern scr_bool lib_input_contains_word_400 (const scr_char *input,
                                             const scr_char *word);
extern scr_int lib_prefix_words_in_input_400 (const scr_char *input,
                                              const scr_char *prefix);

/* sclibrar_examine.cpp */
extern scr_bool lib_list_in_object (scr_gameref_t game, scr_int container,
                                    scr_bool is_described, scr_bool joined);
extern scr_bool lib_list_in_on_object (scr_gameref_t game, scr_int object,
                                       scr_bool is_described);
extern const scr_char *lib_co_400_name_word (scr_gameref_t game,
                                             scr_int object,
                                             const scr_char *input);
extern scr_int lib_co_400_present_namesakes (scr_gameref_t game,
                                             const scr_char *word);
extern scr_int
lib_examine_referencedob_ex_400 (scr_gameref_t game, const scr_char *input,
                                 scr_bool crowd_contest);
extern scr_int lib_examine_npc_overwrite_400 (scr_gameref_t game);

/* sclibrar_dispatch.cpp */
extern scr_bool lib_task_prematches_line (scr_gameref_t game,
                                          const scr_char *input,
                                          scr_int class_filter,
                                          scr_int *match_kind = nullptr);
extern scr_bool lib_rebuilt_raw_dispatch;
extern scr_bool lib_rebuilt_silent_continues;
extern scr_bool lib_rebuilt_fallback_typed;
extern scr_bool lib_try_game_command_short (scr_gameref_t game,
                                            const scr_char *verb,
                                            scr_int object);
extern scr_bool
lib_try_game_command_short_canonical (scr_gameref_t game, const scr_char *verb,
                                      scr_int object);
extern scr_bool
lib_try_game_command_short_definite (scr_gameref_t game, const scr_char *verb,
                                     scr_int object);
extern scr_bool
lib_try_game_command_take_definite (scr_gameref_t game, scr_int object);
extern scr_bool
lib_try_game_command_take_from_parent_400 (scr_gameref_t game, scr_int object,
                                           scr_bool *looked_up);
extern scr_bool
lib_try_game_command_with_object (scr_gameref_t game, const scr_char *verb,
                                  scr_int object, const scr_char *preposition,
                                  scr_int other_object);
extern scr_bool
lib_try_game_command_with_object_400 (scr_gameref_t game, const scr_char *verb,
                                      scr_int object,
                                      const scr_char *preposition,
                                      scr_int other_object);
extern scr_bool lib_try_typed_put_line_400 (scr_gameref_t game);
extern scr_bool
lib_try_game_command_with_npc (scr_gameref_t game, const scr_char *verb,
                               scr_int object, const scr_char *preposition,
                               scr_int npc);
extern scr_bool
lib_parse_multiple_objects (scr_gameref_t game, const scr_char *verb,
                            scr_bool (*resolver) (scr_gameref_t, scr_int,
                                                  scr_int),
                            scr_int resolver_arg, scr_int *count);
extern void lib_print_nothing_held (scr_gameref_t game, scr_bool worn,
                                    scr_bool add_else, const scr_char *tail);
extern scr_bool
lib_multiple_retains_associate (scr_gameref_t game, scr_int associate,
                                const scr_char *verb);
extern scr_int
lib_apply_filter (scr_gameref_t game,
                  scr_bool (*filter) (scr_gameref_t, scr_int, scr_int),
                  scr_int filter_arg, scr_bool is_except, scr_int *references);

/* sclibrar_take.cpp */
extern scr_bool lib_object_too_heavy (scr_gameref_t game, scr_int object);
extern void lib_print_too_heavy (scr_gameref_t game, scr_int object);
extern scr_bool lib_object_too_large (scr_gameref_t game, scr_int object);
extern scr_bool lib_take_from_single_named;
extern const scr_char *lib_take_from_verb (scr_gameref_t game);
extern scr_bool lib_take_container_unheld (scr_gameref_t game,
                                           scr_int container);
extern void lib_print_not_holding (scr_gameref_t game, scr_int container,
                                   const scr_char *suffix);
extern lib_print_item_t lib_object_printer_390 (scr_gameref_t game);

/*
 * lib_drain_multiple_references_if()
 * lib_drain_multiple_references()
 *
 * Fill 'list' with every object still marked in the multiple references,
 * clearing each as it's collected; the filtered form takes only the objects
 * 'keep' accepts and leaves the rest marked.  Shared body for the take, wear,
 * move and put backends' reports of the objects left over ("You are not
 * holding ...", "You can't take ...", "You can't wear ...").
 */
template <typename Keep>
static void
lib_drain_multiple_references_if (scr_gameref_t game, scr_int object_count,
                                  lib_list_t &list, Keep keep)
{
  scr_int object;

  list.clear ();
  for (object = 0; object < object_count; object++)
    {
      if (!game->multiple_references[object] || !keep (object))
        continue;

      list.push_back (object);
      game->multiple_references[object] = FALSE;
    }
}

extern void lib_drain_multiple_references (scr_gameref_t game,
                                           scr_int object_count,
                                           lib_list_t &list);
extern void lib_take_from_object_backend (scr_gameref_t game,
                                          scr_int associate);
extern void lib_take_from_npc_backend (scr_gameref_t game, scr_int associate);

/* sclibrar_take_multiple.cpp */
extern scr_bool lib_take_filter (scr_gameref_t game, scr_int object,
                                 scr_int unused);
extern scr_int lib_take_absent_score (scr_gameref_t game, scr_int object,
                                      const scr_char *input,
                                      const scr_char **term);
extern scr_bool lib_take_scored_fallback;
extern scr_bool lib_take_tie_400 (scr_gameref_t game, const scr_char *line,
                                  scr_int pending, scr_int last_tied,
                                  const std::vector<scr_int> &marked,
                                  scr_int mark_count);
extern void lib_take_from_task_sweep_380 (scr_gameref_t game);
extern scr_bool lib_isheld_390 (scr_gameref_t game, scr_int object);
extern scr_bool lib_name_instr_range (scr_gameref_t game, const scr_char *line,
                                      scr_int object, scr_int *lowest,
                                      scr_int *highest);
extern scr_bool lib_obhere_380 (scr_gameref_t game, scr_int object);
extern scr_bool lib_present_370 (scr_gameref_t game, scr_int object);
extern std::string lib_take_from_head (const scr_char *line, scr_int at);
extern scr_bool
lib_move_named_whole_line_pre400 (scr_gameref_t game,
                                  scr_bool (*resolver) (scr_gameref_t, scr_int,
                                                        scr_int),
                                  scr_int mode, scr_int *references);
extern scr_bool lib_take_multiple_common (scr_gameref_t game,
                                          scr_bool is_except);

/* sclibrar_takefrom.cpp */
extern scr_bool lib_take_from_filter (scr_gameref_t game, scr_int object,
                                      scr_int associate);
extern void lib_take_from_empty (scr_gameref_t game, scr_int associate,
                                 scr_bool is_except);
extern scr_bool lib_take_from_trailing (scr_gameref_t game);
extern scr_bool lib_take_from_and_line (scr_gameref_t game);
extern scr_bool lib_take_from_and (scr_gameref_t game);

/* sclibrar_drop.cpp */

/*
 * The verb-specific half of drop, remove, and put-on.  All three list the
 * objects they acted on, then list the ones they had to leave alone, and
 * differ only in how an object moves and in the words around the lists.
 */
typedef struct
{
  void (*move) (scr_gameref_t game, scr_int object, scr_int target);
  const scr_char *onto;       /* " onto ", or NULL where there is no target */
  const scr_char *does[3];    /* "You drop ", and so on */
  const scr_char *lacks[3];   /* "You are not holding ", and so on */
  scr_char lacks_end;         /* Terminator for the "not holding" list */
  /*
   * Below 3.9 the leftovers are not a list at all.  run380's drops()
   * (@438DD5-438E13) and its remove handler (@430076-4300C8) each walk the
   * objects in index order and, for the first name-match they cannot act
   * on, set the turn's message ONLY IF IT IS STILL EMPTY: "<You> don't have
   * <raw prefix> <short>!" and "<You> <are> not wearing <raw prefix>
   * <short>!" -- so a second unactionable object is never named, and none
   * is once something was dropped or removed on the same line.  run370 is
   * the same code (drop @430BD4, remove @429954).  NULL keeps the list.
   */
  const scr_char *lacks_pre_390[3];
  scr_char lacks_end_pre_390;
  /*
   * run390's drops() keeps that shape -- the first object it cannot drop,
   * and only while nothing has been said -- but names it through the
   * definite helper Proc_2_36_42B0E8 (445CD4-445D0F): p39EXAM `drop stone`
   * with the stone on the floor, and `drop coin` with the coin in the open
   * crate, are "You don't have the stone!" and "... the coin!" (run390
   * runner_probes/exam.run390.b.txt, 2026-09-14).  Its remove handler is
   * unmeasured.
   */
  scr_bool lacks_single_390;
} lib_move_verb_t;

extern const lib_move_verb_t LIB_PUT_ON_VERB;
extern scr_bool lib_move_backend (scr_gameref_t game,
                                  const lib_move_verb_t *verb, scr_int target,
                                  scr_bool has_printed);
extern scr_int lib_name_object_resolve_400 (scr_gameref_t game,
                                            const scr_char *input,
                                            scr_int mode, scr_int *pending,
                                            scr_int *last_tied,
                                            std::vector<scr_int> *marked,
                                            scr_int *mark_count);
extern const scr_char *
lib_drop_named_term_400 (scr_gameref_t game, scr_int object,
                         const scr_char *input, scr_bool last_alias);
extern void lib_print_not_clear_which_400 (scr_gameref_t game,
                                           const scr_char *term);
extern scr_bool lib_co_400_raise_for_pending_tie (scr_gameref_t game);
extern scr_bool lib_give_defer_catch_all;
extern scr_bool lib_wear_is_put_line_380 (scr_gameref_t game);
extern scr_bool lib_wear_would_act_390 (scr_gameref_t game);
extern scr_bool lib_list_in_object_pre_390 (scr_gameref_t game,
                                            scr_int container);

/* sclibrar_open.cpp */

/*
 * The verb-specific half of lock and unlock.  The two commands run the same
 * steps -- disambiguate the object, check its openness, look up the key it
 * takes, check the player holds that key, then flip the state -- and differ
 * only in the openness they act on, the openness they leave behind, and the
 * words they print.
 */
typedef struct
{
  scr_int required_openness;      /* Openness the verb acts on */
  scr_int new_openness;           /* Openness it leaves behind */
  const scr_char *verb;           /* "lock", for disambiguation */
  const scr_char *nothing_to;     /* " anything to lock " */
  const scr_char *wrong_state[2]; /* Singular and plural state refusal */
  const scr_char *cant[3];        /* "You can't lock ", and so on */
  const scr_char *does[3];        /* "You lock ", and so on */
} lib_lock_verb_t;

extern scr_int lib_with_half_400 (scr_gameref_t game, const scr_char *half);
extern scr_bool lib_cant_do_with_400 (scr_gameref_t game, const scr_char *verb,
                                      const scr_char *particle,
                                      scr_bool *handled);
extern const lib_lock_verb_t LIB_UNLOCK_VERB;
extern const lib_lock_verb_t LIB_LOCK_VERB;
extern scr_int lib_lock_absent_object_400 (scr_gameref_t game);
extern scr_bool lib_lock_backend (scr_gameref_t game,
                                  const lib_lock_verb_t *verb,
                                  scr_bool with_key);

/* sclibrar_put_multiple.cpp */
extern scr_int lib_instr (const scr_char *line, const scr_char *term);

/* sclibrar_battle.cpp */
extern void lib_battle_who_store (const std::string &pending);
extern void lib_with_prefix_390_note (scr_gameref_t game);
extern void lib_question_prefix_from_line (scr_gameref_t game);
extern scr_bool lib_checkverb_bare_400 (scr_gameref_t game,
                                        const scr_char *verb,
                                        const scr_char *label);

/* sclibrar_refuse.cpp */
extern scr_bool lib_cant_do_other (scr_gameref_t game, const scr_char *verb);
extern scr_bool lib_cmd_unclear_object (scr_gameref_t game);
extern scr_int lib_seen_named_object_400 (scr_gameref_t game,
                                          const scr_char *input);

/* sclibrar_verbobj.cpp */
extern scr_bool lib_resolve_admit_mode0 (scr_gameref_t game, scr_int object,
                                         scr_int pass);
extern scr_bool lib_resolve_admit_take (scr_gameref_t game, scr_int object,
                                        scr_int pass);
extern scr_bool lib_resolve_admit_parent (scr_gameref_t game, scr_int object,
                                          scr_int pass);
extern scr_int lib_take_resolve_400_string (scr_gameref_t game,
                                            const scr_char *input,
                                            std::vector<scr_int> *tied);
extern scr_int lib_verb_object_resolve_400 (scr_gameref_t game);
extern scr_bool lib_is_put_where_line_400 (scr_gameref_t game);
extern std::string::size_type
lib_put_split_400 (scr_gameref_t game, const std::string &line,
                   scr_bool has_all, scr_bool on_test, scr_bool *on_branch);
extern scr_bool lib_catch_all_names_pre390 (scr_gameref_t game,
                                            scr_int object);

#endif
