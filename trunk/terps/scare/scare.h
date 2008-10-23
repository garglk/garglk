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

#ifndef SCARE_H
#define SCARE_H

/* Enable use with C++. */
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Base type definitions. */
typedef	char		sc_char;
typedef	unsigned char	sc_byte;
typedef	short		sc_int16;
typedef	unsigned short	sc_uint16;
typedef	long		sc_int32;
typedef	unsigned long	sc_uint32;
typedef unsigned short	sc_bool;

/* Enumerated confirmation types, passed to os_confirm(). */
enum {			SC_CONF_QUIT,		SC_CONF_RESTART,
			SC_CONF_SAVE,		SC_CONF_RESTORE,
			SC_CONF_VIEW_HINTS };

/* OS interface function prototypes. */
typedef void		*sc_game;
extern void		os_print_string (const sc_char *string);
extern void		os_print_tag (sc_uint32 tag, const sc_char *arg);
extern void		os_flush (void);
extern void		os_play_sound (sc_char *filepath,
				sc_int32 offset, sc_int32 length,
				sc_bool is_looping);
extern void		os_stop_sound (void);
extern void		os_show_graphic (sc_char *filepath,
				sc_int32 offset, sc_int32 length);
extern sc_bool		os_read_line (sc_char *buffer, sc_uint16 length);
extern sc_bool		os_confirm (sc_uint32 type);
extern void		*os_open_file (sc_bool is_save);
extern void		os_write_file (void *opaque, sc_byte *buffer,
				sc_uint16 length);
extern sc_uint16	os_read_file (void *opaque, sc_byte *buffer,
				sc_uint16 length);
extern void		os_close_file (void *opaque);
extern void		os_display_hints (sc_game game);

extern void		os_print_string_debug (const sc_char *string);
extern sc_bool		os_read_line_debug (sc_char *buffer, sc_uint16 length);

/* Interpreter interface function prototypes. */
extern sc_game		sc_game_from_filename (const sc_char *filename,
				sc_uint32 trace_flags);
extern sc_game		sc_game_from_stream (FILE *stream,
				sc_uint32 trace_flags);
extern sc_game		sc_game_from_callback
				(sc_uint16 (*callback)
						(void *, sc_byte *, sc_uint16),
				void *opaque,
				sc_uint32 trace_flags);
extern void		sc_interpret_game (sc_game game);
extern void		sc_restart_game (sc_game game);
extern sc_bool		sc_save_game (sc_game game);
extern sc_bool		sc_load_game (sc_game game);
extern sc_bool		sc_undo_game_turn (sc_game game);
extern void		sc_quit_game (sc_game game);
extern void		sc_free_game (sc_game game);
extern sc_bool		sc_is_game_running (sc_game game);
extern sc_char		*sc_get_game_name (sc_game game);
extern sc_char		*sc_get_game_author (sc_game game);
extern sc_uint32	sc_get_game_turns (sc_game game);
extern sc_int32		sc_get_game_score (sc_game game);
extern sc_int32		sc_get_game_max_score (sc_game game);
extern sc_char		*sc_get_game_room (sc_game game);
extern sc_char		*sc_get_game_status_line (sc_game game);
extern sc_char		*sc_get_game_preferred_font (sc_game game);
extern sc_bool		sc_get_game_bold_room_names (sc_game game);
extern sc_bool		sc_get_game_verbose (sc_game game);
extern sc_bool		sc_get_game_notify_score_change (sc_game game);
extern sc_bool		sc_has_game_completed (sc_game game);
extern sc_bool		sc_is_game_undo_available (sc_game game);
extern void		sc_set_game_bold_room_names (sc_game game,
				sc_bool flag);
extern void		sc_set_game_verbose (sc_game game, sc_bool flag);
extern void		sc_set_game_notify_score_change (sc_game game,
				sc_bool flag);

extern sc_bool		sc_does_game_use_sounds (sc_game);
extern sc_bool		sc_does_game_use_graphics (sc_game);

typedef void		*sc_game_hint;
extern sc_game_hint	sc_iterate_game_hints (sc_game game, sc_game_hint hint);
extern sc_char		*sc_get_game_hint_question (sc_game game,
				sc_game_hint hint);
extern sc_char		*sc_get_game_subtle_hint (sc_game game,
				sc_game_hint hint);
extern sc_char		*sc_get_game_unsubtle_hint (sc_game game,
				sc_game_hint hint);

extern void		sc_set_game_debugger_enabled (sc_game game,
				sc_bool flag);
extern sc_bool		sc_get_game_debugger_enabled (sc_game game);
extern sc_bool		sc_run_game_debugger_command (sc_game game,
				const sc_char *debug_command);

/* A few possibly useful utilities. */
extern sc_int32		sc_strncasecmp (const sc_char *s1, const sc_char *s2,
				sc_int32 n);
extern sc_int32		sc_strcasecmp (const sc_char *s1, const sc_char *s2);
extern const sc_char	*sc_scare_version (void);
extern sc_uint32	sc_scare_emulation (void);

/* HTML-like tag enumerated values, passed to os_print_tag(). */
enum {			SC_TAG_ITALICS,		SC_TAG_ENDITALICS,
			SC_TAG_BOLD,		SC_TAG_ENDBOLD,
			SC_TAG_UNDERLINE,	SC_TAG_ENDUNDERLINE,
			SC_TAG_SCOLOR,		SC_TAG_ENDSCOLOR,
			SC_TAG_FONT,		SC_TAG_ENDFONT,
			SC_TAG_BGCOLOR,
			SC_TAG_CENTER,		SC_TAG_ENDCENTER,
			SC_TAG_RIGHT,		SC_TAG_ENDRIGHT,
			SC_TAG_WAIT,
			SC_TAG_WAITKEY,
			SC_TAG_CLS,
			SC_TAG_UNKNOWN };

/* British-spelling equivalents for HTML-like tag enumerations. */
enum {			SC_TAG_SCOLOUR		= SC_TAG_SCOLOR,
			SC_TAG_ENDSCOLOUR	= SC_TAG_ENDSCOLOR,
			SC_TAG_BGCOLOUR		= SC_TAG_BGCOLOR,
			SC_TAG_CENTRE		= SC_TAG_CENTER,
			SC_TAG_ENDCENTRE	= SC_TAG_ENDCENTER };

/* Interpreter trace flag bits, passed to sc_interpret_game(). */
enum {			SC_TR_TRACE_PARSE	= 1 << 0,
			SC_TR_TRACE_PROPERTIES	= 1 << 1,
			SC_TR_TRACE_VARIABLES	= 1 << 2,
			SC_TR_TRACE_PARSER	= 1 << 3,
			SC_TR_TRACE_LIBRARY	= 1 << 4,
			SC_TR_TRACE_EVENTS	= 1 << 5,
			SC_TR_TRACE_NPCS	= 1 << 6,
			SC_TR_TRACE_OBJECTS	= 1 << 7,
			SC_TR_TRACE_TASKS	= 1 << 8,
			SC_TR_TRACE_PRINTFILTER	= 1 << 9,
			SC_TR_DUMP_TAF		= 1 << 10,
			SC_TR_DUMP_PROPERTIES	= 1 << 11,
			SC_TR_DUMP_VARIABLES	= 1 << 12 };

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif
