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
 * Prototypes for library/: the command handlers and the hooks the
 * runner's line passes and the engine call.  The library's own
 * umbrella is library/sclibrar.h.
 */

#ifndef SCARIER_LIBRARY_API_H
#define SCARIER_LIBRARY_API_H

#include <string>
#include <vector>
#include "scprops.h"

/* Library perspective enumeration and functions. */
enum
{ LIB_FIRST_PERSON = 0,
  LIB_SECOND_PERSON = 1,
  LIB_THIRD_PERSON = 2
};

/* Globals/Perspective as the Runner of this game's version reads it. */
extern scr_int lib_get_perspective (scr_gameref_t game);
extern scr_int lib_random_roomgroup_member (scr_gameref_t game,
                                            scr_int roomgroup);
extern const scr_char *lib_get_room_name (scr_gameref_t game, scr_int room);
extern scr_bool lib_can_go (scr_gameref_t game, scr_int room,
                            scr_int direction);
/* Silence movement handlers, so they only report whether a move is possible. */
extern void lib_set_movement_probe (scr_bool probe);
/* The word the parser expects for a direction, for the map's click-to-walk. */
extern const scr_char *lib_direction_name (scr_int direction);
extern void lib_print_room_name (scr_gameref_t game, scr_int room);
extern void lib_print_room_description (scr_gameref_t game, scr_int room);
extern scr_bool lib_room_alt_darkens (scr_gameref_t game, scr_int room);
extern std::string lib_definite_object_name (scr_prop_setref_t bundle,
                                             scr_int object);
extern void lib_print_object_np (scr_gameref_t game, scr_int object);
/* An ALR could be matching the Runner's literal " is " in a listing. */
extern scr_bool lib_alrs_see_list_verb (scr_gameref_t game,
                                        scr_int container);
extern void lib_print_npc_np (scr_gameref_t game, scr_int npc);
/* "I'm afraid you are dead!", or its first-person form for a pre-4.0 game. */
extern const scr_char *lib_get_death_message (scr_gameref_t game);
extern scr_bool lib_cmd_go_north (scr_gameref_t game);
extern scr_bool lib_cmd_go_east (scr_gameref_t game);
extern scr_bool lib_cmd_go_south (scr_gameref_t game);
extern scr_bool lib_cmd_go_west (scr_gameref_t game);
extern scr_bool lib_cmd_go_up (scr_gameref_t game);
extern scr_bool lib_cmd_go_down (scr_gameref_t game);
extern scr_bool lib_cmd_go_in (scr_gameref_t game);
extern scr_bool lib_cmd_go_out (scr_gameref_t game);
extern scr_bool lib_cmd_go_northeast (scr_gameref_t game);
extern scr_bool lib_cmd_go_southeast (scr_gameref_t game);
extern scr_bool lib_cmd_go_northwest (scr_gameref_t game);
extern scr_bool lib_cmd_go_southwest (scr_gameref_t game);
extern scr_bool lib_cmd_go_place (scr_gameref_t game);
extern scr_bool lib_go_place_off;
extern scr_bool lib_goto_line_enters (scr_gameref_t game, const scr_char *line);
extern scr_bool lib_goto_line_names_object (scr_gameref_t game,
                                            const scr_char *line);
extern scr_bool lib_therest_absent_370 (scr_gameref_t game);
extern scr_bool lib_with_clause_claims (scr_gameref_t game);
extern scr_bool lib_cmd_verbose (scr_gameref_t game);
extern scr_bool lib_cmd_brief (scr_gameref_t game);
extern scr_bool lib_cmd_notify_on_off (scr_gameref_t game);
extern scr_bool lib_cmd_notify (scr_gameref_t game);
extern scr_bool lib_cmd_time (scr_gameref_t game);
extern scr_bool lib_cmd_date (scr_gameref_t game);
extern scr_bool lib_cmd_quit (scr_gameref_t game);
extern scr_bool lib_cmd_endgame (scr_gameref_t game);
extern scr_bool lib_cmd_control_panel (scr_gameref_t game);
extern scr_bool lib_cmd_restart (scr_gameref_t game);
extern scr_bool lib_cmd_undo (scr_gameref_t game);
extern scr_bool lib_cmd_history (scr_gameref_t game);
extern scr_bool lib_cmd_history_number (scr_gameref_t game);
extern scr_bool lib_cmd_again (scr_gameref_t game);
extern scr_bool lib_cmd_redo_number (scr_gameref_t game);
extern scr_bool lib_cmd_redo_text (scr_gameref_t game);
extern scr_bool lib_cmd_redo_last (scr_gameref_t game);
extern scr_bool lib_cmd_hints (scr_gameref_t game);
extern scr_bool lib_cmd_help (scr_gameref_t game);
extern scr_bool lib_cmd_license (scr_gameref_t game);
extern scr_bool lib_cmd_information (scr_gameref_t game);
extern scr_bool lib_cmd_clear (scr_gameref_t game);
extern scr_bool lib_cmd_statusline (scr_gameref_t game);
extern scr_bool lib_cmd_version (scr_gameref_t game);
extern scr_bool lib_cmd_look (scr_gameref_t game);
extern scr_bool lib_cmd_look_typed (scr_gameref_t game);
extern scr_bool lib_cmd_print_room_exits (scr_gameref_t game);
extern void lib_print_room_exits (scr_gameref_t game, scr_int room);
extern scr_bool lib_cmd_wait (scr_gameref_t game);
extern scr_bool lib_cmd_wait_number (scr_gameref_t game);
extern scr_bool lib_cmd_examine_self (scr_gameref_t game);
extern scr_bool lib_cmd_examine_npc (scr_gameref_t game);
extern scr_bool lib_cmd_examine_object (scr_gameref_t game);
extern scr_bool lib_cmd_count (scr_gameref_t game);
extern scr_bool lib_cmd_take_all (scr_gameref_t game);
extern scr_bool lib_cmd_take_absent (scr_gameref_t game);
extern scr_bool lib_cmd_take_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_take_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_take_all_from (scr_gameref_t game);
extern scr_bool lib_cmd_take_from_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_take_from_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_take_from_nowhere_all (scr_gameref_t game);
extern scr_bool lib_cmd_take_from_nowhere (scr_gameref_t game);
extern scr_bool lib_task_prematches_input (scr_gameref_t game,
                                           scr_int class_filter);
extern scr_int lib_task_prematch_kind_input (scr_gameref_t game,
                                             scr_int class_filter);
extern scr_bool lib_move_what_after_silent_task_pre400 (scr_gameref_t game);
extern scr_bool lib_takes_offers_tasks_370 (scr_gameref_t game,
                                           const scr_char *line);
extern scr_bool lib_take_scored_400 (scr_gameref_t game);
extern scr_bool lib_non_answer;
enum { LIB_NAME_MASK = '#' };
extern void lib_forget_bundle (scr_prop_setref_t bundle);
extern std::string lib_mask_long_names (scr_gameref_t game,
                                        const scr_char *line,
                                        scr_bool objects_only = FALSE);
extern scr_bool lib_take_names_dynamic_400 (scr_gameref_t game,
                                            const scr_char *string);
extern scr_bool lib_cmd_put_no_clause_pre400 (scr_gameref_t game);
extern scr_bool lib_cmd_put_in_nowhere (scr_gameref_t game);
extern scr_bool lib_cmd_put_on_nowhere (scr_gameref_t game);
extern scr_bool lib_cmd_put_in_that_390 (scr_gameref_t game);
extern scr_bool lib_cmd_put_on_that_390 (scr_gameref_t game);
extern scr_bool lib_cmd_take_all_from_npc (scr_gameref_t game);
extern scr_bool lib_cmd_take_from_npc_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_take_from_npc_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_take_npc (scr_gameref_t game);
extern scr_bool lib_cmd_drop_all (scr_gameref_t game);
extern scr_bool lib_cmd_drop_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_drop_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_leave_all_pre400 (scr_gameref_t game);
extern scr_bool lib_cmd_leave_except_multiple_pre400 (scr_gameref_t game);
extern scr_bool lib_cmd_leave_multiple_pre400 (scr_gameref_t game);
extern scr_bool lib_cmd_wear_all (scr_gameref_t game);
extern scr_bool lib_cmd_wear_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_wear_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_remove_all (scr_gameref_t game);
extern scr_bool lib_cmd_remove_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_remove_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_kiss_npc (scr_gameref_t game);
extern scr_bool lib_cmd_kiss_object (scr_gameref_t game);
extern scr_bool lib_cmd_kiss_other (scr_gameref_t game);
extern scr_bool lib_cmd_kiss_ended_400 (scr_gameref_t game);
extern scr_bool lib_cmd_kill_other (scr_gameref_t game);
extern scr_bool lib_cmd_eat_object (scr_gameref_t game);
extern scr_bool lib_cmd_give_object_npc (scr_gameref_t game);
extern scr_bool lib_cmd_inventory (scr_gameref_t game);
extern scr_bool lib_cmd_open_object (scr_gameref_t game);
extern scr_bool lib_cmd_close_object (scr_gameref_t game);
extern scr_bool lib_cmd_unlock_object_with (scr_gameref_t game);
extern scr_bool lib_cmd_lock_object_with (scr_gameref_t game);
extern scr_bool lib_cmd_unlock_object (scr_gameref_t game);
extern scr_bool lib_cmd_lock_object (scr_gameref_t game);
extern scr_bool lib_cmd_ask_npc_about (scr_gameref_t game);
extern scr_bool lib_cmd_put_all_in (scr_gameref_t game);
extern scr_bool lib_cmd_put_in_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_put_in_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_put_all_on (scr_gameref_t game);
extern scr_bool lib_cmd_put_on_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_put_on_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_read_object (scr_gameref_t game);
extern scr_bool lib_cmd_read_other (scr_gameref_t game);
extern scr_bool lib_cmd_stand_on_object (scr_gameref_t game);
extern scr_bool lib_cmd_stand_on_floor (scr_gameref_t game);
extern scr_bool lib_cmd_attack_npc_with (scr_gameref_t game);
extern scr_bool lib_cmd_slap_npc (scr_gameref_t game);
extern scr_bool lib_cmd_slap_npc_with (scr_gameref_t game);
extern scr_bool lib_cmd_slap_object (scr_gameref_t game);
extern scr_bool lib_cmd_slap_other (scr_gameref_t game);
extern scr_bool lib_cmd_slap_what (scr_gameref_t game);
extern scr_bool lib_cmd_chop_npc (scr_gameref_t game);
extern scr_bool lib_cmd_chop_npc_with (scr_gameref_t game);
extern scr_bool lib_cmd_cut_npc (scr_gameref_t game);
extern scr_bool lib_cmd_cut_npc_with (scr_gameref_t game);
extern scr_bool lib_cmd_hit_npc (scr_gameref_t game);
extern scr_bool lib_cmd_hit_npc_with (scr_gameref_t game);
extern scr_bool lib_cmd_shoot_npc (scr_gameref_t game);
extern scr_bool lib_cmd_shoot_npc_with (scr_gameref_t game);
extern scr_bool lib_cmd_stab_npc (scr_gameref_t game);
extern scr_bool lib_cmd_stab_npc_with (scr_gameref_t game);
extern scr_bool lib_cmd_throw_npc_with (scr_gameref_t game);
extern scr_bool lib_cmd_kill_npc (scr_gameref_t game);
extern scr_bool lib_cmd_kill_npc_with (scr_gameref_t game);
extern scr_bool lib_cmd_fight_npc (scr_gameref_t game);
extern scr_bool lib_cmd_fight_npc_with (scr_gameref_t game);
extern scr_bool lib_cmd_attack_npcs (scr_gameref_t game);
extern scr_bool lib_cmd_attack_npcs_with (scr_gameref_t game);
extern const scr_char *lib_battle_line_verb (scr_gameref_t game,
                                            const scr_char *input);
extern scr_int lib_battle_line_npc (scr_gameref_t game,
                                    const scr_char *input);
extern scr_bool lib_cmd_wield (scr_gameref_t game);
extern scr_bool lib_cmd_wield_other (scr_gameref_t game);
extern scr_bool lib_cmd_sit_on_object (scr_gameref_t game);
extern scr_bool lib_cmd_sit_on_floor (scr_gameref_t game);
extern scr_bool lib_cmd_lie_on_object (scr_gameref_t game);
extern scr_bool lib_cmd_lie_on_floor (scr_gameref_t game);
extern scr_bool lib_cmd_sit_scan_370 (scr_gameref_t game);
extern scr_bool lib_sitstand_claims_370 (scr_gameref_t game);
typedef scr_bool (*lib_line_runner_t) (scr_gameref_t game,
                                       const scr_char *line);
extern scr_bool lib_sitstand_anywhere (scr_gameref_t game,
                                       lib_line_runner_t run_line,
                                       std::string *rest);
extern scr_bool lib_openclose_anywhere (scr_gameref_t game,
                                        const scr_char *typed,
                                        lib_line_runner_t run_line,
                                        std::string *rest);
extern scr_bool lib_whereis_anywhere (scr_gameref_t game,
                                      lib_line_runner_t run_line);
extern scr_bool lib_earlier_handler_claims_pre400 (scr_gameref_t game,
                                                   const scr_char *line);
extern scr_bool lib_two_verb_line_pre400 (scr_gameref_t game,
                                          const scr_char *line,
                                          std::string *rewritten);
extern scr_bool lib_cmd_stand_scan_370 (scr_gameref_t game);
extern scr_bool lib_cmd_lie_scan_370 (scr_gameref_t game);
extern scr_bool lib_cmd_get_on_object (scr_gameref_t game);
extern scr_bool lib_cmd_get_off_object (scr_gameref_t game);
extern scr_bool lib_cmd_get_off (scr_gameref_t game);
extern scr_bool lib_cmd_get_down (scr_gameref_t game);
extern scr_bool lib_cmd_save (scr_gameref_t game);
extern scr_bool lib_cmd_restore (scr_gameref_t game);
extern scr_bool lib_cmd_locate_object (scr_gameref_t game);
extern scr_bool lib_cmd_locate_npc (scr_gameref_t game);
extern scr_bool lib_cmd_turns (scr_gameref_t game);
extern scr_bool lib_cmd_score (scr_gameref_t game);
extern scr_bool lib_cmd_status_player (scr_gameref_t game);
extern scr_bool lib_cmd_status_npc (scr_gameref_t game);
extern scr_bool lib_cmd_get_what (scr_gameref_t game);
extern scr_bool lib_cmd_drop_absent_pre390 (scr_gameref_t game);
extern scr_bool lib_cmd_leave_absent_pre400 (scr_gameref_t game);
extern scr_bool lib_cmd_open_other (scr_gameref_t game);
extern scr_bool lib_cmd_open_absent (scr_gameref_t game);
extern scr_bool lib_cmd_close_absent (scr_gameref_t game);
extern scr_bool lib_cmd_close_other (scr_gameref_t game);
extern scr_bool lib_cmd_open_ended_400 (scr_gameref_t game);
extern scr_bool lib_cmd_close_ended_400 (scr_gameref_t game);
extern scr_bool lib_cmd_lock_other (scr_gameref_t game);
extern scr_bool lib_cmd_lock_object_pre_400 (scr_gameref_t game);
extern scr_bool lib_cmd_unlock_object_pre_400 (scr_gameref_t game);
extern scr_bool lib_cmd_lock_what (scr_gameref_t game);
extern scr_bool lib_cmd_unlock_other (scr_gameref_t game);
extern scr_bool lib_cmd_unlock_what (scr_gameref_t game);
extern scr_bool lib_cmd_stand_other (scr_gameref_t game);
extern scr_bool lib_cmd_sit_other (scr_gameref_t game);
extern scr_bool lib_cmd_lie_other (scr_gameref_t game);
extern scr_bool lib_cmd_give_object (scr_gameref_t game);
extern scr_bool lib_cmd_give_what (scr_gameref_t game);
extern scr_bool lib_cmd_remove_what (scr_gameref_t game);
extern scr_bool lib_cmd_drop_what (scr_gameref_t game);
extern scr_bool lib_cmd_leave_what_pre400 (scr_gameref_t game);
extern scr_bool lib_cmd_put_unclear (scr_gameref_t game);
extern scr_bool lib_cmd_wear_what (scr_gameref_t game);
extern scr_bool lib_cmd_profanity (scr_gameref_t game);
extern scr_bool lib_cmd_profanity_390 (scr_gameref_t game);
extern scr_bool lib_cmd_profanity_pre_400 (scr_gameref_t game);
extern scr_bool lib_cmd_examine_all (scr_gameref_t game);
extern scr_bool lib_cmd_examine_other (scr_gameref_t game);
extern scr_bool lib_cmd_look_anywhere_pre_400 (scr_gameref_t game);
extern scr_bool lib_cmd_examine_absent (scr_gameref_t game);
extern scr_bool lib_cmd_locate_other (scr_gameref_t game);
extern scr_bool lib_cmd_unix_like (scr_gameref_t game);
extern scr_bool lib_cmd_dos_like (scr_gameref_t game);
extern scr_bool lib_cmd_ask_object (scr_gameref_t game);
extern scr_bool lib_cmd_ask_npc (scr_gameref_t game);
extern scr_bool lib_cmd_ask_other (scr_gameref_t game);
extern scr_bool lib_cmd_ask_about_nothing (scr_gameref_t game);
extern scr_bool lib_cmd_talk_to_npc_about (scr_gameref_t game);
extern scr_bool lib_cmd_talk_to_npc (scr_gameref_t game);
extern scr_bool lib_cmd_talk_to_npc_pre_390 (scr_gameref_t game);
extern scr_bool lib_cmd_just_a_direction (scr_gameref_t game);
extern scr_bool lib_cmd_block_object (scr_gameref_t game);
extern scr_bool lib_cmd_block_other (scr_gameref_t game);
extern scr_bool lib_cmd_block_what (scr_gameref_t game);
extern scr_bool lib_cmd_break_object (scr_gameref_t game);
extern scr_bool lib_cmd_break_other (scr_gameref_t game);
extern scr_bool lib_cmd_break_absent (scr_gameref_t game);
extern scr_bool lib_cmd_turn_absent (scr_gameref_t game);
extern scr_bool lib_cmd_verb_absent_400 (scr_gameref_t game);
extern scr_bool lib_cmd_break_what (scr_gameref_t game);
extern scr_bool lib_cmd_destroy_what (scr_gameref_t game);
extern scr_bool lib_cmd_smash_what (scr_gameref_t game);
extern scr_bool lib_cmd_buy_object (scr_gameref_t game);
extern scr_bool lib_cmd_buy_other (scr_gameref_t game);
extern scr_bool lib_cmd_buy_absent (scr_gameref_t game);
extern scr_bool lib_cmd_buy_what (scr_gameref_t game);
extern scr_bool lib_cmd_clean_object (scr_gameref_t game);
extern scr_bool lib_cmd_clean_other (scr_gameref_t game);
extern scr_bool lib_cmd_clean_what (scr_gameref_t game);
extern scr_bool lib_cmd_climb_object (scr_gameref_t game);
extern scr_bool lib_cmd_climb_other (scr_gameref_t game);
extern scr_bool lib_cmd_climb_what (scr_gameref_t game);
extern scr_bool lib_cmd_cry (scr_gameref_t game);
extern scr_bool lib_cmd_cut_object (scr_gameref_t game);
extern scr_bool lib_cmd_cut_other (scr_gameref_t game);
extern scr_bool lib_cmd_cut_what (scr_gameref_t game);
extern scr_bool lib_cmd_drink_object (scr_gameref_t game);
extern scr_bool lib_cmd_drink_other (scr_gameref_t game);
extern scr_bool lib_cmd_drink_what (scr_gameref_t game);
extern scr_bool lib_cmd_dance (scr_gameref_t game);
extern scr_bool lib_cmd_eat_other (scr_gameref_t game);
extern scr_bool lib_cmd_feed (scr_gameref_t game);
extern scr_bool lib_cmd_fight (scr_gameref_t game);
extern scr_bool lib_cmd_feel (scr_gameref_t game);
extern scr_bool lib_cmd_clear_object (scr_gameref_t game);
extern scr_bool lib_cmd_clear_other (scr_gameref_t game);
extern scr_bool lib_cmd_fix_object (scr_gameref_t game);
extern scr_bool lib_cmd_fix_other (scr_gameref_t game);
extern scr_bool lib_cmd_fix_what (scr_gameref_t game);
extern scr_bool lib_cmd_fly (scr_gameref_t game);
extern scr_bool lib_cmd_hint (scr_gameref_t game);
extern scr_bool lib_cmd_attack_npc (scr_gameref_t game);
extern scr_bool lib_cmd_hit_object (scr_gameref_t game);
extern scr_bool lib_cmd_hit_other (scr_gameref_t game);
extern scr_bool lib_cmd_hit_what (scr_gameref_t game);
extern scr_bool lib_cmd_hum (scr_gameref_t game);
extern scr_bool lib_cmd_jump (scr_gameref_t game);
extern scr_bool lib_cmd_kick_object (scr_gameref_t game);
extern scr_bool lib_cmd_kick_other (scr_gameref_t game);
extern scr_bool lib_cmd_kick_what (scr_gameref_t game);
extern scr_bool lib_cmd_light_object (scr_gameref_t game);
extern scr_bool lib_cmd_light_other (scr_gameref_t game);
extern scr_bool lib_cmd_light_what (scr_gameref_t game);
extern scr_bool lib_cmd_lift_object (scr_gameref_t game);
extern scr_bool lib_cmd_lift_other (scr_gameref_t game);
extern scr_bool lib_cmd_lift_what (scr_gameref_t game);
extern scr_bool lib_cmd_listen (scr_gameref_t game);
extern scr_bool lib_cmd_mend_object (scr_gameref_t game);
extern scr_bool lib_cmd_mend_other (scr_gameref_t game);
extern scr_bool lib_cmd_mend_what (scr_gameref_t game);
extern scr_bool lib_cmd_move_object (scr_gameref_t game);
extern scr_bool lib_cmd_move_other (scr_gameref_t game);
extern scr_bool lib_cmd_move_what (scr_gameref_t game);
extern scr_bool lib_cmd_please (scr_gameref_t game);
extern scr_bool lib_cmd_press_object (scr_gameref_t game);
extern scr_bool lib_cmd_press_other (scr_gameref_t game);
extern scr_bool lib_cmd_press_what (scr_gameref_t game);
extern scr_bool lib_cmd_pull_object (scr_gameref_t game);
extern scr_bool lib_cmd_pull_other (scr_gameref_t game);
extern scr_bool lib_cmd_pull_what (scr_gameref_t game);
extern scr_bool lib_cmd_punch (scr_gameref_t game);
extern scr_bool lib_cmd_push_object (scr_gameref_t game);
extern scr_bool lib_cmd_push_other (scr_gameref_t game);
extern scr_bool lib_cmd_push_what (scr_gameref_t game);
extern scr_bool lib_cmd_repair_object (scr_gameref_t game);
extern scr_bool lib_cmd_repair_other (scr_gameref_t game);
extern scr_bool lib_cmd_repair_what (scr_gameref_t game);
extern scr_bool lib_cmd_rub_object (scr_gameref_t game);
extern scr_bool lib_cmd_rub_other (scr_gameref_t game);
extern scr_bool lib_cmd_rub_what (scr_gameref_t game);
extern scr_bool lib_cmd_run (scr_gameref_t game);
extern scr_bool lib_cmd_say (scr_gameref_t game);
extern scr_bool lib_cmd_sell_object (scr_gameref_t game);
extern scr_bool lib_cmd_sell_other (scr_gameref_t game);
extern scr_bool lib_cmd_sell_what (scr_gameref_t game);
extern scr_bool lib_cmd_shake_object (scr_gameref_t game);
extern scr_bool lib_cmd_shake_other (scr_gameref_t game);
extern scr_bool lib_cmd_shake_what (scr_gameref_t game);
extern scr_bool lib_cmd_shout (scr_gameref_t game);
extern scr_bool lib_cmd_sing (scr_gameref_t game);
extern scr_bool lib_cmd_sleep (scr_gameref_t game);
extern scr_bool lib_cmd_smell_object (scr_gameref_t game);
extern scr_bool lib_cmd_smell_other (scr_gameref_t game);
extern scr_bool lib_cmd_stop_object (scr_gameref_t game);
extern scr_bool lib_cmd_stop_other (scr_gameref_t game);
extern scr_bool lib_cmd_stop_what (scr_gameref_t game);
extern scr_bool lib_cmd_suck_object (scr_gameref_t game);
extern scr_bool lib_cmd_suck_other (scr_gameref_t game);
extern scr_bool lib_cmd_suck_what (scr_gameref_t game);
extern scr_bool lib_cmd_talk (scr_gameref_t game);
extern scr_bool lib_cmd_speak_pre390 (scr_gameref_t game);
extern scr_bool lib_with_arm_390_applies (scr_gameref_t game);
extern scr_bool lib_with_arm_390 (scr_gameref_t game);
extern scr_bool lib_cmd_thank (scr_gameref_t game);
extern scr_bool lib_cmd_touch_object (scr_gameref_t game);
extern scr_bool lib_cmd_touch_other (scr_gameref_t game);
extern scr_bool lib_cmd_touch_what (scr_gameref_t game);
extern scr_bool lib_cmd_turn_object (scr_gameref_t game);
extern scr_bool lib_cmd_turn_other (scr_gameref_t game);
extern scr_bool lib_cmd_turn_what (scr_gameref_t game);
extern scr_bool lib_cmd_unblock_object (scr_gameref_t game);
extern scr_bool lib_cmd_unblock_other (scr_gameref_t game);
extern scr_bool lib_cmd_unblock_what (scr_gameref_t game);
extern scr_bool lib_cmd_wash_object (scr_gameref_t game);
extern scr_bool lib_cmd_wash_other (scr_gameref_t game);
extern scr_bool lib_cmd_wash_what (scr_gameref_t game);
extern scr_bool lib_cmd_whistle (scr_gameref_t game);
extern scr_bool lib_cmd_interrogation (scr_gameref_t game);
extern scr_bool lib_cmd_xyzzy (scr_gameref_t game);
extern scr_bool lib_cmd_egotistic (scr_gameref_t game);
extern scr_bool lib_cmd_yes_or_no (scr_gameref_t game);
extern scr_bool lib_cmd_verb_object (scr_gameref_t game);
extern scr_bool lib_cmd_put_where_400 (scr_gameref_t game);
extern scr_bool lib_cmd_put_container_400 (scr_gameref_t game);
extern scr_bool lib_put_held_unsplit_400 (scr_gameref_t game,
                                          const scr_char *input);
extern scr_bool lib_put_task_tie_400 (scr_gameref_t game,
                                     const scr_char *input);
extern scr_bool lib_put_container_fits_400 (scr_gameref_t game,
                                           const scr_char *input);
extern scr_bool lib_put_clauses_400 (scr_gameref_t game,
                                    const scr_char *input,
                                    std::vector<std::string> &clauses);
extern scr_bool lib_cmd_verb_npc (scr_gameref_t game);
extern void lib_debug_trace (scr_bool flag);

/* Library hooks called from the runner's line passes (runner/). */
extern void lib_verb_object_note_line_top (scr_gameref_t game);
extern void lib_co_note_line_top (scr_gameref_t game);
extern void lib_co_400_reset (void);
extern void lib_co_400_begin_line (scr_bool is_new_line);
extern void lib_antecedent_begin_line_400 (scr_gameref_t game,
                                           const scr_char *line);
extern void lib_openclose_with_antecedent_400 (scr_gameref_t game,
                                               const scr_char *line);
extern scr_bool lib_co_400_question_pending (void);
extern void lib_co_400_take_question (void);
extern scr_bool lib_co_400_named_question_raised (void);
extern void lib_co_400_drop_question (void);
extern void lib_co_400_note_refusal (void);
extern scr_bool lib_co_400_line_refused (void);
extern void lib_co_400_print_still_ambiguous (scr_gameref_t game);
extern scr_bool lib_co_400_pending_is_npc (void);
extern std::string lib_co_400_npc_answer_line (const scr_char *line);
extern std::string lib_co_400_object_answer_line (const scr_char *line);
extern void lib_co_400_get_question (scr_bool *pending, std::string *term,
                                     std::string *command,
                                     std::vector<scr_int> *candidates,
                                     scr_bool *offered,
                                     std::vector<scr_int> *offered_list);
extern void lib_co_400_set_question (scr_bool pending, const std::string &term,
                                     const std::string &command,
                                     const std::vector<scr_int> &candidates,
                                     scr_bool offered,
                                     const std::vector<scr_int> &offered_list);
extern void lib_battle_who_get_prefix (std::string *pending,
                                       std::string *at_line);
extern void lib_battle_who_set_prefix (const std::string &pending,
                                       const std::string &at_line);
extern scr_bool lib_co_400_line_leaves_which_pending (scr_gameref_t game,
                                                     const scr_char *line);
extern scr_bool lib_openclose_with_half_raise_400 (scr_gameref_t game,
                                                  const scr_char *line);
extern scr_bool lib_openclose_with_half_400 (scr_gameref_t game,
                                             const scr_char *line);
extern scr_bool lib_npc_400_line_names_namesakes (scr_gameref_t game,
                                                  const scr_char *line);
extern scr_bool lib_npc_400_raise_for_line_string (scr_gameref_t game,
                                                   const scr_char *line);
extern scr_bool lib_ask_npc_topic_after_task_390 (scr_gameref_t game,
                                                  size_t mark,
                                                  scr_bool over_empty_buffer);
extern scr_bool lib_talk_to_hint_after_failed_task_390 (scr_gameref_t game,
                                                        const scr_char *input,
                                                        size_t mark);
extern scr_bool lib_line_names_npc_390 (scr_gameref_t game,
                                        const scr_char *input);
extern scr_bool lib_put_where_question_400 (scr_gameref_t game,
                                           std::string *question);
extern scr_bool lib_input_contains_word (const scr_char *input,
                                         const scr_char *word);
extern void lib_battle_who_reset (void);
extern void lib_put_reset (void);
extern void lib_battle_who_begin_element (scr_bool new_line);
extern void lib_battle_who_note_unanswered (void);
extern void lib_battle_400_claim_line (void);
extern scr_bool lib_battle_400_line_claimed (void);
extern scr_bool lib_battle_which_pending (void);
extern std::string lib_battle_who_continuation (const scr_char *command,
                                                scr_bool status);
extern scr_bool lib_question_with_rule (scr_gameref_t game,
                                        const scr_char *line);
extern void lib_with_prefix_390_reset (void);
extern void lib_with_prefix_390_get (std::string *pending);
extern void lib_with_prefix_390_set (const std::string &pending);
extern void lib_with_prefix_390_begin_element (void);
extern void lib_with_prefix_390_end_element (void);
extern std::string lib_with_prefix_390_continuation (const scr_char *command,
                                                     scr_bool status);
extern void lib_prepass_seen_3738 (scr_gameref_t game,
                                   const scr_char *command);
extern scr_bool lib_co_ambiguity_prompt (scr_gameref_t game,
                                        const scr_char *command);

#endif
