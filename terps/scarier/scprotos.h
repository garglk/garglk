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

#include <stddef.h>

#include "scarier.h"

#ifndef SCARIER_PROTOTYPES_H
#define SCARIER_PROTOTYPES_H

/* Runtime version and emulated version, for %version% variable and so on. */
#ifndef SCARIER_VERSION
# define SCARIER_VERSION "1.4.0"
#endif
#ifndef SCARIER_PATCH_LEVEL
# define SCARIER_PATCH_LEVEL ""
#endif
#ifndef SCARIER_EMULATION
# define SCARIER_EMULATION 4046
#endif

/* True and false, unless already defined. */
#ifndef FALSE
# define FALSE 0
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif

/*
 * Fast non-local jumps.
 *
 * The expression evaluator, command parser, and restriction evaluator each arm
 * a setjmp() recovery point on essentially every turn, and the runner arms one
 * per game.  The C standard setjmp()/longjmp() save and restore the process
 * signal mask -- on macOS/BSD via a sigprocmask() (and sigaltstack()) syscall
 * pair on *every* call.  SCARE never alters the signal mask between a setjmp()
 * and its matching longjmp(), so that work is pure waste: profiling the
 * walkthrough corpus showed the kernel mask handling to be the single largest
 * steady-state cost, ahead even of the property-tree hot path.  _setjmp() /
 * _longjmp() (POSIX) give byte-identical control flow but skip the mask, for a
 * measured ~5-13% whole-run speedup with no output change.  Every set/longjmp
 * pair in the engine is file-local, so the swap is self-contained.
 */
#define scr_setjmp(buffer)        _setjmp (buffer)
#define scr_longjmp(buffer, val)  _longjmp ((buffer), (val))

/* Vartype typedef, supports relaxed typing. */
typedef union
{
  scr_int integer;
  scr_bool boolean;
  const scr_char *string;
  scr_char *mutable_string;
  void *voidp;
} scr_vartype_t;

/* Standard reader and writer callback function typedefs. */
typedef scr_int (*scr_read_callbackref_t) (void *, scr_byte *, scr_int);
typedef void (*scr_write_callbackref_t) (void *, const scr_byte *, scr_int);

/*
 * Small utility and wrapper functions.  For printf wrappers, try to apply
 * gcc printf argument checking; this code is cautious about applying the
 * checks.
 */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
extern void scr_trace (const scr_char *format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));
extern void scr_error (const scr_char *format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2)));
extern void scr_fatal (const scr_char *format, ...)
  __attribute__ ((__format__ (__printf__, 1, 2), __noreturn__));
#else
extern void scr_trace (const scr_char *format, ...);
extern void scr_error (const scr_char *format, ...);
extern void scr_fatal (const scr_char *format, ...) __attribute__ ((__noreturn__));
#endif

#ifdef __cplusplus
#include <string>
/*
 * Exception thrown by scr_fatal() instead of abort()-ing the host.  Every
 * engine fatal -- allocation failure, malformed-game consistency check, etc. --
 * raises this, and the public entry points in scinterf.cpp catch it at the host
 * boundary and return a clean failure rather than crashing Spatterlight.  The
 * engine still leaks on this path (raw malloc'd state is not unwound -- see P3),
 * but the whole game session is torn down at the catch site anyway.
 */
struct scr_fatal_error
{
  std::string message;
  explicit scr_fatal_error (const std::string &m) : message (m) {}
};
#endif
extern void *scr_malloc (size_t size);
extern void *scr_realloc (void *pointer, size_t size);
extern void scr_free (void *pointer);

#ifdef __cplusplus
#include <memory>
/*
 * RAII owner for a scr_malloc'd C string.  Frees with scr_free() at scope exit,
 * so an scr_fatal_error / run_loop_halt thrown between acquiring the buffer and
 * the old manual scr_free() no longer leaks it (P3).  This keeps the engine's
 * char* contract intact: .get() hands the raw pointer back to the existing C
 * call sites and to the pointer-aliasing logic that decides which buffer "wins",
 * so callers are otherwise unchanged.
 */
struct scr_free_deleter
{
  void operator () (void *pointer) const { scr_free (pointer); }
};
typedef std::unique_ptr<scr_char, scr_free_deleter> scr_owned_string;
#endif

extern void scr_set_congruential_random (void);
extern void scr_set_platform_random (void);
extern scr_bool scr_is_congruential_random (void);
extern void scr_seed_random (scr_uint new_seed);
extern scr_int scr_rand (void);
extern scr_int scr_randomint (scr_int low, scr_int high);
extern scr_bool scr_strempty (const scr_char *string);
extern scr_char *scr_trim_string (scr_char *string);
extern scr_char *scr_normalize_string (scr_char *string);
extern scr_bool scr_compare_word (const scr_char *string,
                                const scr_char *word, scr_int length);
extern scr_uint scr_hash (const scr_char *string);

/* TAF file reader/decompressor enumerations, opaque typedef and functions. */
enum
{ TAF_VERSION_NONE = 0,
  TAF_VERSION_400 = 400,
  TAF_VERSION_390 = 390,
  TAF_VERSION_380 = 380
};

typedef struct scr_taf_s *scr_tafref_t;
extern void taf_destroy (scr_tafref_t taf);
extern scr_tafref_t taf_create (scr_read_callbackref_t callback, void *opaque);
extern scr_tafref_t taf_create_tas (scr_read_callbackref_t callback,
                                   void *opaque);
extern void taf_first_line (scr_tafref_t taf);
extern const scr_char *taf_next_line (scr_tafref_t taf);
extern scr_bool taf_more_lines (scr_tafref_t taf);
extern scr_int taf_get_game_data_length (scr_tafref_t taf);
extern scr_int taf_get_version (scr_tafref_t taf);
extern scr_bool taf_debug_is_taf_string (scr_tafref_t taf, const void *addr);
extern void taf_debug_dump (scr_tafref_t taf);

/* Properties store enumerations, opaque typedef, and functions. */
enum
{ PROP_KEY_STRING = 's',
  PROP_KEY_INTEGER = 'i'
};
enum
{ PROP_INTEGER = 'I',
  PROP_BOOLEAN = 'B',
  PROP_STRING = 'S'
};

typedef struct scr_prop_set_s *scr_prop_setref_t;
extern scr_prop_setref_t prop_create (const scr_tafref_t taf);
extern void prop_destroy (scr_prop_setref_t bundle);
extern void prop_put (scr_prop_setref_t bundle,
                      const scr_char *format, scr_vartype_t vt_value,
                      const scr_vartype_t vt_key[]);
extern scr_bool prop_get (scr_prop_setref_t bundle,
                         const scr_char *format, scr_vartype_t *vt_value,
                         const scr_vartype_t vt_key[]);
extern void prop_solidify (scr_prop_setref_t bundle);
extern scr_int prop_get_integer (scr_prop_setref_t bundle,
                                const scr_char *format,
                                const scr_vartype_t vt_key[]);
extern scr_bool prop_get_boolean (scr_prop_setref_t bundle,
                                 const scr_char *format,
                                 const scr_vartype_t vt_key[]);
extern const scr_char *prop_get_string (scr_prop_setref_t bundle,
                                       const scr_char *format,
                                       const scr_vartype_t vt_key[]);
extern scr_int prop_get_child_count (scr_prop_setref_t bundle,
                                    const scr_char *format,
                                    const scr_vartype_t vt_key[]);
extern scr_bool prop_put_integer (scr_prop_setref_t bundle,
                                 const scr_char *format,
                                 scr_int value, const scr_vartype_t vt_key[]);
extern scr_bool prop_put_string (scr_prop_setref_t bundle,
                                const scr_char *format,
                                const scr_char *value,
                                const scr_vartype_t vt_key[]);
extern void prop_adopt (scr_prop_setref_t bundle, void *addr);
extern void prop_debug_trace (scr_bool flag);
extern void prop_debug_dump (scr_prop_setref_t bundle);

/* Game parser enumeration and functions. */
enum
{ ROOMLIST_NO_ROOMS = 0,
  ROOMLIST_ONE_ROOM = 1,
  ROOMLIST_SOME_ROOMS = 2,
  ROOMLIST_ALL_ROOMS = 3,
  ROOMLIST_NPC_PART = 4
};

extern scr_bool parse_game (scr_tafref_t taf, scr_prop_setref_t bundle);
extern void parse_debug_trace (scr_bool flag);

/* Game state structure for modules that use it. */
typedef struct scr_game_s *scr_gameref_t;

/* Hint type definition, a thinly disguised pointer to task entry. */
typedef struct scr_taskstate_s *scr_hintref_t;

/* Variables set enumerations, opaque typedef, and functions. */
enum
{ TAFVAR_NUMERIC = 0,
  TAFVAR_STRING = 1
};
enum
{ VAR_INTEGER = 'I',
  VAR_STRING = 'S'
};

typedef struct scr_var_set_s *scr_var_setref_t;
extern void var_put (scr_var_setref_t vars,
                     const scr_char *name, scr_int type, scr_vartype_t vt_value);
extern scr_bool var_get (scr_var_setref_t vars,
                        const scr_char *name, scr_int *type,
                        scr_vartype_t *vt_rvalue);
extern void var_put_integer (scr_var_setref_t vars,
                             const scr_char *name, scr_int value);
extern scr_int var_get_integer (scr_var_setref_t vars, const scr_char *name);
extern void var_put_string (scr_var_setref_t vars,
                            const scr_char *name, const scr_char *string);
extern const scr_char *var_get_string (scr_var_setref_t vars,
                                      const scr_char *name);
extern scr_var_setref_t var_create (scr_prop_setref_t bundle);
extern void var_destroy (scr_var_setref_t vars);
extern void var_register_game (scr_var_setref_t vars, scr_gameref_t game);
extern void var_set_ref_character (scr_var_setref_t vars, scr_int character);
extern void var_set_ref_object (scr_var_setref_t vars, scr_int object);
extern void var_set_ref_number (scr_var_setref_t vars, scr_int number);
extern void var_set_ref_text (scr_var_setref_t vars, const scr_char *text);
extern scr_int var_get_ref_character (scr_var_setref_t vars);
extern scr_int var_get_ref_object (scr_var_setref_t vars);
extern scr_int var_get_ref_number (scr_var_setref_t vars);
extern const scr_char *var_get_ref_text (scr_var_setref_t vars);
extern scr_uint var_get_elapsed_seconds (scr_var_setref_t vars);
extern void var_set_elapsed_seconds (scr_var_setref_t vars, scr_uint seconds);
extern void var_debug_trace (scr_bool flag);
extern void var_debug_dump (scr_var_setref_t vars);

/* Expression evaluation functions. */
extern scr_bool expr_eval_numeric_expression (const scr_char *expression,
                                             scr_var_setref_t vars,
                                             scr_int *rvalue);
extern scr_bool expr_eval_string_expression (const scr_char *expression,
                                            scr_var_setref_t vars,
                                            scr_char **rvalue);

/* Print filtering opaque typedef and functions. */
typedef struct scr_filter_s *scr_filterref_t;
extern scr_filterref_t pf_create (void);
extern void pf_destroy (scr_filterref_t filter);
extern void pf_buffer_string (scr_filterref_t filter,
                              const scr_char *string);
extern void pf_buffer_character (scr_filterref_t filter,
                                 scr_char character);
extern void pf_buffer_paragraph (scr_filterref_t filter,
                                 const scr_char *string);
extern void pf_buffer_paragraph_line (scr_filterref_t filter,
                                      const scr_char *string);
extern void pf_prepend_string (scr_filterref_t filter,
                               const scr_char *string);
extern void pf_new_sentence (scr_filterref_t filter);
extern void pf_mute (scr_filterref_t filter);
extern void pf_clear_mute (scr_filterref_t filter);
extern void pf_buffer_tag (scr_filterref_t filter, scr_int tag);
extern void pf_strip_tags (scr_char *string);
extern void pf_strip_tags_for_hints (scr_char *string);
extern scr_char *pf_filter (const scr_char *string,
                           scr_var_setref_t vars, scr_prop_setref_t bundle);
extern scr_char *pf_filter_for_info (const scr_char *string,
                                    scr_var_setref_t vars);
extern void pf_flush (scr_filterref_t filter,
                      scr_var_setref_t vars, scr_prop_setref_t bundle);
extern void pf_checkpoint (scr_filterref_t filter,
                           scr_var_setref_t vars, scr_prop_setref_t bundle);
extern const scr_char *pf_get_buffer (scr_filterref_t filter);
extern scr_char *pf_transfer_buffer (scr_filterref_t filter);
extern void pf_empty (scr_filterref_t filter);
extern scr_char *pf_escape (const scr_char *string);
extern scr_char *pf_filter_input (const scr_char *string,
                                 scr_prop_setref_t bundle);
extern void pf_debug_trace (scr_bool flag);
extern scr_bool pf_text_ends_with_break (const scr_char *text);

/* Game memo opaque typedef and functions. */
typedef struct scr_memo_set_s *scr_memo_setref_t;
extern scr_memo_setref_t memo_create (void);
extern void memo_destroy (scr_memo_setref_t memento);
extern void memo_save_game (scr_memo_setref_t memento, scr_gameref_t game);
extern scr_bool memo_load_game (scr_memo_setref_t memento, scr_gameref_t game);
extern scr_bool memo_is_load_available (scr_memo_setref_t memento);
extern void memo_clear_games (scr_memo_setref_t memento);
extern scr_int memo_get_undo_count (scr_memo_setref_t memento);
extern const scr_byte *memo_get_undo (scr_memo_setref_t memento,
                                      scr_int index_, scr_int *length);
extern void memo_append_undo (scr_memo_setref_t memento,
                              const scr_byte *data, scr_int length);
extern void memo_save_command (scr_memo_setref_t memento,
                               const scr_char *command, scr_int timestamp,
                               scr_int turns);
extern void memo_unsave_command (scr_memo_setref_t memento);
extern scr_int memo_get_command_count (scr_memo_setref_t memento);
extern void memo_first_command (scr_memo_setref_t memento);
extern void memo_next_command (scr_memo_setref_t memento,
                               const scr_char **command, scr_int *sequence,
                               scr_int *timestamp, scr_int *turns);
extern scr_bool memo_more_commands (scr_memo_setref_t memento);
extern const scr_char *memo_find_command (scr_memo_setref_t memento,
                                         scr_int sequence);
extern void memo_clear_commands (scr_memo_setref_t memento);

/* Game state functions. */
extern scr_gameref_t gs_create (scr_var_setref_t vars, scr_prop_setref_t bundle,
                               scr_filterref_t filter);
extern scr_bool gs_is_game_valid (scr_gameref_t game);
extern void gs_copy (scr_gameref_t to, scr_gameref_t from);
extern void gs_destroy (scr_gameref_t game);

/* Game state accessors and mutators. */
extern void gs_move_player_to_room (scr_gameref_t game, scr_int room);
extern scr_bool gs_player_in_room (scr_gameref_t game, scr_int room);
extern scr_var_setref_t gs_get_vars (scr_gameref_t gs);
extern scr_prop_setref_t gs_get_bundle (scr_gameref_t gs);
extern scr_filterref_t gs_get_filter (scr_gameref_t gs);
extern scr_memo_setref_t gs_get_memento (scr_gameref_t gs);
extern void gs_set_playerroom (scr_gameref_t gs, scr_int room);
extern void gs_set_playerposition (scr_gameref_t gs, scr_int position);
extern void gs_set_playerparent (scr_gameref_t gs, scr_int parent);
extern scr_int gs_playerroom (scr_gameref_t gs);
extern scr_int gs_playerposition (scr_gameref_t gs);
extern scr_int gs_playerparent (scr_gameref_t gs);
extern scr_int gs_carried_weight (scr_gameref_t gs);
extern scr_int gs_carried_size (scr_gameref_t gs);
extern scr_int gs_event_count (scr_gameref_t gs);
extern void gs_set_event_state (scr_gameref_t gs, scr_int event, scr_int state);
extern void gs_set_event_time (scr_gameref_t gs, scr_int event, scr_int etime);
extern scr_int gs_event_state (scr_gameref_t gs, scr_int event);
extern scr_int gs_event_time (scr_gameref_t gs, scr_int event);
extern void gs_decrement_event_time (scr_gameref_t gs, scr_int event);
extern scr_int gs_room_count (scr_gameref_t gs);
extern void gs_set_room_seen (scr_gameref_t gs, scr_int room, scr_bool seen);
extern scr_bool gs_room_seen (scr_gameref_t gs, scr_int room);
extern scr_int gs_task_count (scr_gameref_t gs);
extern void gs_set_task_done (scr_gameref_t gs, scr_int task, scr_bool done);
extern void gs_set_task_scored (scr_gameref_t gs, scr_int task, scr_bool scored);
extern scr_bool gs_task_done (scr_gameref_t gs, scr_int task);
extern scr_bool gs_task_scored (scr_gameref_t gs, scr_int task);
extern scr_int gs_object_count (scr_gameref_t gs);
extern void gs_set_object_openness (scr_gameref_t gs,
                                    scr_int object, scr_int openness);
extern void gs_set_object_state (scr_gameref_t gs, scr_int object, scr_int state);
extern void gs_set_object_seen (scr_gameref_t gs, scr_int object, scr_bool seen);
extern void gs_set_object_unmoved (scr_gameref_t gs,
                                   scr_int object, scr_bool unmoved);
extern void gs_set_object_static_unmoved (scr_gameref_t gs,
                                          scr_int object, scr_bool unmoved);
extern scr_int gs_object_openness (scr_gameref_t gs, scr_int object);
extern scr_int gs_object_state (scr_gameref_t gs, scr_int object);
extern scr_bool gs_object_seen (scr_gameref_t gs, scr_int object);
extern scr_bool gs_object_unmoved (scr_gameref_t gs, scr_int object);
extern scr_bool gs_object_static_unmoved (scr_gameref_t gs, scr_int object);
extern scr_int gs_object_position (scr_gameref_t gs, scr_int object);
extern scr_int gs_object_parent (scr_gameref_t gs, scr_int object);
extern void gs_object_move_onto (scr_gameref_t gs, scr_int object, scr_int onto);
extern void gs_object_move_into (scr_gameref_t gs, scr_int object, scr_int into);
extern void gs_object_make_hidden (scr_gameref_t gs, scr_int object);
extern void gs_object_player_get (scr_gameref_t gs, scr_int object);
extern void gs_object_npc_get (scr_gameref_t gs, scr_int object, scr_int npc);
extern void gs_object_player_wear (scr_gameref_t gs, scr_int object);
extern void gs_object_npc_wear (scr_gameref_t gs, scr_int object, scr_int npc);
extern void gs_object_to_room (scr_gameref_t gs, scr_int object, scr_int room);
extern scr_int gs_npc_count (scr_gameref_t gs);
extern void gs_set_npc_location (scr_gameref_t gs, scr_int npc, scr_int location);
extern scr_int gs_npc_location (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_position (scr_gameref_t gs, scr_int npc, scr_int position);
extern scr_int gs_npc_position (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_parent (scr_gameref_t gs, scr_int npc, scr_int parent);
extern scr_int gs_npc_parent (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_seen (scr_gameref_t gs, scr_int npc, scr_bool seen);
extern scr_bool gs_npc_seen (scr_gameref_t gs, scr_int npc);
extern void gs_set_playerstamina (scr_gameref_t gs, scr_int stamina);
extern scr_int gs_playerstamina (scr_gameref_t gs);
extern void gs_set_playerstaminacounter (scr_gameref_t gs, scr_int counter);
extern scr_int gs_playerstaminacounter (scr_gameref_t gs);
extern void gs_set_playerwield (scr_gameref_t gs, scr_int object);
extern scr_int gs_playerwield (scr_gameref_t gs);
extern void gs_set_npc_stamina (scr_gameref_t gs, scr_int npc, scr_int stamina);
extern scr_int gs_npc_stamina (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_staminacounter (scr_gameref_t gs, scr_int npc,
                                       scr_int counter);
extern scr_int gs_npc_staminacounter (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_attackcounter (scr_gameref_t gs, scr_int npc,
                                      scr_int counter);
extern scr_int gs_npc_attackcounter (scr_gameref_t gs, scr_int npc);
extern struct scr_battle_s *gs_player_battle (scr_gameref_t gs);
extern struct scr_battle_s *gs_npc_battle (scr_gameref_t gs, scr_int npc);
extern scr_int gs_npc_walkstep_count (scr_gameref_t gs, scr_int npc);
extern void gs_set_npc_walkstep (scr_gameref_t gs, scr_int npc,
                                 scr_int walk, scr_int walkstep);
extern scr_int gs_npc_walkstep (scr_gameref_t gs, scr_int npc, scr_int walk);
extern void gs_decrement_npc_walkstep (scr_gameref_t gs,
                                       scr_int npc, scr_int walkstep);
extern void gs_clear_npc_references (scr_gameref_t gs);
extern void gs_clear_object_references (scr_gameref_t gs);
extern void gs_set_multiple_references (scr_gameref_t gs);
extern void gs_clear_multiple_references (scr_gameref_t gs);

/* Pattern matching functions. */
extern scr_bool uip_match (const scr_char *pattern,
                          const scr_char *string, scr_gameref_t game);
extern scr_char *uip_replace_pronouns (scr_gameref_t game, const scr_char *string);
extern void uip_assign_pronouns (scr_gameref_t game, const scr_char *string);
extern void uip_forget_game (const void *game);
extern void uip_debug_trace (scr_bool flag);
extern void task_forget_game (const void *game);
extern void ser_forget_game (const void *game);
extern void evt_forget_game (const void *game);
extern void run_forget_game (const void *game);

/* Library perspective enumeration and functions. */
enum
{ LIB_FIRST_PERSON = 0,
  LIB_SECOND_PERSON = 1,
  LIB_THIRD_PERSON = 2
};

extern void lib_warn_battle_system (void);
extern scr_int lib_random_roomgroup_member (scr_gameref_t game, scr_int roomgroup);
extern const scr_char *lib_get_room_name (scr_gameref_t game, scr_int room);
extern scr_bool lib_can_go (scr_gameref_t game, scr_int room,
                            scr_int direction);
/* The word the parser expects for a direction, for the map's click-to-walk. */
extern const scr_char *lib_direction_name (scr_int direction);
extern void lib_print_room_name (scr_gameref_t game, scr_int room);
extern void lib_print_room_description (scr_gameref_t game, scr_int room);
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
extern scr_bool lib_cmd_go_room (scr_gameref_t game);
extern scr_bool lib_cmd_verbose (scr_gameref_t game);
extern scr_bool lib_cmd_brief (scr_gameref_t game);
extern scr_bool lib_cmd_notify_on_off (scr_gameref_t game);
extern scr_bool lib_cmd_notify (scr_gameref_t game);
extern scr_bool lib_cmd_time (scr_gameref_t game);
extern scr_bool lib_cmd_date (scr_gameref_t game);
extern scr_bool lib_cmd_quit (scr_gameref_t game);
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
extern scr_bool lib_cmd_print_room_exits (scr_gameref_t game);
extern scr_bool lib_cmd_wait (scr_gameref_t game);
extern scr_bool lib_cmd_wait_number (scr_gameref_t game);
extern scr_bool lib_cmd_examine_self (scr_gameref_t game);
extern scr_bool lib_cmd_examine_npc (scr_gameref_t game);
extern scr_bool lib_cmd_examine_object (scr_gameref_t game);
extern scr_bool lib_cmd_count (scr_gameref_t game);
extern scr_bool lib_cmd_take_all (scr_gameref_t game);
extern scr_bool lib_cmd_take_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_take_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_take_all_from (scr_gameref_t game);
extern scr_bool lib_cmd_take_from_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_take_from_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_take_all_from_npc (scr_gameref_t game);
extern scr_bool lib_cmd_take_from_npc_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_take_from_npc_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_take_npc (scr_gameref_t game);
extern scr_bool lib_cmd_drop_all (scr_gameref_t game);
extern scr_bool lib_cmd_drop_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_drop_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_wear_all (scr_gameref_t game);
extern scr_bool lib_cmd_wear_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_wear_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_remove_all (scr_gameref_t game);
extern scr_bool lib_cmd_remove_except_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_remove_multiple (scr_gameref_t game);
extern scr_bool lib_cmd_kiss_npc (scr_gameref_t game);
extern scr_bool lib_cmd_kiss_object (scr_gameref_t game);
extern scr_bool lib_cmd_kiss_other (scr_gameref_t game);
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
extern scr_bool lib_cmd_wield (scr_gameref_t game);
extern scr_bool lib_cmd_unwield (scr_gameref_t game);
extern scr_bool lib_cmd_sit_on_object (scr_gameref_t game);
extern scr_bool lib_cmd_sit_on_floor (scr_gameref_t game);
extern scr_bool lib_cmd_lie_on_object (scr_gameref_t game);
extern scr_bool lib_cmd_lie_on_floor (scr_gameref_t game);
extern scr_bool lib_cmd_get_off_object (scr_gameref_t game);
extern scr_bool lib_cmd_get_off (scr_gameref_t game);
extern scr_bool lib_cmd_save (scr_gameref_t game);
extern scr_bool lib_cmd_restore (scr_gameref_t game);
extern scr_bool lib_cmd_locate_object (scr_gameref_t game);
extern scr_bool lib_cmd_locate_npc (scr_gameref_t game);
extern scr_bool lib_cmd_turns (scr_gameref_t game);
extern scr_bool lib_cmd_score (scr_gameref_t game);
extern scr_bool lib_cmd_status_player (scr_gameref_t game);
extern scr_bool lib_cmd_status_npc (scr_gameref_t game);
extern scr_bool lib_cmd_get_what (scr_gameref_t game);
extern scr_bool lib_cmd_open_what (scr_gameref_t game);
extern scr_bool lib_cmd_close_other (scr_gameref_t game);
extern scr_bool lib_cmd_lock_other (scr_gameref_t game);
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
extern scr_bool lib_cmd_wear_what (scr_gameref_t game);
extern scr_bool lib_cmd_profanity (scr_gameref_t game);
extern scr_bool lib_cmd_examine_all (scr_gameref_t game);
extern scr_bool lib_cmd_examine_other (scr_gameref_t game);
extern scr_bool lib_cmd_locate_other (scr_gameref_t game);
extern scr_bool lib_cmd_unix_like (scr_gameref_t game);
extern scr_bool lib_cmd_dos_like (scr_gameref_t game);
extern scr_bool lib_cmd_ask_object (scr_gameref_t game);
extern scr_bool lib_cmd_ask_npc (scr_gameref_t game);
extern scr_bool lib_cmd_ask_other (scr_gameref_t game);
extern scr_bool lib_cmd_block_object (scr_gameref_t game);
extern scr_bool lib_cmd_block_other (scr_gameref_t game);
extern scr_bool lib_cmd_block_what (scr_gameref_t game);
extern scr_bool lib_cmd_break_object (scr_gameref_t game);
extern scr_bool lib_cmd_break_other (scr_gameref_t game);
extern scr_bool lib_cmd_break_what (scr_gameref_t game);
extern scr_bool lib_cmd_destroy_what (scr_gameref_t game);
extern scr_bool lib_cmd_smash_what (scr_gameref_t game);
extern scr_bool lib_cmd_buy_object (scr_gameref_t game);
extern scr_bool lib_cmd_buy_other (scr_gameref_t game);
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
extern scr_bool lib_cmd_shake_npc (scr_gameref_t game);
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
extern scr_bool lib_cmd_verb_npc (scr_gameref_t game);
extern void lib_debug_trace (scr_bool flag);

/* Resource opaque typedef and control functions. */
typedef struct scr_resource_s *scr_resourceref_t;
extern scr_bool res_has_sound (scr_gameref_t game);
extern scr_bool res_has_graphics (scr_gameref_t game);
extern void res_clear_resource (scr_resourceref_t resource);
extern scr_bool res_compare_resource (scr_resourceref_t from,
                                     scr_resourceref_t with);
extern void res_handle_resource (scr_gameref_t game,
                                 const scr_char *partial_format,
                                 const scr_vartype_t vt_partial[]);
extern void res_sync_resources (scr_gameref_t game);
extern void res_cancel_resources (scr_gameref_t game);

/* Game runner functions. */
extern scr_bool run_game_task_commands (scr_gameref_t game,
                                       const scr_char *string);
extern void run_npc_walk_task (scr_gameref_t game, scr_int walktask);
extern scr_bool run_does_command_match (scr_gameref_t game,
                                       const scr_char *string);
extern scr_gameref_t run_create (scr_read_callbackref_t callback, void *opaque);
extern void run_interpret (scr_gameref_t game);
extern void run_destroy (scr_gameref_t game);
extern void run_restart (scr_gameref_t game);
extern void run_save (scr_gameref_t game,
                      scr_write_callbackref_t callback, void *opaque);
extern scr_bool run_save_prompted (scr_gameref_t game);
extern scr_bool run_restore (scr_gameref_t game,
                            scr_read_callbackref_t callback, void *opaque);
extern scr_bool run_restore_prompted (scr_gameref_t game);
extern scr_bool run_undo (scr_gameref_t game);
extern void run_quit (scr_gameref_t game);
extern scr_bool run_is_running (scr_gameref_t game);
extern scr_bool run_has_completed (scr_gameref_t game);
extern scr_bool run_is_undo_available (scr_gameref_t game);
extern void run_debug_trace (scr_bool flag);
extern void run_get_attributes (scr_gameref_t game,
                                const scr_char **game_name,
                                const scr_char **game_author,
                                const scr_char **game_compile_date,
                                scr_int *turns, scr_int *score,
                                scr_int *max_score,
                                const scr_char **current_room_name,
                                const scr_char **status_line,
                                const scr_char **preferred_font,
                                scr_bool *bold_room_names, scr_bool *verbose,
                                scr_bool *notify_score_change);
extern void run_set_attributes (scr_gameref_t game,
                                scr_bool bold_room_names, scr_bool verbose,
                                scr_bool notify_score_change);
extern scr_hintref_t run_hint_iterate (scr_gameref_t game, scr_hintref_t hint);
extern const scr_char *run_get_hint_question (scr_gameref_t game,
                                             scr_hintref_t hint);
extern const scr_char *run_get_subtle_hint (scr_gameref_t game,
                                           scr_hintref_t hint);
extern const scr_char *run_get_unsubtle_hint (scr_gameref_t game,
                                             scr_hintref_t hint);

/* Event functions. */
extern scr_bool evt_can_see_event (scr_gameref_t game, scr_int event);
extern void evt_tick_events (scr_gameref_t game);
extern void evt_debug_trace (scr_bool flag);

/* Task functions. */
extern scr_bool task_has_hints (scr_gameref_t game, scr_int task);
extern const scr_char *task_get_hint_question (scr_gameref_t game, scr_int task);
extern const scr_char *task_get_hint_subtle (scr_gameref_t game, scr_int task);
extern const scr_char *task_get_hint_unsubtle (scr_gameref_t game, scr_int task);
extern scr_bool task_can_run_task_directional (scr_gameref_t game,
                                              scr_int task, scr_bool forwards);
extern scr_bool task_can_run_task (scr_gameref_t game, scr_int task);
extern scr_bool task_run_task (scr_gameref_t game, scr_int task, scr_bool forwards);
extern void task_debug_trace (scr_bool flag);
extern void task_set_move_assist (scr_bool flag);
extern scr_bool task_get_move_assist (void);

/* Task restriction functions. */
extern scr_bool restr_pass_task_object_state (scr_gameref_t game,
                                             scr_int var1, scr_int var2);
extern scr_bool restr_eval_task_restrictions (scr_gameref_t game,
                                             scr_int task, scr_bool *pass,
                                             const scr_char **fail_message);
extern void restr_debug_trace (scr_bool flag);

/* NPC gender enumeration and functions. */
enum
{ NPC_MALE = 0,
  NPC_FEMALE = 1,
  NPC_NEUTER = 2
};

extern scr_bool npc_in_room (scr_gameref_t game, scr_int npc, scr_int room);
extern scr_int npc_count_in_room (scr_gameref_t game, scr_int room);
extern void npc_setup_initial (scr_gameref_t game);
extern void npc_start_npc_walk (scr_gameref_t game, scr_int npc, scr_int walk);
extern void npc_tick_npcs (scr_gameref_t game);
extern void npc_turn_update (scr_gameref_t game);
extern void npc_debug_trace (scr_bool flag);

/* Battle system functions, in scnpcs.c. */
extern scr_bool battle_is_enabled (scr_gameref_t game);
extern void battle_set_combat_assist (scr_bool flag);
extern scr_bool battle_get_combat_assist (void);
extern scr_int battle_attribute (scr_gameref_t game, scr_int npc,
                                const scr_char *base);
extern scr_int battle_attribute_max (scr_gameref_t game, scr_int npc,
                                    const scr_char *base);
extern void battle_start (scr_gameref_t game);
extern void battle_change_attribute (scr_gameref_t game, scr_int npc,
                                     scr_int attribute, scr_int value);
extern scr_bool battle_is_weapon (scr_gameref_t game, scr_int object);
extern scr_int battle_weapon_method (scr_gameref_t game, scr_int object);
extern scr_int battle_player_default_weapon (scr_gameref_t game);
extern scr_int battle_combatant_weapon (scr_gameref_t game, scr_int npc);
extern void battle_attribute_report (scr_gameref_t game, scr_int npc,
                                     const scr_char *base,
                                     scr_int *lo, scr_int *hi, scr_int *current);
extern void battle_player_attack (scr_gameref_t game, scr_int npc, scr_int weapon);
extern void battle_tick (scr_gameref_t game);

/* Object open/closed state enumeration and functions. */
enum
{ OBJ_WONTCLOSE = 0,
  OBJ_OPEN = 5,
  OBJ_CLOSED = 6,
  OBJ_LOCKED = 7
};

extern scr_bool obj_is_static (scr_gameref_t game, scr_int object);
extern scr_bool obj_is_container (scr_gameref_t game, scr_int object);
extern scr_bool obj_is_surface (scr_gameref_t game, scr_int object);
extern scr_int obj_container_object (scr_gameref_t game, scr_int n);
extern scr_int obj_surface_object (scr_gameref_t game, scr_int n);
extern scr_bool obj_indirectly_in_room (scr_gameref_t game,
                                       scr_int object, scr_int room);
extern scr_bool obj_indirectly_held_by_player (scr_gameref_t game, scr_int object);
extern scr_bool obj_directly_in_room (scr_gameref_t game,
                                     scr_int object, scr_int room);
extern scr_int obj_stateful_object (scr_gameref_t game, scr_int n);
extern scr_int obj_dynamic_object (scr_gameref_t game, scr_int n);
extern scr_int obj_wearable_object (scr_gameref_t game, scr_int n);
extern scr_int obj_standable_object (scr_gameref_t game, scr_int n);
extern scr_int obj_get_size (scr_gameref_t game, scr_int object);
extern scr_int obj_get_weight (scr_gameref_t game, scr_int object);
extern scr_int obj_get_player_size_limit (scr_gameref_t game);
extern scr_int obj_get_player_weight_limit (scr_gameref_t game);
extern scr_int obj_get_container_maxsize (scr_gameref_t game, scr_int object);
extern scr_int obj_get_container_capacity (scr_gameref_t game, scr_int object);
extern scr_int obj_lieable_object (scr_gameref_t game, scr_int n);
extern scr_bool obj_appears_plural (scr_gameref_t game, scr_int object);
extern void obj_setup_initial (scr_gameref_t game);
extern scr_int obj_container_index (scr_gameref_t game, scr_int object);
extern scr_int obj_surface_index (scr_gameref_t game, scr_int object);
extern scr_int obj_stateful_index (scr_gameref_t game, scr_int object);
extern scr_char *obj_state_name (scr_gameref_t game, scr_int object);
extern scr_bool obj_shows_initial_description (scr_gameref_t game, scr_int object);
extern void obj_turn_update (scr_gameref_t game);
extern void obj_debug_trace (scr_bool flag);

/* Game serialization functions. */
extern void ser_set_fast_compression (scr_bool fast);
extern void ser_save_game (scr_gameref_t game,
                           scr_write_callbackref_t callback, void *opaque);
extern scr_bool ser_save_game_prompted (scr_gameref_t game);
extern scr_bool ser_load_game (scr_gameref_t game,
                              scr_read_callbackref_t callback, void *opaque);
extern scr_bool ser_load_game_prompted (scr_gameref_t game);

/* Locale support, and locale-sensitive functions. */
extern void loc_detect_game_locale (scr_prop_setref_t bundle);
extern scr_bool loc_set_locale (const scr_char *name);
extern const scr_char *loc_get_locale (void);
extern scr_bool scr_isspace (scr_char character);
extern scr_bool scr_isdigit (scr_char character);
extern scr_bool scr_isalpha (scr_char character);
extern scr_char scr_toupper (scr_char character);
extern scr_char scr_tolower (scr_char character);
extern void loc_debug_dump (void);

/* Debugger interface. */
typedef struct scr_debugger_s *scr_debuggerref_t;
extern scr_bool debug_run_command (scr_gameref_t game,
                                  const scr_char *debug_command);
extern scr_bool debug_cmd_debugger (scr_gameref_t game);
extern void debug_set_enabled (scr_gameref_t game, scr_bool enable);
extern scr_bool debug_get_enabled (scr_gameref_t game);
extern void debug_game_started (scr_gameref_t game);
extern void debug_game_ended (scr_gameref_t game);
extern void debug_turn_update (scr_gameref_t game);

/* OS interface functions. */
extern scr_bool if_get_trace_flag (scr_uint bitmask);
extern void if_print_string (const scr_char *string);
extern void if_print_debug (const scr_char *string);
extern void if_print_character (scr_char character);
extern void if_print_debug_character (scr_char character);
extern void if_print_tag (scr_int tag, const scr_char *arg);
extern void if_read_line (scr_char *buffer, scr_int length);
extern void if_read_debug (scr_char *buffer, scr_int length);
extern scr_bool if_confirm (scr_int type);
extern void *if_open_saved_game (scr_bool is_save);
extern void if_write_saved_game (void *opaque,
                                 const scr_byte *buffer, scr_int length);
extern scr_int if_read_saved_game (void *opaque,
                                  scr_byte *buffer, scr_int length);
extern void if_close_saved_game (void *opaque);
extern void if_display_hints (scr_gameref_t game);
extern void if_update_sound (const scr_char *filepath,
                             scr_int sound_offset,
                             scr_int sound_length, scr_bool is_looping);
extern void if_update_graphic (const scr_char *filepath,
                               scr_int graphic_offset,
                               scr_int graphic_length);

/* Developer dump/trace tooling (scdump.c); compiled only with SCARIER_DUMP_TOOLS. */
#ifdef SCARIER_DUMP_TOOLS
extern void scr_dump_structure_once (scr_gameref_t game);
extern void scr_dump_npc_trace (scr_gameref_t game);
#endif

#endif
