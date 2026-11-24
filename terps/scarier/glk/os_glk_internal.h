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
 *   os_glk_styles.cpp      the style census and assignment (no Glk calls;
 *                          its own header, os_glk_styles.h)
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
 * glkunix_arguments definitions in os_glk_platform.cpp C linkage so the C
 * glkimp host can find them (matching the bocfel / Question C++ ports). */
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
#include "os_glk_styles.h"           /* the game's style assignment */

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
 * UTF-8 step for the ADRIFT 5 ones).  gsc_status_draw() needs all three.
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

/* One Glk sound channel per ADRIFT 5 audio channel; see
   gsc_a5_media.channels. */
enum { GSC_A5_MAX_CHANNELS = 9 };

/*
 * What crosses between the os_glk*.cpp files, grouped by the file that
 * defines it.  Everything not named here is static to its own file.
 */

/* os_glk.cpp: module state -- the windows and streams, the options, the
 * running game (whichever engine), the game palette -- and the window,
 * memory and event utilities. */

/* The story windows and the transcript, input log and read log streams. */
struct gsc_io_s
{
  /* Two windows, one for the main text, and one for a status line. */
  winid_t main_window = nullptr;
  winid_t status_window = nullptr;

  /*
   * Whether nothing has been printed to the main window since it was last
   * cleared.  A window that is still empty can be cleared again for free,
   * which is the one moment a background colour change can repaint the pane
   * without costing the player any text; see SCR_TAG_BGCOLOUR.
   */
  int main_window_empty = TRUE;

  /*
   * Transcript stream and input log.  These are NULL if there is no current
   * collection of these strings.
   */
  strid_t transcript_stream = nullptr;
  strid_t inputlog_stream = nullptr;

  /* Input read log stream, for reading back an input log. */
  strid_t readlog_stream = nullptr;
};
typedef struct gsc_io_s gsc_io_t;

extern gsc_io_t gsc_io;

/* Options that may be turned off or set by command line flags. */
struct gsc_options_s
{
  int commands_enabled = TRUE;
  int abbreviations_enabled = TRUE;
  int unicode_enabled = TRUE;
};
typedef struct gsc_options_s gsc_options_t;

extern gsc_options_t gsc_options;

/* The game palette, colour mode, and the story window's measured colours. */
struct gsc_colour_s
{
  /*
   * ADRIFT <=4 Runner default palette; the ADRIFT 5 loop overwrites these
   * from the adventure when it loads one.  See gsc_set_colour for where the
   * numbers come from.
   */
  glui32 background = 0x000000;
  glui32 output = 0x00ff00;
  glui32 input = 0xff3232;

  /*
   * Whether the game's own palette is in force ("glk colour", set by
   * gsc_set_colour in os_glk_commands.cpp).  The status line picks its style
   * by it too; where the Glk library has no zcolors extension the command is
   * not offered and this stays FALSE.
   */
  int enabled = FALSE;

  /*
   * Whether colour mode should be on before the game prints a word: the "-c"
   * command line switch, sparing the player a "glk colour on" at every
   * launch, or a game that asks for its own palette by the colours it was
   * written in (see gsc_colour_detect).  Acted on once the windows exist; see
   * gsc_colour_startup_apply.
   */
  int startup = FALSE;

  /*
   * True when Normal colour stylehints were already set before the first
   * window open (the -c path, and the detected one).  gsc_set_colour consumes
   * this so it can skip a pointless rebuild of the just-opened tree.
   */
  int hints_preapplied = FALSE;

  /*
   * The colour last named on the story window (gsc_colour_apply), so that
   * drawing the status bar in its own two colours can put the story's back
   * afterwards -- a library whose zcolors are global as well as per-stream
   * would otherwise go on painting the story window in the bar's colours.
   * See gsc_status_end.  Only read while colour mode is on, and
   * gsc_set_colour colours the story window before it turns the mode on, so
   * it is always set by then.
   */
  glui32 main_fg = 0;

  /*
   * Cached measurement of the story window's style_Normal colours: -1 for not
   * yet measured, 0 for a library that cannot measure, 1 for a filled cache
   * (the colours themselves are file-static in os_glk.cpp).  Measuring is not
   * free -- under Spatterlight every glk_style_measure call is a window flush
   * plus a synchronous round-trip to the application process -- and the
   * consumers ask often: gsc_colour_visible once per styled fragment of
   * output, the map redraw once per prompt while its pane is open.  The
   * answer only changes when colour mode itself toggles (gsc_set_colour,
   * which also rebuilds the window tree) or when the library's style
   * preference flips, and the latter reaches us as an Arrange event; those
   * two places reset the state to -1.
   */
  int normal_measure_state = -1;
};
typedef struct gsc_colour_s gsc_colour_t;

extern gsc_colour_t gsc_colour;
extern scr_game gsc_game;
extern char gsc_game_path[2048];
extern a5_adventure_t *gsc_a5_adv;
extern int gsc_is_a5;
extern char gsc_game_key[40];
extern a5_run_t *gsc_a5_run;
extern int gsc_a5_real_time;
extern winid_t gsc_a5_side_window;

enum { GSC_A5_POPUP_ELSEWHERE, GSC_A5_POPUP_BOOT, GSC_A5_POPUP_TURN };

/*
 * What a relaunch replays to get back to the <waitkey> pause or %PopUp...%
 * question the game was closed at, and what an autosave taken there
 * records so that it can.
 */
struct gsc_replay_s
{
  /*
   * ADRIFT <=4, set while a relaunch runs a turn (or the opening) again to
   * get back to the <waitkey> pause the game was closed at: the text,
   * pictures and sounds on the way there are already on the restored
   * screen, so none of it is put out a second time.  The pause is the
   * sc_waitkey_skip'th since the last line was read, sc_waitkey_count being
   * how many there have been.  See gsc_sc_autosave_waitkey().
   */
  int sc_silent = FALSE;
  int sc_waitkey_count = 0;
  int sc_waitkey_skip = 0;

  /*
   * Set while the opening is replayed only to reach a saved state (the
   * Spatterlight autorestore boot), so a %PopUpInput% naming prompt in it is
   * answered with its default instead of asking the player again.
   */
  int a5_popup_silent = FALSE;

  /*
   * Where a %PopUp...% question is being asked from (a GSC_A5_POPUP_*), the
   * command whose turn it is, and the answers that turn (or the opening) has
   * been given so far: what an autosave taken at the question records, and
   * what a relaunch plays back to reach it again.  See
   * gsc_a5_autosave_popup().
   */
  int a5_popup_context = GSC_A5_POPUP_ELSEWHERE;
  std::string a5_popup_command;
  std::vector<std::string> a5_popup_answers;

  /*
   * The context an autorestore is playing back to a question, if any, with
   * the command to run again and the answers still to be given.
   */
  int a5_popup_replay = GSC_A5_POPUP_ELSEWHERE;
  std::string a5_replay_command;
  std::vector<std::string> a5_replay_answers;

  /*
   * How many <waitkey> pauses the display of the turn in hand (or of the
   * opening) has reached, and how many of them an autorestore's replay
   * passes in silence: a game closed at one comes back at it.  See
   * gsc_a5_autosave_waitkey().
   */
  int a5_waitkey_count = 0;
  int a5_waitkey_skip = 0;
};
typedef struct gsc_replay_s gsc_replay_t;

extern gsc_replay_t gsc_replay;

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
extern void gsc_status_draw (const scr_char *room, const scr_char *status,
                             const gsc_status_writer_t *writer);
extern void gsc_status_notify (void);
extern void gsc_status_redraw (void);

/* os_glk_output.cpp: output styles, colour, inline graphics, hints. */
extern void gsc_reset_glk_style (void);
extern void gsc_styles_census (scr_game game);
extern void gsc_styles_census_a5 (const a5_adventure_t *adventure);
extern const gsc_style_table_t *gsc_styles_table (void);
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
/*
 * The title screen (os_glk_resources.cpp): the graphics window a tall cover
 * fills while it is up, the cover's resource number, and whether the player
 * has given any input yet, which takes the title screen down.
 */
struct gsc_title_s
{
  winid_t window = nullptr;
  glui32 image = 0;
  int seen_input = FALSE;
};
typedef struct gsc_title_s gsc_title_t;

extern gsc_title_t gsc_title;
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

/* ADRIFT 5 graphics and sound. */
struct gsc_a5_media_s
{
  /* Whether the library can draw images and play sounds. */
  int graphics_ok = FALSE;
  int sound_ok = FALSE;

  /* Whether the game is a Blorb registered as the resource map. */
  int have_blorb = FALSE;

  /*
   * One Glk sound channel per ADRIFT audio channel.  The Runner has exactly 8
   * (clsSound.vb: Channels(7), numbered 1..8 in the <audio> tag; anything out
   * of that range is a no-op); slot 0 here is simply never used.
   */
  schanid_t channels[GSC_A5_MAX_CHANNELS] = {};

  /*
   * The resource playing on each channel, so a repeated play of the same
   * sound leaves it alone rather than restarting it (see gsc_a5_show_media).
   * It is cleared when the sound finishes (see gsc_a5_sound_finished).
   */
  glui32 chan_sound[GSC_A5_MAX_CHANNELS] = {};
};
typedef struct gsc_a5_media_s gsc_a5_media_t;

extern gsc_a5_media_t gsc_a5_media;
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

/*
 * os_glk_map.cpp: the map pane's state, read by the event loops, the
 * autosave and the "glk" command table.
 *
 * The map is shown in a graphics window to the right of the story on request
 * ("map").  Both engines use this pane; what differs is where the map comes
 * from.  An ADRIFT 5 map is authored data, parsed once per adventure, aliasing
 * its XML, and so dropped whenever the adventure is.  An ADRIFT 4 map has to
 * be derived from the exit graph, and it depends on the room you are standing
 * in and on how much you have explored -- so it is rebuilt on every redraw, as
 * the ADRIFT 4 Runner rebuilt it every time it drew (drawmap with mode = 1).
 */
struct gsc_map_pane_s
{
  winid_t window = nullptr;
  map_t *map = nullptr;
  int shown = FALSE;

  /*
   * Whether the map pane is wanted at all: the player's remembered choice, or
   * failing that the layout the game shipped (gsc_map_auto_reveal).  Wanted
   * is not the same as shown -- a map with nothing on it yet is held back
   * until there is something to see (gsc_map_worth_opening), and the per-turn
   * redraw opens it then.  FALSE until something decides.
   */
  int want = FALSE;

  /*
   * Where the map pane sits: the standard 40% pane to the right of the story,
   * or -- for games with wide maps -- a 30% band across the top of the whole
   * screen, above the status line ("glk map top").  The choice is kept, so
   * hiding and re-showing the map does not move it, and it is remembered with
   * the visibility (gsc_map_pref_write) so neither does restarting.
   */
  int at_top = FALSE;

  /*
   * Set by "-m" or SCR_MAP_ENABLED: start every game with the map showing,
   * whatever the player last chose or the game shipped (gsc_map_pref_read).
   */
  int startup = FALSE;

  /*
   * Set when the game defines a MAP command of its own (Lost Coastlines has a
   * sea chart): the game's command wins, and the pane is reached with the
   * "glk map" escape instead.
   */
  int taken = FALSE;

  /*
   * A manual zoom ("glk zoom in/out/N", or the zoom buttons), as pixels per
   * map unit; 0 while the map is fitting itself to its window ("glk zoom
   * auto", the default).  A manual zoom is kept until "auto" puts it back.
   */
  int zoom = 0;

  /*
   * The camera of the last map redraw, so a mouse click can be hit-tested
   * against exactly what is on screen.
   */
  map_camera_t cam = {};

  /*
   * Whether a map too big for its window keeps the player's room in view.  A
   * pan button turns that off, so the view stays where the player put it; it
   * comes back when they walk into a different room, which last_player (the
   * room of the last redraw; a room key, as long as gsc_walk.a5_to's) is
   * there to notice.
   */
  int follow = TRUE;
  char last_player[256] = "";

  /*
   * Forces the whole map surface out on the next redraw, instead of only the
   * rows that changed since the last one (see gsc_map_screen in
   * os_glk_map.cpp).
   */
  int full_flush = TRUE;
};
typedef struct gsc_map_pane_s gsc_map_pane_t;

extern gsc_map_pane_t gsc_map_pane;

/*
 * A walk in progress, from clicking a room on the map (clsCharacter.WalkTo /
 * DoWalk): the target room, and the room we set off from on the last step so
 * a step that fails to move us can abort the walk, as DoWalk's sLastPosition
 * does.  Each step is submitted as an ordinary direction command, one room
 * per turn.
 */
struct gsc_walk_s
{
  /*
   * A click beside the player's room rather than on another one walks a
   * single step that way, wherever it leads: the direction to take, or -1.
   * Both engines share it, as only one of them is ever running.
   */
  int step_dir = -1;

  /* ADRIFT 5: room keys. */
  char a5_to[256] = "";
  char a5_last[256] = "";
  int a5_clicked = FALSE;
  int a5_steps = 0;

  /*
   * The same walk, for ADRIFT 4.  Room keys here are just room numbers, and
   * the step is submitted as a direction word through SCARE's own parser.
   */
  char sc_to[16] = "";
  char sc_last[16] = "";
  int sc_clicked = FALSE;
  int sc_steps = 0;
};
typedef struct gsc_walk_s gsc_walk_t;

extern gsc_walk_t gsc_walk;
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
