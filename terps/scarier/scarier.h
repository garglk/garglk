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

#include <stdio.h>

#ifndef SCARIER_H
#define SCARIER_H

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * Base type definitions.  SCARIER integer types need to be at least 32 bits,
 * so using long here is a good bet for almost all ANSI C implementations for
 * 32 and 64 bit platforms; maybe also for any 16 bit ones.  For 64 bit
 * platforms configured for LP64, SCARIER integer types will consume more space
 * in data structures.  Values won't wrap identically to 32 bit ones, but
 * games shouldn't be relying on wrapping anyway.  One final note -- in several
 * places, SCARIER allocates 32 bytes into which it will sprintf() a long; this
 * is fine for both standard 32 bit and LP64 64 bit platforms, but is unsafe
 * should SCARIER ever be configured for 128 bit definitions of scr_[u]int.
 */
typedef char scr_char;
typedef unsigned char scr_byte;
typedef long scr_int;
typedef unsigned long scr_uint;
typedef int scr_bool;

/* Enumerated confirmation types, passed to os_confirm(). */
enum
{ SCR_CONF_QUIT = 0,
  SCR_CONF_RESTART, SCR_CONF_SAVE, SCR_CONF_RESTORE, SCR_CONF_VIEW_HINTS
};

/* HTML-like tag enumerated values, passed to os_print_tag(). */
enum
{ SCR_TAG_UNKNOWN = 0, SCR_TAG_ITALICS, SCR_TAG_ENDITALICS, SCR_TAG_BOLD,
  SCR_TAG_ENDBOLD, SCR_TAG_UNDERLINE, SCR_TAG_ENDUNDERLINE, SCR_TAG_COLOR,
  SCR_TAG_ENDCOLOR, SCR_TAG_FONT, SCR_TAG_ENDFONT, SCR_TAG_BGCOLOR, SCR_TAG_CENTER,
  SCR_TAG_ENDCENTER, SCR_TAG_RIGHT, SCR_TAG_ENDRIGHT, SCR_TAG_WAIT, SCR_TAG_WAITKEY,
  SCR_TAG_CLS,

  /* British spelling equivalents. */
  SCR_TAG_COLOUR = SCR_TAG_COLOR,
  SCR_TAG_ENDCOLOUR = SCR_TAG_ENDCOLOR,
  SCR_TAG_BGCOLOUR = SCR_TAG_BGCOLOR,
  SCR_TAG_CENTRE = SCR_TAG_CENTER,
  SCR_TAG_ENDCENTRE = SCR_TAG_ENDCENTER
};

/* OS interface function prototypes; interpreters must define these. */
typedef void *scr_game;
extern void os_print_string (const scr_char *string);
extern void os_print_tag (scr_int tag, const scr_char *argument);
extern void os_play_sound (const scr_char *filepath,
                           scr_int offset, scr_int length, scr_bool is_looping);
extern void os_stop_sound (void);
extern void os_show_graphic (const scr_char *filepath,
                             scr_int offset, scr_int length);
extern scr_bool os_read_line (scr_char *buffer, scr_int length);
extern scr_bool os_confirm (scr_int type);
extern void *os_open_file (scr_bool is_save);
extern void os_write_file (void *opaque, const scr_byte *buffer, scr_int length);
extern scr_int os_read_file (void *opaque, scr_byte *buffer, scr_int length);
extern void os_close_file (void *opaque);
extern void os_display_hints (scr_game game);

extern void os_print_string_debug (const scr_char *string);
extern scr_bool os_read_line_debug (scr_char *buffer, scr_int length);

/* Interpreter trace flag bits, passed to scr_set_trace_flags(). */
enum
{ SCR_TRACE_PARSE = 1, SCR_TRACE_PROPERTIES = 2, SCR_TRACE_VARIABLES = 4,
  SCR_TRACE_PARSER = 8, SCR_TRACE_LIBRARY = 16, SCR_TRACE_EVENTS = 32,
  SCR_TRACE_NPCS = 64, SCR_TRACE_OBJECTS = 128, SCR_TRACE_TASKS = 256,
  SCR_TRACE_PRINTFILTER = 512,

  SCR_DUMP_TAF = 1024, SCR_DUMP_PROPERTIES = 2048, SCR_DUMP_VARIABLES = 4096,
  SCR_DUMP_PARSER_TREES = 8192, SCR_DUMP_LOCALE_TABLES = 16384
};

/* Module-wide trace control function prototype. */
extern void scr_set_trace_flags (scr_uint trace_flags);

/* Interpreter interface function prototypes. */
extern scr_game scr_game_from_filename (const scr_char *filename);
extern scr_game scr_game_from_stream (FILE *stream);
extern scr_game scr_game_from_callback (scr_int (*callback)
                                      (void *, scr_byte *, scr_int),
                                      void *opaque);
extern void scr_interpret_game (scr_game game);
extern void scr_restart_game (scr_game game);
extern scr_bool scr_save_game (scr_game game);
extern scr_bool scr_load_game (scr_game game);
extern scr_bool scr_undo_game_turn (scr_game game);
extern void scr_quit_game (scr_game game);
extern scr_bool scr_save_game_to_filename (scr_game game, const scr_char *filename);
extern void scr_save_game_to_stream (scr_game game, FILE *stream);
extern void scr_save_game_to_callback (scr_game game,
                                      void (*callback)
                                      (void *, const scr_byte *, scr_int),
                                      void *opaque);
extern scr_bool scr_load_game_from_filename (scr_game game,
                                           const scr_char *filename);
extern scr_bool scr_load_game_from_stream (scr_game game, FILE *stream);
extern scr_bool scr_save_undo_game_to_callback (scr_game game,
                                       void (*callback)
                                       (void *, const scr_byte *, scr_int),
                                       void *opaque);
extern scr_bool scr_load_undo_game_from_callback (scr_game game,
                                       scr_int (*callback)
                                       (void *, scr_byte *, scr_int),
                                       void *opaque);
extern void scr_note_resources_synced (scr_game game);
extern void scr_note_autorestored (void);
extern scr_bool scr_load_game_from_callback (scr_game game,
                                           scr_int (*callback)
                                           (void *, scr_byte *, scr_int),
                                           void *opaque);
extern void scr_free_game (scr_game game);
extern scr_bool scr_is_game_running (scr_game game);
extern scr_bool scr_does_command_match (scr_game game, const scr_char *string);
extern const scr_char *scr_get_game_name (scr_game game);
extern const scr_char *scr_get_game_author (scr_game game);
extern const scr_char *scr_get_game_compile_date (scr_game game);
extern scr_int scr_get_game_turns (scr_game game);
extern scr_int scr_get_game_score (scr_game game);
extern scr_int scr_get_game_max_score (scr_game game);
extern const scr_char *scr_get_game_room (scr_game game);
extern const scr_char *scr_get_game_status_line (scr_game game);
extern const scr_char *scr_get_game_preferred_font (scr_game game);
extern scr_bool scr_get_game_bold_room_names (scr_game game);
extern scr_bool scr_get_game_verbose (scr_game game);
extern scr_bool scr_get_game_notify_score_change (scr_game game);
extern scr_bool scr_has_game_completed (scr_game game);
extern scr_bool scr_is_game_undo_available (scr_game game);
extern void scr_set_game_bold_room_names (scr_game game, scr_bool flag);
extern void scr_set_game_verbose (scr_game game, scr_bool flag);
extern void scr_set_game_notify_score_change (scr_game game, scr_bool flag);

extern scr_bool scr_get_game_capacity_recompute (scr_game game);
extern void scr_set_game_capacity_recompute (scr_game game, scr_bool flag);

extern scr_bool scr_does_game_use_sounds (scr_game);
extern scr_bool scr_does_game_use_graphics (scr_game);

typedef void *scr_game_hint;
extern scr_game_hint scr_get_first_game_hint (scr_game game);
extern scr_game_hint scr_get_next_game_hint (scr_game game, scr_game_hint hint);
extern const scr_char *scr_get_game_hint_question (scr_game game,
                                                 scr_game_hint hint);
extern const scr_char *scr_get_game_subtle_hint (scr_game game,
                                               scr_game_hint hint);
extern const scr_char *scr_get_game_unsubtle_hint (scr_game game,
                                                 scr_game_hint hint);

extern void scr_set_game_debugger_enabled (scr_game game, scr_bool flag);
extern scr_bool scr_get_game_debugger_enabled (scr_game game);
extern scr_bool scr_run_game_debugger_command (scr_game game,
                                             const scr_char *debug_command);
extern void scr_set_portable_random (scr_bool flag);
extern void scr_reseed_random_sequence (scr_uint new_seed);
extern void scr_set_combat_assist (scr_bool flag);
extern scr_bool scr_get_combat_assist (void);
extern void scr_set_move_assist (scr_bool flag);
extern scr_bool scr_get_move_assist (void);

/* Locale control and query functions. */
extern scr_bool scr_set_locale (const scr_char *name);
extern const scr_char *scr_get_locale (void);

/* A few possibly useful utilities. */
extern scr_int scr_strncasecmp (const scr_char *s1, const scr_char *s2, scr_int n);
extern scr_int scr_strcasecmp (const scr_char *s1, const scr_char *s2);
extern const scr_char *scr_scarier_version (void);
extern scr_int scr_scarier_emulation (void);

#if defined(__cplusplus)
}
#endif

#endif
