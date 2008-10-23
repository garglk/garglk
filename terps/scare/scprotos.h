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

#include <stdio.h>

#include "scare.h"

#ifndef PROTOTYPES_H
#define PROTOTYPES_H

/* Runtime version and emulated version, for %version% variable and so on. */
#ifndef SCARE_VERSION
# define SCARE_VERSION		"1.3.3"
#endif
#ifndef SCARE_PATCH_LEVEL
# define SCARE_PATCH_LEVEL	""
#endif
#ifndef SCARE_EMULATION
# define SCARE_EMULATION	4044
#endif

/* True, false, and null definitions in case they're missing. */
#ifndef FALSE
# define	FALSE	0
#endif
#ifndef TRUE
# define	TRUE	(!FALSE)
#endif
#ifndef NULL
# if defined(__cplusplus)
#  define	NULL	0
# else
#  define	NULL	((void *)0)
# endif
#endif

/* Vartype typedef, supports relaxed typing. */
typedef union sc_vartype_u {
	sc_int32	integer;
	sc_bool		boolean;
	sc_char		*string;
	void		*voidp;
} sc_vartype_t;

/*
 * Small utility and wrapper functions.  For printf wrappers, try to apply
 * GNU GCC printf argument checking; this code is cautious about applying
 * the checks.
 */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
extern void		sc_trace (const sc_char *format, ...)
			__attribute__ ((__format__ (__printf__, 1, 2)));
extern void		sc_error (const sc_char *format, ...)
			__attribute__ ((__format__ (__printf__, 1, 2)));
extern void		sc_fatal (const sc_char *format, ...)
			__attribute__ ((__format__ (__printf__, 1, 2)));
#else
extern void		sc_trace (const sc_char *format, ...);
extern void		sc_error (const sc_char *format, ...);
extern void		sc_fatal (const sc_char *format, ...);
#endif
extern void		*sc_malloc (size_t size);
extern void		*sc_realloc (void *pointer, size_t size);
extern void		sc_free (void *pointer);
extern sc_int32		sc_rand (void);
extern sc_int32		sc_randomint (sc_int32 low, sc_int32 high);
extern sc_bool		sc_strempty (const sc_char *string);
extern sc_uint32	sc_hash (const sc_char *name);

/* TAF file reader/decompressor opaque typedef and functions. */
typedef	struct sc_taf_s	*sc_tafref_t;
extern void		taf_destroy (sc_tafref_t taf);
extern sc_tafref_t	taf_create (sc_uint16 (*callback)
					(void *, sc_byte *, sc_uint16),
						void *opaque,
				sc_bool is_gamefile);
extern sc_tafref_t	taf_create_file (FILE *stream, sc_bool is_gamefile);
extern sc_tafref_t	taf_create_filename (const sc_char *filename,
				sc_bool is_gamefile);
extern void		taf_first_line (sc_tafref_t taf);
extern sc_char		*taf_next_line (sc_tafref_t taf);
extern sc_bool		taf_more_lines (sc_tafref_t taf);
extern sc_uint32	taf_get_game_data_length (sc_tafref_t taf);
extern sc_uint16	taf_get_version (sc_tafref_t taf);
extern void		taf_debug_dump (sc_tafref_t taf);

/* Properties store opaque typedef, and functions. */
typedef struct sc_prop_set_s
			*sc_prop_setref_t;
extern sc_prop_setref_t	prop_create (sc_tafref_t taf);
extern void		prop_destroy (sc_prop_setref_t bundle);
extern void		prop_put (sc_prop_setref_t bundle,
				sc_char *format, sc_vartype_t vt_value,
				sc_vartype_t vt_key[]);
extern sc_bool		prop_get (sc_prop_setref_t bundle,
				sc_char *format, sc_vartype_t *vt_value,
				sc_vartype_t vt_key[]);
extern void		prop_solidify (sc_prop_setref_t bundle);
extern sc_int32		prop_get_integer (sc_prop_setref_t bundle,
				sc_char *format, sc_vartype_t vt_key[]);
extern sc_bool		prop_get_boolean (sc_prop_setref_t bundle,
				sc_char *format, sc_vartype_t vt_key[]);
extern sc_char		*prop_get_string (sc_prop_setref_t bundle,
				sc_char *format, sc_vartype_t vt_key[]);
extern void		prop_adopt (sc_prop_setref_t bundle, void *addr);
extern void		prop_debug_trace (sc_bool flag);
extern void		prop_debug_dump (sc_prop_setref_t bundle);

/* Game parser functions. */
extern sc_bool		parse_game (sc_tafref_t taf, sc_prop_setref_t bundle);
extern void		parse_debug_trace (sc_bool flag);

/* Game state structure for modules that use it. */
typedef struct sc_gamestate_s
			*sc_gamestateref_t;

/* Hint type definition, a thinly disguised pointer to task entry. */
typedef struct sc_taskstate_s
			*sc_hintref_t;

/* Variables set functions. */
typedef struct sc_var_set_s
			*sc_var_setref_t;
extern void		var_put (sc_var_setref_t vars,
				sc_char *name, sc_char type,
				sc_vartype_t vt_value);
extern sc_bool		var_get (sc_var_setref_t vars,
				sc_char *name, sc_char *type,
				sc_vartype_t *vt_rvalue);
extern void		var_put_integer (sc_var_setref_t vars,
				sc_char *name, sc_int32 value);
extern sc_int32		var_get_integer (sc_var_setref_t vars,
				sc_char *name);
extern void		var_put_string (sc_var_setref_t vars,
				sc_char *name, sc_char *string);
extern sc_char		*var_get_string (sc_var_setref_t vars,
				sc_char *name);
extern sc_var_setref_t	var_create (sc_prop_setref_t bundle);
extern void		var_destroy (sc_var_setref_t vars);
extern void		var_register_gamestate (sc_var_setref_t vars,
				sc_gamestateref_t gamestate);
extern void		var_set_ref_character (sc_var_setref_t vars,
				sc_int32 character);
extern void		var_set_ref_object (sc_var_setref_t vars,
				sc_int32 object);
extern void		var_set_ref_number (sc_var_setref_t vars,
				sc_int32 number);
extern void		var_set_ref_text (sc_var_setref_t vars,
				sc_char *text);
extern sc_int32		var_get_ref_character (sc_var_setref_t vars);
extern sc_int32		var_get_ref_object (sc_var_setref_t vars);
extern sc_int32		var_get_ref_number (sc_var_setref_t vars);
extern sc_char		*var_get_ref_text (sc_var_setref_t vars);
extern sc_uint32	var_get_elapsed_seconds (sc_var_setref_t vars);
extern void		var_set_elapsed_seconds (sc_var_setref_t vars,
				sc_uint32 seconds);
extern void		var_debug_trace (sc_bool flag);
extern void		var_debug_dump (sc_var_setref_t vars);

/* Expression evaluation functions. */
extern sc_bool		expr_eval_numeric_expression (sc_char *expression,
				sc_var_setref_t vars, sc_int32 *rvalue);
extern sc_bool		expr_eval_string_expression (sc_char *expression,
				sc_var_setref_t vars, sc_char **rvalue);

/* Print filtering functions. */
typedef struct sc_printfilter_s
			*sc_printfilterref_t;
extern sc_printfilterref_t
			pf_create (void);
extern void		pf_destroy (sc_printfilterref_t filter);
extern void		pf_buffer_string (sc_printfilterref_t filter,
				const sc_char *string);
extern void		pf_buffer_character (sc_printfilterref_t filter,
				sc_char character);
extern void		pf_new_sentence (sc_printfilterref_t filter);
extern void		pf_buffer_tag (sc_printfilterref_t filter,
				sc_uint32 tag);
extern void		pf_strip_tags (sc_char *string);
extern void		pf_strip_tags_for_hints (sc_char *string);
extern sc_char		*pf_filter (const sc_char *string,
				sc_var_setref_t vars, sc_prop_setref_t bundle);
extern sc_char		*pf_filter_for_info (const sc_char *string,
				sc_var_setref_t vars);
extern void		pf_flush (sc_printfilterref_t filter,
				sc_var_setref_t vars, sc_prop_setref_t bundle);
extern sc_char		*pf_get_buffer (sc_printfilterref_t filter);
extern sc_char		*pf_filter_input (const sc_char *string,
				sc_prop_setref_t bundle);
extern void		pf_debug_trace (sc_bool flag);

/* Game state functions. */
extern sc_gamestateref_t
			gs_create (sc_var_setref_t vars,
				sc_prop_setref_t bundle,
				sc_printfilterref_t filter);
extern void		gs_copy (sc_gamestateref_t to,
				sc_gamestateref_t from);
extern void		gs_destroy (sc_gamestateref_t gamestate);
extern sc_bool		gs_is_gamestate_valid (sc_gamestateref_t gamestate);

/* Game state accessors and mutators. */
extern void		gs_move_player_to_room (sc_gamestateref_t gamestate,
				sc_int32 room);
extern sc_bool		gs_player_in_room (sc_gamestateref_t gamestate,
				sc_int32 room);
extern void		gs_set_event_state (sc_gamestateref_t gs,
				sc_int32 event, sc_int32 state);
extern void		gs_set_event_time (sc_gamestateref_t gs,
				sc_int32 event, sc_int32 etime);
extern sc_int32		gs_get_event_state (sc_gamestateref_t gs,
				sc_int32 event);
extern sc_int32		gs_get_event_time (sc_gamestateref_t gs,
				sc_int32 event);
extern void		gs_dec_event_time (sc_gamestateref_t gs,
				sc_int32 event);
extern void		gs_set_room_seen (sc_gamestateref_t gs,
				sc_int32 room, sc_bool seen);
extern sc_bool		gs_get_room_seen (sc_gamestateref_t gs,
				sc_int32 room);
extern void		gs_set_task_done (sc_gamestateref_t gs,
				sc_int32 task, sc_bool done);
extern void		gs_set_task_scored (sc_gamestateref_t gs,
				sc_int32 task, sc_bool scored);
extern sc_bool		gs_get_task_done (sc_gamestateref_t gs,
				sc_int32 task);
extern sc_bool		gs_get_task_scored (sc_gamestateref_t gs,
				sc_int32 task);
extern void		gs_set_object_openness (sc_gamestateref_t gs,
				sc_int32 object, sc_int32 openness);
extern void		gs_set_object_state (sc_gamestateref_t gs,
				sc_int32 object, sc_int32 state);
extern void		gs_set_object_seen (sc_gamestateref_t gs,
				sc_int32 object, sc_bool seen);
extern void		gs_set_object_unmoved (sc_gamestateref_t gs,
				sc_int32 object, sc_bool unmoved);
extern sc_int32		gs_get_object_openness (sc_gamestateref_t gs,
				sc_int32 object);
extern sc_int32		gs_get_object_state (sc_gamestateref_t gs,
				sc_int32 object);
extern sc_bool		gs_get_object_seen (sc_gamestateref_t gs,
				sc_int32 object);
extern sc_bool		gs_get_object_unmoved (sc_gamestateref_t gs,
				sc_int32 object);
extern sc_int32		gs_get_object_position (sc_gamestateref_t gs,
				sc_int32 object);
extern sc_int32		gs_get_object_parent (sc_gamestateref_t gs,
				sc_int32 object);
extern void		gs_object_move_onto (sc_gamestateref_t gs,
				sc_int32 object, sc_int32 onto);
extern void		gs_object_move_into (sc_gamestateref_t gs,
				sc_int32 object, sc_int32 into);
extern void		gs_object_make_hidden (sc_gamestateref_t gs,
				sc_int32 object);
extern void		gs_object_player_get (sc_gamestateref_t gs,
				sc_int32 object);
extern void		gs_object_npc_get (sc_gamestateref_t gs,
				sc_int32 object, sc_int32 npc);
extern void		gs_object_player_wear (sc_gamestateref_t gs,
				sc_int32 object);
extern void		gs_object_npc_wear (sc_gamestateref_t gs,
				sc_int32 object, sc_int32 npc);
extern void		gs_object_to_room (sc_gamestateref_t gs,
				sc_int32 object, sc_int32 room);

/* Pattern matching functions. */
extern sc_bool		uip_match (const sc_char *pattern,
				const sc_char *string,
				sc_gamestateref_t gamestate);
extern void		uip_debug_trace (sc_bool flag);

/* Library functions. */
extern void		lib_warn_battle_system (sc_gamestateref_t gamestate);
extern sc_int32		lib_random_roomgroup_member
				(sc_gamestateref_t gamestate,
				sc_int32 roomgroup);
extern sc_char		*lib_get_room_name (sc_gamestateref_t gamestate,
				sc_int32 room);
extern void		lib_print_room_name (sc_gamestateref_t gamestate,
				sc_int32 room);
extern void		lib_print_room_description (sc_gamestateref_t gamestate,
				sc_int32 room);
extern sc_bool		lib_cmd_go_north (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_go_east (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_go_south (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_go_west (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_go_up (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_go_down (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_go_in (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_go_out (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_go_northeast (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_go_southeast (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_go_northwest (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_go_southwest (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_verbose (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_brief (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_notify_on (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_notify_off (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_notify (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_again (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_quit (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_restart (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_undo (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_hints (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_help (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_license (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_information (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_clear (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_version (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_look (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_print_room_exits (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_wait (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_examine_self (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_examine_npc (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_examine_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_get_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_get_object_from (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_get_npc (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_drop_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_get_all (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_get_all_from (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_drop_all (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_wear_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_remove_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_remove_all (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_kiss_npc (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_kiss_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_kiss_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_eat_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_give_object_npc (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_inventory (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_open_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_close_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_unlock_object_with(sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_lock_object_with (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_unlock_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_lock_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_ask_npc_about (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_put_object_on (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_put_object_in (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_read_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_stand_on_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_stand_on_floor (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_attack_npc (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_attack_npc_with (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_sit_on_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_sit_on_floor (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_lie_on_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_lie_on_floor (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_save (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_restore (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_locate_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_locate_npc (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_turns (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_score (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_get_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_open_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_close_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_give_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_remove_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_drop_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_wear_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_profanity (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_examine_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_locate_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_unix_like (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_dos_like (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_ask_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_block_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_block_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_break_npc (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_break_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_buy (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_clean_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_clean_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_climb_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_climb_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_cry (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_cut_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_cut_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_dance (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_feed_npc (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_feed_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_feed_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_feel (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_fix_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_fix_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_fly (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_hint (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_hit_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_hit_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_hum (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_jump (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_kick_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_kick_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_light_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_light_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_listen (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_mend_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_mend_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_move_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_move_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_please (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_press_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_press_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_pull_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_pull_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_punch (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_push_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_push_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_repair_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_repair_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_run (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_say (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_sell_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_sell_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_shake_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_shake_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_shout (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_sing (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_sleep (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_smell_npc (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_smell_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_suck_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_suck_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_talk (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_thank (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_turn_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_turn_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_unblock_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_unblock_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_wash_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_wash_other (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_whistle (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_interrogation (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_xyzzy (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_yes (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_verb_object (sc_gamestateref_t gamestate);
extern sc_bool		lib_cmd_verb_npc (sc_gamestateref_t gamestate);
extern void		lib_debug_trace (sc_bool flag);

/* Resource control functions. */
typedef struct sc_resource_s
			*sc_resourceref_t;
extern sc_bool		res_has_sound (sc_gamestateref_t gamestate);
extern sc_bool		res_has_graphics (sc_gamestateref_t gamestate);
extern void		res_clear_resource (sc_resourceref_t resource);
extern sc_bool		res_compare_resource (sc_resourceref_t from,
				sc_resourceref_t with);
extern void		res_handle_resource (sc_gamestateref_t gamestate,
				const sc_char *partial_format,
				sc_vartype_t vt_partial[]);
extern void		res_sync_resources (sc_gamestateref_t gamestate);
extern void		res_cancel_resources (sc_gamestateref_t gamestate);

/* Game runner functions. */
extern sc_bool		run_task_commands (sc_gamestateref_t gamestate,
				const sc_char *string, sc_bool unrestricted);
extern sc_gamestateref_t
			run_create (sc_uint16 (*callback)
					(void *, sc_byte *, sc_uint16),
						void *opaque,
						sc_uint32 trace_flags);
extern sc_gamestateref_t
			run_create_file (FILE *stream,
				sc_uint32 trace_flags);
extern sc_gamestateref_t
			run_create_filename (const sc_char *filename,
				sc_uint32 trace_flags);
extern void		run_interpret (sc_gamestateref_t gamestate);
extern void		run_destroy (sc_gamestateref_t gamestate);
extern void		run_restart (sc_gamestateref_t gamestate);
extern sc_bool		run_save (sc_gamestateref_t gamestate);
extern sc_bool		run_restore (sc_gamestateref_t gamestate);
extern sc_bool		run_undo (sc_gamestateref_t gamestate);
extern void		run_quit (sc_gamestateref_t gamestate);
extern sc_bool		run_is_running (sc_gamestateref_t gamestate);
extern sc_bool		run_has_completed (sc_gamestateref_t gamestate);
extern sc_bool		run_is_undo_available (sc_gamestateref_t gamestate);
extern void		run_debug_trace (sc_bool flag);
extern void		run_get_attributes (sc_gamestateref_t gamestate,
				sc_char **game_name, sc_char **game_author,
				sc_uint32 *turns, sc_int32 *score,
				sc_int32 *max_score,
				sc_char **current_room_name,
				sc_char **status_line,
				sc_char **preferred_font,
				sc_bool *bold_room_names, sc_bool *verbose,
				sc_bool *notify_score_change);
extern void		run_set_attributes (sc_gamestateref_t gamestate,
				sc_bool bold_room_names, sc_bool verbose,
				sc_bool notify_score_change);
extern sc_hintref_t	run_hint_iterate (sc_gamestateref_t gamestate,
				sc_hintref_t hint);
extern sc_char		*run_get_hint_question (sc_gamestateref_t gamestate,
				sc_hintref_t hint);
extern sc_char		*run_get_subtle_hint (sc_gamestateref_t gamestate,
				sc_hintref_t hint);
extern sc_char		*run_get_unsubtle_hint (sc_gamestateref_t gamestate,
				sc_hintref_t hint);

/* Event functions. */
extern sc_bool		evt_can_see_event (sc_gamestateref_t gamestate,
				sc_int32 event);
extern void		evt_tick_events (sc_gamestateref_t gamestate);
extern void		evt_debug_trace (sc_bool flag);

/* Task functions. */
extern sc_bool		task_has_hints (sc_gamestateref_t gamestate,
				sc_int32 task);
extern sc_char		*task_get_hint_question (sc_gamestateref_t gamestate,
				sc_int32 task);
extern sc_char		*task_get_hint_subtle (sc_gamestateref_t gamestate,
				sc_int32 task);
extern sc_char		*task_get_hint_unsubtle (sc_gamestateref_t gamestate,
				sc_int32 task);
extern sc_bool		task_can_run_task (sc_gamestateref_t gamestate,
				sc_int32 task);
extern sc_bool		task_run_task (sc_gamestateref_t gamestate,
				sc_int32 task, sc_bool forwards);
extern void		task_debug_trace (sc_bool flag);

/* Task restriction functions. */
extern sc_bool		restr_pass_task_object_state
				(sc_gamestateref_t gamestate,
				sc_int32 var1, sc_int32 var2);
extern sc_bool		restr_evaluate_task_restrictions
				(sc_gamestateref_t gamestate,
				sc_int32 task,
				sc_bool *pass, sc_char **fail_message);
extern void		restr_debug_trace (sc_bool flag);

/* NPC functions. */
extern sc_bool		npc_in_room (sc_gamestateref_t gamestate,
				sc_int32 npc, sc_int32 room);
extern sc_int32		npc_count_in_room (sc_gamestateref_t gamestate,
				sc_int32 room);
extern void		npc_setup_initial (sc_gamestateref_t gamestate);
extern void		npc_start_npc_walk (sc_gamestateref_t gamestate,
				sc_int32 npc, sc_int32 walk);
extern void		npc_tick_npcs (sc_gamestateref_t gamestate);
extern void		npc_turn_update (sc_gamestateref_t gamestate);
extern void		npc_debug_trace (sc_bool flag);

/* Object functions. */
extern sc_bool		obj_is_static (sc_gamestateref_t gamestate,
				sc_int32 object);
extern sc_bool		obj_is_container (sc_gamestateref_t gamestate,
				sc_int32 object);
extern sc_bool		obj_is_surface (sc_gamestateref_t gamestate,
				sc_int32 object);
extern sc_int32		obj_container_object (sc_gamestateref_t gamestate,
				sc_int32 n);
extern sc_int32		obj_surface_object (sc_gamestateref_t gamestate,
				sc_int32 n);
extern sc_bool		obj_indirectly_in_room (sc_gamestateref_t gamestate,
				sc_int32 object, sc_int32 room);
extern sc_bool		obj_indirectly_held_by_player
				(sc_gamestateref_t gamestate, sc_int32 object);
extern sc_bool		obj_directly_in_room (sc_gamestateref_t gamestate,
				sc_int32 object, sc_int32 room);
extern sc_int32		obj_stateful_object (sc_gamestateref_t gamestate,
				sc_int32 n);
extern sc_int32		obj_dynamic_object (sc_gamestateref_t gamestate,
				sc_int32 n);
extern sc_int32		obj_standable_object (sc_gamestateref_t gamestate,
				sc_int32 n);
extern sc_int32		obj_get_size (sc_gamestateref_t gamestate,
				sc_int32 object);
extern sc_int32		obj_get_weight (sc_gamestateref_t gamestate,
				sc_int32 object);
extern sc_int32		obj_get_container_maxsize (sc_gamestateref_t gamestate,
				sc_int32 object);
extern sc_int32		obj_get_container_capacity (sc_gamestateref_t gamestate,
				sc_int32 object);
extern sc_int32		obj_lieable_object (sc_gamestateref_t gamestate,
				sc_int32 n);
extern void		obj_setup_initial (sc_gamestateref_t gamestate);
extern sc_int32		obj_container_index (sc_gamestateref_t gamestate,
				sc_int32 object);
extern sc_int32		obj_surface_index (sc_gamestateref_t gamestate,
				sc_int32 object);
extern sc_int32		obj_stateful_index (sc_gamestateref_t gamestate,
				sc_int32 object);
extern sc_char		*obj_state_name (sc_gamestateref_t gamestate,
				sc_int32 object);
extern sc_bool		obj_shows_initial_description
				(sc_gamestateref_t gamestate,
				sc_int32 object);
extern void		obj_turn_update (sc_gamestateref_t gamestate);
extern void		obj_debug_trace (sc_bool flag);

/* Game serialization functions. */
extern sc_bool		ser_save_game (sc_gamestateref_t gamestate);
extern sc_bool		ser_load_game (sc_gamestateref_t gamestate);

/* Debugger interface. */
typedef struct sc_debugger_s
			*sc_debuggerref_t;
extern sc_bool		debug_run_command (sc_gamestateref_t gamestate,
				const sc_char *debug_command);
extern sc_bool		debug_cmd_debugger (sc_gamestateref_t gamestate);
extern void		debug_set_enabled (sc_gamestateref_t gamestate,
				sc_bool enable);
extern sc_bool		debug_get_enabled (sc_gamestateref_t gamestate);
extern void		debug_game_started (sc_gamestateref_t gamestate);
extern void		debug_game_ended (sc_gamestateref_t gamestate);
extern void		debug_turn_update (sc_gamestateref_t gamestate);

/* OS interface functions. */
extern void		if_print_string (const sc_char *string);
extern void		if_print_debug (const sc_char *string);
extern void		if_print_character (sc_char character);
extern void		if_print_debug_character (sc_char character);
extern void		if_print_tag (sc_uint32 tag, const sc_char *arg);
extern void		if_read_line (sc_char *buffer, sc_uint16 length);
extern void		if_read_debug (sc_char *buffer, sc_uint16 length);
extern sc_bool		if_confirm (sc_uint32 type);
extern void		*if_open_saved_game (sc_bool is_save);
extern void		if_write_saved_game (void *, sc_byte *, sc_uint16);
extern sc_uint16	if_read_saved_game (void *, sc_byte *, sc_uint16);
extern void		if_close_saved_game (void *opaque);
extern void		if_display_hints (sc_gamestateref_t gamestate);
extern void		if_update_sound (sc_char *filepath,
				sc_int32 sound_offset,
				sc_int32 sound_length, sc_bool is_looping);
extern void		if_update_graphic (sc_char *filepath,
				sc_int32 graphic_offset,
				sc_int32 graphic_length);

#endif
