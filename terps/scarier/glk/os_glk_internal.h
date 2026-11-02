/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

/*
 * os_glk_internal.h: what the os_glk*.cpp translation units share.
 *
 * The Glk front end is split by topic:
 *
 *   os_glk.cpp             module state, utilities, events, files, startup,
 *                          the ADRIFT <=4 main loop
 *   os_glk_locale.cpp      codepages and character output
 *   os_glk_status.cpp      the status line
 *   os_glk_output.cpp      tags, fonts, colour mode, inline graphics, hints
 *   os_glk_symbols.cpp     symbol-font to Unicode tables
 *   os_glk_resources.cpp   ADRIFT <=4 sound/graphics, the title window
 *   os_glk_commands.cpp    the "glk" commands and their help
 *   os_glk_input.cpp       ADRIFT <=4 line input
 *   os_glk_a5.cpp          the ADRIFT 5 driver and its main loop
 *   os_glk_a5_display.cpp  ADRIFT 5 text display and media
 *   os_glk_autosave.cpp    Spatterlight autosave/autorestore
 *   os_glk_map.cpp         the map pane, both engines
 *   os_glk_platform.cpp    per-host linkage: glk_main, the startup entry
 *                          points, opening a game file by path
 *
 * Only the names that cross between them are here -- everything else stays
 * static to its own file.
 */

#ifndef SCARIER_OS_GLK_INTERNAL_H
#define SCARIER_OS_GLK_INTERNAL_H

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "core/scarier.h"

/* The Glk headers and the glkimp host symbols (glk_*, garglk_*, glkunix_*,
 * gli_determinism, win_*) are C; these are C++ translation units, so include
 * them with C linkage.  This also gives the glk_main / glkunix_startup_code /
 * glkunix_arguments definitions in os_glk_platform.cpp C linkage so the C glkimp host
 * can find them (matching the bocfel / Question C++ ports). */
extern "C" {
#include "glk.h"
}

#if defined (SPATTERLIGHT)
/* Spatterlight autosave/autorestore (scarier-autosave.mm).  glkimp.h brings
 * the full window/stream/channel structs -- the autosave carries their
 * serialization tags -- plus gli_determinism; randomness.h the shared
 * erkyrath RNG, whose exact state rides along so deterministic randomness
 * continues across an autorestore. */
extern "C" {
#include "glkimp.h"
#include "randomness.h"
}
#include "platform/scarier-autosave.h"
#endif

/* Blorb resource map, for ADRIFT 5 graphics/sound (the game file is a Blorb),
 * and for Gargoyle's file resources. */
extern "C" {
#include "gi_blorb.h"
}

#include "core/scprotos.h" /* for SCARIER_VERSION */

/* ADRIFT 5 engine -- driven by the dedicated a5 Glk loop (gsc_a5_main) when
 * the game file is detected as ADRIFT 5 rather than ADRIFT <=4. */
#include "adrift5/a5deobf.h"         /* a5_deflate / a5_inflate save framing */
#include "adrift5/a5map.h"           /* the ADRIFT 5 map (Map.vb) */
#include "core/scmap.h"                   /* the ADRIFT 4 map (run400 Form29) */
#include "adrift5/a5model.h"
#include "adrift5/a5parse.h"         /* a5parse_direction_name, for map-click walks */
#include "adrift5/a5restr.h"         /* a5restr_exit_in_direction, for map stub arrows */
#include "adrift5/a5run.h"
#include "adrift5/a5state.h"
#include "adrift5/a5text.h"          /* A5_MEDIA_* kinds */

/*
 * True and false definitions -- usually defined in glkstart.h, but we need
 * them early, so we'll define them here too.
 */
#ifndef FALSE
# define FALSE 0
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif

/* Host feature macros. */

/* Whether this Glk library has the garglk_set_zcolors extension, and with it
   the "glk colour" mode (documented where gsc_set_colour lives); the status
   line draws itself in the game's colours too.  GLK_MODULE_GARGLKTEXT is the
   extension's own feature macro, which brings in remglk and remglk-rs (and
   with it Emglken) as well as Gargoyle itself. */
#if defined(SPATTERLIGHT) || defined(GARGLK) \
    || defined(GLK_MODULE_GARGLKTEXT) \
    || defined(GLK_MODULE_GARGLK_FILE_RESOURCES)
# define GSC_HAVE_ZCOLORS 1
#endif

/* Whether a cover image can be shown as a title screen, a graphics window
   over the whole display (see gsc_show_title_graphic).  The window itself
   needs nothing but core Glk graphics; what varies is where the image comes
   from, so the hosts that can turn a chunk of the game file into a Glk image
   number are exactly the ones that can have it -- Spatterlight through its
   image cache, Gargoyle through garglk_add_resource_from_file. */
#if defined(SPATTERLIGHT) || defined(GLK_MODULE_GARGLK_FILE_RESOURCES)
# define GSC_HAVE_TITLE_WINDOW 1
#endif

/* Whether this Glk library has garglk_unput_string_count_uni(), used to take
   a dangling prompt back off the story window (see gsc_unput_tail).  Both
   Gargoyle and Spatterlight declare it with the rest of the garglk text
   extensions. */
#if defined(SPATTERLIGHT) || defined(GARGLK)
# define GSC_HAVE_UNPUT 1
#endif

/* Types and constants shared across the os_glk*.cpp files. */

/* Unicode values up to 256 are equivalent to iso 8859-1. */
static const glui32 GSC_ISO_8859_EQUIVALENCE = 256;

/* Quote used to suppress abbreviation expansion and local commands. */
static const char GSC_QUOTED_INPUT = '\'';

/* Special out-of-band os_confirm() options used locally with os_glk. */
static const scr_int GSC_CONF_SUBTLE_HINT = INT_MAX,
                    GSC_CONF_UNSUBTLE_HINT = INT_MAX - 1,
                    GSC_CONF_CONTINUE_HINTS = INT_MAX - 2;

/*
 * Symbol fonts.  A <font face="..."> naming one of these selects a font whose
 * bytes are pictograms rather than letters, so the text inside the tag has to
 * be translated to the equivalent Unicode symbols rather than printed as
 * Latin-1 (see gsc_put_string_symbol()).
 */
typedef enum {
  GSC_SYMBOL_NONE = 0,
  GSC_SYMBOL_WEBDINGS,
  GSC_SYMBOL_WINGDINGS,
  GSC_SYMBOL_WINGDINGS3,
  GSC_SYMBOL_SYMBOL
} gsc_symbol_font_t;

/* "No colour set here": outside the 24-bit range an RGB colour occupies, and
   distinct from the zcolor_* sentinels, which are at the top of the range. */
static const glui32 GSC_COLOUR_NONE = 0x01000000;

/* Depth of the font and colour stacks for nesting tags. */
enum { GSC_MAX_STYLE_NESTING = 32 };

/* Number of hints to refuse before offering to end hint display. */
static const scr_int GSC_HINT_REFUSAL_LIMIT = 5;

/*
 * gsc_status_writer_t
 *
 * How a given engine's status line renders text: the number of grid cells a
 * string fills, the call that prints it, and the start of the character after
 * the one at a given point (a byte step for the ADRIFT <=4 codepage strings, a
 * UTF-8 step for the ADRIFT 5 ones).  gsc_status_put_right() needs all three.
 */
typedef struct
{
  glui32 (*width) (const scr_char *string);
  void (*print) (const scr_char *string);
  const scr_char *(*next) (const scr_char *string);
} gsc_status_writer_t;

/* A Glk UNDO, RESTORE, RESTART or QUIT awaiting a safe moment to happen;
   see gsc_command_undo() in os_glk_commands.cpp. */
enum gsc_meta_t
{
  GSC_META_NONE = 0, GSC_META_UNDO, GSC_META_RESTORE,
  GSC_META_RESTART, GSC_META_QUIT
};

/* One Glk sound channel per ADRIFT 5 audio channel; see gsc_a5_channels. */
enum { GSC_A5_MAX_CHANNELS = 9 };

/*
 * What crosses between the os_glk*.cpp files, grouped by the file that
 * defines it.  Everything not named here is static to its own file.
 */

/* os_glk.cpp: module state -- the windows and streams, the options, the
 * running game (whichever engine), the game palette -- and the window,
 * memory and event utilities. */
extern winid_t gsc_main_window;
extern winid_t gsc_status_window;
extern int gsc_main_window_empty;
extern int gsc_sc_silent;
extern int gsc_sc_waitkey_count;
extern int gsc_sc_waitkey_skip;
extern strid_t gsc_transcript_stream;
extern strid_t gsc_inputlog_stream;
extern strid_t gsc_readlog_stream;
extern int gsc_commands_enabled;
extern int gsc_abbreviations_enabled;
extern int gsc_unicode_enabled;
extern glui32 gsc_colour_background;
extern glui32 gsc_colour_output;
extern glui32 gsc_colour_input;
extern int gsc_colour_enabled;
extern int gsc_colour_startup;
extern int gsc_colour_hints_preapplied;
extern glui32 gsc_colour_main_fg;
extern int gsc_normal_measure_state;
extern scr_game gsc_game;
extern char gsc_game_path[2048];
extern a5_adventure_t *gsc_a5_adv;
extern int gsc_is_a5;
extern char gsc_game_key[40];
extern a5_run_t *gsc_a5_run;
extern int gsc_a5_real_time;
extern int gsc_a5_popup_silent;

/* Where a %PopUp...% question is being asked from, the command whose turn
   it is, and the answers that turn (or the opening) has been given so far:
   what an autosave taken at the question records, and what a relaunch plays
   back to reach it again.  See gsc_a5_autosave_popup(). */
enum { GSC_A5_POPUP_ELSEWHERE, GSC_A5_POPUP_BOOT, GSC_A5_POPUP_TURN };
extern int gsc_a5_popup_context;
extern std::string gsc_a5_popup_command;
extern std::vector<std::string> gsc_a5_popup_answers;
extern int gsc_a5_popup_replay;
extern std::string gsc_a5_replay_command;
extern std::vector<std::string> gsc_a5_replay_answers;
extern int gsc_a5_waitkey_count;
extern int gsc_a5_waitkey_skip;
extern winid_t gsc_a5_side_window;

extern scr_bool gsc_normal_measure (glui32 *fg, glui32 *bg);
extern scr_bool gsc_colour_visible (void);
extern void gsc_put_literal (const char *string);
extern void *gsc_malloc (size_t size);
extern void *gsc_realloc (void *pointer, size_t size);
extern void gsc_hint_window_styles (void);
extern void gsc_open_main_window (void);
extern void gsc_open_status_window (void);
extern void gsc_short_delay (void);
extern void gsc_event_wait_2 (glui32 wait_type_1, glui32 wait_type_2,
                              event_t *event);
extern void gsc_event_wait (glui32 wait_type, event_t *event);

#ifdef SPATTERLIGHT
extern int gsc_autorestored;
extern int gsc_in_debug_read;
extern int gsc_autorestore_wanted (void);
#endif

/* os_glk_platform.cpp: a file on disk as a read-only Glk stream, nullptr if
 * the host cannot reach it. */
extern strid_t gsc_open_game_path (const char *path, glui32 rock);

/* os_glk.cpp, for the host entry points in os_glk_platform.cpp: startup once
 * the game stream is open, the 3.7-4.0 main loop, and the complaint startup
 * leaves for it when there is no game to run. */
extern const char *gsc_game_message;
extern int gsc_startup_code (strid_t game_stream, strid_t restore_stream,
                             scr_uint trace_flags, scr_bool enable_debugger,
                             scr_bool stable_random, const scr_char *locale);
extern void gsc_main (void);

/* os_glk_locale.cpp: the game locale and character output. */
extern const scr_bool gsc_has_unicode;
extern scr_bool gsc_main_at_line_start;
extern void gsc_set_locale (const scr_char *name);
extern void gsc_put_string (const scr_char *string);
extern void gsc_put_string_alternate (const scr_char *string);
extern glui32 gsc_status_printed_width (const scr_char *string);
extern scr_int gsc_read_line (scr_char *buffer, scr_int length);

/* os_glk_status.cpp: the status line. */
extern scr_bool gsc_status_begin (glui32 *width);
extern void gsc_status_end (void);
extern void gsc_status_put_right (glui32 width, glui32 room_end,
                                  const scr_char *status,
                                  const gsc_status_writer_t *writer);
extern void gsc_status_notify (void);
extern void gsc_status_redraw (void);

/* os_glk_output.cpp: output styles, colour, inline graphics, hints. */
extern void gsc_reset_glk_style (void);
extern void gsc_normal_char (char c);
extern void gsc_normal_string (const char *message);
extern void gsc_standout_char (char c);
extern void gsc_standout_string (const char *message);
extern void gsc_header_string (const char *message);
extern scr_bool gsc_report_fatal (scr_bool stopping);
extern void gsc_put_prompt (const char *prompt);
extern void gsc_echo_input (void (*put) (const char *), const char *line,
                            scr_bool end_line);
extern char *gsc_copy_string (const char *string);
extern glui32 gsc_colour_lookup (const char *token);
extern void gsc_colour_apply (winid_t win, glui32 fg);
extern void gsc_colour_echo (scr_bool typing);
extern void gsc_handle_wait_tag (const scr_char *argument);
extern int gsc_hint_present (const char *question, const char *subtle,
                             const char *unsubtle);
extern void gsc_note_help_request (const char *command);
extern void gsc_output_provide_help_hint (void);
extern void gsc_output_silence_help_hints (void);
#ifdef GSC_HAVE_ZCOLORS
extern scr_bool gsc_colour_detect (winid_t window);
#endif
#if defined(GLK_MODULE_GARGLK_FILE_RESOURCES) || defined(SPATTERLIGHT)
extern scr_bool gsc_graphic_drawn_since_input;
extern void gsc_draw_inline_graphic (glui32 id);
#endif

/* os_glk_symbols.cpp: symbol-font translation. */
extern gsc_symbol_font_t gsc_symbol_font_from_face (const char *face);
extern void gsc_put_string_symbol (const scr_char *string,
                                   gsc_symbol_font_t symbol_font);

/* os_glk_resources.cpp: ADRIFT <=4 resources and the title window. */
extern int gsc_service_window_event (const event_t *event);
#ifdef GLK_MODULE_GARGLK_FILE_RESOURCES
extern char gsc_gamefile[1024];   /* os_glk.cpp */
#endif
#if defined(GLK_MODULE_GARGLK_FILE_RESOURCES) || defined(SPATTERLIGHT)
extern schanid_t sound_channel;
#endif
#ifdef GSC_HAVE_TITLE_WINDOW
extern winid_t gsc_graphics_window;
extern glui32 gsc_title_image;
extern int gsc_seen_input;
extern int gsc_show_title_graphic (glui32 image);
extern void gsc_close_title_graphic (void);
#endif

/* os_glk_commands.cpp: the "glk" commands. */
extern scr_bool gsc_patches_enabled;
extern gsc_meta_t gsc_meta_pending;
extern int gsc_command_escape (const char *string);
extern void gsc_command_usage (const char *command);
extern void gsc_command_help (const char *command);
extern scr_bool gsc_get_capacity (void);
extern void gsc_set_capacity (scr_bool state);
extern void gsc_colour_startup_prepare (void);
extern void gsc_colour_startup_apply (void);
extern const char *gsc_undo_refusal (scr_int turns);
extern gsc_meta_t gsc_meta_take (void);
extern void gsc_meta_perform (void);
extern void gsc_meta_report (void);

/* os_glk_input.cpp: ADRIFT <=4 line input. */
extern glui32 gsc_readlog_line (char *buffer, glui32 length);
extern scr_char gsc_get_choice_key (const char *choices);

/* os_glk_a5.cpp: the ADRIFT 5 driver. */
extern int gsc_a5_graphics_ok;
extern int gsc_a5_sound_ok;
extern schanid_t gsc_a5_channels[GSC_A5_MAX_CHANNELS];
extern glui32 gsc_a5_chan_sound[GSC_A5_MAX_CHANNELS];
extern void gsc_a5_put_string (const char *string);
extern void gsc_a5_main (void);

/* os_glk_a5_display.cpp: ADRIFT 5 text display and media. */
extern void gsc_a5_display (const char *text);
extern winid_t gsc_a5_open_side_window (void);
extern int gsc_a5_show_media (a5_run_t *run);
extern void gsc_a5_sound_finished (const event_t *event);
extern void gsc_a5_media_fire (a5_run_t *run, int idx);
extern void gsc_a5_present_intro_media (a5_run_t *run);
extern void gsc_a5_undo_look (a5_run_t *run);
extern void gsc_a5_status (a5_run_t *run);

#ifdef SPATTERLIGHT
/* os_glk_resources.cpp: load a media file from beside the game. */
extern glui32 gsc_load_external_resource (const char *filepath, int is_sound);

/* os_glk_autosave.cpp: Spatterlight autosave and autorestore. */
extern void gsc_autosave (void);
extern void gsc_a5_note_turn_start (void);
extern void gsc_a5_autosave_popup (void);
extern void gsc_a5_autosave_waitkey (void);
extern bool gsc_a5_autosave_at_boot_popup (void);
extern void gsc_sc_note_turn_start (void);
extern void gsc_sc_note_command (const scr_char *line);
extern void gsc_sc_note_mid_turn_input (void);
extern bool gsc_sc_take_replay_command (scr_char *buffer, scr_int length);
extern bool gsc_sc_boot_replay (void);
extern void gsc_sc_autosave_waitkey (void);
extern bool gsc_sc_apply_all (const std::string &data);
extern bool gsc_a5_apply_all (const std::string &data);
extern void gsc_autorestore_replace_state (bool (*apply)
                                           (const std::string &data));
#endif

/* os_glk_map.cpp: the map pane's state, read by the event loops, the
 * autosave and the "glk" command table. */
extern winid_t gsc_map_window;
extern map_t *gsc_map;
extern int gsc_map_shown;
extern int gsc_map_want;
extern int gsc_map_at_top;
extern int gsc_map_taken;
extern int gsc_map_zoom;
extern map_camera_t gsc_map_cam;
extern int gsc_map_follow;
extern char gsc_map_last_player[256];
extern int gsc_map_full_flush;
extern char gsc_a5_walk_to[256];
extern int gsc_a5_walk_clicked;
extern int gsc_sc_walk_clicked;
extern int gsc_line_echo_manual;

extern void gsc_map_screen_drop (void);
extern void gsc_a5_walk_stop (void);
extern int gsc_map_click (event_t *event);
extern void gsc_map_click_view (event_t *event);
extern void gsc_a5_map_names_clear (void);
extern void gsc_map_redraw (void);
extern int gsc_sc_walk_next (scr_char *buffer, scr_int length);
extern int gsc_a5_walk_next (a5_run_t *run, char *buf, int bufsize);
extern int gsc_map_default_shown (void);
extern int gsc_map_pref_read (int *at_top);
extern void gsc_map_show (void);
extern void gsc_map_hide (void);
extern void gsc_map_toggle (void);
extern void gsc_map_auto_reveal (void);
extern void gsc_map_notice_restart (void);
extern void gsc_command_map (const char *argument);
extern void gsc_command_zoom (const char *argument);

#endif
