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

/*
 * Module notes:
 *
 * o The Glk interface makes no effort to set text colours, background
 *   colours, and so forth, and minimal effort to set fonts and other style
 *   effects.
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scarier.h"

/* The Glk headers and the glkimp host symbols (glk_*, garglk_*, glkunix_*,
 * gli_determinism, win_*) are C; this is a C++ translation unit, so include
 * them with C linkage.  This also gives the glk_main / glkunix_startup_code /
 * glkunix_arguments definitions below C linkage so the C glkimp host can find
 * them (matching the bocfel / geas C++ ports). */
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
#include "scarier-autosave.h"

/* Autosave at a top-level command prompt (both engines), and the engine
 * state containers; defined with the rest of the autosave support further
 * down, after the Glk object globals they stash. */
static void gsc_autosave (void);
static bool gsc_sc_apply_all (const std::string &data);
static bool gsc_a5_apply_all (const std::string &data);

/*
 * gsc_autorestore_wanted()
 *
 * True when this session is about to autorestore, i.e. no window may be
 * opened before the restore replaces the Glk library state.
 *
 * The restore adopts the archived windows wholesale (gli_replace_window_list),
 * which first CLOSES whatever windows this process has already opened.  On
 * the host side an open during an autorestore is harmless -- it hands back
 * the window it already rebuilt from its GUI snapshot rather than making a
 * second one -- but the matching close is not: it deletes that restored
 * window for good, and the adopted library never opens it again.  A window
 * this process re-creates at boot is therefore a window the player loses:
 * with the map pane open only the map came back (the one window boot does
 * not touch), and with it closed, nothing did.
 *
 * So both engines gate every startup glk_window_open on this and let the
 * archive supply the windows instead.
 */
static int
gsc_autorestore_wanted (void)
{
  return scarier_autosave_exists ();
}
/* Set after a successful autorestore: the restored transcript already ends
 * with the old prompt, so the next prompt print is skipped once. */
static int gsc_autorestored = 0;
/* Set around the debugger's read; a debug prompt is mid-turn, not a state
 * worth autosaving. */
static int gsc_in_debug_read = 0;
#endif

#ifdef GLK_MODULE_GARGLK_FILE_RESOURCES
extern "C" {
#include "gi_blorb.h"
}

char gamefile[1024];

static const char *find_last_of(const char *str, const char *chars)
{
  const char *found = NULL;
  while (*chars != 0) {
    const char *p = strrchr(str, *chars++);
    if (p != NULL && (found == NULL || p > found)) {
      found = p;
    }
  }

  return found;
}
#endif

#include "scprotos.h" /* for SCARIER_VERSION */

/* ADRIFT 5 engine -- driven by the dedicated a5 Glk loop (gsc_a5_main) when
 * the game file is detected as ADRIFT 5 rather than ADRIFT <=4. */
#include "adrift5/a5deobf.h"         /* a5_deflate / a5_inflate save framing */
#include "adrift5/a5map.h"           /* the ADRIFT 5 map (Map.vb) */
#include "scmap.h"           /* the ADRIFT 4 map (run400 Form29) */
#include "adrift5/a5model.h"
#include "adrift5/a5parse.h"         /* a5parse_direction_name, for map-click walks */
#include "adrift5/a5restr.h"         /* a5restr_exit_in_direction, for map stub arrows */
#include "adrift5/a5run.h"
#include "adrift5/a5state.h"
#include "adrift5/a5text.h"          /* A5_MEDIA_* kinds */

/* Blorb resource map, for ADRIFT 5 graphics/sound (the game file is a Blorb).
 * Included unconditionally here -- the GARGLK block below only includes it when
 * GLK_MODULE_GARGLK_FILE_RESOURCES is defined, which the Spatterlight build is
 * not; gi_blorb.h has its own include guard so this is a no-op when it is. */
extern "C" {
#include "gi_blorb.h"
}

/*
 * True and false definitions -- usually defined in glkstart.h, but we need
 * them early, so we'll define them here too.  We also need NULL, but that's
 * normally from stdio.h or one of it's cousins.
 */
#ifndef FALSE
# define FALSE 0
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif


/*---------------------------------------------------------------------*/
/*  Module variables, miscellaneous other stuff                        */
/*---------------------------------------------------------------------*/

/* Glk SCARIER interface version number.  Each byte is printed in DECIMAL by
   gsc_command_print_version_number (so 0x00010400 -> "1.4.0"); keep every
   component below 10, or encode it as its hex value (11 -> 0x0b).  SCARE
   1.3.10 wrote 0x...10 here and printed as "1.3.16" for years. */
static const glui32 GSC_PORT_VERSION = 0x00010400;

/* Two windows, one for the main text, and one for a status line. */
static winid_t gsc_main_window = NULL,
               gsc_status_window = NULL;

/* Whether nothing has been printed to the main window since it was last
   cleared.  A window that is still empty can be cleared again for free, which
   is the one moment a background colour change can repaint the pane without
   costing the player any text; see SCR_TAG_BGCOLOUR. */
static int gsc_main_window_empty = TRUE;

/*
 * Transcript stream and input log.  These are NULL if there is no current
 * collection of these strings.
 */
static strid_t gsc_transcript_stream = NULL,
               gsc_inputlog_stream = NULL;

/* Input read log stream, for reading back an input log. */
static strid_t gsc_readlog_stream = NULL;

/* Options that may be turned off or set by command line flags. */
static int gsc_commands_enabled = TRUE,
           gsc_abbreviations_enabled = TRUE,
           gsc_unicode_enabled = TRUE;

/* Whether this Glk library has the garglk_set_zcolors extension, and with it
   the "glk colour" mode.  Defined up here rather than down with the rest of
   the colour code because the status line, drawn earlier in the file, draws
   itself in the game's colours too; the mode itself is documented where
   gsc_set_colour lives. */
#if defined(SPATTERLIGHT) || defined(GARGLK) \
    || defined(GLK_MODULE_GARGLK_FILE_RESOURCES)
# define GSC_HAVE_ZCOLORS 1
#endif

/* Whether a cover image can be shown in a pane of its own above the story
   window (see gsc_show_title_graphic).  The pane itself needs nothing but core
   Glk graphics; what varies is where the image comes from, so the hosts that
   can turn a chunk of the game file into a Glk image number are exactly the
   ones that can have it -- Spatterlight through its image cache, Gargoyle
   through garglk_add_resource_from_file. */
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

/* ADRIFT <=4 Runner default palette; the ADRIFT 5 loop overwrites these from
   the adventure when it loads one.  See gsc_set_colour for where the numbers
   come from. */
static glui32 gsc_colour_background = 0x000000,
              gsc_colour_output = 0x00ff00,
              gsc_colour_input = 0xff3232;

/* Whether the game's own palette is in force ("glk colour", set by
   gsc_set_colour far below).  It lives up here because the status line, drawn
   earlier in the file, picks its style by it; where the Glk library has no
   zcolors extension the command is not offered and this stays FALSE. */
static int gsc_colour_enabled = FALSE;
/* Whether colour mode should be on before the game prints a word: the "-c"
   command line switch, sparing the player a "glk colour on" at every launch, or
   a game that asks for its own palette by the colours it was written in (see
   gsc_colour_detect).  Acted on once the windows exist; see
   gsc_colour_startup_apply. */
static int gsc_colour_startup = FALSE;
/* True when Normal colour stylehints were already set before the first window
   open (the -c path, and the detected one).  gsc_set_colour consumes this so it
   can skip a pointless rebuild of the just-opened tree. */
static int gsc_colour_hints_preapplied = FALSE;
/* The colour last named on the story window (gsc_colour_apply), so that drawing
   the status bar in its own two colours can put the story's back afterwards --
   a library whose zcolors are global as well as per-stream would otherwise go
   on painting the story window in the bar's colours.  See gsc_status_end.  Only
   read while colour mode is on, and gsc_set_colour colours the story window
   before it turns the mode on, so it is always set by then. */
static glui32 gsc_colour_main_fg = 0;

/* Cached measurement of the story window's style_Normal colours: -1 for not
   yet measured, 0 for a library that cannot measure, 1 for a filled cache.
   Measuring is not free -- under Spatterlight every glk_style_measure call is
   a window flush plus a synchronous round-trip to the application process --
   and the consumers ask often: gsc_colour_visible once per styled fragment
   of output, the map redraw once per prompt while its pane is open.  The
   answer only changes when colour mode itself toggles (gsc_set_colour, which
   also rebuilds the window tree) or when the library's style preference
   flips, and the latter reaches us as an Arrange event; those two places
   reset the state to -1. */
static int gsc_normal_measure_state = -1;
static glui32 gsc_normal_measured_fg, gsc_normal_measured_bg;

/*
 * gsc_normal_measure()
 *
 * Report the story window's style_Normal text and background colour as the
 * library measures them -- the game palette when colour mode's stylehints
 * are being honoured, the theme when they are ignored.  Answers from the
 * cache above when it is warm; FALSE when the library cannot measure.
 */
static scr_bool
gsc_normal_measure (glui32 *fg, glui32 *bg)
{
  if (gsc_normal_measure_state < 0)
    {
      glui32 mfg, mbg;

      if (glk_style_measure (gsc_main_window, style_Normal,
                             stylehint_TextColor, &mfg)
          && glk_style_measure (gsc_main_window, style_Normal,
                                stylehint_BackColor, &mbg))
        {
          gsc_normal_measured_fg = mfg;
          gsc_normal_measured_bg = mbg;
          gsc_normal_measure_state = 1;
        }
      else
        gsc_normal_measure_state = 0;
    }

  if (gsc_normal_measure_state == 0)
    return FALSE;
  *fg = gsc_normal_measured_fg;
  *bg = gsc_normal_measured_bg;
  return TRUE;
}

/*
 * gsc_colour_visible()
 *
 * Whether colour mode is on *and* the library is really using the Normal
 * colours we published as stylehints.  Spatterlight's "Games can set colors
 * and styles" and Gargoyle's stylehint preference (despite the name, it also
 * gates zcolors) both leave the story window on the theme while still
 * accepting our stylehint_set / garglk_set_zcolors calls.  Places where a
 * colour stands in for a style (the status bar, secondary-colour text) have
 * to follow the theme too in that case.  The map measures style_Normal
 * instead, so it tracks whichever palette the library is actually using.
 *
 * Detection is portable: after colour mode sets Normal TextColor to the
 * game's output colour and (re)opens windows, measure that style -- if the
 * library ignored the hint, measure reports the theme and we treat colours
 * as not visible.  A preference change reaches us as an Arrange event
 * (Spatterlight's glkimp compares do_styles), which redraws the bar and map.
 * The measurement is cached between those events, so the common
 * per-fragment call costs two comparisons.
 */
static scr_bool
gsc_colour_visible (void)
{
  glui32 fg, bg;

  if (!gsc_colour_enabled)
    return FALSE;
  if (gsc_main_window == NULL)
    return TRUE;

  /* A library that cannot measure leaves no way to tell; believe the
     colours are being honoured. */
  if (!gsc_normal_measure (&fg, &bg))
    return TRUE;
  return fg == gsc_colour_output && bg == gsc_colour_background;
}

/* Adrift game to interpret. */
static scr_game gsc_game = NULL;

/* ADRIFT 5 game.  When the file loaded at startup is an ADRIFT 5 game,
 * gsc_a5_adv holds the parsed adventure and gsc_is_a5 is set; glk_main then
 * runs the dedicated a5 turn loop (gsc_a5_main) instead of the scare engine.
 * gsc_game_path is the on-disk path, needed by a5model_load (which reads the
 * file itself rather than via a Glk stream). */
static char gsc_game_path[2048];
static a5_adventure_t *gsc_a5_adv = NULL;
static int gsc_is_a5 = FALSE;

/* A name for this game to file per-game settings under, for the games that
   carry no IFID to use instead: ADRIFT 4 has none at all, and a few ADRIFT 5
   games were built without one.  It is derived from the game file's contents
   (gsc_hash_game_stream), so it follows the game when the file is moved or
   renamed, and two games never share it the way two files both called
   "adventure.taf" would.  Empty until the startup code has read the file. */
static char gsc_game_key[40];

/* Current a5 run, kept here so status redraws on window resize can reach it. */
static a5_run_t *gsc_a5_run = NULL;
static void gsc_a5_status (a5_run_t *run);
static void gsc_a5_display (const char *text);
static int gsc_a5_show_media (a5_run_t *run);
static void gsc_a5_media_fire (a5_run_t *run, int idx);
static void gsc_a5_undo_look (a5_run_t *run);

/* Set when this session drives the game's TimeBased events off a wall-clock
   1-second Glk timer (the real Runner's tmrEvents_Tick) instead of the
   engine's deterministic one-tick-per-input substitute.  Decided per run by
   gsc_a5_start_real_time. */
static int gsc_a5_real_time = FALSE;

/* Set while the opening is replayed only to reach a saved state (the
   Spatterlight autorestore boot), so a %PopUpInput% naming prompt in it is
   answered with its default instead of asking the player again. */
static int gsc_a5_popup_silent = FALSE;

/* Author-defined secondary output window (ADRIFT 5 <window NAME>), opened
   lazily as a right-hand text buffer the first time the game routes text to
   one, then kept open like the official Runner.  This build supports a single
   side window (games such as Alien Diver use exactly one, "Status"), so every
   <window NAME> shares it regardless of NAME. */
static winid_t gsc_a5_side_window = NULL;

/* The map, shown in a graphics window to the right of the story on request
   ("map").  Both engines use this pane; what differs is where the map comes
   from.  An ADRIFT 5 map is authored data, parsed once per adventure, aliasing
   its XML, and so dropped whenever the adventure is.  An ADRIFT 4 map has to be
   derived from the exit graph, and it depends on the room you are standing in
   and on how much you have explored -- so it is rebuilt on every redraw, as the
   ADRIFT 4 runner rebuilt it every time it drew (drawmap with mode = 1). */
static winid_t gsc_map_window = NULL;
static map_t *gsc_map = NULL;
static int gsc_map_shown = FALSE;
/* Whether the map pane is wanted at all: the player's remembered choice, or
   failing that the layout the game shipped (gsc_map_auto_reveal).  Wanted is
   not the same as shown -- a map with nothing on it yet is held back until
   there is something to see (gsc_map_worth_opening), and the per-turn redraw
   opens it then.  FALSE until something decides. */
static int gsc_map_want = FALSE;
/* Where the map pane sits: the standard 40% pane to the right of the story,
   or -- for games with wide maps -- a 30% band across the top of the whole
   screen, above the status line ("glk map top").  The choice is kept, so
   hiding and re-showing the map does not move it, and it is remembered with
   the visibility (gsc_map_pref_write) so neither does restarting. */
static int gsc_map_at_top = FALSE;
/* Set when the game defines a MAP command of its own (Lost Coastlines has a
   sea chart): the game's command wins, and the pane is reached with the
   "glk map" escape instead. */
static int gsc_map_taken = FALSE;
/* The restart the pane was last set up for (run_get_restart_count), so that an
   ADRIFT 4 restart -- which happens entirely inside the interpreter -- is
   noticed as the replayed opening arrives (gsc_map_notice_restart). */
static scr_int gsc_map_restarts = 0;
static void gsc_map_redraw (void);

/* The camera and pixel size of the last map redraw, so a mouse click can be
   hit-tested against exactly what is on screen. */
static map_camera_t gsc_map_cam;
static int gsc_map_px_w = 0, gsc_map_px_h = 0;

/* A manual zoom ("glk zoom in/out"), as pixels per map unit; 0 while the map
   is fitting itself to its window ("glk zoom auto", the default).  A manual
   zoom is kept until "auto" puts it back; meanwhile the view pans to keep the
   player on-screen. */
static int gsc_map_zoom = 0;

/* The pixels currently on screen, kept so that a redraw need only send the rows
   that have changed.  The map is redrawn at every prompt, but most turns do not
   move the player and so do not change a single pixel; a turn that does move him
   usually changes only part of the pane.  Sending the whole surface regardless
   is what costs: Glk has no blit, so each row of the map arrives at the display
   as a run of glk_window_fill_rect() calls, and Spatterlight turns every one of
   them into an NSRectFill.  Comparing against these pixels first is much cheaper
   than drawing them again.
   The comparison is only valid while the window still holds what we last put
   there, so gsc_map_full_flush forces the whole surface out again whenever it
   may not: after an arrange or redraw event (the window is resized, and its
   backing image with it), and when the pane is opened or cleared. */
static map_surface_t *gsc_map_screen = NULL;
static int gsc_map_full_flush = TRUE;

static void
gsc_map_screen_drop (void)
{
  map_surface_free (gsc_map_screen);
  gsc_map_screen = NULL;
  gsc_map_full_flush = TRUE;
}

/* A walk in progress, from clicking a room on the map (clsCharacter.WalkTo /
   DoWalk): the target room, and the room we set off from on the last step so a
   step that fails to move us can abort the walk, as DoWalk's sLastPosition
   does.  Each step is submitted as an ordinary direction command, one room per
   turn. */
static char gsc_a5_walk_to[256] = "";
static char gsc_a5_walk_last[256] = "";
static int gsc_a5_walk_clicked = FALSE;
static int gsc_a5_walk_steps = 0;
/* A walk is bounded: a game that shuffles the player around could otherwise
   keep the route recomputing for ever. */
#define GSC_A5_WALK_MAX 100
static void gsc_a5_map_view (a5_state_t *st, map_view_t *view);
static int gsc_a5_walk_next (a5_run_t *run, char *buf, int bufsize);

static void
gsc_a5_walk_stop (void)
{
  gsc_a5_walk_to[0] = '\0';
  gsc_a5_walk_last[0] = '\0';
  gsc_a5_walk_steps = 0;
}

/* The same walk, for ADRIFT 4.  Room keys here are just room numbers, and the
   step is submitted as a direction word through SCARE's own parser. */
static char gsc_sc_walk_to[16] = "";
static char gsc_sc_walk_last[16] = "";
static int gsc_sc_walk_steps = 0;
#define GSC_SC_WALK_MAX 100

static int gsc_sc_walk_next (scr_char *buffer, scr_int length);
static void gsc_map_toggle (void);
static void gsc_map_auto_reveal (void);
static void gsc_map_notice_restart (void);
static void gsc_map_show (void);
static int gsc_map_available (void);
static int gsc_map_pref_read (int *at_top);
static int gsc_map_default_shown (void);
static winid_t gsc_a5_open_side_window (void);

static void
gsc_sc_walk_stop (void)
{
  gsc_sc_walk_to[0] = '\0';
  gsc_sc_walk_last[0] = '\0';
  gsc_sc_walk_steps = 0;
}

/* Special out-of-band os_confirm() options used locally with os_glk. */
static const scr_int GSC_CONF_SUBTLE_HINT = INT_MAX,
                    GSC_CONF_UNSUBTLE_HINT = INT_MAX - 1,
                    GSC_CONF_CONTINUE_HINTS = INT_MAX - 2;

/* Forward declaration of event wait functions, and a short delay. */
static void gsc_event_wait_2 (glui32 wait_type_1,
                              glui32 wait_type_2, event_t *event);
static void gsc_event_wait (glui32 wait_type, event_t *event);
static void gsc_short_delay (void);


/*---------------------------------------------------------------------*/
/*  Glk port utility functions                                         */
/*---------------------------------------------------------------------*/

/*
 * gsc_put_literal()
 *
 * Print a string literal to the current Glk stream.  glk_put_string() takes
 * a non-const char * for historical reasons but never writes through it, so
 * this wrapper holds the const_cast needed to pass literals from C++.
 */
static void
gsc_put_literal (const char *string)
{
  if (gsc_main_window
      && glk_stream_get_current () == glk_window_get_stream (gsc_main_window))
    gsc_main_window_empty = FALSE;
  glk_put_string (const_cast<char *> (string));
}


/*
 * gsc_fatal()
 *
 * Fatal error handler.  The function returns, expecting the caller to
 * abort() or otherwise handle the error.
 */
static void
gsc_fatal (const char *string)
{
  /*
   * If the failure happens too early for us to have a window, print
   * the message to stderr.
   */
  if (!gsc_main_window)
    {
      fprintf (stderr, "\n\nINTERNAL ERROR: %s\n", string);

      fprintf (stderr, "\nPlease record the details of this error, try to"
                       " note down everything you did to cause it, and email"
                       " this information to simon_baldwin@yahoo.com.\n\n");
      return;
    }

  /* Cancel all possible pending window input events. */
  glk_cancel_line_event (gsc_main_window, NULL);
  glk_cancel_char_event (gsc_main_window);

  /* Print a message indicating the error, and exit. */
  glk_set_window (gsc_main_window);
  glk_set_style (style_Normal);
  gsc_put_literal ("\n\nINTERNAL ERROR: ");
  gsc_put_literal (string);

  gsc_put_literal ("\n\nPlease record the details of this error, try to"
                   " note down everything you did to cause it, and email"
                   " this information to simon_baldwin@yahoo.com.\n\n");
}


/*
 * gsc_hint_window_styles()
 * gsc_open_main_window()
 * gsc_open_status_window()
 *
 * Window prologue shared by the ADRIFT <=4 and ADRIFT 5 main loops.
 *
 * Style hints have to be set before the window they apply to is opened.
 * Centered text (<center>/<centre> sections) renders through two user styles
 * hinted for centered justification: User1 plain, User2 bold (Glk styles
 * don't combine, so <center><b> title lines need their own style).  Right-
 * aligned text (<right>) uses style_Note with RightFlush.  Italic uses
 * Emphasized (or Alert when combined with bold).  Libraries that ignore
 * justification hints show alignment styles as ordinary left-flush text.
 * User1 on the grid is the reverse-video status line.
 *
 * Header and Subheader are hinted the other way, left-flush.  This port uses
 * them purely as stand-ins for a large <font size=...> and for <b> (see
 * gsc_set_glk_style()) -- they never mean "this is a heading" -- but a library
 * is free to render Header as a centered heading, and some do (Spatterlight's
 * Zoom theme centers it).  Justification is a paragraph attribute, so a single
 * Header-styled character is enough to centre the whole paragraph it starts:
 * "The Warlord, The Princess & The Bulldog" opens every room description with
 * a <font size=+10> drop cap, which centered the entire description under that
 * theme.  In the Runner nothing but <center>/<right> ever moves text off the
 * left margin, so pin these two down and leave alignment to those tags alone.
 *
 * The status window is a nicety; we can live without it.  It is opened
 * separately from the main window because the <=4 path prints its
 * "no game file" complaint in between.
 */
static void
gsc_hint_window_styles (void)
{
  glk_stylehint_set (wintype_TextBuffer, style_User1,
                     stylehint_Justification, stylehint_just_Centered);
  glk_stylehint_set (wintype_TextBuffer, style_User2,
                     stylehint_Justification, stylehint_just_Centered);
  glk_stylehint_set (wintype_TextBuffer, style_User2, stylehint_Weight, 1);

  glk_stylehint_set (wintype_TextBuffer, style_Note,
                     stylehint_Justification, stylehint_just_RightFlush);

  glk_stylehint_set (wintype_TextBuffer, style_Header,
                     stylehint_Justification, stylehint_just_LeftFlush);
  glk_stylehint_set (wintype_TextBuffer, style_Subheader,
                     stylehint_Justification, stylehint_just_LeftFlush);

  glk_stylehint_set (wintype_TextGrid, style_User1, stylehint_ReverseColor, 1);
}

static void
gsc_open_main_window (void)
{
  gsc_main_window = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
  if (!gsc_main_window)
    {
      gsc_fatal ("GLK: Can't open main window");
      glk_exit ();
    }
  glk_window_clear (gsc_main_window);
  glk_set_window (gsc_main_window);
  glk_set_style (style_Normal);
}

static void
gsc_open_status_window (void)
{
  gsc_status_window = glk_window_open (gsc_main_window,
                                       winmethod_Above | winmethod_Fixed,
                                       1, wintype_TextGrid, 0);
}


/*
 * gsc_malloc()
 *
 * Non-failing malloc; call gsc_fatal and exit if memory allocation fails.
 */
static void *
gsc_malloc (size_t size)
{
  void *pointer;

  pointer = (decltype(pointer)) malloc (size > 0 ? size : 1);
  if (!pointer)
    {
      gsc_fatal ("GLK: Out of system memory");
      glk_exit ();
    }

  return pointer;
}

/*
 * gsc_realloc()
 *
 * Non-failing realloc; call gsc_fatal and exit if memory allocation fails.
 */
static void *
gsc_realloc (void *pointer, size_t size)
{
  void *result;

  result = realloc (pointer, size > 0 ? size : 1);
  if (!result)
    {
      gsc_fatal ("GLK: Out of system memory");
      glk_exit ();
    }

  return result;
}


/*---------------------------------------------------------------------*/
/*  Glk port locale data                                               */
/*---------------------------------------------------------------------*/

/* Unicode values up to 256 are equivalent to iso 8859-1. */
static const glui32 GSC_ISO_8859_EQUIVALENCE = 256;

/*
 * Lookup table pair for converting a given single character into unicode and
 * iso 8859-1 (the lower byte of the unicode representation, assuming an upper
 * byte of zero), and an ascii substitute should nothing else be available.
 * Tables are 256 elements; although the first 128 characters of a codepage
 * are usually standard ascii, making tables full-sized allows for support of
 * codepages where they're not (dingbats, for example).
 */
enum { GSC_TABLE_SIZE = 256 };
typedef struct {
  const glui32 unicode[GSC_TABLE_SIZE];
  const scr_char *const ascii[GSC_TABLE_SIZE];
} gsc_codepages_t;

/*
 * Locale contains a name and a pair of codepage structures, a main one and
 * an alternate.  The latter is intended for monospaced output.
 */
typedef struct {
  const scr_char *const name;
  const gsc_codepages_t main;
  const gsc_codepages_t alternate;
} gsc_locale_t;


/*
 * Locale for Latin1 -- cp1252 and cp850.
 *
 * The ascii representations of characters in this table are based on the
 * general look of the characters, rather than pronounciation.  Accented
 * characters are generally rendered unaccented, and box drawing, shading,
 * and other non-alphanumeric glyphs as either a similar shape, or as a
 * character that might be recognizable as what it's trying to emulate.
 */
static const gsc_locale_t GSC_LATIN1_LOCALE = {
  "Latin1",
  /* cp1252 to unicode. */
{ { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x000a, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0021, 0x0022, 0x0023,
    0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c,
    0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035,
    0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e,
    0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f, 0x0050,
    0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059,
    0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062,
    0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b,
    0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074,
    0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d,
    0x007e, 0x0000, 0x20ac, 0x0000, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020,
    0x2021, 0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017d, 0x0000,
    0x0000, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x02dc,
    0x2122, 0x0161, 0x203a, 0x0153, 0x0000, 0x017e, 0x0178, 0x00a0, 0x00a1,
    0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7, 0x00a8, 0x00a9, 0x00aa,
    0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af, 0x00b0, 0x00b1, 0x00b2, 0x00b3,
    0x00b4, 0x00b5, 0x00b6, 0x00b7, 0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc,
    0x00bd, 0x00be, 0x00bf, 0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5,
    0x00c6, 0x00c7, 0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce,
    0x00cf, 0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,
    0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df, 0x00e0,
    0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7, 0x00e8, 0x00e9,
    0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef, 0x00f0, 0x00f1, 0x00f2,
    0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7, 0x00f8, 0x00f9, 0x00fa, 0x00fb,
    0x00fc, 0x00fd, 0x00fe, 0x00ff },
  /* cp1252 to ascii. */
  { NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  "        ",
    "\n",  NULL,  NULL,  "\n",  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
    NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
    " ",   "!",   "\"",  "#",   "$",   "%",   "&",   "'",   "(",   ")",   "*",
    "+",   ",",   "-",   ".",   "/",   "0",   "1",   "2",   "3",   "4",   "5",
    "6",   "7",   "8",   "9",   ":",   ";",   "<",   "=",   ">",   "?",   "@",
    "A",   "B",   "C",   "D",   "E",   "F",   "G",   "H",   "I",   "J",   "K",
    "L",   "M",   "N",   "O",   "P",   "Q",   "R",   "S",   "T",   "U",   "V",
    "W",   "X",   "Y",   "Z",   "[",   "\\",  "]",   "^",   "_",   "`",   "a",
    "b",   "c",   "d",   "e",   "f",   "g",   "h",   "i",   "j",   "k",   "l",
    "m",   "n",   "o",   "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "{",   "|",   "}",   "~",   NULL,  "E",   NULL,  ",",
    "f",   ",,",  "...", "+",   "#",   "^",   "%",   "S",   "<",   "OE",  NULL,
    "Z",   NULL,  NULL,  "'",   "'",   "\"",  "\"",  "*",   "-",   "-",   "~",
    "[TM]","s",   ">",   "oe",  NULL,  "z",   "Y",   " ",   "!",   "c",   "GBP",
    "*",   "Y",   "|",   "S",   "\"",  "(C)", "a",   "<<",  "-",   "-",   "(R)",
    "-",   "o",   "+/-", "2",   "3",   "'",   "u",   "P",   "*",   ",",   "1",
    "o",   ">>",  "1/4", "1/2", "3/4", "?",   "A",   "A",   "A",   "A",   "A",
    "A",   "AE",  "C",   "E",   "E",   "E",   "E",   "I",   "I",   "I",   "I",
    "D",   "N",   "O",   "O",   "O",   "O",   "O",   "x",   "O",   "U",   "U",
    "U",   "U",   "Y",   "p",   "ss",  "a",   "a",   "a",   "a",   "a",   "a",
    "ae",  "c",   "e",   "e",   "e",   "e",   "i",   "i",   "i",   "i",   "d",
    "n",   "o",   "o",   "o",   "o",   "o",   "/",   "o",   "u",   "u",   "u",
    "u",   "y",   "P",   "y" } },
  /* cp850 to unicode. */
{ { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x000a, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0021, 0x0022, 0x0023,
    0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c,
    0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035,
    0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e,
    0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f, 0x0050,
    0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059,
    0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062,
    0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b,
    0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074,
    0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d,
    0x007e, 0x0000, 0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5,
    0x00e7, 0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
    0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9, 0x00ff,
    0x00d6, 0x00dc, 0x00f8, 0x00a3, 0x00d8, 0x00d7, 0x0192, 0x00e1, 0x00ed,
    0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba, 0x00bf, 0x00ae, 0x00ac,
    0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb, 0x2591, 0x2592, 0x2593, 0x2502,
    0x2524, 0x00c1, 0x00c2, 0x00c0, 0x00a9, 0x2563, 0x2551, 0x2557, 0x255d,
    0x00a2, 0x00a5, 0x2510, 0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c,
    0x00e3, 0x00c3, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c,
    0x00a4, 0x00f0, 0x00d0, 0x00ca, 0x00cb, 0x00c8, 0x0131, 0x00cd, 0x00ce,
    0x00cf, 0x2518, 0x250c, 0x2588, 0x2584, 0x00a6, 0x00cc, 0x2580, 0x00d3,
    0x00df, 0x00d4, 0x00d2, 0x00f5, 0x00d5, 0x00b5, 0x00fe, 0x00de, 0x00da,
    0x00db, 0x00d9, 0x00fd, 0x00dd, 0x00af, 0x00b4, 0x00ad, 0x00b1, 0x2017,
    0x00be, 0x00b6, 0x00a7, 0x00f7, 0x00b8, 0x00b0, 0x00a8, 0x00b7, 0x00b9,
    0x00b3, 0x00b2, 0x25a0, 0x00a0 },
  /* cp850 to ascii. */
  { NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  "        ",
    "\n",  NULL,  NULL,  "\n",  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
    NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
    " ",   "!",   "\"",  "#",   "$",   "%",   "&",   "'",   "(",   ")",   "*",
    "+",   ",",   "-",   ".",   "/",   "0",   "1",   "2",   "3",   "4",   "5",
    "6",   "7",   "8",   "9",   ":",   ";",   "<",   "=",   ">",   "?",   "@",
    "A",   "B",   "C",   "D",   "E",   "F",   "G",   "H",   "I",   "J",   "K",
    "L",   "M",   "N",   "O",   "P",   "Q",   "R",   "S",   "T",   "U",   "V",
    "W",   "X",   "Y",   "Z",   "[",   "\\",  "]",   "^",   "_",   "`",   "a",
    "b",   "c",   "d",   "e",   "f",   "g",   "h",   "i",   "j",   "k",   "l",
    "m",   "n",   "o",   "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "{",   "|",   "}",   "~",   NULL,  "C",   "u",   "e",
    "a",   "a",   "a",   "a",   "c",   "e",   "e",   "e",   "i",   "i",   "i",
    "A",   "A",   "E",   "ae",  "AE",  "o",   "o",   "o",   "u",   "u",   "y",
    "O",   "U",   "o",   "GBP", "O",   "x",   "f",   "a",   "i",   "o",   "u",
    "n",   "N",   "a",   "o",   "?",   "(R)", "-",   "1/2", "1/4", "i",   "<<",
    ">>",  "#",   "#",   "#",   "|",   "+",   "A",   "A",   "A",   "(C)", "+",
    "|",   "+",   "+",   "c",   "Y",   "+",   "+",   "+",   "+",   "+",   "-",
    "+",   "a",   "A",   "+",   "+",   "+",   "+",   "+",   "=",   "+",   "*",
    "d",   "D",   "E",   "E",   "E",   "i",   "I",   "I",   "I",   "+",   "+",
    ".",   ".",   "|",   "I",   ".",   "O",   "ss",  "O",   "O",   "o",   "O",
    "u",   "p",   "P",   "U",   "U",   "U",   "y",   "Y",   "-",   "'",   "-",
    "+/-", "=",   "3/4", "P",   "S",   "/",   ",",   "deg", "\"",  "*",   "1",
    "3",   "2",   ".",   " " } }
};


/*
 * Locale for Cyrillic -- cp1251 and cp866.
 *
 * The ascii representations in this table, for alphabetic characters, follow
 * linguistic rather than appearance rules, the essence of gost 16876-71.
 * Capitalized cyrillic letters that translate to multiple ascii characters
 * have the first ascii character only of the sequence translated.  This gives
 * the best appearance in normal sentences, but is not optimal in a run of
 * all capitals (headings, for example).  For non-alphanumeric characters,
 * the general appearance and shape of the character being emulated is used.
 */
static const gsc_locale_t GSC_CYRILLIC_LOCALE = {
  "Cyrillic",
  /* cp1251 to unicode. */
{ { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x000a, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0021, 0x0022, 0x0023,
    0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c,
    0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035,
    0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e,
    0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f, 0x0050,
    0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059,
    0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062,
    0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b,
    0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074,
    0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d,
    0x007e, 0x0000, 0x0402, 0x0403, 0x201a, 0x0453, 0x201e, 0x2026, 0x2020,
    0x2021, 0x20ac, 0x2030, 0x0409, 0x2039, 0x040a, 0x040c, 0x040b, 0x040f,
    0x0452, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x0000,
    0x2122, 0x0459, 0x203a, 0x045a, 0x045c, 0x045b, 0x045f, 0x00a0, 0x040e,
    0x045e, 0x0408, 0x00a4, 0x0490, 0x00a6, 0x00a7, 0x0401, 0x00a9, 0x0404,
    0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x0407, 0x00b0, 0x00b1, 0x0406, 0x0456,
    0x0491, 0x00b5, 0x00b6, 0x00b7, 0x0451, 0x2116, 0x0454, 0x00bb, 0x0458,
    0x0405, 0x0455, 0x0457, 0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415,
    0x0416, 0x0417, 0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e,
    0x041f, 0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
    0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f, 0x0430,
    0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439,
    0x043a, 0x043b, 0x043c, 0x043d, 0x043e, 0x043f, 0x0440, 0x0441, 0x0442,
    0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044a, 0x044b,
    0x044c, 0x044d, 0x044e, 0x044f },
  /* cp1251 to gost 16876-71 ascii. */
  { NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  "        ",
    "\n",  NULL,  NULL,  "\n",  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
    NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
    " ",   "!",   "\"",  "#",   "$",   "%",   "&",   "'",   "(",   ")",   "*",
    "+",   ",",   "-",   ".",   "/",   "0",   "1",   "2",   "3",   "4",   "5",
    "6",   "7",   "8",   "9",   ":",   ";",   "<",   "=",   ">",   "?",   "@",
    "A",   "B",   "C",   "D",   "E",   "F",   "G",   "H",   "I",   "J",   "K",
    "L",   "M",   "N",   "O",   "P",   "Q",   "R",   "S",   "T",   "U",   "V",
    "W",   "X",   "Y",   "Z",   "[",   "\\",  "]",   "^",   "_",   "`",   "a",
    "b",   "c",   "d",   "e",   "f",   "g",   "h",   "i",   "j",   "k",   "l",
    "m",   "n",   "o",   "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "{",   "|",   "}",   "~",   NULL,  NULL,  NULL,  ",",
    NULL,  ",,",  "...", "+",   "#",   "E",   "%",   NULL,  "<",   NULL,  NULL,
    NULL,  NULL,  NULL,  "'",   "'",   "\"",  "\"",  "*",   "-",   "-",   NULL,
    "[TM]",NULL,  ">",   NULL,  NULL,  NULL,  NULL,  " ",   NULL,  NULL,  NULL,
    "*",   "G",   "|",   "S",   "Jo",  "(C)", "Je",  "<<",  "-",   "-",   "(R)",
    "Ji",  "o",   "+/-", "I",   "i",   "g",   "u",   "P",   "*",   "jo",  NULL,
    "je",  ">>",  "j",   "S",   "s",   "ji",  "A",   "B",   "V",   "G",   "D",
    "E",   "Zh",  "Z",   "I",   "Jj",  "K",   "L",   "M",   "N",   "O",   "P",
    "R",   "S",   "T",   "U",   "F",   "Kh",  "C",   "Ch",  "Sh",  "Shh", "\"",
    "Y",   "'",   "Eh",  "Ju",  "Ja",  "a",   "b",   "v",   "g",   "d",   "e",
    "zh",  "z",   "i",   "jj",  "k",   "l",   "m",   "n",   "o",   "p",   "r",
    "s",   "t",   "u",   "f",   "kh",  "c",   "ch",  "sh",  "shh", "\"",  "y",
    "'",   "eh",  "ju",  "ja" } },
  /* cp866 to unicode. */
{ { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x000a, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0021, 0x0022, 0x0023,
    0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c,
    0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035,
    0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e,
    0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f, 0x0050,
    0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059,
    0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062,
    0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b,
    0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074,
    0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d,
    0x007e, 0x0000, 0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416,
    0x0417, 0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f,
    0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428,
    0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f, 0x0430, 0x0431,
    0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043a,
    0x043b, 0x043c, 0x043d, 0x043e, 0x043f, 0x2591, 0x2592, 0x2593, 0x2502,
    0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d,
    0x255c, 0x255b, 0x2510, 0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c,
    0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c,
    0x2567, 0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
    0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580, 0x0440,
    0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449,
    0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 0x044f, 0x0401, 0x0451, 0x0404,
    0x0454, 0x0407, 0x0457, 0x040e, 0x045e, 0x00b0, 0x2022, 0x00b7, 0x221a,
    0x2116, 0x00a4, 0x25a0, 0x00a0 },
  /* cp866 to gost 16876-71 ascii. */
  { NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  "        ",
    "\n",  NULL,  NULL,  "\n",  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
    NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
    " ",   "!",   "\"",  "#",   "$",   "%",   "&",   "'",   "(",   ")",   "*",
    "+",   ",",   "-",   ".",   "/",   "0",   "1",   "2",   "3",   "4",   "5",
    "6",   "7",   "8",   "9",   ":",   ";",   "<",   "=",   ">",   "?",   "@",
    "A",   "B",   "C",   "D",   "E",   "F",   "G",   "H",   "I",   "J",   "K",
    "L",   "M",   "N",   "O",   "P",   "Q",   "R",   "S",   "T",   "U",   "V",
    "W",   "X",   "Y",   "Z",   "[",   "\\",  "]",   "^",   "_",   "`",   "a",
    "b",   "c",   "d",   "e",   "f",   "g",   "h",   "i",   "j",   "k",   "l",
    "m",   "n",   "o",   "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "{",   "|",   "}",   "~",   NULL,  "A",   "B",   "V",
    "G",   "D",   "E",   "Zh",  "Z",   "I",   "Jj",  "K",   "L",   "M",   "N",
    "O",   "P",   "R",   "S",   "T",   "U",   "F",   "Kh",  "C",   "Ch",  "Sh",
    "Shh", "\"",  "Y",   "'",   "Eh",  "Ju",  "Ja",  "a",   "b",   "v",   "g",
    "d",   "e",   "zh",  "z",   "i",   "jj",  "k",   "l",   "m",   "n",   "o",
    "p",   "#",   "#",   "#",   "|",   "+",   "+",   "+",   "+",   "+",   "+",
    "|",   "+",   "+",   "+",   "+",   "+",   "+",   "+",   "+",   "+",   "-",
    "+",   "+",   "+",   "+",   "+",   "+",   "+",   "+",   "|",   "+",   "+",
    "+",   "+",   "+",   "+",   "+",   "+",   "+",   "+",   "+",   "+",   "+",
    "+",   ".",   ".",   ".",   ".",   "r",   "s",   "t",   "u",   "f",   "kh",
    "c",   "ch",  "sh",  "shh", "\"",  "y",   "'",   "eh",  "ju",  "ja",  "Jo",
    "jo",  "Je",  "je",  "Ji",  "ji",  NULL,  NULL,  "deg", "*",    "*",  NULL,
    NULL,  "*",   ".",   " " } }
};


/*---------------------------------------------------------------------*/
/*  Glk port locale control and conversion functions                   */
/*---------------------------------------------------------------------*/

#ifdef GLK_MODULE_UNICODE
static const scr_bool gsc_has_unicode = TRUE;
#else
static const scr_bool gsc_has_unicode = FALSE;

/* Gestalt selector and stubs for non-unicode capable libraries. */
static const glui32 gestalt_Unicode = 15;

static void
glk_put_char_uni (glui32 ch)
{
  glui32 unused;
  unused = ch;
  gsc_fatal ("GLK: Stub unicode function called");
}

static void
glk_request_line_event_uni (winid_t win,
                            glui32 *buf, glui32 maxlen, glui32 initlen)
{
  (void) win;
  (void) buf;
  (void) maxlen;
  (void) initlen;
  gsc_fatal ("GLK: Stub unicode function called");
}

#endif

/*
 * Known valid character printing range.  Some Glk libraries aren't accurate
 * about what will and what won't print when queried with glk_gestalt(), so
 * we also make an explicit range check against guaranteed to print chars.
 */
static const glui32 GSC_MIN_PRINTABLE = ' ',
                    GSC_MAX_PRINTABLE = '~';


/* List of pointers to supported and available locales, NULL terminated. */
static const gsc_locale_t *const GSC_AVAILABLE_LOCALES[] = {
  &GSC_LATIN1_LOCALE,
  &GSC_CYRILLIC_LOCALE,
  NULL
};

/*
 * The locale for the game, set below explicitly or on game startup, and
 * a fallback locale to use in case none has been set.
 */
static const gsc_locale_t *gsc_locale = NULL;
static const gsc_locale_t *const gsc_fallback_locale = &GSC_LATIN1_LOCALE;

/*
 * gsc_current_locale()
 *
 * The locale to read and write with: the game's, or the fallback.
 */
static const gsc_locale_t *
gsc_current_locale (void)
{
  return gsc_locale ? gsc_locale : gsc_fallback_locale;
}


/*
 * gsc_set_locale()
 *
 * Set a locale explicitly from the name passed in.
 */
static void
gsc_set_locale (const scr_char *name)
{
  const gsc_locale_t *matched = NULL;
  const gsc_locale_t *const *iterator;
  assert (name);

  /*
   * Search locales for a matching name, abbreviated if necessary.  Stop on
   * the first match found.
   */
  for (iterator = GSC_AVAILABLE_LOCALES; *iterator; iterator++)
    {
      const gsc_locale_t *const locale = *iterator;

      if (scr_strncasecmp (name, locale->name, strlen (name)) == 0)
        {
          matched = locale;
          break;
        }
    }

  /* If matched, set the global locale. */
  if (matched)
    gsc_locale = matched;
}


/*
 * gsc_put_char_uni()
 *
 * Wrapper around glk_put_char_uni().  Handles, inelegantly, the problem of
 * having to write transcripts as ascii.
 */
static void
gsc_put_char_uni (glui32 unicode, const char *ascii)
{
  /* If there is an transcript stream, temporarily disconnect it. */
  if (gsc_transcript_stream)
    glk_window_set_echo_stream (gsc_main_window, NULL);

  glk_put_char_uni (unicode);

  /* Print ascii to the transcript, then reattach it. */
  if (gsc_transcript_stream)
    {
      if (ascii)
        glk_put_string_stream (gsc_transcript_stream, (char *) ascii);
      else
        glk_put_char_stream (gsc_transcript_stream, '?');

      glk_window_set_echo_stream (gsc_main_window, gsc_transcript_stream);
    }
}


/*
 * Tracks whether the next character written to the main window would start a
 * new line, i.e. the last thing printed there was a newline.  Used by
 * os_show_graphic() to avoid emitting a redundant leading break (and a blank
 * line) when an image is already at the start of a line.
 */
static scr_bool gsc_main_at_line_start = TRUE;

/*
 * gsc_put_char_locale()
 *
 * Write a single character using the supplied locale.  Select either the
 * main or the alternate codepage depending on the flag passed in.
 */
static void
gsc_put_char_locale (scr_char ch,
                     const gsc_locale_t *locale, scr_bool is_alternate)
{
  const gsc_codepages_t *codepage;

  /*
   * Track whether the next main-window character would start a new line, but
   * only when output is actually targeting the main window.  Status-window
   * rendering also routes through here, and must not clobber the flag that
   * os_show_graphic() relies on to decide whether to emit a leading break.
   */
  if (gsc_main_window
      && glk_stream_get_current () == glk_window_get_stream (gsc_main_window))
    {
      gsc_main_at_line_start = (ch == '\n');
      gsc_main_window_empty = FALSE;
    }
  unsigned char character;
  glui32 unicode;
  const char *ascii;

  /*
   * Select either the main or the alternate codepage for this locale, and
   * retrieve the unicode and ascii representations of the character.
   */
  codepage = is_alternate ? &locale->alternate : &locale->main;
  character = (unsigned char) ch;
  unicode = codepage->unicode[character];
  ascii = codepage->ascii[character];

  /*
   * If a unicode representation exists, use for either iso 8859-1 or, if
   * possible, direct unicode output.
   */
  if (unicode > 0)
    {
      /*
       * If unicode is in the range 1-255, this value is directly equivalent
       * to the iso 8859-1 representation; otherwise the character has no
       * direct iso 8859-1 glyph.
       */
      if (unicode < GSC_ISO_8859_EQUIVALENCE)
        {
          /*
           * If the iso 8859-1 character is one that this Glk library will
           * print exactly, print and return.  We add a check here for the
           * guaranteed printable characters, since some Glk libraries don't
           * return the correct values for gestalt_CharOutput for these.
           */
          if (unicode == '\n'
              || (unicode >= GSC_MIN_PRINTABLE && unicode <= GSC_MAX_PRINTABLE)
              || glk_gestalt (gestalt_CharOutput,
                              unicode) == gestalt_CharOutput_ExactPrint)
            {
              glk_put_char ((unsigned char) unicode);
              return;
            }
        }

      /*
       * If no usable iso 8859-1 representation, see if unicode is enabled and
       * if the Glk library can print the character exactly.  If yes, output
       * the character that way.
       *
       * TODO Using unicode output currently disrupts transcript output.  Any
       * echo stream connected for a transcript here will be a text rather than
       * a unicode stream, so probably won't output the character correctly.
       * For now, if there's a transcript, we try to write ascii output.
       */
      if (gsc_unicode_enabled)
        {
          if (glk_gestalt (gestalt_CharOutput,
                           unicode) == gestalt_CharOutput_ExactPrint)
            {
              gsc_put_char_uni (unicode, ascii);
              return;
            }
        }
    }

  /*
   * No success with iso 8859-1 or unicode, so try for an ascii substitute.
   * Substitute strings use only 7-bit ascii, and so all are safe to print
   * directly with Glk.
   */
  if (ascii)
    {
      glk_put_string ((char *) ascii);
      return;
    }

  /* No apparent way to output this character, so print a '?'. */
  glk_put_char ('?');
}


/*
 * gsc_put_char()
 * gsc_put_char_alternate()
 * gsc_put_buffer_using()
 * gsc_put_buffer()
 * gsc_put_string()
 * gsc_put_string_alternate()
 *
 * Public functions for writing using the current or fallback locale.
 */
static void
gsc_put_char (const scr_char character)
{
  gsc_put_char_locale (character, gsc_current_locale (), FALSE);
}

static void
gsc_put_char_alternate (const scr_char character)
{
  gsc_put_char_locale (character, gsc_current_locale (), TRUE);
}

static void
gsc_put_buffer_using (const scr_char *buffer,
                      scr_int length, void (*putchar_function) (scr_char))
{
  scr_int index_;

  for (index_ = 0; index_ < length; index_++)
    putchar_function (buffer[index_]);
}

static void
gsc_put_buffer (const scr_char *buffer, scr_int length)
{
  assert (buffer);

  gsc_put_buffer_using (buffer, length, gsc_put_char);
}

static void
gsc_put_string (const scr_char *string)
{
  assert (string);

  gsc_put_buffer_using (string, strlen (string), gsc_put_char);
}

static void
gsc_put_string_alternate (const scr_char *string)
{
  assert (string);

  gsc_put_buffer_using (string, strlen (string), gsc_put_char_alternate);
}


/*
 * gsc_unicode_to_locale()
 * gsc_unicode_buffer_to_locale()
 *
 * Convert a unicode character back to an scr_char through a locale.  Used for
 * reverse translations in line input.  Returns '?' if there is no translation
 * available.
 */
static scr_char
gsc_unicode_to_locale (glui32 unicode, const gsc_locale_t *locale)
{
  const gsc_codepages_t *codepage;
  scr_int character;

  /* Always use the main codepage for input. */
  codepage = &locale->main;

  /*
   * Search the unicode table sequentially for the input character.  This is
   * inefficient, but because game input is usually not copious, excusable.
   */
  for (character = 0; character < GSC_TABLE_SIZE; character++)
    {
      if (codepage->unicode[character] == unicode)
        break;
    }

  /* Return the character translation, or '?' if none. */
  return character < GSC_TABLE_SIZE ? (scr_char) character : '?';
}

static void
gsc_unicode_buffer_to_locale (const glui32 *unicode, scr_int length,
                              scr_char *buffer, const gsc_locale_t *locale)
{
  scr_int index_;

  for (index_ = 0; index_ < length; index_++)
    buffer[index_] = gsc_unicode_to_locale (unicode[index_], locale);
}


/* Colour mode; defined with the rest of the style handling further down.
   Switches later text between the game's typed colour and its normal one, and
   does nothing at all unless colour mode is on. */
static void gsc_colour_echo (scr_bool typing);


/*
 * gsc_read_line_locale()
 *
 * Read in a line and translate out of the given locale.  Returns the count
 * of characters placed in the buffer.
 */
static scr_int
gsc_read_line_locale (scr_char *buffer,
                      scr_int length, const gsc_locale_t *locale)
{
  event_t event;

  /* The Runner echoes what the player types in its own "typed" colour, a
     different one from the text the game writes back. */
  gsc_colour_echo (TRUE);

  /*
   * If we have unicode, we have to use it to ensure that characters not in
   * the Latin1 locale are properly translated.
   */
  if (gsc_unicode_enabled)
    {
      glui32 *unicode;

      /*
       * Allocate a unicode buffer long enough to hold all the characters,
       * then read in a unicode line.
       */
      unicode = (decltype(unicode)) gsc_malloc (length * sizeof (*unicode));
      memset (unicode, 0, length * sizeof (*unicode));
      glk_request_line_event_uni (gsc_main_window, unicode, length, 0);
      gsc_event_wait (evtype_LineInput, &event);

      /* Convert the unicode buffer out, then free it. */
      gsc_unicode_buffer_to_locale (unicode, event.val1, buffer, locale);
      free (unicode);

      /* Return the count of characters placed in the buffer. */
      gsc_colour_echo (FALSE);
      return event.val1;
    }

  /* No success with unicode, so fall back to standard line input. */
  glk_request_line_event (gsc_main_window, buffer, length, 0);
  gsc_event_wait (evtype_LineInput, &event);

  /* Return the count of characters placed in the buffer. */
  gsc_colour_echo (FALSE);
  return event.val1;
}


/*
 * gsc_read_line()
 *
 * Public function for reading using the current or fallback locale.
 */
static scr_int
gsc_read_line (scr_char *buffer, scr_int length)
{
  return gsc_read_line_locale (buffer, length, gsc_current_locale ());
}


/*---------------------------------------------------------------------*/
/*  Glk port status line functions                                     */
/*---------------------------------------------------------------------*/

/* Size of saved status buffer used for non-windowing Glk status lines. */
enum { GSC_STATUS_BUFFER_LENGTH = 74 };

/* Scratch space gsc_status_line_text() formats or trims a status line into. */
enum { GSC_STATUS_TEXT_LENGTH = 256 };

/* Whitespace characters, used to detect empty status elements. */
static const scr_char *const GSC_WHITESPACE = "\t\n\v\f\r ";


/*
 * gsc_is_string_usable()
 *
 * Return TRUE if string is non-null, not zero-length or contains characters
 * other than whitespace.
 */
static scr_bool
gsc_is_string_usable (const scr_char *string)
{
  /* If non-null, scan for any non-space character. */
  if (string)
    {
      scr_int index_;

      for (index_ = 0; string[index_] != '\0'; index_++)
        {
          if (!strchr (GSC_WHITESPACE, string[index_]))
            return TRUE;
        }
    }

  /* NULL, or no characters other than whitespace. */
  return FALSE;
}


/*
 * gsc_status_begin()
 * gsc_status_end()
 *
 * Open and close a status line redraw, shared by the ADRIFT <=4 and ADRIFT 5
 * status lines.  gsc_status_begin() makes the status window current, blanks
 * it, and fills its width with spaces in the bar's style so that the bar
 * spans the whole line; it returns FALSE, having done nothing, if there is
 * no status window or it has no height, in which case the caller has nothing
 * to draw.  gsc_status_end() hands the current window back to the main one.
 */
static scr_bool
gsc_status_begin (glui32 *width)
{
  glui32 height, index;

  if (!gsc_status_window)
    return FALSE;

  glk_window_get_size (gsc_status_window, width, &height);
  if (height == 0)
    return FALSE;

  glk_window_clear (gsc_status_window);
  glk_window_move_cursor (gsc_status_window, 0, 0);
  glk_set_window (gsc_status_window);

#ifdef GSC_HAVE_ZCOLORS
  /* The colours have to be named again after every clear, not once when colour
     mode is turned on: in Gargoyle a grid clear re-seeds the window's
     attributes from the library's global override colours, which would wipe a
     colour set earlier on this stream. */
  if (gsc_colour_enabled)
    garglk_set_zcolors_stream (glk_window_get_stream (gsc_status_window),
                               gsc_colour_background, gsc_colour_output);
#endif

  /* Out of colour mode the bar is a reverse-video User1 one, the way every
     other Glk port draws a status line.  In colour mode the bar is still
     reversed -- the Runner draws it as the inverse of the story text, the
     game's text colour behind and its background colour for the letters -- but
     the inversion is in the colours named just above rather than in the style.
     Doing it that way is what makes every library agree: stylehint_ReverseColor
     is honoured by some and dropped by others once a zcolor is in force, so a
     User1 bar here would come out inverted in one interpreter and plain in the
     next.  An interpreter that is ignoring the colours we name takes the
     reverse-video bar as well: the pair named just above would be dropped,
     leaving the bar in the theme's plain text colours and so indistinguishable
     from the story window. */
  glk_set_style (gsc_colour_visible () ? style_Normal : style_User1);
  for (index = 0; index < *width; index++)
    glk_put_char (' ');
  glk_window_move_cursor (gsc_status_window, 0, 0);

  return TRUE;
}

static void
gsc_status_end (void)
{
#ifdef GSC_HAVE_ZCOLORS
  /* Name the story window's own colours again.  A library whose zcolors are
     global as well as per-stream (Gargoyle) paints window backgrounds from the
     last colours it was told about, so leaving the bar's inverted pair in force
     would tint the story window too.  Safe with respect to the input echo:
     gsc_colour_echo() runs from gsc_read_line_locale(), after the status
     redraw, so it always has the last word on the prompt's colour. */
  if (gsc_colour_enabled && gsc_main_window)
    garglk_set_zcolors_stream (glk_window_get_stream (gsc_main_window),
                               gsc_colour_main_fg, gsc_colour_background);
#endif

  glk_set_window (gsc_main_window);
}


/*
 * gsc_status_line_text()
 *
 * The game's status line or, when the game does not set a usable one, the
 * score, either way with trailing whitespace removed, and using the caller's
 * buffer where it needs one.
 *
 * Authors pad StatusBoxText by hand to place it -- Three Monkeys, One Cage
 * stores "     The game is %winnable%        " -- because the Runner draws the
 * status box left-aligned.  Right-justifying that string as it stands ends the
 * line with the author's eight spaces, leaving the text itself floating short
 * of the right margin and looking mis-aligned.  The trailing run carries no
 * information here, so drop it and justify the text.  Leading spaces are kept:
 * they cost nothing at the left of a right-justified string, and are eaten
 * first by head-truncation in a narrow window.
 */
static const scr_char *
gsc_status_line_text (char *buffer, size_t length)
{
  const scr_char *status;
  size_t trimmed;

  status = scr_get_game_status_line (gsc_game);
  if (!gsc_is_string_usable (status))
    {
      snprintf (buffer, length, "Score: %ld", scr_get_game_score (gsc_game));
      return buffer;
    }

  for (trimmed = strlen (status);
       trimmed > 0 && strchr (GSC_WHITESPACE, status[trimmed - 1]);
       trimmed--)
    ;

  /* Nothing to trim, or a status too long to copy: use it where it lies. */
  if (status[trimmed] == '\0' || trimmed >= length)
    return status;

  memcpy (buffer, status, trimmed);
  buffer[trimmed] = '\0';
  return buffer;
}


/*
 * gsc_status_printed_width()
 *
 * The number of grid cells gsc_put_string() will fill for this string.  Nearly
 * always one per byte, but a character with neither an iso 8859-1 nor a
 * printable unicode form falls back to a multi-character ascii substitute (see
 * gsc_put_char_locale()) -- "sh" for a Cyrillic sha, say -- so a plain strlen()
 * can under-count.  This mirrors gsc_put_char_locale()'s decision tree so that
 * right-justification lands where the text actually ends.
 *
 * Upstream instead subtracted a flat ten-column "slop" from the right margin to
 * absorb that case, which left every status line -- all-ascii ones included --
 * floating well short of the right edge.
 */
static glui32
gsc_status_printed_width (const scr_char *string)
{
  const gsc_codepages_t *codepage = &gsc_current_locale ()->main;
  glui32 width = 0;
  scr_int index_;

  for (index_ = 0; string[index_] != '\0'; index_++)
    {
      const unsigned char character = (unsigned char) string[index_];
      const glui32 unicode = codepage->unicode[character];
      const char *const ascii = codepage->ascii[character];

      if (unicode > 0)
        {
          /* Printed as iso 8859-1, or directly as unicode: one cell either way. */
          if (unicode < GSC_ISO_8859_EQUIVALENCE
              && (unicode == '\n'
                  || (unicode >= GSC_MIN_PRINTABLE
                      && unicode <= GSC_MAX_PRINTABLE)
                  || glk_gestalt (gestalt_CharOutput,
                                  unicode) == gestalt_CharOutput_ExactPrint))
            {
              width++;
              continue;
            }
          if (gsc_unicode_enabled
              && glk_gestalt (gestalt_CharOutput,
                              unicode) == gestalt_CharOutput_ExactPrint)
            {
              width++;
              continue;
            }
        }

      /* Otherwise an ascii substitute, or the '?' stand-in for no mapping. */
      width += ascii ? (glui32) strlen (ascii) : 1;
    }

  return width;
}


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

static const scr_char *
gsc_status_next_byte (const scr_char *string)
{
  return string + 1;
}

static const gsc_status_writer_t GSC_STATUS_WRITER = {
  gsc_status_printed_width, gsc_put_string, gsc_status_next_byte
};

/* Head-truncation marker, and the least text worth printing after it. */
static const char *const GSC_STATUS_ELLIPSIS = "...";
enum { GSC_STATUS_ELLIPSIS_WIDTH = 3, GSC_STATUS_MIN_TAIL = 1 };


/*
 * gsc_status_put_right()
 *
 * Print the status text right-justified on the status line, ending one column
 * short of the right edge to match the one-column indent the room name gets at
 * the left.  The room name is already on the line, and room_end is the first
 * free column after it, so the status may use only the columns from there on,
 * and must leave at least one blank between the two.
 *
 * A status too long for that gap is truncated at its head: the tail carries
 * the parts that change -- score, moves, time, whatever the game keeps there --
 * so it is the head that gets replaced with "...".  If not even the "..." and a
 * character of text will fit, the status is dropped rather than allowed to run
 * into the room name.
 */
static void
gsc_status_put_right (glui32 width, glui32 room_end,
                      const scr_char *status,
                      const gsc_status_writer_t *writer)
{
  glui32 avail, status_width;

  /* Columns between the room name (plus one blank) and the right margin. */
  if (width < room_end + 2)
    return;
  avail = width - room_end - 2;

  status_width = writer->width (status);
  if (status_width <= avail)
    {
      glk_window_move_cursor (gsc_status_window,
                              width - status_width - 1, 0);
      writer->print (status);
      return;
    }

  /* Too wide: find the longest tail that fits alongside the "...". */
  if (avail < GSC_STATUS_ELLIPSIS_WIDTH + GSC_STATUS_MIN_TAIL)
    return;

  while (*status != '\0')
    {
      status = writer->next (status);

      status_width = writer->width (status);
      if (status_width <= avail - GSC_STATUS_ELLIPSIS_WIDTH)
        {
          glk_window_move_cursor (gsc_status_window,
                                  width - status_width
                                  - GSC_STATUS_ELLIPSIS_WIDTH - 1, 0);
          writer->print (GSC_STATUS_ELLIPSIS);
          writer->print (status);
          return;
        }
    }
}


/*
 * gsc_status_update()
 *
 * Update the status line from the current game state.  This is for windowing
 * Glk libraries.
 */
static void
gsc_status_update (void)
{
  glui32 width;
  assert (gsc_status_window);

  if (gsc_status_begin (&width))
    {
      const scr_char *room;

      /* See if the game is indicating any current player room. */
      room = scr_get_game_room (gsc_game);
      if (!gsc_is_string_usable (room))
        {
          /*
           * Player location is indeterminate, so print out a generic status,
           * showing the game name and author.
           */
          glk_window_move_cursor (gsc_status_window, 1, 0);
          gsc_put_string (scr_get_game_name (gsc_game));
          gsc_put_literal (" | ");
          gsc_put_string (scr_get_game_author (gsc_game));
        }
      else
        {
          const scr_char *status;
          char text[GSC_STATUS_TEXT_LENGTH] = {0};

          /* Print the player location. */
          glk_window_move_cursor (gsc_status_window, 1, 0);
          gsc_put_string (room);

          /* Get the game's status line, or if none, format score. */
          status = gsc_status_line_text (text, sizeof (text));

          /* Print the status line or score at window right. */
          gsc_status_put_right (width, 1 + gsc_status_printed_width (room),
                                status, &GSC_STATUS_WRITER);
        }

      gsc_status_end ();
    }
}


/*
 * gsc_status_safe_strcat()
 *
 * Helper for gsc_status_print(), concatenates strings only up to the
 * available length.
 */
static void
gsc_status_safe_strcat (char *dest, size_t length, const char *src)
{
  size_t available, src_length;

  /* Append only as many characters as will fit. */
  src_length = strlen (src);
  available = length - strlen (dest) - 1;
  if (available > 0)
    strncat (dest, src, src_length < available ? src_length : available);
}


/*
 * gsc_status_print()
 *
 * Print the current contents of the completed status line buffer out in the
 * main window, if it has changed since the last call.  This is for non-
 * windowing Glk libraries.
 */
static void
gsc_status_print (void)
{
  static char current_status[GSC_STATUS_BUFFER_LENGTH + 1];

  const scr_char *room;

  /* Do nothing if the game isn't indicating any current player room. */
  room = scr_get_game_room (gsc_game);
  if (gsc_is_string_usable (room))
    {
      char buffer[GSC_STATUS_BUFFER_LENGTH + 1] = {0};
      const scr_char *status;
      char text[GSC_STATUS_TEXT_LENGTH] = {0};

      /* Make an attempt at a status line, starting with player location. */
      gsc_status_safe_strcat (buffer, sizeof (buffer), room);

      /* Get the game's status line, or if none, format score. */
      status = gsc_status_line_text (text, sizeof (text));

      /* Append the status line or score. */
      gsc_status_safe_strcat (buffer, sizeof (buffer), " | ");
      gsc_status_safe_strcat (buffer, sizeof (buffer), status);

      /* If this matches the current saved status line, do nothing more. */
      if (strcmp (buffer, current_status) != 0)
        {
          /* Bracket, and output the status line buffer. */
          gsc_put_literal ("[ ");
          gsc_put_string (buffer);
          gsc_put_literal (" ]\n");

          /* Save the details of the printed status buffer. */
          snprintf(current_status, sizeof(current_status), "%s", buffer);
        }
    }
}


/*
 * gsc_status_notify()
 *
 * Front end function for updating status.  Either updates the status window
 * or prints the status line to the main window.
 */
static void
gsc_status_notify (void)
{
  if (gsc_is_a5)
    {
      /* ADRIFT 5 games have no scare game state, so both v4 renderers would
         ask scr_* about a NULL game and paint "[invalid game]".  os_confirm()
         reaches here for the shared hint display and the quit/restart
         prompts, which both engines use. */
      if (gsc_a5_run)
        gsc_a5_status (gsc_a5_run);
      return;
    }

  if (gsc_status_window)
    gsc_status_update ();
  else
    gsc_status_print ();
}


/*
 * gsc_status_redraw()
 *
 * Redraw the contents of any status window with the constructed status string.
 * This function should be called on the appropriate Glk window resize and
 * arrange events.
 */
static void
gsc_status_redraw (void)
{
  if (gsc_status_window)
    {
      winid_t parent;

      /*
       * Rearrange the status window, without changing its actual arrangement
       * in any way.  This is a hack to work round incorrect window repainting
       * in Xglk; it forces a complete repaint of affected windows on Glk
       * window resize and arrange events, and works in part because Xglk
       * doesn't check for actual arrangement changes in any way before
       * invalidating its windows.  The hack should be harmless to Glk
       * libraries other than Xglk, moreover, we're careful to activate it
       * only on resize and arrange events.
       */
      parent = glk_window_get_parent (gsc_status_window);
      glk_window_set_arrangement (parent,
                                  winmethod_Above | winmethod_Fixed, 1, NULL);
      if (gsc_is_a5)
        {
          /* ADRIFT 5 games have no scare game state; use the a5 renderer. */
          if (gsc_a5_run)
            gsc_a5_status (gsc_a5_run);
        }
      else
        gsc_status_update ();
    }

  /* The map is rasterised to the pixel size of its window, so a resize has to
     redraw it, not just repaint it -- and the window has lost what we drew
     there, so all of it must go out again, not just what the game changed. */
  gsc_map_full_flush = TRUE;
  gsc_map_redraw ();
}


/*---------------------------------------------------------------------*/
/*  Glk port output functions                                          */
/*---------------------------------------------------------------------*/

/*
 * Flag for if the user entered "help" as their last input, or if hints have
 * been silenced as a result of already using a Glk command.
 */
static int gsc_help_requested = FALSE,
           gsc_help_hints_silenced = FALSE;

/*
 * Symbol fonts.  A <font face="..."> naming one of these selects a font whose
 * bytes are pictograms rather than letters, so the text inside the tag has to
 * be translated to the equivalent Unicode symbols rather than printed as
 * Latin-1 (see gsc_put_string_symbol()).
 */
typedef enum {
  GSC_SYMBOL_NONE = 0,
  GSC_SYMBOL_WEBDINGS
} gsc_symbol_font_t;

/*
 * Adrift text colours ("glk colour").
 *
 * Adrift games are written for a Runner that paints its output pane in a
 * palette rather than in the interpreter's theme: a background, an "output"
 * (replies) colour for the story, and an "input" (typed) colour that <c>...</c>
 * also asks for, on top of whatever <font colour="..."> the text names.  Glk
 * has no colour of its own, so by default this port drops all of that and
 * shows the story in the interpreter's styles, as SCARE always has.
 *
 * "glk colour on" turns the palette back on, through the Gargoyle/Spatterlight
 * garglk_set_zcolors extension.  Where that extension is missing (cheapglk,
 * glkterm) the mode cannot be offered at all, so everything below compiles out
 * and the command says so.
 *
 * A game that cannot be read without the palette -- an ADRIFT 5 adventure that
 * set colours of its own, or text that paints a background or names a colour
 * too close to the interpreter's to see -- turns the mode on for itself at load
 * time, before a word is printed; gsc_colour_detect is what asks.
 *
 * Where the palette comes from differs by engine.  ADRIFT 5 stores it in the
 * adventure itself (a5model.h bg_colour and friends).  ADRIFT <=4 stores none:
 * the .taf has no colour fields, and the colours are Runner preferences.  The
 * defaults below are the ones run400.exe ships, read out of the settings it
 * writes under HKCU\Software\VB and VBA Program Settings\ADRIFT\Runner:
 * Background 0, Text1 3289855 and Text2 65280 -- OLE colour integers (low byte
 * red), i.e. black, #FF3232 for typed text and #00FF00 for replies.  They match
 * the swatches in the Runner's own Options -> Display & Media dialog ("Set
 * typed colour", "Set replies colour", "Set background colour").
 */
/* GSC_HAVE_ZCOLORS, the palette itself and the on/off flag are all declared
   with the other module options at the top of the file, where the status line
   -- which draws itself in the game's colours as well -- can see them. */

/* "No colour set here": outside the 24-bit range an RGB colour occupies, and
   distinct from the zcolor_* sentinels, which are at the top of the range. */
static const glui32 GSC_COLOUR_NONE = 0x01000000;

/* Font descriptor type, encapsulating size, monospaced boolean, face, and the
   text colour a <font colour="..."> asked for (GSC_COLOUR_NONE when it asked
   for none, which inherits the enclosing colour). */
typedef struct {
  scr_bool is_monospaced;
  scr_int size;
  gsc_symbol_font_t symbol_font;
  glui32 colour;
} gsc_font_size_t;

/* Font stack and attributes for nesting tags. */
enum { GSC_MAX_STYLE_NESTING = 32 };
static gsc_font_size_t gsc_font_stack[GSC_MAX_STYLE_NESTING];
static glui32 gsc_font_index = 0;
static glui32 gsc_attribute_bold = 0,
              gsc_attribute_italic = 0,
              gsc_attribute_underline = 0,
              gsc_attribute_secondary_colour = 0,
              gsc_attribute_center = 0,
              gsc_attribute_right = 0;

/* Notional default font size, and limit font sizes. */
static const scr_int GSC_DEFAULT_FONT_SIZE = 12,
                    GSC_MEDIUM_FONT_SIZE = 14,
                    GSC_LARGE_FONT_SIZE = 16;

/* Milliseconds per second and timeouts count for delay tags. */
static const glui32 GSC_MILLISECONDS_PER_SECOND = 1000;
static const glui32 GSC_TIMEOUTS_COUNT = 10;

/* Number of hints to refuse before offering to end hint display. */
static const scr_int GSC_HINT_REFUSAL_LIMIT = 5;

/* The keypresses used to cancel any <wait x.x> early. */
static const glui32 GSC_CANCEL_WAIT_1 = ' ',
                    GSC_CANCEL_WAIT_2 = keycode_Return;


/*
 * gsc_note_help_request()
 * gsc_output_silence_help_hints()
 * gsc_output_provide_help_hint()
 *
 * Register a standalone "help" at the start of the player's input, and print
 * a note of how to get Glk command help from the interpreter unless silenced.
 */
static void
gsc_note_help_request (const char *command)
{
  if (scr_strncasecmp (command, "help", strlen ("help")) == 0
      && strspn (command + strlen ("help"), "\t ")
         == strlen (command + strlen ("help")))
    gsc_help_requested = TRUE;
}

static void
gsc_output_silence_help_hints (void)
{
  gsc_help_hints_silenced = TRUE;
}

static void
gsc_output_provide_help_hint (void)
{
  if (gsc_help_requested && !gsc_help_hints_silenced)
    {
      glk_set_style (style_Emphasized);
      gsc_put_literal ("[Try 'glk help' for help on special interpreter"
                       " commands]\n");

      gsc_help_requested = FALSE;
      glk_set_style (style_Normal);
    }
}


/*
 * gsc_colour_lookup()
 *
 * Turn an Adrift colour token -- a name, or a hex triplet with or without its
 * leading '#' -- into an 0xRRGGBB colour.  Returns GSC_COLOUR_NONE for the
 * empty token and for "default", both of which mean "back to the surrounding
 * colour" rather than a colour of their own.
 *
 * The names, and their values, are the Runner's own table (Global.ColourLookup),
 * including its aliases (cyan/turquoise/aqua, magenta/fuchsia) and its
 * treatment of anything unrecognised as a hex triplet right-padded with zeros.
 * `token` must already be lowercased and unquoted.
 */
static glui32
gsc_colour_lookup (const char *token)
{
  static const struct { const char *name; glui32 rgb; } COLOUR_NAMES[] = {
    {"black",     0x000000}, {"blue",      0x0000ff},
    {"cyan",      0x00ffff}, {"turquoise", 0x00ffff}, {"aqua", 0x00ffff},
    {"gray",      0x808080}, {"grey",      0x808080},
    {"green",     0x008000}, {"lime",      0x00ff00},
    {"magenta",   0xff00ff}, {"fuchsia",   0xff00ff},
    {"maroon",    0x800000}, {"navy",      0x000080},
    {"olive",     0x808000}, {"orange",    0xff8000},
    {"pink",      0xff8888}, {"purple",    0x800080},
    {"red",       0xff0000}, {"silver",    0xc0c0c0},
    {"teal",      0x008080}, {"white",     0xffffff},
    {"yellow",    0xffff00}, {NULL, 0}
  };
  char hex[7];
  int index_;
  glui32 rgb;

  if (token == NULL || token[0] == '\0' || strcmp (token, "default") == 0)
    return GSC_COLOUR_NONE;

  for (index_ = 0; COLOUR_NAMES[index_].name; index_++)
    {
      if (strcmp (token, COLOUR_NAMES[index_].name) == 0)
        return COLOUR_NAMES[index_].rgb;
    }

  /* Not a name, so a hex triplet.  The Runner drops a leading '#', gives up on
     anything longer than six digits (white), and right-pads a short one with
     zeros -- "<font colour=f00>" is 0xf00000 there, not 0xff0000. */
  if (token[0] == '#')
    token++;
  if (strlen (token) > 6)
    return 0xffffff;
  for (index_ = 0; index_ < 6; index_++)
    hex[index_] = token[index_] != '\0' ? token[index_] : '0';
  hex[6] = '\0';
  for (index_ = 0; index_ < 6; index_++)
    {
      if (!isxdigit ((unsigned char) hex[index_]))
        return GSC_COLOUR_NONE;
    }
  rgb = (glui32) strtoul (hex, NULL, 16);
  return rgb;
}


/*
 * gsc_colour_from_tag()
 *
 * Dig the value of `key` -- "colour" or "bgcolour" -- out of a tag argument,
 * accepting the American spelling too, and return the colour it names.
 * Returns GSC_COLOUR_NONE when the tag carries no such attribute, so a
 * <font face=...> with no colour leaves the colour alone.
 *
 * The key has to match at a word boundary or "colour" would also be found
 * inside "bgcolour", and a background would silently become a foreground.
 *
 * Whitespace is allowed on either side of the '=', as it is in the HTML the
 * Runner hands its tags to; see a5_font_colour() in adrift5/a5text.cpp.
 */
static glui32
gsc_colour_from_tag (const scr_char *tag, const char *key)
{
  char american[16], token[32];
  const char *keys[2];
  scr_char *lower;
  glui32 result = GSC_COLOUR_NONE;
  int pass;
  scr_int index_;

  /* Both the key and the value it finds are compared lowercased; the tag
     arrives as the author wrote it. */
  lower = (decltype (lower)) gsc_malloc (strlen (tag) + 1);
  memcpy (lower, tag, strlen (tag) + 1);
  for (index_ = 0; lower[index_] != '\0'; index_++)
    lower[index_] = glk_char_to_lower (lower[index_]);

  /* "colour" -> "color": drop the 'u' before the final 'r'. */
  {
    size_t length = strlen (key), index_ = 0, out = 0;

    assert (length < sizeof (american));
    for (; index_ < length; index_++)
      {
        if (!(key[index_] == 'u' && index_ + 2 == length))
          american[out++] = key[index_];
      }
    american[out] = '\0';
  }
  keys[0] = key;
  keys[1] = american;

  for (pass = 0; pass < 2 && result == GSC_COLOUR_NONE; pass++)
    {
      const scr_char *at = lower;

      while ((at = strstr (at, keys[pass])) != NULL)
        {
          const scr_char *value;
          size_t length = 0;

          if (at > lower && (isalnum ((unsigned char) at[-1]) || at[-1] == '_'))
            {
              at += strlen (keys[pass]);
              continue;
            }
          value = at + strlen (keys[pass]);
          while (isspace ((unsigned char) *value))
            value++;
          if (*value != '=')
            {
              /* A "colour" that is not an attribute assignment at all. */
              at += strlen (keys[pass]);
              continue;
            }
          value++;
          while (isspace ((unsigned char) *value))
            value++;
          if (*value == '"' || *value == '\'')
            {
              scr_char quote = *value++;
              while (value[length] != '\0' && value[length] != quote)
                length++;
            }
          else
            {
              while (value[length] != '\0'
                     && !isspace ((unsigned char) value[length])
                     && value[length] != '>' && value[length] != '"')
                length++;
            }
          if (length >= sizeof (token))
            length = sizeof (token) - 1;
          memcpy (token, value, length);
          token[length] = '\0';
          result = gsc_colour_lookup (token);
          break;
        }
    }

  free (lower);
  return result;
}


#ifdef GSC_HAVE_ZCOLORS
/*
 * gsc_colour_luminance()
 * gsc_colour_legible()
 *
 * Whether text in one colour can be read against another.  `gsc_colour_legible`
 * answers the WCAG contrast test -- the ratio of the two relative luminances,
 * each lifted by 0.05, against a 3:1 floor, which is the threshold for text
 * that is large or merely decorative rather than a page of body copy.
 *
 * Luminance is the sRGB one with the transfer curve approximated as a plain
 * square rather than the piecewise 2.4 power, which keeps this in integers (the
 * scale is 255*255) and, checked against the exact formula over the Runner's
 * whole colour table, changes no verdict here: red, green, blue, teal, navy and
 * grey stay legible on white, while white, silver, yellow, lime, cyan, orange
 * and pink stay illegible on it.
 */
static glui32
gsc_colour_luminance (glui32 rgb)
{
  const glui32 r = (rgb >> 16) & 0xff, g = (rgb >> 8) & 0xff, b = rgb & 0xff;

  return (2126 * r * r + 7152 * g * g + 722 * b * b) / 10000;
}

static scr_bool
gsc_colour_legible (glui32 fg, glui32 bg)
{
  /* 0.05 of the 255*255 luminance scale, the constant WCAG adds to both sides
     so that black on black is a ratio of 1 rather than a division by zero. */
  static const glui32 FLARE = 3251;
  const glui32 one = gsc_colour_luminance (fg) + FLARE,
               two = gsc_colour_luminance (bg) + FLARE;
  const glui32 lighter = one > two ? one : two,
               darker = one > two ? two : one;

  return lighter >= 3 * darker;
}


/*
 * gsc_colour_wanted_t
 * gsc_colour_wanted_by()
 *
 * Decide, from one authored string, whether this game was written for the
 * Runner's own pane rather than for an interpreter theme -- the question
 * gsc_colour_detect() asks of every string in the game.
 *
 * Two things in a string answer it, and a bare <font colour="..."> is not one
 * of them: a game that puts a single red word in a paragraph is not asking for
 * a black screen, and a quarter of the ADRIFT <=4 corpus embeds a colour that
 * mild.  What does answer it is
 *
 *   - <bgcolour="..."> (ADRIFT <=4), which repaints the output pane outright.
 *     Without colour mode the repaint has nowhere to land, so the text it was
 *     meant to sit behind arrives on whatever the theme provides;
 *
 *   - a <font colour="..."> the theme cannot show: white, yellow, lime and the
 *     other pale colours authors reach for when they know the pane behind them
 *     is black.  On a light theme that text is invisible, which is the failure
 *     colour mode exists to prevent; on a dark theme it reads perfectly well
 *     already, and `bg` being the measured theme background is what lets the
 *     same game decide differently in each.
 *
 * <c> needs no case of its own even though it colours text too: it names the
 * palette's input colour, which for ADRIFT <=4 is the Runner's red (legible on
 * any theme) and for ADRIFT 5 is the author's, already caught by
 * a5model_custom_palette.
 */
typedef struct {
  glui32 background;      /* what the story text would otherwise sit on */
} gsc_colour_wanted_t;

static scr_bool
gsc_colour_wanted_by (const scr_char *text, void *opaque)
{
  const gsc_colour_wanted_t *wanted = (const gsc_colour_wanted_t *) opaque;
  const scr_char *at;

  for (at = text; (at = strchr (at, '<')) != NULL; at++)
    {
      const scr_char *end = strchr (at, '>');
      scr_char *tag;
      size_t length;
      glui32 colour;

      if (end == NULL)
        break;

      /* Only <font ...> and <bgcolour=...> can carry one, and every other tag
         in the language is far commoner than either; test the name first so
         the tag body is only copied for the few that could match. */
      length = (size_t) (end - at) - 1;
      if (scr_strncasecmp (at + 1, "font", 4) != 0
          && scr_strncasecmp (at + 1, "bgcolo", 6) != 0)
        continue;

      tag = (decltype (tag)) gsc_malloc (length + 1);
      memcpy (tag, at + 1, length);
      tag[length] = '\0';

      colour = gsc_colour_from_tag (tag, "bgcolour");
      if (colour != GSC_COLOUR_NONE)
        {
          free (tag);
          return TRUE;
        }
      colour = gsc_colour_from_tag (tag, "colour");
      free (tag);
      if (colour != GSC_COLOUR_NONE
          && !gsc_colour_legible (colour, wanted->background))
        return TRUE;

      at = end;
    }
  return FALSE;
}


/*
 * gsc_colour_detect()
 *
 * Whether this game starts in its own palette without being asked for it.
 *
 * ADRIFT 5 answers from its header: an adventure whose palette differs from the
 * Runner's default has a look the author sat down and chose, and showing it in
 * the interpreter's theme instead throws that away.  ADRIFT <=4 has no header
 * to ask -- the .taf carries no colour fields at all, the palette being a Runner
 * preference -- so both engines fall back on the authored text, which is where
 * gsc_colour_wanted_by describes what counts.
 *
 * Called while the loading window is still up, so the theme's background can be
 * measured before colour mode publishes stylehints of its own; the answer feeds
 * gsc_colour_startup, which the -c switch also sets, and so reaches the palette
 * by the same path -- hints published before the first window open, and no
 * rebuild.  That also means it is never consulted on an autorestore, which is
 * right: a restored session brings back the mode the player left it in.
 */
static scr_bool
gsc_colour_detect (winid_t window)
{
  gsc_colour_wanted_t wanted;
  glui32 measured;

  /* White when the library cannot measure: an interpreter that answers nothing
     is likeliest to be showing the light background Glk defaults to, and that
     is the case where pale authored text disappears. */
  wanted.background = 0xffffff;
  if (window != NULL
      && glk_style_measure (window, style_Normal, stylehint_BackColor,
                            &measured))
    wanted.background = measured;

  if (gsc_is_a5)
    return gsc_a5_adv != NULL
           && (a5model_custom_palette (gsc_a5_adv)
               || a5model_scan_text (gsc_a5_adv, gsc_colour_wanted_by,
                                     &wanted));
  return gsc_game != NULL
         && scr_game_scan_strings (gsc_game, gsc_colour_wanted_by, &wanted);
}
#endif


/*
 * gsc_colour_apply()
 *
 * Set the colours later text written to `win` comes out in: `fg` if it names
 * one, otherwise the game's normal output colour, always over the game's
 * background.  Does nothing at all unless the colour mode is on, so every
 * caller can call it unconditionally.
 */
static void
gsc_colour_apply (winid_t win, glui32 fg)
{
#ifdef GSC_HAVE_ZCOLORS
  if (!gsc_colour_enabled || win == NULL)
    return;
  if (fg == GSC_COLOUR_NONE)
    fg = gsc_colour_output;
  if (win == gsc_main_window)
    gsc_colour_main_fg = fg;
  garglk_set_zcolors_stream (glk_window_get_stream (win),
                             fg, gsc_colour_background);
#else
  (void) win;
  (void) fg;
#endif
}


/*
 * gsc_colour_echo()
 *
 * Switch between the colour the player's typing is echoed in and the colour
 * the game's own text comes out in; see the forward declaration above
 * gsc_read_line_locale().
 */
static void
gsc_colour_echo (scr_bool typing)
{
  gsc_colour_apply (gsc_main_window,
                    typing ? gsc_colour_input : GSC_COLOUR_NONE);
}


/*
 * gsc_put_prompt()
 *
 * Print the input prompt.  The Runner has no inline prompt of its own -- it
 * takes commands in a separate box, drawn in the "input" colour, the same
 * secondary colour <c> spans use -- so in colour mode the ">" belongs to the
 * typing it introduces rather than to the game's output, and is written in
 * that colour.  Outside colour mode gsc_colour_echo does nothing and this is
 * an ordinary print.
 *
 * The colour is deliberately left in force: what follows a prompt is always
 * the player's line, which the read routines echo in the same ink before
 * putting the output colour back.
 */
static void
gsc_put_prompt (const char *prompt)
{
  gsc_colour_echo (TRUE);
  gsc_put_literal (prompt);
}


/*
 * gsc_font_top()
 *
 * The current top of the font stack, or the default font on an empty stack.
 */
static gsc_font_size_t
gsc_font_top (void)
{
  gsc_font_size_t font;

  if (gsc_font_index > 0)
    font = gsc_font_stack[gsc_font_index - 1];
  else
    {
      font.is_monospaced = FALSE;
      font.size = GSC_DEFAULT_FONT_SIZE;
      font.symbol_font = GSC_SYMBOL_NONE;
      font.colour = GSC_COLOUR_NONE;
    }
  return font;
}


/*
 * gsc_set_glk_style()
 *
 * Set a Glk style based on the top of the font stack and attributes.
 */
static void
gsc_set_glk_style (void)
{
  const gsc_font_size_t font = gsc_font_top ();
  const scr_bool is_monospaced = font.is_monospaced;
  const scr_int font_size = font.size;

  /*
   * In colour mode the Adrift colours ride alongside the Glk style: an
   * explicit <font colour=...> if one is open, otherwise the typed colour
   * while <c> is (the Runner's "replies" colour being the normal one).  The
   * Glk style is still set below, so bold and centering keep working; only
   * the ink changes.
   */
  gsc_colour_apply (gsc_main_window,
                    font.colour != GSC_COLOUR_NONE ? font.colour
                    : gsc_attribute_secondary_colour > 0 ? gsc_colour_input
                    : GSC_COLOUR_NONE);

  /*
   * Map the font and current attributes into a Glk style.  Because Glk styles
   * aren't cumulative this has to be done by precedences.
   */
  if (is_monospaced)
    {
      /*
       * No matter the size or attributes, if monospaced use Preformatted
       * style, as it's all we have.
       */
      glk_set_style (style_Preformatted);
    }
  else if (gsc_attribute_right > 0)
    {
      /* RightFlush-hinted style_Note; character styles are not combined. */
      glk_set_style (style_Note);
    }
  else if (gsc_attribute_center > 0)
    {
      /*
       * Centered sections use the two justification-hinted user styles set up
       * before the main window opened: User1 for plain centered text, User2
       * for centered bold (title lines are typically <center><b>..., and Glk
       * styles don't combine, so bold gets its own centered style).
       */
      glk_set_style (gsc_attribute_bold > 0 ? style_User2 : style_User1);
    }
  else
    {
      /*
       * For large and medium point sizes, use Header or Subheader styles
       * respectively.
       */
      if (font_size >= GSC_LARGE_FONT_SIZE)
        glk_set_style (style_Header);
      else if (font_size >= GSC_MEDIUM_FONT_SIZE)
        glk_set_style (style_Subheader);
      else
        {
          /*
           * For bold, use Subheader; for italics or underline, use Emphasized.
           * With colours disabled, <c> secondary-colour text keeps its old
           * Emphasized stand-in so it stays visually distinct; bold still
           * wins over it, as it always has.
           */
          if (gsc_attribute_bold > 0
              && (gsc_attribute_italic > 0 || gsc_attribute_underline > 0))
            glk_set_style (style_Alert);
          else if (gsc_attribute_bold > 0)
            glk_set_style (style_Subheader);
          else if (gsc_attribute_italic > 0
                   || gsc_attribute_underline > 0
                   || (gsc_attribute_secondary_colour > 0
                       && !gsc_colour_visible ()))
            glk_set_style (style_Emphasized);
          else
            {
              /*
               * There's nothing special about this text, so drop down to
               * Normal style.
               */
              glk_set_style (style_Normal);
            }
        }
    }
}


/*
 * Webdings-to-Unicode translation table, indexed by character code.
 *
 * Webdings is a symbol font: its byte values are pictograms, not letters, so a
 * game that writes <font face="Webdings">...</font> is drawing pictures with
 * what would otherwise be text.  Glk has no way to select a font by name, but
 * most Webdings pictograms were given Unicode code points in Unicode 7.0 (the
 * Wingdings/Webdings additions), so they can be reproduced by translating to
 * the equivalent Unicode character and printing that instead.
 *
 * The character-to-pictogram assignments were read off the glyph names in the
 * `post` table of the Webdings font itself, rather than guessed: entry 0xFF,
 * for instance, is the glyph named "peace", i.e. U+1F54A DOVE OF PEACE.  That
 * is the one this table actually exists for -- Topaz (Woodfish) draws a dove
 * under its title with <font face="Webdings" size=72>\xFF</font>, which
 * without translation prints as a stray "y-umlaut".  The rest are filled in
 * where the Unicode equivalent is unambiguous; entries left 0 have no good
 * equivalent and print as '?'.
 */
static const glui32 GSC_WEBDINGS_TO_UNICODE[] = {
  /* 0x00 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /* 0x20 space, spider, web, no-piracy, sunglasses, trophy, medal, link */
  0x0020, 0x1F577, 0x1F578, 0x1F572, 0x1F576, 0x1F3C6, 0x1F396, 0x1F517,
  /* 0x28 speech bubbles, then new/updated/hot/ribbon/checkerboard (no map) */
  0x1F5E8, 0x1F5E9, 0, 0, 0, 0, 0, 0,
  /* 0x30 window controls (no map), then transport controls */
  0, 0, 0, 0, 0, 0, 0, 0x23EA,
  0x23E9, 0x23EE, 0x23ED, 0x23F8, 0x23F9, 0x23FA, 0, 0x1F5F3,
  /* 0x40 tools, construction, town/city/site/desert/factory (no map), home */
  0x1F6E0, 0x1F6A7, 0, 0, 0, 0, 0, 0,
  0x1F3E0, 0x1F3D6, 0x1F3DD, 0x1F6E3, 0, 0x26F0, 0x1F441, 0x1F442,
  /* 0x50 park, tent, rail (no map), stadium, ship, sound on/off */
  0x1F3DE, 0x26FA, 0, 0x1F3DF, 0x1F6F3, 0x1F50A, 0x1F507, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  /* 0x60 loop/check (no map), bicycle, ... fire ... */
  0, 0, 0x1F6B2, 0, 0, 0, 0x1F525, 0,
  0x2695, 0x2139, 0x1F6E9, 0x1F6F0, 0, 0, 0, 0x26F5,
  /* 0x70 police, refresh/close/help (no map), train, metro, bus, flag */
  0x1F693, 0, 0, 0, 0x1F686, 0x1F687, 0x1F68D, 0x1F6A9,
  0, 0x26D4, 0x1F6AD, 0, 0, 0, 0, 0,
  /* 0x80 men, women, boy/girl (no map), baby */
  0x1F6B9, 0x1F6BA, 0, 0, 0x1F6BC, 0, 0, 0x26F7,
  0, 0x1F3CC, 0x1F3CA, 0x1F3C4, 0x1F3CD, 0x1F3CE, 0x1F698, 0,
  /* 0x90 ... credit card ... mail ... */
  0, 0, 0, 0x1F4B3, 0, 0, 0, 0,
  0, 0, 0, 0x1F582, 0, 0, 0, 0,
  /* 0xA0 ... clipboard, calendar, book, news, art, theatre, music */
  0, 0, 0, 0, 0x1F4CB, 0, 0x1F4C5, 0x1F56E,
  0, 0x1F5DE, 0, 0, 0, 0x1F3A8, 0x1F3AD, 0x1F3B5,
  /* 0xB0 microphone, headphones, disc, film frames, ticket, clapper, video */
  0, 0x1F3A4, 0x1F3A7, 0x1F4BF, 0x1F39E, 0, 0x1F39F, 0x1F3AC,
  0, 0x1F4F9, 0, 0x1F4FB, 0x1F39A, 0x1F39B, 0x1F4FA, 0,
  /* 0xC0 ... joystick ... fax, pager, mobile ... printer */
  0, 0, 0, 0x1F579, 0, 0, 0x1F4E0, 0x1F4DF,
  0x1F4F1, 0, 0x1F5A8, 0, 0, 0, 0, 0x1F512,
  /* 0xD0 unlocked, ... weather from 0xD5 */
  0x1F513, 0, 0, 0, 0, 0x2600, 0x1F324, 0x1F325,
  0x1F326, 0x2601, 0x1F328, 0x1F327, 0x1F329, 0x1F32A, 0x1F32C, 0x1F32B,
  /* 0xE0 moon, thermometer, ... dining ... parking, wheelchair, warning */
  0x1F319, 0x1F321, 0, 0, 0x1F374, 0, 0, 0,
  0x1F17F, 0x267F, 0x26A0, 0, 0, 0, 0, 0,
  /* 0xF0 ... airplane, animal, bird, fish, dog, cat, rockets */
  0, 0x2708, 0, 0x1F426, 0x1F41F, 0x1F415, 0x1F408, 0,
  /* 0xF8 rockets (no map), world map, globes, and the peace dove at 0xFF */
  0, 0, 0, 0x1F5FA, 0x1F30D, 0x1F30E, 0x1F30F, 0x1F54A
};

/*
 * gsc_put_string_symbol()
 *
 * Write a string whose bytes are symbol-font pictograms, translating each to
 * its Unicode equivalent.  Characters with no equivalent, and any character
 * the Glk library cannot print exactly, fall back to '?' -- the same thing
 * gsc_put_char_locale() does when it runs out of options.
 */
static void
gsc_put_string_symbol (const scr_char *string, gsc_symbol_font_t symbol_font)
{
  scr_int index_;

  assert (string);

  /* This path writes through glk_put_char directly rather than through
     gsc_put_char_locale, so mark the window here as well. */
  if (string[0] != '\0' && gsc_main_window
      && glk_stream_get_current () == glk_window_get_stream (gsc_main_window))
    gsc_main_window_empty = FALSE;

  for (index_ = 0; string[index_] != '\0'; index_++)
    {
      const unsigned char character = (unsigned char) string[index_];
      glui32 unicode = 0;

      if (symbol_font == GSC_SYMBOL_WEBDINGS)
        unicode = GSC_WEBDINGS_TO_UNICODE[character];

      /*
       * Newlines are layout, not pictograms, and have to survive translation
       * so that centring and line breaks still work inside the tag.
       */
      if (character == '\n')
        {
          glk_put_char ('\n');
          continue;
        }

      if (unicode > 0 && unicode < GSC_ISO_8859_EQUIVALENCE)
        {
          glk_put_char ((unsigned char) unicode);
          continue;
        }

      if (unicode > 0 && gsc_unicode_enabled
          && glk_gestalt (gestalt_CharOutput,
                          unicode) == gestalt_CharOutput_ExactPrint)
        {
          gsc_put_char_uni (unicode, NULL);
          continue;
        }

      glk_put_char ('?');
    }
}


/*
 * gsc_handle_font_tag()
 * gsc_handle_endfont_tag()
 *
 * Push the settings of a font tag onto the font stack, and pop on end of
 * font tag.  Set the appropriate Glk style.
 */
static void
gsc_handle_font_tag (const scr_char *argument)
{
  /* Ignore the call on stack overrun. */
  if (gsc_font_index < GSC_MAX_STYLE_NESTING)
    {
      scr_char *lower, *face, *size;
      scr_int index_;

      /* Start from the current top of stack, or default on empty stack. */
      gsc_font_size_t font = gsc_font_top ();

      /* Copy and convert argument to all lowercase. */
      lower = (decltype(lower)) gsc_malloc (strlen (argument) + 1);
      memcpy (lower, argument, strlen (argument) + 1);
      for (index_ = 0; lower[index_] != '\0'; index_++)
        lower[index_] = glk_char_to_lower (lower[index_]);

      /* Find any face= portion of the tag argument. */
      face = strstr (lower, "face=");
      if (face)
        {
          /*
           * There may be plenty of monospaced fonts, but we do only courier
           * and terminal.
           */
          font.is_monospaced = strncmp (face, "face=\"courier\"", 14) == 0
                               || strncmp (face, "face=\"terminal\"", 15) == 0;

          /*
           * Symbol faces need their content translated to Unicode rather than
           * printed as text; see gsc_put_string_symbol().
           */
          font.symbol_font = strncmp (face, "face=\"webdings\"", 15) == 0
                             ? GSC_SYMBOL_WEBDINGS : GSC_SYMBOL_NONE;
        }

      /*
       * Find any colour= portion of the tag argument.  Absent one the font
       * keeps the colour it inherited from the enclosing tag, which is what
       * the Runner does -- <font colour="red"><font size=20>x</font></font>
       * is red throughout.
       */
      {
        const glui32 colour = gsc_colour_from_tag (lower, "colour");
        if (colour != GSC_COLOUR_NONE)
          font.colour = colour;
      }

      /* Find the size= portion of the tag argument. */
      size = strstr (lower, "size=");
      if (size)
        {
          scr_uint value;

          /* Deal with incremental and absolute sizes. */
          if (strncmp (size, "size=+", 6) == 0
              && sscanf (size, "size=+%lu", &value) == 1)
            font.size += value;
          else if (strncmp (size, "size=-", 6) == 0
                   && sscanf (size, "size=-%lu", &value) == 1)
            font.size -= value;
          else if (sscanf (size, "size=%lu", &value) == 1)
            font.size = value;
        }

      /* Done with tag argument copy. */
      free (lower);

      /*
       * Push the new font setting onto the font stack, and set Glk style.
       */
      gsc_font_stack[gsc_font_index++] = font;
      gsc_set_glk_style ();
    }
}

static void
gsc_handle_endfont_tag (void)
{
  /* Unless underrun, pop the font stack and set Glk style. */
  if (gsc_font_index > 0)
    {
      gsc_font_index--;
      gsc_set_glk_style ();
    }
}


/*
 * gsc_handle_attribute_tag()
 *
 * Increment the required attribute nesting counter, or decrement on end
 * tag.  Set the appropriate Glk style.
 */
static void
gsc_handle_attribute_tag (scr_int tag)
{
  /*
   * Increment the required attribute nesting counter, and set Glk style.
   */
  switch (tag)
    {
    case SCR_TAG_BOLD:
      gsc_attribute_bold++;
      break;
    case SCR_TAG_ITALICS:
      gsc_attribute_italic++;
      break;
    case SCR_TAG_UNDERLINE:
      gsc_attribute_underline++;
      break;
    case SCR_TAG_COLOUR:
      gsc_attribute_secondary_colour++;
      break;
    default:
      break;
    }
  gsc_set_glk_style ();
}

static void
gsc_handle_endattribute_tag (scr_int tag)
{
  /*
   * Decrement the required attribute nesting counter, unless underrun, and
   * set Glk style.
   */
  switch (tag)
    {
    case SCR_TAG_ENDBOLD:
      if (gsc_attribute_bold > 0)
        gsc_attribute_bold--;
      break;
    case SCR_TAG_ENDITALICS:
      if (gsc_attribute_italic > 0)
        gsc_attribute_italic--;
      break;
    case SCR_TAG_ENDUNDERLINE:
      if (gsc_attribute_underline > 0)
        gsc_attribute_underline--;
      break;
    case SCR_TAG_ENDCOLOUR:
      if (gsc_attribute_secondary_colour > 0)
        gsc_attribute_secondary_colour--;
      break;
    default:
      break;
    }
  gsc_set_glk_style ();
}


/*
 * gsc_handle_wait_tag()
 *
 * If Glk offers timers, delay for the requested period.  Otherwise, this
 * function does nothing.
 */
static void
gsc_handle_wait_tag (const scr_char *argument)
{
  double delay = 0.0;

#ifdef SPATTERLIGHT
  /* Honour Spatterlight's delays preference: when it is off, timed waits are
     skipped outright.  Both engines funnel their pauses through here -- the
     ADRIFT 4 <wait x.x> tag and the ADRIFT 5 A5_WAIT_MARK span. */
  if (!gli_sa_delays)
    return;
#endif
  /* Ignore the wait tag if the Glk doesn't have timers. */
  if (!glk_gestalt (gestalt_Timer, 0))
    return;

  /* Determine the delay time, and convert to milliseconds. */
  if (sscanf (argument, "%lf", &delay) == 1 && delay > 0.0)
    {
      glui32 milliseconds, timeout;

      /*
       * Work with timeouts at 1/10 of the wait period, to minimize Glk
       * timer jitter.  Allow the timeout to be canceled by keypress, as a
       * user convenience.
       */
      milliseconds = (glui32) (delay * GSC_MILLISECONDS_PER_SECOND);
      timeout = milliseconds / GSC_TIMEOUTS_COUNT;
      if (timeout > 0)
        {
          glui32 delayed;
          scr_bool is_completed;

          /* Request timer events, and let a keypress cancel the wait. */
          glk_request_char_event (gsc_main_window);
          glk_request_timer_events (timeout);

          /* Loop until delay completed or canceled by a keypress. */
          is_completed = TRUE;
          for (delayed = 0; delayed < milliseconds; delayed += timeout)
            {
              event_t event;

              gsc_event_wait_2 (evtype_CharInput, evtype_Timer, &event);
              if (event.type == evtype_CharInput)
                {
                  /* Cancel the delay, or reissue the input request. */
                  if (event.val1 == GSC_CANCEL_WAIT_1
                      || event.val1 == GSC_CANCEL_WAIT_2)
                    {
                      is_completed = FALSE;
                      break;
                    }
                  else
                    glk_request_char_event (gsc_main_window);
                }
             }

          /* Cancel any pending character input, and stop timers. */
          if (is_completed)
            glk_cancel_char_event (gsc_main_window);
          glk_request_timer_events (0);
        }
    }
}


/*
 * gsc_reset_glk_style()
 *
 * Drop all stacked fonts and nested attributes, and return to normal Glk
 * style.
 */
static void
gsc_reset_glk_style (void)
{
  /* Reset the font stack and attributes, and set a normal style.  Centering
     goes too: this runs at prompts, and a dangling <center> must not leave the
     prompt, the player's input, and every turn after it centered (the a5
     renderer drops a dangling <center> at the end of its block for the same
     reason). */
  gsc_font_index = 0;
  gsc_attribute_bold = 0;
  gsc_attribute_italic = 0;
  gsc_attribute_underline = 0;
  gsc_attribute_secondary_colour = 0;
  gsc_attribute_center = 0;
  gsc_attribute_right = 0;
  gsc_set_glk_style ();
}


#if defined(GLK_MODULE_GARGLK_FILE_RESOURCES) || defined(SPATTERLIGHT)
/*
 * Tracking for redrawing a graphic that an immediate screen clear would
 * otherwise wipe.  Adrift shows graphics in a separate pane that text clears
 * don't affect; this port draws them inline in the main window, so a graphic
 * drawn at the start of a turn is lost if that same turn's text then clears
 * the screen (as "examine girl" in Majesty Mall Center does).  We remember the
 * graphic drawn since the last player input and, on a clear, redraw it.
 */
static glui32 gsc_pending_graphic_id = 0;
static scr_bool gsc_graphic_drawn_since_input = FALSE;

/*
 * gsc_draw_inline_graphic()
 *
 * Draw a graphic inline in the main window, on its own line -- break before it
 * if not already at the start of a line, and always after it so the following
 * text starts below the image rather than wrapping up alongside its tall line
 * fragment -- and note it for a possible redraw after a screen clear.
 */
static void
gsc_draw_inline_graphic (glui32 id)
{
  strid_t stream = glk_window_get_stream (gsc_main_window);

  if (!gsc_main_at_line_start)
    glk_put_char_stream (stream, '\n');
  glk_image_draw (gsc_main_window, id, imagealign_InlineDown, 0);
  glk_put_char_stream (stream, '\n');
  gsc_main_at_line_start = TRUE;

  gsc_pending_graphic_id = id;
  gsc_graphic_drawn_since_input = TRUE;
}
#endif


/*
 * os_print_tag()
 *
 * Interpret selected Adrift output control tags.  Not all are implemented
 * here; several are ignored.
 */
void
os_print_tag (scr_int tag, const scr_char *argument)
{
  event_t event;
  assert (argument);

  switch (tag)
    {
    case SCR_TAG_CLS:
      /* Clear the main text display window. */
      glk_window_clear (gsc_main_window);
      gsc_main_at_line_start = TRUE;
      gsc_main_window_empty = TRUE;
#if defined(GLK_MODULE_GARGLK_FILE_RESOURCES) || defined(SPATTERLIGHT)
      /*
       * If a graphic was drawn earlier this turn (no player input since), the
       * clear just wiped it.  Redraw it so it survives the clear, matching
       * Adrift's separate graphics pane.
       */
      if (gsc_graphic_drawn_since_input)
        gsc_draw_inline_graphic (gsc_pending_graphic_id);
#endif
      break;

    case SCR_TAG_FONT:
      /* Handle with specific tag handler function. */
      gsc_handle_font_tag (argument);
      break;

    case SCR_TAG_ENDFONT:
      /* Handle with specific endtag handler function. */
      gsc_handle_endfont_tag ();
      break;

    case SCR_TAG_BOLD:
    case SCR_TAG_ITALICS:
    case SCR_TAG_UNDERLINE:
    case SCR_TAG_COLOUR:
      /* Handle with common attribute tag handler function. */
      gsc_handle_attribute_tag (tag);
      break;

    case SCR_TAG_ENDBOLD:
    case SCR_TAG_ENDITALICS:
    case SCR_TAG_ENDUNDERLINE:
    case SCR_TAG_ENDCOLOUR:
      /* Handle with common attribute endtag handler function. */
      gsc_handle_endattribute_tag (tag);
      break;

    case SCR_TAG_CENTER:
    case SCR_TAG_ENDCENTER:
      {
        /*
         * Alignment in the Runner is a flat state, not a nesting depth: it
         * keeps one byte holding centered, right or left, and <center> sets
         * centered while </center>, <right> and </right> set it away again
         * (run400 Sub_22_27 stores 2, 1 and 0 into it, with no count of how
         * many <center>s are open).  Counting nesting instead leaves a game
         * that opens <center> twice and closes it once -- as "To Hell in a
         * Hamper" does on its title page -- centered for the rest of the game.
         *
         * Justification is a paragraph attribute, so a change of alignment
         * needs its own paragraph: put the newline first -- it closes the
         * previous paragraph in that paragraph's own style (on ENDCENTER it is
         * the centered paragraph's terminator) -- and only then switch styles.
         * A tag that doesn't change the alignment breaks no paragraph.
         */
        glui32 centered = (tag == SCR_TAG_CENTER);

        if (centered != gsc_attribute_center || gsc_attribute_right > 0)
          {
            glk_put_char ('\n');
            gsc_attribute_center = centered;
            gsc_attribute_right = 0;
            gsc_set_glk_style ();
          }
      }
      break;

    case SCR_TAG_RIGHT:
    case SCR_TAG_ENDRIGHT:
      {
        /*
         * Same flat alignment state as <center> (run400 Sub_22_27).  Right uses
         * style_Note (RightFlush-hinted); entering or leaving right also clears
         * centering, as the Runner does.
         */
        glui32 right = (tag == SCR_TAG_RIGHT);

        if (right != gsc_attribute_right || gsc_attribute_center > 0)
          {
            glk_put_char ('\n');
            gsc_attribute_right = right;
            gsc_attribute_center = 0;
            gsc_set_glk_style ();
          }
      }
      break;

    case SCR_TAG_BGCOLOUR:
      /*
       * <bgcolour="red"> repaints the Runner's output pane.  There is no Glk
       * way to change a window's background short of clearing it, and a game
       * that sets a background mid-turn is not asking for its text to be
       * wiped, so only the colour later text is drawn over changes; the
       * window catches up at the next clear.  The argument is the whole tag
       * contents (scprintf passes it verbatim, to match <font colour=...>),
       * so it is parsed the same way -- and "default" returns the mode's own
       * background rather than clearing the colour.
       *
       * The one repaint that is free is the one onto an empty window: a game
       * that clears the screen and then names its colour -- The Dead Man ends
       * each blackout vision with "<cls><bgcolor=default>" -- has nothing on
       * screen to lose, and without the second clear its pane keeps the vision's
       * white behind every line that follows.
       */
      {
        const glui32 colour = gsc_colour_from_tag (argument, "bgcolour");
        const glui32 wanted = (colour == GSC_COLOUR_NONE) ? 0x000000 : colour;
        const scr_bool changed = (wanted != gsc_colour_background);

        gsc_colour_background = wanted;
        gsc_set_glk_style ();

        if (changed && gsc_colour_enabled
            && gsc_main_window_empty && gsc_main_window)
          glk_window_clear (gsc_main_window);
      }
      break;

    case SCR_TAG_WAIT:
      /*
       * Update the status line now only if it has its own window, then
       * handle with a specialized handler.
       */
      if (gsc_status_window)
        gsc_status_notify ();
      gsc_handle_wait_tag (argument);
      break;

    case SCR_TAG_WAITKEY:
      /*
       * If reading an input log, ignore; it disrupts replay.  Write a newline
       * to separate off any unterminated game output instead.
       */
      if (!gsc_readlog_stream)
        {
          /* Update the status line now only if it has its own window. */
          if (gsc_status_window)
            gsc_status_notify ();

          /* Request a character event, and wait for it to be filled. */
          glk_request_char_event (gsc_main_window);
          gsc_event_wait (evtype_CharInput, &event);
        }
      else
        glk_put_char ('\n');
      break;

    default:
      /* Ignore unimplemented and unknown tags. */
      break;
    }
}


/*
 * os_print_string()
 *
 * Print a text string to the main output window.
 */
void
os_print_string (const scr_char *string)
{
  assert (string);
  assert (glk_stream_get_current ());

  /* The first output of a replayed opening is where a restart becomes visible
     to the front end; take the map pane down before the text is laid out. */
  gsc_map_notice_restart ();

  /*
   * If the current top of the font stack is monospaced, we may need to use an
   * alternative function to write this string.
   *
   * The main window should always be the currently set window at this point,
   * so we never be attempting monospaced output to the status window.
   * Nevertheless, check anyway.
   */
  if (gsc_font_top ().symbol_font != GSC_SYMBOL_NONE
      && glk_stream_get_current () == glk_window_get_stream (gsc_main_window))
    gsc_put_string_symbol (string, gsc_font_top ().symbol_font);
  else if (gsc_font_top ().is_monospaced
      && glk_stream_get_current () == glk_window_get_stream (gsc_main_window))
    gsc_put_string_alternate (string);
  else
    gsc_put_string (string);
}


/*
 * os_print_string_debug()
 *
 * Debugging output goes to the main Glk window -- no special effects or
 * dedicated debugging window attempted.
 */
void
os_print_string_debug (const scr_char *string)
{
  assert (string);
  assert (glk_stream_get_current ());

  gsc_put_string (string);
}


/*
 * gsc_styled_string()
 * gsc_styled_char()
 * gsc_standout_string()
 * gsc_standout_char()
 * gsc_normal_string()
 * gsc_normal_char()
 * gsc_header_string()
 *
 * Convenience functions to print strings in assorted styles.  A standout
 * string is one that hints that it's from the interpreter, not the game.
 */
static void
gsc_styled_string (glui32 style, const char *message)
{
  assert (message);

  glk_set_style (style);
  glk_put_string ((char *) message);
  glk_set_style (style_Normal);
}

static void
gsc_styled_char (glui32 style, char c)
{
  char buffer[2];

  buffer[0] = c;
  buffer[1] = '\0';
  gsc_styled_string (style, buffer);
}

static void
gsc_standout_string (const char *message)
{
  gsc_styled_string (style_Emphasized, message);
}

static void
gsc_standout_char (char c)
{
  gsc_styled_char (style_Emphasized, c);
}

static void
gsc_normal_string (const char *message)
{
  gsc_styled_string (style_Normal, message);
}

static void
gsc_normal_char (char c)
{
  gsc_styled_char (style_Normal, c);
}

static void
gsc_header_string (const char *message)
{
  gsc_styled_string (style_Header, message);
}


/*
 * gsc_copy_string()
 *
 * strdup() that passes NULL through rather than crashing on it; hint strings
 * are NULL when the game leaves them empty.
 */
static char *
gsc_copy_string (const char *string)
{
  return string ? strdup (string) : NULL;
}


/*
 * gsc_hint_present()
 *
 * Offer one hint: pop the question, then each of its two answers behind its
 * own confirm, the subtle nudge before the sledgehammer.  Returns FALSE when
 * the player turned an answer down, which is what the callers' refusal count
 * measures; turning the subtle answer down skips this hint's sledgehammer too.
 *
 * Both engines' hint displays funnel through here, so the ADRIFT 4 hints
 * (task Question/Hint1/Hint2, reached by the game's own "hints" command) and
 * the ADRIFT 5 ones (clsHint, reached by "glk hints") cannot drift apart.
 *
 * All three strings must stay valid for the whole call; see os_display_hints
 * for why that is not free on the ADRIFT 4 side.
 */
static int
gsc_hint_present (const char *question, const char *subtle,
                  const char *unsubtle)
{
  gsc_normal_char ('\n');
  gsc_standout_string (question);
  gsc_normal_char ('\n');

  if (subtle)
    {
      if (!os_confirm (GSC_CONF_SUBTLE_HINT))
        return FALSE;
      gsc_normal_char ('\n');
      gsc_standout_string (subtle);
      gsc_normal_string ("\n\n");
    }

  if (unsubtle)
    {
      if (!os_confirm (GSC_CONF_UNSUBTLE_HINT))
        return FALSE;
      gsc_normal_char ('\n');
      gsc_standout_string (unsubtle);
      gsc_normal_string ("\n\n");
    }

  return TRUE;
}


/*
 * os_display_hints()
 *
 * This is a very basic hints display.  In mitigation, very few games use
 * hints at all, and those that do are usually sparse in what they hint at, so
 * it's sort of good enough for the moment.
 */
void
os_display_hints (scr_game game)
{
  scr_game_hint hint;
  scr_int refused;

  /* For each hint, print the question, and confirm hint display. */
  refused = 0;
  for (hint = scr_get_first_game_hint (game);
       hint; hint = scr_get_next_game_hint (game, hint))
    {
      const scr_char *unsubtle;
      char *question, *subtle;

      /* If enough refusals, offer a way out of the loop. */
      if (refused >= GSC_HINT_REFUSAL_LIMIT)
        {
          if (!os_confirm (GSC_CONF_CONTINUE_HINTS))
            break;
          refused = 0;
        }

      /* All three getters hand back the same buffer, refilled -- SCARE's
         run_get_hint_common() notes its return is "valid only until the next
         hint call" -- so the first two have to be copied before the fetch
         that overwrites them.  Print without copying and the question comes
         out as whatever the sledgehammer left behind.  The last fetch has
         nothing after it, so it can be used where it lies. */
      question = gsc_copy_string (scr_get_game_hint_question (game, hint));
      subtle = gsc_copy_string (scr_get_game_subtle_hint (game, hint));
      unsubtle = scr_get_game_unsubtle_hint (game, hint);

      if (!gsc_hint_present (question, subtle, unsubtle))
        refused++;

      free (question);
      free (subtle);
    }
}


/*---------------------------------------------------------------------*/
/*  Glk resource handling functions                                    */
/*---------------------------------------------------------------------*/

#ifdef GLK_MODULE_GARGLK_FILE_RESOURCES
static schanid_t sound_channel;

void
os_play_sound (const scr_char *filepath,
               scr_int offset, scr_int length, scr_bool is_looping)
{
  if (sound_channel == NULL) {
    sound_channel = glk_schannel_create(0);
  }

  if (sound_channel == NULL) {
    return;
  }

  glui32 id = garglk_add_resource_from_file(giblorb_ID_Snd, gamefile, offset, length);
  if (id != 0) {
    glk_schannel_play_ext(sound_channel, id, is_looping ? 0xffffffff : 1, 0);
  }
}

void
os_stop_sound (void)
{
  if (sound_channel != NULL) {
    glk_schannel_stop(sound_channel);
  }
}
#elif defined(SPATTERLIGHT)
/*
 * Spatterlight resource handling.  Pre-load a chunk of the game file into
 * the Spatterlight Glk image/sound cache via win_loadimage/win_loadsound
 * and then dispatch through the standard Glk drawing/playing routines.
 */
extern "C" char *gli_game_path;
extern "C" int  win_findimage (int resno);
extern "C" void win_loadimage (int resno, const char *filename, int offset, int reslen);
extern "C" int  win_findsound (int resno);
extern "C" void win_loadsound (int resno, char *filename, int offset, int reslen);

static glui32
gsc_resource_id (scr_int offset, scr_int length)
{
  /* Synthesize a stable id from offset and length so repeat calls for the
   * same resource hit the cache instead of re-loading from disk.  Fold the
   * full 64-bit offset into 32 bits rather than shift-then-truncate: a plain
   * "(offset << 12) ^ length" cast to glui32 drops the high bits of any
   * offset >= 2^20, so distinct chunks in a large game could collide on one
   * cached id and cause the wrong image or sound to be drawn or played. */
  unsigned long long hash;

  hash = (unsigned long long) (scr_uint) offset;
  hash = (hash ^ (unsigned long long) (scr_uint) length) * 0x9E3779B97F4A7C15ULL;
  return (glui32) (hash ^ (hash >> 32));
}

static schanid_t sound_channel;

void
os_play_sound (const scr_char *filepath,
               scr_int offset, scr_int length, scr_bool is_looping)
{
  glui32 id;
  (void) filepath;

  if (length <= 0 || gli_game_path == NULL)
    return;

  if (sound_channel == NULL)
    sound_channel = glk_schannel_create (0);
  if (sound_channel == NULL)
    return;

  id = gsc_resource_id (offset, length);
  if (!win_findsound ((int) id))
    win_loadsound ((int) id, gli_game_path, (int) offset, (int) length);
  glk_schannel_play_ext (sound_channel, id, is_looping ? 0xffffffff : 1, 0);
}

void
os_stop_sound (void)
{
  if (sound_channel != NULL)
    glk_schannel_stop (sound_channel);
}
#else
/*
 * os_play_sound()
 * os_stop_sound()
 *
 * Stub functions.  The unused variables defeat gcc warnings.
 */
void
os_play_sound (const scr_char *filepath,
               scr_int offset, scr_int length, scr_bool is_looping)
{
  (void) filepath;
  (void) offset;
  (void) length;
  (void) is_looping;
}

void
os_stop_sound (void)
{
}
#endif


#ifdef GSC_HAVE_TITLE_WINDOW
/*
 * Title/cover graphic support.  Adrift games can carry an "IntroRes" cover
 * image, shown by the engine before the game's first turn.  Adrift intros
 * routinely clear the main window with <cls> tags -- "To Hell in a Hamper"
 * emits one as its very first output -- so drawing the cover inline would wipe
 * it immediately.  Instead, any graphic requested before the player's first
 * input is shown in a temporary graphics window at the top of the display,
 * closed on that first keypress.  This mirrors the original Runner, which
 * shows the title in a separate picture pane that text clears don't touch.
 */
static winid_t gsc_graphics_window = NULL;
static glui32 gsc_title_image = 0;
#ifdef SPATTERLIGHT
/* The title image's chunk in the game file, so an autorestore can re-load it
   into the app-side image cache (the cache does not survive a relaunch). */
static scr_int gsc_title_offset = 0;
static scr_int gsc_title_length = 0;
#endif
static int gsc_seen_input = FALSE;

/*
 * gsc_title_redraw()
 *
 * Draw, or redraw on a resize, the title image scaled to fit within the
 * graphics window, preserving its aspect ratio and centring it.
 */
static void
gsc_title_redraw (void)
{
  glui32 win_width, win_height, img_width, img_height, draw_width, draw_height;

  if (gsc_graphics_window == NULL || gsc_title_image == 0)
    return;

  glk_window_get_size (gsc_graphics_window, &win_width, &win_height);
  if (win_width == 0 || win_height == 0)
    return;
  if (!glk_image_get_info (gsc_title_image, &img_width, &img_height)
      || img_width == 0 || img_height == 0)
    return;

  /* Fit within the pane, limited by width or height, whichever binds first. */
  if (win_width * img_height <= win_height * img_width)
    {
      draw_width = win_width;
      draw_height = img_height * win_width / img_width;
    }
  else
    {
      draw_height = win_height;
      draw_width = img_width * win_height / img_height;
    }

  glk_window_fill_rect (gsc_graphics_window, 0, 0, 0, win_width, win_height);
  glk_image_draw_scaled (gsc_graphics_window, gsc_title_image,
                         (glsi32) ((win_width - draw_width) / 2),
                         (glsi32) ((win_height - draw_height) / 2),
                         draw_width, draw_height);
}

/*
 * gsc_show_title_graphic()
 *
 * Open a temporary graphics window above the main text, sized to the image's
 * aspect ratio, and draw the title image into it.  Returns TRUE on success,
 * FALSE if graphics windows are unavailable, in which case the caller falls
 * back to an inline draw.
 */
static int
gsc_show_title_graphic (glui32 image)
{
  glui32 win_width, win_height, img_width, img_height, pane_height;

  if (!glk_gestalt (gestalt_Graphics, 0)
      || !glk_gestalt (gestalt_DrawImage, wintype_Graphics))
    return FALSE;
  if (!glk_image_get_info (image, &img_width, &img_height)
      || img_width == 0 || img_height == 0)
    return FALSE;

  if (gsc_graphics_window == NULL)
    {
      gsc_graphics_window = glk_window_open (gsc_main_window,
                                             winmethod_Above | winmethod_Fixed,
                                             0, wintype_Graphics, 0);
      if (gsc_graphics_window == NULL)
        return FALSE;
    }

  gsc_title_image = image;

  /* Size the pane to fit the image width, preserving its aspect ratio. */
  glk_window_get_size (gsc_graphics_window, &win_width, &win_height);
  if (win_width == 0)
    {
      glk_window_close (gsc_graphics_window, NULL);
      gsc_graphics_window = NULL;
      gsc_title_image = 0;
      return FALSE;
    }
  pane_height = img_height * win_width / img_width;
  glk_window_set_arrangement (glk_window_get_parent (gsc_graphics_window),
                              winmethod_Above | winmethod_Fixed,
                              pane_height, gsc_graphics_window);

  gsc_title_redraw ();
  return TRUE;
}

/*
 * gsc_close_title_graphic()
 *
 * Close the temporary title window, if open, returning its space to the main
 * window.  Called when the player provides their first input.
 */
static void
gsc_close_title_graphic (void)
{
  if (gsc_graphics_window != NULL)
    {
      glk_window_close (gsc_graphics_window, NULL);
      gsc_graphics_window = NULL;
      gsc_title_image = 0;
    }
}
#endif /* GSC_HAVE_TITLE_WINDOW */


/*
 * gsc_refresh_windows()
 *
 * Refresh the size-sensitive windows, on Glk Arrange and Redraw events.
 */
static void
gsc_refresh_windows (void)
{
  /* An Arrange is how a library preference flip announces itself, and the
     status redraw below asks gsc_colour_visible, so the cache goes first. */
  gsc_normal_measure_state = -1;

  gsc_status_redraw ();
#ifdef GSC_HAVE_TITLE_WINDOW
  gsc_title_redraw ();
#endif
}


#ifdef GLK_MODULE_GARGLK_FILE_RESOURCES
/*
 * os_show_graphic()
 *
 * Use the Gargoyle-specific garglk_add_resource_from_file().  Before the
 * player's first input, show the image as a title in a dedicated graphics
 * window; afterwards, draw it inline in the main window.
 */
void
os_show_graphic (const scr_char *filepath, scr_int offset, scr_int length)
{
  if (length <= 0 || gsc_main_window == NULL)
    return;

  glui32 id = garglk_add_resource_from_file(giblorb_ID_Pict, gamefile, offset, length);
  if (id == 0)
    return;

  if (!gsc_seen_input && gsc_show_title_graphic (id))
    return;

  gsc_draw_inline_graphic(id);
}
#elif defined(SPATTERLIGHT)
/*
 * os_show_graphic()
 *
 * Pre-load the requested image chunk from the game file into the
 * Spatterlight Glk image cache.  Before the player's first input, show it as
 * a title image in a dedicated graphics window; afterwards, draw it inline in
 * the main window.
 */
void
os_show_graphic (const scr_char *filepath, scr_int offset, scr_int length)
{
  glui32 id;
  (void) filepath;

  if (length <= 0 || gsc_main_window == NULL || gli_game_path == NULL)
    return;

  id = gsc_resource_id (offset, length);
  if (!win_findimage ((int) id))
    win_loadimage ((int) id, gli_game_path, (int) offset, (int) length);

  if (!gsc_seen_input && gsc_show_title_graphic (id))
    {
      gsc_title_offset = offset;
      gsc_title_length = length;
      return;
    }

  gsc_draw_inline_graphic (id);
}
#else
/*
 * os_show_graphic()
 *
 * For graphic-capable Glk libraries on Linux, attempt graphics using xv.  The
 * graphic capability test isn't really required, it's just a way of having
 * graphics behave without surprises; someone using a non-graphical Glk
 * probably won't expect graphics to pop up.
 *
 * For other cases, this is a stub function, with unused variables to defeat
 * gcc warnings.
 */
#ifdef LINUX_GRAPHICS
static int gsclinux_graphics_enabled = TRUE;
static char *gsclinux_game_file = NULL;
void
os_show_graphic (const scr_char *filepath, scr_int offset, scr_int length)
{
  (void) filepath;

  if (length > 0
      && gsclinux_graphics_enabled && glk_gestalt (gestalt_Graphics, 0))
    {
      scr_char *buffer;

      /*
       * Try to extract data with dd.  Assuming that works, background xv to
       * display the image, then background a job to delay ten seconds and
       * then delete the temporary file containing the image.  Systems lacking
       * xv can usually use a small script, named xv, to invoke eog or an
       * alternative image display binary.  Not exactly finessed.
       */
      assert (gsclinux_game_file);
      buffer = (decltype(buffer)) gsc_malloc (strlen (gsclinux_game_file) + 128);
      sprintf (buffer, "dd if=%s ibs=1c skip=%ld count=%ld obs=100k"
               " of=/tmp/scarier.jpg 2>/dev/null",
               gsclinux_game_file, offset, length);
      system (buffer);
      free (buffer);
      system ("xv /tmp/scarier.jpg >/dev/null 2>&1 &");
      system ("( sleep 10; rm /tmp/scarier.jpg ) >/dev/null 2>&1 &");
    }
}
#else
void
os_show_graphic (const scr_char *filepath, scr_int offset, scr_int length)
{
  (void) filepath;
  (void) offset;
  (void) length;
}
#endif
#endif


/*---------------------------------------------------------------------*/
/*  Glk command escape functions                                       */
/*---------------------------------------------------------------------*/

/* Print the one-line synopsis of what a Glk command accepts, held in the
   command table so that a command handed an argument it doesn't understand
   and that command's "glk help" entry always agree. */
static void gsc_command_usage (const char *command);

/*
 * gsc_open_log_stream()
 *
 * Prompt for a log file and open a stream on it, in the way all three of the
 * logging commands below want it done.  `must_exist` is for the read log,
 * which reads back a file rather than writing one.  Returns NULL, having
 * already complained under `label`, where the player cancelled the prompt or
 * the file would not open.
 */
static strid_t
gsc_open_log_stream (const char *label, glui32 usage, glui32 mode,
                     scr_bool must_exist)
{
  frefid_t fileref;
  strid_t stream;

  fileref = glk_fileref_create_by_prompt (usage, (glui32) mode, 0);
  if (fileref && must_exist && !glk_fileref_does_file_exist (fileref))
    {
      glk_fileref_destroy (fileref);
      fileref = NULL;
    }
  if (!fileref)
    {
      gsc_standout_string (label);
      gsc_standout_string (" failed.\n");
      return NULL;
    }

  stream = glk_stream_open_file (fileref, (glui32) mode, 0);
  glk_fileref_destroy (fileref);
  if (!stream)
    {
      gsc_standout_string (label);
      gsc_standout_string (" failed.\n");
      return NULL;
    }

  return stream;
}


/*
 * gsc_command_logging()
 *
 * The shape all three logging commands take: "on" opens the log stream via
 * the file prompt and "off" closes it, each saying so unless the log was
 * already that way; a bare command acts rather than reports -- it is a
 * synonym for "on", since that is what someone typing it almost always wants;
 * "status" reports instead, and is what the summary polls; anything else is a
 * usage error.
 *
 * `stream` is the log's stream global, `label` opens every message, and
 * `is_transcript` additionally attaches or detaches the stream as the main
 * window's echo stream, which is what makes the transcript a transcript.
 */
static void
gsc_command_logging (const char *argument, const char *name,
                     const char *label, strid_t *stream,
                     glui32 usage, glui32 mode, scr_bool must_exist,
                     scr_bool is_transcript)
{
  assert (argument);

  if (scr_strcasecmp (argument, "on") == 0 || strlen (argument) == 0)
    {
      if (*stream)
        {
          gsc_normal_string (label);
          gsc_normal_string (" is already on.\n");
          return;
        }

      *stream = gsc_open_log_stream (label, usage, mode, must_exist);
      if (!*stream)
        return;

      if (is_transcript)
        glk_window_set_echo_stream (gsc_main_window, *stream);

      gsc_normal_string (label);
      gsc_normal_string (" is now on.\n");
    }

  else if (scr_strcasecmp (argument, "off") == 0)
    {
      if (!*stream)
        {
          gsc_normal_string (label);
          gsc_normal_string (" is already off.\n");
          return;
        }

      glk_stream_close (*stream, NULL);
      *stream = NULL;

      if (is_transcript)
        glk_window_set_echo_stream (gsc_main_window, NULL);

      gsc_normal_string (label);
      gsc_normal_string (" is now off.\n");
    }

  else if (scr_strcasecmp (argument, "status") == 0)
    {
      gsc_normal_string (label);
      gsc_normal_string (" is ");
      gsc_normal_string (*stream ? "on" : "off");
      gsc_normal_string (".\n");
    }

  else
    {
      gsc_command_usage (name);
    }
}


/*
 * gsc_command_script()
 * gsc_command_inputlog()
 * gsc_command_readlog()
 *
 * Turn game output scripting (logging), game input logging, and input log
 * read-back on and off.
 */
static void
gsc_command_script (const char *argument)
{
  gsc_command_logging (argument, "script", "Glk transcript",
                       &gsc_transcript_stream,
                       fileusage_Transcript | fileusage_TextMode,
                       filemode_WriteAppend, FALSE, TRUE);
}

static void
gsc_command_inputlog (const char *argument)
{
  gsc_command_logging (argument, "inputlog", "Glk input logging",
                       &gsc_inputlog_stream,
                       fileusage_InputRecord | fileusage_BinaryMode,
                       filemode_WriteAppend, FALSE, FALSE);
}

static void
gsc_command_readlog (const char *argument)
{
  gsc_command_logging (argument, "readlog", "Glk read log",
                       &gsc_readlog_stream,
                       fileusage_InputRecord | fileusage_BinaryMode,
                       filemode_Read, TRUE, FALSE);
}


/*
 * gsc_command_toggle()
 *
 * The shape every on/off port option takes: "on" and "off" set it, saying so
 * unless it was already that way; a bare command reports the current setting;
 * anything else is a usage error.
 *
 * `label` opens each of those messages and carries its own verb, since some
 * of the options are plural ("Glk abbreviation expansions are ...", "Glk
 * combat assist is ..."), and `on_detail` and `off_detail` finish the two
 * "is now ..." messages -- for most options just ".\n", but the ones that
 * deviate from the original Runner say what they do and why there.
 *
 * `empty_acts` is for options where a bare command is worth treating as "on"
 * rather than as a question, and which therefore never report.
 */
static void
gsc_command_toggle (const char *argument, const char *name,
                    const char *label, scr_bool state,
                    void (*set_state) (scr_bool),
                    const char *on_detail, const char *off_detail,
                    scr_bool empty_acts)
{
  const scr_bool is_empty = strlen (argument) == 0;

  assert (argument);

  if (scr_strcasecmp (argument, "on") == 0 || (empty_acts && is_empty))
    {
      if (state)
        {
          gsc_normal_string (label);
          gsc_normal_string (" already on.\n");
          return;
        }

      set_state (TRUE);
      gsc_normal_string (label);
      gsc_normal_string (" now on");
      gsc_normal_string (on_detail);
    }

  else if (scr_strcasecmp (argument, "off") == 0)
    {
      if (!state)
        {
          gsc_normal_string (label);
          gsc_normal_string (" already off.\n");
          return;
        }

      set_state (FALSE);
      gsc_normal_string (label);
      gsc_normal_string (" now off");
      gsc_normal_string (off_detail);
    }

  else if (is_empty)
    {
      gsc_normal_string (label);
      gsc_normal_char (' ');
      gsc_normal_string (state ? "on" : "off");
      gsc_normal_string (".\n");
    }

  else
    {
      gsc_command_usage (name);
    }
}


/*
 * gsc_command_abbreviations()
 *
 * Turn abbreviation expansions on and off.
 */
static void
gsc_set_abbreviations (scr_bool state)
{
  gsc_abbreviations_enabled = state;
}

static void
gsc_command_abbreviations (const char *argument)
{
  gsc_command_toggle (argument, "abbreviations",
                      "Glk abbreviation expansions are",
                      gsc_abbreviations_enabled, gsc_set_abbreviations,
                      ".\n", ".\n", FALSE);
}


/*
 * gsc_command_capacity()
 *
 * Select how the player's carried load is accounted for.  When off (the
 * default) SCARIER mirrors the real ADRIFT Runner, keeping a running total
 * updated on take and drop.  When on, SCARIER recomputes the load afresh from
 * the objects currently held on each check (legacy SCARIER behaviour).
 */
static void
gsc_set_capacity (scr_bool state)
{
  scr_set_game_capacity_recompute (gsc_game, state);
}

static void
gsc_command_capacity (const char *argument)
{
  gsc_command_toggle (argument, "capacity",
                      "Glk carrying capacity recompute is",
                      scr_get_game_capacity_recompute (gsc_game),
                      gsc_set_capacity,
                      "; the load is summed afresh from held objects on each"
                      " check (legacy SCARIER behaviour).\n",
                      "; a running total is kept as the original ADRIFT Runner"
                      " does.\n", FALSE);
}


/*
 * gsc_command_combat_assist()
 *
 * Turn the optional Battle-System combat assist on and off.  This is a
 * deliberately non-faithful aid for amateur ADRIFT games that left every
 * character's Accuracy and Agility unconfigured (0), so the 4.0 "accuracy >
 * agility" hit test (0 > 0) never lands and combat stalemates forever.  When on,
 * such fully-unconfigured games get an automatic hit, letting combat play out on
 * the author's strength-vs-defence basis.  Games that do configure combat (e.g.
 * Sun Empire) are never affected.  Off by default.
 */
static void
gsc_command_combat_assist (const char *argument)
{
  gsc_command_toggle (argument, "combatassist", "Glk combat assist is",
                      scr_get_combat_assist (), scr_set_combat_assist,
                      ".  Note this deviates from the original ADRIFT Runner"
                      " and is intended only for games whose combat data is"
                      " broken (every character's Accuracy and Agility left at"
                      " 0).  It takes effect for the next fight; games that"
                      " configure combat are unaffected.\n",
                      "; combat matches the original ADRIFT Runner.\n", FALSE);
}


/*
 * gsc_command_move_assist()
 *
 * Turn the optional move assist on and off.  This is a deliberately non-faithful
 * aid for a few native-4.0 games authored with a move task action's "To:" combo
 * left at VB's default -1 (the destination room sitting in Var3).  The reference
 * Runner silently ignores such a move, which in e.g. To Hell & Beyond traps the
 * player in the mansion.  When on, an unset (-1) move whose Var3 names a real
 * room is honoured as "to room".  Off by default.
 */
static void
gsc_command_move_assist (const char *argument)
{
  gsc_command_toggle (argument, "moveassist", "Glk move assist is",
                      scr_get_move_assist (), scr_set_move_assist,
                      ".  Note this deviates from the original ADRIFT Runner"
                      " and is intended only for games with a broken move task"
                      " (a destination room left unset) that would otherwise be"
                      " unwinnable.\n",
                      "; moves match the original ADRIFT Runner.\n", FALSE);
}


/*
 * gsc_command_verbose()
 *
 * Turn the game's verbose room descriptions on and off.  This mirrors the
 * ADRIFT Runner's Verbose user-interface option: when on, the game always gives
 * long descriptions of locations, even ones already visited.  Handling it as a
 * Glk port command (rather than as a game command) means it works even when a
 * game defines its own "verbose" task that would otherwise shadow it.  The
 * plain "verbose" and "brief" game commands continue to work as before.
 */
static void
gsc_set_verbose (scr_bool state)
{
  scr_set_game_verbose (gsc_game, state);
}

static void
gsc_command_verbose (const char *argument)
{
  gsc_command_toggle (argument, "verbose", "Glk verbose descriptions are",
                      scr_get_game_verbose (gsc_game), gsc_set_verbose,
                      "; the game always gives long descriptions of locations,"
                      " even ones you've visited before.\n",
                      "; long descriptions are given for places never before"
                      " visited and short descriptions otherwise.\n", TRUE);
}


/*
 * gsc_set_colour()
 * gsc_command_colour()
 *
 * Turn Adrift colours on and off.
 *
 * Adrift games are written for a Runner that paints its output pane in a
 * palette of its own rather than in the interpreter's theme -- black behind,
 * green replies, red typed text for ADRIFT 4 (the Runner's defaults, and the
 * only place they live: nothing in the .taf carries a colour), the author's
 * own four colours for ADRIFT 5 -- and a game that writes white text, or
 * chooses colours to sit on black, needs that background to read at all.  Off
 * by default, since the interpreter's own theme is what most players want, and
 * on from the start for a game that shows it needs the palette to be read at
 * all (gsc_colour_detect) or when the player asked for the Runner's look with
 * "-c" (gsc_colour_startup_apply).
 *
 * Colour mode publishes the game palette as Normal TextColor/BackColor
 * stylehints so glk_style_measure (and libraries that honour hints) see it,
 * and uses zcolors for per-span output/input colours.  Mid-session toggles
 * rebuild the Glk window tree so open windows pick up the new hints
 * (libraries snapshot stylehints at window open).  Both directions wipe the
 * transcript: Glk gives no other way to repaint a window's background.
 */
#ifdef GSC_HAVE_ZCOLORS
/*
 * gsc_colour_set_normal_hints()
 *
 * Publish or withdraw the game's Normal colours as stylehints.  Set before
 * opening windows (or rebuild after) so measure and themed text agree.
 */
static void
gsc_colour_set_normal_hints (scr_bool on)
{
  if (on)
    {
      glk_stylehint_set (wintype_TextBuffer, style_Normal,
                         stylehint_TextColor, (glsi32) gsc_colour_output);
      glk_stylehint_set (wintype_TextBuffer, style_Normal,
                         stylehint_BackColor, (glsi32) gsc_colour_background);
      glk_stylehint_set (wintype_TextGrid, style_Normal,
                         stylehint_TextColor, (glsi32) gsc_colour_output);
      glk_stylehint_set (wintype_TextGrid, style_Normal,
                         stylehint_BackColor, (glsi32) gsc_colour_background);
    }
  else
    {
      glk_stylehint_clear (wintype_TextBuffer, style_Normal,
                           stylehint_TextColor);
      glk_stylehint_clear (wintype_TextBuffer, style_Normal,
                           stylehint_BackColor);
      glk_stylehint_clear (wintype_TextGrid, style_Normal,
                           stylehint_TextColor);
      glk_stylehint_clear (wintype_TextGrid, style_Normal,
                           stylehint_BackColor);
    }
}

/*
 * gsc_colour_rebuild_windows()
 *
 * Tear down the Glk window tree and recreate every window that was open, so
 * new Normal stylehints are snapshotted into open windows.  The panes reopen
 * in the order a session opens them -- title graphic, side window, then the
 * map -- because each split carves up the story window's remaining space:
 * reopening the side window after the map would land it to the map's left
 * instead of its right, and the player would find the layout rearranged by
 * a colour toggle.  The map redraws itself and the title graphic is redrawn
 * from its retained image number; the side window's text, like the story
 * window's, is gone -- the game reprints it the next time it writes there.
 */
static void
gsc_colour_rebuild_windows (void)
{
  int was_map = gsc_map_shown;
  int was_side = gsc_a5_side_window != NULL;
#ifdef GSC_HAVE_TITLE_WINDOW
  int was_title = gsc_graphics_window != NULL;
#endif
  winid_t root;

  root = glk_window_get_root ();
  if (root)
    glk_window_close (root, NULL);

  gsc_main_window = NULL;
  gsc_status_window = NULL;
  gsc_map_window = NULL;
  gsc_a5_side_window = NULL;
#ifdef GSC_HAVE_TITLE_WINDOW
  gsc_graphics_window = NULL;
#endif
  gsc_map_shown = FALSE;
  gsc_map_screen_drop ();

  gsc_open_main_window ();
  gsc_open_status_window ();

#ifdef GSC_HAVE_TITLE_WINDOW
  if (was_title && gsc_title_image != 0)
    gsc_show_title_graphic (gsc_title_image);
#endif
  if (was_side)
    gsc_a5_open_side_window ();
  if (was_map)
    gsc_map_show ();
}

/*
 * gsc_colour_repaint()
 *
 * Take a text window into the game's palette, or hand it back to the theme.
 * Both directions end in a clear, because a Glk window keeps the background a
 * clear gave it and there is no other way to repaint one.
 */
static void
gsc_colour_repaint (winid_t win, scr_bool state)
{
  strid_t stream;
  glui32 bg;

  if (win == NULL)
    return;
  stream = glk_window_get_stream (win);

  if (state)
    {
      /* Set the colours first: a clear paints the window in the background
         colour currently in force, so the order is what makes it black. */
      gsc_colour_apply (win, GSC_COLOUR_NONE);
      glk_window_clear (win);
    }
  else
    {
      /* Handing a window back is fiddlier than taking it, because a window
         keeps the background a clear gave it: dropping the zcolors to Default
         stops later text being coloured, but leaves the window itself black.
         Only a clear made while a background zcolor is in force repaints it,
         so the theme's own background has to be named for one last clear.
         Three steps, and the order of the first two matters:

           - drop to Default first so any live zcolor is gone before measure;
           - measure style_Normal's background (style table / stylehints, not
             zcolors) and clear with that as the background zcolor;
           - drop to Default again, so text from here on follows the theme. */
      garglk_set_zcolors_stream (stream, zcolor_Default, zcolor_Default);
      if (glk_style_measure (win, style_Normal, stylehint_BackColor, &bg))
        garglk_set_zcolors_stream (stream, zcolor_Default, bg);
      glk_window_clear (win);
      garglk_set_zcolors_stream (stream, zcolor_Default, zcolor_Default);
    }
}

static void
gsc_set_colour (scr_bool state)
{
  scr_bool rebuild;

  gsc_colour_enabled = state;
  gsc_colour_set_normal_hints (state);
  gsc_normal_measure_state = -1;

  /* -c pre-applies hints before the first open; do not destroy that tree. */
  rebuild = gsc_main_window != NULL
            && !(gsc_colour_hints_preapplied && state);
  gsc_colour_hints_preapplied = FALSE;

  if (rebuild)
    gsc_colour_rebuild_windows ();

  gsc_colour_repaint (gsc_main_window, state);

  /* An ADRIFT 5 game may keep a second pane of its own (Alien Diver's
     "Status"), and it is as much the game's window as the story one: it has to
     change palette with it, or the pane would sit there in the theme's colours
     with the game's text on top.  A rebuild reopens the pane already painted
     (gsc_a5_open_side_window paints when colour mode is on, and a fresh window
     starts in the theme when it is off), so this repaint matters only for a
     path that skipped the rebuild -- and costs nothing when it was redundant,
     the pane being empty until the game next writes there. */
  gsc_colour_repaint (gsc_a5_side_window, state);

  /* The status line has a window of its own, and it follows the story window
     into and out of colour mode whether or not it is asked to: Gargoyle's
     zcolors are global as well as per-stream (gli_override_fg/bg apply to
     every window at once).  Turning colour mode off therefore has to name
     Default on the status stream as well, or the bar would keep the colours
     it was last drawn in.  Going the other way needs nothing here, because
     gsc_status_begin() names the bar's colours on every redraw -- it has to,
     since a grid clear re-seeds the window's attributes.  The redraw below is
     what actually repaints the bar either way, a grid window keeping the
     pixels it was last given. */
  if (gsc_status_window)
    {
      if (!state)
        garglk_set_zcolors_stream (glk_window_get_stream (gsc_status_window),
                                   zcolor_Default, zcolor_Default);
      gsc_status_notify ();
    }

  /* The map pane is painted from style_Normal via measure, and it is drawn
     from the prompt the turn loop prints -- which the command loop that
     brought us here does not reach again until the next real turn.  Repaint
     it now, or the map would sit in the old palette for one command more. */
  gsc_map_screen_drop ();
  gsc_map_redraw ();

  gsc_main_at_line_start = TRUE;
  gsc_main_window_empty = TRUE;
}
#endif

/*
 * gsc_colour_startup_apply()
 *
 * Act on the "-c" switch, or on what the loader detected about the game's own
 * colours, once the story and status windows exist and the ADRIFT 5 loader (if
 * any) has replaced the palette globals with the adventure's own four colours
 * -- both true by the time either main() calls this.  Not called on an
 * autorestore: a restored session brings its own colour state back with it, and
 * neither a switch on the command line nor a guess about the game has any
 * business overriding what the player left the game in.
 */
static void
gsc_colour_startup_apply (void)
{
#ifdef GSC_HAVE_ZCOLORS
  if (gsc_colour_startup)
    gsc_set_colour (TRUE);
#endif
}

static void
gsc_command_colour (const char *argument)
{
#ifdef GSC_HAVE_ZCOLORS
  /* A bare "glk colour" turns colours on rather than asking after them, so
     it is no use as a poll; "status" reports without acting, as it does for the
     logging commands, and is what the summary asks with. */
  const scr_bool poll = scr_strcasecmp (argument, "status") == 0;

  gsc_command_toggle (poll ? "" : argument, "colour",
                      "Glk Adrift colours are",
                      gsc_colour_enabled, gsc_set_colour,
                      "; text is drawn in the game's own colours, on the"
                      " black background it was written for.\n",
                      "; text follows the interpreter's own theme.\n", !poll);
#else
  assert (argument);
  gsc_normal_string ("Glk Adrift colours are not available with this"
                     " interpreter.\n");
#endif
}


/*
 * gsc_command_undo()
 * gsc_command_restore()
 * gsc_command_restart()
 * gsc_command_quit()
 *
 * UNDO, RESTORE, RESTART and QUIT as Glk commands.
 *
 * Every ADRIFT game understands these words already -- but only while its
 * parser is the thing reading the line.  A game that asks a question of its
 * own takes whatever is typed as the answer: the "please enter your name"
 * task many games open with, an ADRIFT 5 %PopUpInput% or %PopUpChoice%
 * dialog.  At such a prompt QUIT names your character "quit", and there is no
 * way out of a game gone wrong short of killing the interpreter.  Prefixed
 * with "glk" the four are caught by the front end before the game sees them,
 * and so work wherever a line of input is read.
 *
 * The handlers record what was asked and nothing more; the action itself is
 * carried out by the loop that read the line (gsc_meta_perform for scare,
 * gsc_a5_meta_perform for the a5 loop).  It cannot happen here: all four end
 * a turn abruptly -- scare's throw out of the interpreter's main loop, the a5
 * loop's wholesale replacement of the run -- and here we are still inside the
 * command dispatcher, which has the input line to free, and possibly inside
 * the engine itself, midway through rendering the text that asked the
 * question.
 */
enum gsc_meta_t
{
  GSC_META_NONE = 0, GSC_META_UNDO, GSC_META_RESTORE,
  GSC_META_RESTART, GSC_META_QUIT
};

/* The action a handler below asked for, awaiting a safe moment to happen. */
static gsc_meta_t gsc_meta_pending = GSC_META_NONE;

/*
 * Something to say about an action once it has happened.  Held rather than
 * printed because a successful scare restore, undo-from-memo, or restart
 * never returns to the caller that asked for it; whichever of gsc_meta_report()
 * and the next prompt comes first prints it.
 */
static const char *gsc_meta_message = NULL;

static gsc_meta_t
gsc_meta_take (void)
{
  const gsc_meta_t action = gsc_meta_pending;

  gsc_meta_pending = GSC_META_NONE;
  return action;
}

static void
gsc_meta_report (void)
{
  if (gsc_meta_message)
    {
      gsc_normal_string (gsc_meta_message);
      gsc_meta_message = NULL;
    }
}

static void
gsc_command_undo (const char *argument)
{
  assert (argument);
  gsc_meta_pending = GSC_META_UNDO;
}

static void
gsc_command_restore (const char *argument)
{
  assert (argument);
  gsc_meta_pending = GSC_META_RESTORE;
}

static void
gsc_command_restart (const char *argument)
{
  assert (argument);
  gsc_meta_pending = GSC_META_RESTART;
}

static void
gsc_command_quit (const char *argument)
{
  assert (argument);
  gsc_meta_pending = GSC_META_QUIT;
}


/*
 * gsc_meta_perform()
 *
 * Carry out a pending meta-command on the scare engine, called from
 * os_read_line once the line that asked for it has been dealt with.  Each of
 * the four does exactly what the game's own command of that name does,
 * confirmation and all -- the point of these is to reach the action, not to
 * behave differently once there.
 *
 * scr_quit_game(), and a successful scr_restart_game(), scr_load_game() or
 * scr_undo_game_turn() that falls back on a memo, unwind out of the
 * interpreter's main loop and never return here; anything to say about them is
 * left with gsc_meta_report() beforehand for the next prompt to print.
 */
static void
gsc_meta_perform (void)
{
  const gsc_meta_t action = gsc_meta_take ();

  if (action == GSC_META_NONE)
    return;

  /* No game to act on.  Quitting is still meaningful -- it is the way out of
     an interpreter showing nothing but an error -- but the rest are not. */
  if (gsc_game == NULL || !scr_is_game_running (gsc_game))
    {
      if (action == GSC_META_QUIT)
        glk_exit ();
      gsc_normal_string ("There is no game running to do that to.\n");
      return;
    }

  switch (action)
    {
    case GSC_META_UNDO:
      /* Unconfirmed, as the game's own UNDO is. */
      gsc_meta_message = "[The previous turn has been undone.]\n";
      if (!scr_undo_game_turn (gsc_game))
        {
          gsc_meta_message = NULL;
          gsc_normal_string (scr_get_game_turns (gsc_game) == 0
                             ? "You can't undo what hasn't been done.\n"
                             : "Sorry, no more undo is available.\n");
        }
      gsc_meta_report ();
      break;

    case GSC_META_RESTORE:
      if (os_confirm (SCR_CONF_RESTORE))
        {
          gsc_meta_message = "Ok.\n";
          if (!scr_load_game (gsc_game))
            {
              gsc_meta_message = NULL;
              gsc_normal_string ("Restore failed.\n");
            }
          gsc_meta_report ();
        }
      break;

    case GSC_META_RESTART:
      /* Says nothing: the replayed opening is the report. */
      if (os_confirm (SCR_CONF_RESTART))
        scr_restart_game (gsc_game);
      break;

    case GSC_META_QUIT:
      if (os_confirm (SCR_CONF_QUIT))
        scr_quit_game (gsc_game);
      break;

    default:
      break;
    }
}


/*
 * gsc_command_print_version_number()
 * gsc_command_version()
 *
 * Print out the Glk library version number.
 */
static void
gsc_command_print_version_number (glui32 version)
{
  char buffer[64];

  snprintf (buffer, sizeof(buffer), "%lu.%lu.%lu",
           (unsigned long) version >> 16,
           (unsigned long) (version >> 8) & 0xff,
           (unsigned long) version & 0xff);
  gsc_normal_string (buffer);
}

static void
gsc_command_version (const char *argument)
{
  glui32 version;
  assert (argument);

  gsc_normal_string ("This is version ");
  gsc_command_print_version_number (GSC_PORT_VERSION);
  gsc_normal_string (" of the Glk SCARIER port.\n");

  version = glk_gestalt (gestalt_Version, 0);
  gsc_normal_string ("The Glk library version is ");
  gsc_command_print_version_number (version);
  gsc_normal_string (".\n");
}


/*
 * gsc_command_commands()
 *
 * Turn command escapes off.  Once off, there's no way to turn them back on.
 * Commands must be on already to enter this function.
 */
static void
gsc_command_commands (const char *argument)
{
  assert (argument);

  if (scr_strcasecmp (argument, "on") == 0)
    {
      gsc_normal_string ("Glk commands are already on.\n");
    }

  else if (scr_strcasecmp (argument, "off") == 0)
    {
      gsc_commands_enabled = FALSE;
      gsc_normal_string ("Glk commands are now off.\n");
    }

  else if (strlen (argument) == 0)
    {
      gsc_normal_string ("Glk commands are ");
      gsc_normal_string (gsc_commands_enabled ? "on" : "off");
      gsc_normal_string (".\n");
    }

  else
    {
      gsc_command_usage ("commands");
    }
}


/*
 * gsc_command_license()
 *
 * Print licensing terms.
 */
static void
gsc_command_license (const char *argument)
{
  assert (argument);

  gsc_normal_string ("This program is free software; you can redistribute it"
                     " and/or modify it under the terms of version 2 of the"
                     " GNU General Public License as published by the Free"
                     " Software Foundation.\n\n");

  gsc_normal_string ("This program is distributed in the hope that it will be"
                     " useful, but ");
  gsc_standout_string ("WITHOUT ANY WARRANTY");
  gsc_normal_string ("; without even the implied warranty of ");
  gsc_standout_string ("MERCHANTABILITY");
  gsc_normal_string (" or ");
  gsc_standout_string ("FITNESS FOR A PARTICULAR PURPOSE");
  gsc_normal_string (".  See the GNU General Public License for more"
                     " details.\n\n");

  gsc_normal_string ("You should have received a copy of the GNU General"
                     " Public License along with this program; if not, write"
                     " to the Free Software Foundation, Inc., 51 Franklin"
                     " Street, Fifth Floor, Boston, MA 02110-1301 USA\n\n");

  gsc_normal_string ("Please report any bugs, omissions, or misfeatures to ");
  gsc_standout_string ("simon_baldwin@yahoo.com");
  gsc_normal_string (".\n");
}


/*
 * gsc_a5_hint_text()
 *
 * Render one of a clsHint's answer blocks (<Subtle>/<Sledgehammer>) to plain
 * text, or NULL when the game leaves it empty -- an author who wrote only a
 * subtle nudge should not be asked about a blank sledgehammer.  The caller
 * owns the returned string.
 */
static char *
gsc_a5_hint_text (a5_state_t *st, const a5_xml_node_t *wrapper)
{
  char *text;

  if (wrapper == NULL)
    return NULL;

  text = a5text_describe (st, wrapper);
  if (text != NULL && text[strspn (text, " \t\n\r")] == '\0')
    {
      free (text);
      return NULL;
    }
  return text;
}


/*
 * gsc_a5_display_hints()
 *
 * The ADRIFT 5 hints display.  The runner keeps clsHint out of the parser
 * altogether and puts it behind a Hints window on the menu bar, so there is no
 * game command to collide with; here it hangs off "glk hints" instead, which
 * also leaves the ~40% of ADRIFT 5 games that implement a HINT task of their
 * own free to answer a bare "hint" themselves.
 *
 * A hint carries its own restriction block, which is how an author keeps the
 * list to the puzzles actually in play; hints whose block fails are passed
 * over exactly as though they were not there.
 */
static void
gsc_a5_display_hints (void)
{
  a5_state_t *st = a5run_state (gsc_a5_run);
  int index_, refused, offered;

  refused = 0;
  offered = 0;
  for (index_ = 0; index_ < gsc_a5_adv->n_hints; index_++)
    {
      const a5_hint_t *hint = &gsc_a5_adv->hints[index_];
      char *subtle, *sledgehammer;

      if (!a5restr_pass (st, hint->restrictions))
        continue;

      subtle = gsc_a5_hint_text (st, hint->subtle);
      sledgehammer = gsc_a5_hint_text (st, hint->sledgehammer);

      /* Games ship half-finished hints -- an editor row with an empty question
         and neither answer written (Death Shack's Hint2, Blender's Hint2).
         There is nothing to ask about, so pass over them silently rather than
         prompt against a blank heading. */
      if ((hint->question == NULL || hint->question[0] == '\0')
          && subtle == NULL && sledgehammer == NULL)
        continue;

      /* If enough refusals, offer a way out of the loop. */
      if (refused >= GSC_HINT_REFUSAL_LIMIT)
        {
          if (!os_confirm (GSC_CONF_CONTINUE_HINTS))
            {
              free (subtle);
              free (sledgehammer);
              break;
            }
          refused = 0;
        }

      /* clsHint.Question is a plain String, not a Description: it is shown as
         the author typed it, with no segment selection or ALR pass. */
      if (!gsc_hint_present (hint->question ? hint->question : "", subtle,
                             sledgehammer))
        refused++;
      offered++;

      free (subtle);
      free (sledgehammer);
    }

  if (offered == 0)
    gsc_normal_string ("There are currently no hints available.\n");
}


/*
 * gsc_command_hints()
 *
 * Show the game's built-in hints, if it has any.
 */
static void
gsc_command_hints (const char *argument)
{
  assert (argument);

  if (gsc_is_a5)
    {
      if (gsc_a5_run == NULL || gsc_a5_adv == NULL || gsc_a5_adv->n_hints == 0)
        {
          gsc_normal_string ("This game does not have any hints.\n");
          return;
        }
      gsc_a5_display_hints ();
      return;
    }

  /* ADRIFT <=4 hints hang off tasks, and the game's own "hints" command is
     the usual way in; this is the same display, for the player who reached
     for the Glk command instead.  scr_get_first_game_hint filters to the
     hints currently in play, so an empty iteration is the "not yet" case
     rather than a game without hints. */
  if (gsc_game == NULL)
    return;
  if (scr_get_first_game_hint (gsc_game) == NULL)
    {
      gsc_normal_string ("There are currently no hints available.\n");
      return;
    }
  os_display_hints (gsc_game);
}


/* Glk subcommands and handler functions. */
typedef const struct
{
  const char * const command;                     /* Glk subcommand. */
  void (* const handler) (const char *argument);  /* Subcommand handler. */
  const int takes_argument;                       /* Argument flag. */
  const int in_adrift5;                           /* Offered in the a5 loop. */
  const int is_alias;                             /* Another name for an
                                                     entry listed above. */
  const char * const usage_subject;               /* Noun for the synopsis. */
  const char * const * const usage_options;       /* Arguments accepted, NULL
                                                     terminated; NULL for a
                                                     command taking none. */
} gsc_command_t;
typedef gsc_command_t *gsc_commandref_t;

/* Argument lists for the one-line synopsis printed by gsc_command_usage(),
   and at the foot of a command's entry in "glk help". */
static const char * const GSC_USAGE_ONOFFSTATUS[] = {"on", "off", "status",
                                                     NULL};
static const char * const GSC_USAGE_ONOFF[] = {"on", "off", NULL};
static const char * const GSC_USAGE_MAP[] = {"on", "off", "top", "right",
                                             "zoom [in | out | auto]", NULL};
static const char * const GSC_USAGE_ZOOM[] = {"in", "out", "auto", NULL};

static void gsc_command_summary (const char *argument);
static void gsc_command_help (const char *argument);
static void gsc_command_map (const char *argument);
static void gsc_command_zoom (const char *argument);

/* Commands flagged FALSE for in_adrift5 are ADRIFT <=4 engine specifics:
   abbreviations (the ADRIFT 5 standard library already defines x/l/i/z...),
   capacity, combatassist, moveassist (4.0 Battle System / task quirks), and
   verbose (a 4.0 room-description mode; ADRIFT 5 leaves this to the game).

   Entries flagged is_alias are alternative names for a command listed above
   them.  They are found by the dispatcher and by "glk help", but left out of
   the command listing and the summary poll, so that the alias neither pads
   the list nor makes its command report itself twice. */
static gsc_command_t GSC_COMMAND_TABLE[] = {
  {"summary",        gsc_command_summary,        FALSE, TRUE,  FALSE,
   NULL,                          NULL},
  {"script",         gsc_command_script,         TRUE,  TRUE,  FALSE,
   "script",                      GSC_USAGE_ONOFFSTATUS},
  {"transcript",     gsc_command_script,         TRUE,  TRUE,  TRUE,
   "transcript",                  GSC_USAGE_ONOFFSTATUS},
  {"inputlog",       gsc_command_inputlog,       TRUE,  TRUE,  FALSE,
   "input logging",               GSC_USAGE_ONOFFSTATUS},
  {"readlog",        gsc_command_readlog,        TRUE,  TRUE,  FALSE,
   "read log",                    GSC_USAGE_ONOFFSTATUS},
  {"abbreviations",  gsc_command_abbreviations,  TRUE,  FALSE, FALSE,
   "abbreviation expansions",     GSC_USAGE_ONOFF},
  {"capacity",       gsc_command_capacity,       TRUE,  FALSE, FALSE,
   "carrying capacity recompute", GSC_USAGE_ONOFF},
  {"combatassist",   gsc_command_combat_assist,  TRUE,  FALSE, FALSE,
   "combat assist",               GSC_USAGE_ONOFF},
  {"moveassist",     gsc_command_move_assist,    TRUE,  FALSE, FALSE,
   "move assist",                 GSC_USAGE_ONOFF},
  {"verbose",        gsc_command_verbose,        TRUE,  FALSE, FALSE,
   "verbose descriptions",        GSC_USAGE_ONOFF},
  {"version",        gsc_command_version,        FALSE, TRUE,  FALSE,
   NULL,                          NULL},
  {"map",            gsc_command_map,            TRUE,  TRUE,  FALSE,
   "map",                         GSC_USAGE_MAP},
  {"zoom",           gsc_command_zoom,           TRUE,  TRUE,  FALSE,
   "zoom",                        GSC_USAGE_ZOOM},
  {"commands",       gsc_command_commands,       TRUE,  TRUE,  FALSE,
   "commands",                    GSC_USAGE_ONOFF},
  /* "color" is a full alias rather than a prefix: neither spelling is a
     prefix of the other, so each resolves on its own, at the cost of "glk col"
     matching both and reporting itself ambiguous.  The plurals are what a
     player who has just read "Glk Adrift colours are..." is likely to type
     back; they are prefixed by the singulars, and rely on gsc_command_lookup()
     preferring an exact spelling to keep "glk colour" unambiguous. */
  {"colour",         gsc_command_colour,         TRUE,  TRUE,  FALSE,
   "Adrift colours",              GSC_USAGE_ONOFFSTATUS},
  {"colours",        gsc_command_colour,         TRUE,  TRUE,  TRUE,
   "Adrift colours",              GSC_USAGE_ONOFFSTATUS},
  {"color",          gsc_command_colour,         TRUE,  TRUE,  TRUE,
   "Adrift colours",              GSC_USAGE_ONOFFSTATUS},
  {"colors",         gsc_command_colour,         TRUE,  TRUE,  TRUE,
   "Adrift colours",              GSC_USAGE_ONOFFSTATUS},
  /* The four meta-commands.  "restore" and "restart" share a prefix, so each
     needs five letters to resolve; "quit" answers to "glk q". */
  {"undo",           gsc_command_undo,           FALSE, TRUE,  FALSE,
   NULL,                          NULL},
  {"restore",        gsc_command_restore,        FALSE, TRUE,  FALSE,
   NULL,                          NULL},
  {"restart",        gsc_command_restart,        FALSE, TRUE,  FALSE,
   NULL,                          NULL},
  {"quit",           gsc_command_quit,           FALSE, TRUE,  FALSE,
   NULL,                          NULL},
  /* No "hint" alias row: it would be redundant, as a bare prefix of the sole
     "hints" row it already resolves. */
  {"hints",          gsc_command_hints,          FALSE, TRUE,  FALSE,
   NULL,                          NULL},
  {"license",        gsc_command_license,        FALSE, TRUE,  FALSE,
   NULL,                          NULL},
  {"help",           gsc_command_help,           TRUE,  TRUE,  FALSE,
   NULL,                          NULL},
  {NULL, NULL, FALSE, FALSE, FALSE, NULL, NULL}
};


/*
 * gsc_command_in_scope()
 *
 * Return TRUE if a Glk command table entry applies to the engine driving the
 * current game: everything for ADRIFT <=4 (scare), only the entries flagged
 * in_adrift5 for ADRIFT 5 (the a5 loop).
 */
static int
gsc_command_in_scope (gsc_commandref_t entry)
{
  return !gsc_is_a5 || entry->in_adrift5;
}


/*
 * gsc_command_is_action()
 *
 * True for the Glk commands that do something when invoked rather than
 * carrying a setting.  They have nothing to report, so "glk summary" -- which
 * polls every other handler with an empty argument -- has to leave them alone:
 * asking after the settings must not display the help, or quit the game.
 */
static int
gsc_command_is_action (gsc_commandref_t entry)
{
  return entry->handler == gsc_command_summary
         || entry->handler == gsc_command_license
         || entry->handler == gsc_command_help
         || entry->handler == gsc_command_map
         || entry->handler == gsc_command_zoom
         || entry->handler == gsc_command_hints
         || entry->handler == gsc_command_undo
         || entry->handler == gsc_command_restore
         || entry->handler == gsc_command_restart
         || entry->handler == gsc_command_quit;
}


/*
 * gsc_command_lookup()
 *
 * Find the table entry for a Glk command name, which the player may have
 * abbreviated.  An abbreviation has to be a prefix of exactly one entry, but
 * an exact spelling always wins outright, so that a command which is itself
 * the prefix of another -- "colour" of "colours" -- still resolves.
 *
 * Returns the entry, or NULL if nothing matched or an abbreviation was
 * ambiguous; the number of entries the name could have stood for is stored in
 * matches, so a caller can tell those two apart.
 */
static gsc_commandref_t
gsc_command_lookup (const char *command, int *matches)
{
  gsc_commandref_t entry, matched;
  int count;
  assert (command);

  count = 0;
  matched = NULL;
  for (entry = GSC_COMMAND_TABLE; entry->command; entry++)
    {
      if (!gsc_command_in_scope (entry))
        continue;

      if (scr_strcasecmp (command, entry->command) == 0)
        {
          if (matches)
            *matches = 1;
          return entry;
        }

      if (scr_strncasecmp (command, entry->command, strlen (command)) == 0)
        {
          count++;
          matched = entry;
        }
    }

  if (matches)
    *matches = count;
  return count == 1 ? matched : NULL;
}


/*
 * gsc_command_usage()
 *
 * Print the one-line synopsis of the arguments a Glk command accepts, for
 * example "Glk map can be on, off, top, right, or zoom [in | out | auto]."  Both
 * a command handed an argument it doesn't understand and the foot of that
 * command's "glk help" entry print this, so the two can never disagree.
 */
static void
gsc_command_usage (const char *command)
{
  gsc_commandref_t entry;
  int count, index_;
  assert (command);

  for (entry = GSC_COMMAND_TABLE; entry->command; entry++)
    {
      if (scr_strcasecmp (command, entry->command) == 0)
        break;
    }
  if (!entry->command || !entry->usage_options)
    return;

  gsc_normal_string ("Glk ");
  gsc_normal_string (entry->usage_subject);
  gsc_normal_string (" can be ");

  for (count = 0; entry->usage_options[count]; count++)
    ;
  for (index_ = 0; index_ < count; index_++)
    {
      if (index_ > 0)
        gsc_normal_string (index_ < count - 1 ? ", "
                                              : count > 2 ? ", or " : " or ");
      gsc_standout_string (entry->usage_options[index_]);
    }
  gsc_normal_string (".\n");
}


/*
 * gsc_command_summary()
 *
 * Report all current Glk settings.
 */
static void
gsc_command_summary (const char *argument)
{
  gsc_commandref_t entry;
  assert (argument);

  /*
   * Call handlers that have status to report with an empty argument,
   * prompting each to print its current setting.  The logging commands and
   * the colour mode act rather than report on an empty argument, so those four
   * are polled with an explicit "status" instead; the commands that only ever
   * act (gsc_command_is_action) are not called at all.
   */
  for (entry = GSC_COMMAND_TABLE; entry->command; entry++)
    {
      int is_log;

      if (gsc_command_is_action (entry)
            || entry->is_alias
            || !gsc_command_in_scope (entry))
        continue;

      is_log = entry->handler == gsc_command_script
               || entry->handler == gsc_command_inputlog
               || entry->handler == gsc_command_readlog
               || entry->handler == gsc_command_colour;
      entry->handler (is_log ? "status" : "");
    }
}


/*
 * gsc_command_help()
 *
 * Document the available Glk commands.
 */
static void
gsc_command_help (const char *command)
{
  gsc_commandref_t entry, matched;
  int matches;
  assert (command);

  if (strlen (command) == 0)
    {
      gsc_commandref_t last;

      /* Zoom is left off the list; it belongs to the map, and is documented
         under "glk help map" instead.  Aliases are left off too, and named in
         the help for the command they stand in for. */
      last = NULL;
      for (entry = GSC_COMMAND_TABLE; entry->command; entry++)
        {
          if (gsc_command_in_scope (entry)
              && !entry->is_alias
              && entry->handler != gsc_command_zoom)
            last = entry;
        }

      gsc_normal_string ("Glk commands are");
      for (entry = GSC_COMMAND_TABLE; entry->command; entry++)
        {
          if (!gsc_command_in_scope (entry)
              || entry->is_alias
              || entry->handler == gsc_command_zoom)
            continue;

          gsc_normal_string (entry == last ? " and " : " ");
          gsc_standout_string (entry->command);
          gsc_normal_string (entry == last ? ".\n\n" : ",");
        }

      gsc_normal_string ("Glk commands may be abbreviated, as long as"
                         " the abbreviation is unambiguous.  Use ");
      gsc_standout_string ("glk help");
      gsc_normal_string (" followed by a Glk command name for help on that"
                         " command.\n");
      return;
    }

  matched = gsc_command_lookup (command, &matches);
  if (!matched)
    {
      gsc_normal_string ("The Glk command ");
      gsc_standout_string (command);
      gsc_normal_string (matches == 0 ? " is not valid.  Try "
                                      : " is ambiguous.  Try ");
      gsc_standout_string ("glk help");
      gsc_normal_string (" for more information.\n");
      return;
    }

  if (matched->handler == gsc_command_summary)
    {
      gsc_normal_string ("Prints a summary of all the current Glk SCARIER"
                         " settings.\n");
    }

  else if (matched->handler == gsc_command_map)
    {
      gsc_normal_string ("Shows the game's map beside the story, as the"
                         " ADRIFT Runner does: the rooms you have visited,"
                         " the ways between them, and where you are now.\n\nUse ");
      gsc_standout_string ("glk map on");
      gsc_normal_string (" to show the map and ");
      gsc_standout_string ("glk map off");
      gsc_normal_string (" to hide it again; plain ");
      gsc_standout_string ("map");
      gsc_normal_string (" toggles it too, unless the game uses MAP for"
                         " something of its own.\n\nSome ADRIFT 5 games ask"
                         " to open with their map already showing, and this"
                         " one may be one of them; either way, whichever of ");
      gsc_standout_string ("glk map on");
      gsc_normal_string (" or ");
      gsc_standout_string ("glk map off");
      gsc_normal_string (" you use last is remembered for this game, and the"
                         " next session starts that way.  A map with nothing"
                         " on it yet -- during a title or options screen, say"
                         " -- waits rather than opening empty, and appears as"
                         " soon as you reach somewhere it can show.\n\nFor"
                         " games with wide maps, ");
      gsc_standout_string ("glk map top");
      gsc_normal_string (" (or ");
      gsc_standout_string ("glk map above");
      gsc_normal_string (") moves the map to a band across the top of the"
                         " screen, above the status line; ");
      gsc_standout_string ("glk map right");
      gsc_normal_string (" puts it back beside the story.  This is remembered"
                         " for the game as well, so the map comes back where"
                         " you left it.\n\nThe map zooms"
                         " itself to fit its window.  Use ");
      gsc_standout_string ("glk zoom in");
      gsc_normal_string (" and ");
      gsc_standout_string ("glk zoom out");
      gsc_normal_string (" to zoom by hand instead; the view then pans to"
                         " keep you on-screen.  ");
      gsc_standout_string ("glk zoom auto");
      gsc_normal_string (" restores the automatic fit.\n");
    }

  else if (matched->handler == gsc_command_zoom)
    {
      gsc_normal_string ("Zooms the game's map, which otherwise fits itself"
                         " to its window.\n\nUse ");
      gsc_standout_string ("glk zoom in");
      gsc_normal_string (" and ");
      gsc_standout_string ("glk zoom out");
      gsc_normal_string (" to zoom by hand; the view then pans to keep you"
                         " on-screen.  Plain ");
      gsc_standout_string ("glk zoom");
      gsc_normal_string (" zooms in, and ");
      gsc_standout_string ("glk zoom auto");
      gsc_normal_string (" (or ");
      gsc_standout_string ("glk zoom default");
      gsc_normal_string (") restores the automatic fit.  Each is also"
                         " understood with a map prefix, as in ");
      gsc_standout_string ("glk map zoom in");
      gsc_normal_string (".\n");
    }

  else if (matched->handler == gsc_command_script)
    {
      gsc_normal_string ("Logs the game's output to a file.\n\nUse ");
      gsc_standout_string ("glk script on");
      gsc_normal_string (" to begin logging game output, and ");
      gsc_standout_string ("glk script off");
      gsc_normal_string (" to end it; plain ");
      gsc_standout_string ("glk script");
      gsc_normal_string (" begins logging too.  Glk SCARIER will ask you for a"
                         " file when you turn scripts on.  ");
      gsc_standout_string ("glk script status");
      gsc_normal_string (" says whether logging is currently on.\n\nThe word ");
      gsc_standout_string ("transcript");
      gsc_normal_string (" may be used in place of ");
      gsc_standout_string ("script");
      gsc_normal_string (" in any of these, as in ");
      gsc_standout_string ("glk transcript on");
      gsc_normal_string (".\n");
    }

  else if (matched->handler == gsc_command_inputlog)
    {
      gsc_normal_string ("Records the commands you type into a game.\n\nUse ");
      gsc_standout_string ("glk inputlog on");
      gsc_normal_string (", to begin recording your commands, and ");
      gsc_standout_string ("glk inputlog off");
      gsc_normal_string (" to turn off input logs; plain ");
      gsc_standout_string ("glk inputlog");
      gsc_normal_string (" begins recording too, and ");
      gsc_standout_string ("glk inputlog status");
      gsc_normal_string (" says whether recording is currently on.  You can"
                         " play back recorded commands into a game with the ");
      gsc_standout_string ("glk readlog");
      gsc_normal_string (" command.\n");
    }

  else if (matched->handler == gsc_command_readlog)
    {
      gsc_normal_string ("Plays back commands recorded with ");
      gsc_standout_string ("glk inputlog on");
      gsc_normal_string (".\n\nUse ");
      gsc_standout_string ("glk readlog on");
      gsc_normal_string (", or just ");
      gsc_standout_string ("glk readlog");
      gsc_normal_string (".  Command play back stops at the end of the"
                         " file.  You can also play back commands from a"
                         " text file created using any standard editor.  ");
      gsc_standout_string ("glk readlog status");
      gsc_normal_string (" says whether play back is currently on.\n");
    }

  else if (matched->handler == gsc_command_abbreviations)
    {
      gsc_normal_string ("Controls abbreviation expansion.\n\nGlk SCARIER"
                         " automatically expands several standard single"
                         " letter abbreviations for you; for example, \"x\""
                         " becomes \"examine\".  Use ");
      gsc_standout_string ("glk abbreviations on");
      gsc_normal_string (" to turn this feature on, and ");
      gsc_standout_string ("glk abbreviations off");
      gsc_normal_string (" to turn it off.  While the feature is on, you"
                         " can bypass abbreviation expansion for an"
                         " individual game command by prefixing it with a"
                         " single quote.  Abbreviations never override the"
                         " game's own commands: if the game already recognises"
                         " the single letter you typed (for example as a"
                         " battle or menu choice), it is passed through"
                         " unchanged.\n");
    }

  else if (matched->handler == gsc_command_capacity)
    {
      gsc_normal_string ("Controls how your carried load is accounted for.\n\n"
                         "By default SCARIER mirrors the real ADRIFT Runner,"
                         " keeping a running total updated as you take and drop"
                         " objects.  Use ");
      gsc_standout_string ("glk capacity on");
      gsc_normal_string (" to instead recompute the load afresh from the"
                         " objects you are currently holding on each check"
                         " (legacy SCARIER behaviour), and ");
      gsc_standout_string ("glk capacity off");
      gsc_normal_string (" to return to matching the Runner.  This affects only"
                         " when an over-encumbered take is refused.\n");
    }

  else if (matched->handler == gsc_command_combat_assist)
    {
      gsc_normal_string ("Helps with broken combat.\n\nSome amateur ADRIFT games"
                         " left every character's Accuracy and Agility at 0, so"
                         " no attack ever lands and combat stalemates forever."
                         "  Use ");
      gsc_standout_string ("glk combatassist on");
      gsc_normal_string (" to give such games an automatic hit, letting combat"
                         " play out on the author's strength-vs-defence basis,"
                         " and ");
      gsc_standout_string ("glk combatassist off");
      gsc_normal_string (" to turn it off.  This deliberately deviates from the"
                         " original ADRIFT Runner; games that do configure"
                         " combat are never affected.  For a few games known"
                         " to be uncompletable without it, the assist is"
                         " switched on automatically at startup.\n");
    }

  else if (matched->handler == gsc_command_move_assist)
    {
      gsc_normal_string ("Helps with a broken move task.\n\nA few games were"
                         " authored with a move's destination room left unset;"
                         " the original ADRIFT Runner ignores such a move, which"
                         " can make the game impossible to finish.  Use ");
      gsc_standout_string ("glk moveassist on");
      gsc_normal_string (" to honour these moves to the named room, and ");
      gsc_standout_string ("glk moveassist off");
      gsc_normal_string (" to turn it off.  This deliberately deviates from the"
                         " original ADRIFT Runner.  For a few games known to"
                         " be uncompletable without it, the assist is switched"
                         " on automatically at startup.\n");
    }

  else if (matched->handler == gsc_command_verbose)
    {
      gsc_normal_string ("Controls verbose room descriptions.\n\nUse ");
      gsc_standout_string ("glk verbose on");
      gsc_normal_string (" to make the game always give long descriptions of"
                         " locations, even ones you have visited before, and ");
      gsc_standout_string ("glk verbose off");
      gsc_normal_string (" to give long descriptions only for places never"
                         " before visited.  This mirrors the ADRIFT Runner's"
                         " Verbose option, and works even when a game defines"
                         " its own \"verbose\" command.\n");
    }

  else if (matched->handler == gsc_command_version)
    {
      gsc_normal_string ("Prints the version numbers of the Glk library"
                         " and the Glk SCARIER port.\n");
    }

  else if (matched->handler == gsc_command_commands)
    {
      gsc_normal_string ("Turn off Glk commands.\n\nUse ");
      gsc_standout_string ("glk commands off");
      gsc_normal_string (" to disable all Glk commands, including this one."
                         "  Once turned off, there is no way to turn Glk"
                         " commands back on while inside the game.\n");
    }

  else if (matched->handler == gsc_command_colour)
    {
      gsc_normal_string ("Shows the story in the colours ADRIFT would have"
                         " used.\n\nUse ");
      gsc_standout_string ("glk colour on");
      gsc_normal_string (" to clear the screen to black and draw what follows"
                         " in the game's own colours -- the ADRIFT Runner's"
                         " green replies and red typed text for an ADRIFT 4"
                         " game, or the colours the author chose for an"
                         " ADRIFT 5 one -- honouring any colour the game asks"
                         " for as it goes.  Use ");
      gsc_standout_string ("glk colour off");
      gsc_normal_string (" to clear the screen again and go back to the"
                         " colours of the interpreter's own theme.  ");
      gsc_standout_string ("glk color");
      gsc_normal_string (", ");
      gsc_standout_string ("glk colours");
      gsc_normal_string (", and ");
      gsc_standout_string ("glk colors");
      gsc_normal_string (" are the same command.\n\nGames written for a black"
                         " screen can be hard to read without this, so a game"
                         " that sets colours of its own, or that would show"
                         " text too close to the interpreter's own colours to"
                         " read, starts with this turned on; games that never"
                         " set a colour of their own look much the same either"
                         " way.\n");
    }

  else if (matched->handler == gsc_command_undo
           || matched->handler == gsc_command_restore
           || matched->handler == gsc_command_restart
           || matched->handler == gsc_command_quit)
    {
      gsc_normal_string ("Takes back a turn, restores a saved game, starts"
                         " over, or stops playing.\n\n");
      gsc_standout_string ("glk undo");
      gsc_normal_string (", ");
      gsc_standout_string ("glk restore");
      gsc_normal_string (", ");
      gsc_standout_string ("glk restart");
      gsc_normal_string (" and ");
      gsc_standout_string ("glk quit");
      gsc_normal_string (" do just what the game's own commands of those"
                         " names do.  They are here for the places where"
                         " those are out of reach: a game that asks a question"
                         " of its own -- for your name, say -- takes anything"
                         " you type as the answer, and would read ");
      gsc_standout_string ("quit");
      gsc_normal_string (" as the name you had chosen.  A Glk command is"
                         " recognised at any prompt.\n");
    }

  else if (matched->handler == gsc_command_hints)
    {
      gsc_normal_string ("Shows the hints the game's author wrote, as the"
                         " ADRIFT Runner's Hints window does.\n\nEach hint"
                         " asks its question first, then offers a subtle"
                         " answer and, after that, one that simply tells you;"
                         " answer ");
      gsc_standout_string ("N");
      gsc_normal_string (" to either and the next hint comes up.  Only the"
                         " hints for puzzles you have reached are listed.\n\n");
      gsc_standout_string ("glk hint");
      gsc_normal_string (" abbreviates to the same command.  Many games answer"
                         " a plain ");
      gsc_standout_string ("hint");
      gsc_normal_string (" with hints of their own, which are a different"
                         " thing and are worth trying too.\n");
    }

  else if (matched->handler == gsc_command_license)
    {
      gsc_normal_string ("Prints Glk SCARIER's software license.\n");
    }

  else if (matched->handler == gsc_command_help)
    {
      gsc_command_help ("");
      return;
    }

  else
    gsc_normal_string ("There is no help available on that Glk command."
                       "  Sorry.\n");

  /* Close with the same synopsis the command itself prints when handed an
     argument it doesn't understand. */
  if (matched->usage_options)
    {
      gsc_normal_char ('\n');
      gsc_command_usage (matched->command);
    }
}


/*
 * gsc_command_escape()
 *
 * This function is handed each input line.  If the line contains a specific
 * Glk port command, handle it and return TRUE, otherwise return FALSE.
 */
static int
gsc_command_escape (const char *string)
{
  int posn;
  char *string_copy, *command, *argument;
  assert (string);

  /*
   * Return FALSE if the string doesn't begin with the Glk command escape
   * introducer.
   */
  posn = strspn (string, "\t ");
  if (scr_strncasecmp (string + posn, "glk", strlen ("glk")) != 0)
    return FALSE;

  /* Take a copy of the string, without any leading space or introducer. */
  string_copy = (decltype(string_copy)) gsc_malloc (strlen (string + posn) + 1 - strlen ("glk"));
  strncpy (string_copy, string + posn + strlen ("glk"), strlen (string + posn) + 1 - strlen ("glk"));

  /*
   * Find the subcommand; the first word in the string copy.  Find its end,
   * and ensure it terminates with a NUL.
   */
  posn = strspn (string_copy, "\t ");
  command = string_copy + posn;
  posn += strcspn (string_copy + posn, "\t ");
  if (string_copy[posn] != '\0')
    string_copy[posn++] = '\0';

  /*
   * Now find any argument data for the command: the rest of the line, less
   * leading and trailing whitespace.  Most subcommands take a single word,
   * but "glk map zoom in" takes two.
   */
  posn += strspn (string_copy + posn, "\t ");
  argument = string_copy + posn;
  posn = (int) strlen (argument);
  while (posn > 0
         && (argument[posn - 1] == ' ' || argument[posn - 1] == '\t'))
    argument[--posn] = '\0';

  /*
   * Try to handle the command and argument as a Glk subcommand.  If it
   * doesn't run unambiguously, print command usage.  Treat an empty command
   * as "help".
   */
  if (strlen (command) > 0)
    {
      gsc_commandref_t matched;
      int matches;

      /* Find the table entry the command names, allowing abbreviation. */
      matched = gsc_command_lookup (command, &matches);

      /* If the match was unambiguous, call the command handler. */
      if (matched)
        {
          gsc_normal_char ('\n');

          /* "glk <command> help" is another way of writing "glk help
             <command>"; the two print the same thing. */
          if (scr_strcasecmp (argument, "help") == 0
              && matched->handler != gsc_command_help)
            {
              gsc_command_help (matched->command);
              free (string_copy);
              return TRUE;
            }

          matched->handler (argument);

          if (!matched->takes_argument && strlen (argument) > 0)
            {
              gsc_normal_string ("[The ");
              gsc_standout_string (matched->command);
              gsc_normal_string (" command ignores arguments.]\n");
            }
        }

      /* No match, or the command was ambiguous. */
      else
        {
          gsc_normal_string ("\nThe Glk command ");
          gsc_standout_string (command);
          gsc_normal_string (" is ");
          gsc_normal_string (matches == 0 ? "not valid" : "ambiguous");
          gsc_normal_string (".  Try ");
          gsc_standout_string ("glk help");
          gsc_normal_string (" for more information.\n");
        }
    }
  else
    {
      gsc_normal_char ('\n');
      gsc_command_help ("");
    }

  /* The string contained a Glk command; return TRUE. */
  free (string_copy);
  return TRUE;
}


/*---------------------------------------------------------------------*/
/*  Glk port input functions                                           */
/*---------------------------------------------------------------------*/

/* Quote used to suppress abbreviation expansion and local commands. */
static const char GSC_QUOTED_INPUT = '\'';


/* Table of single-character command abbreviations. */
typedef const struct
{
  const char abbreviation;      /* Abbreviation character. */
  const char *const expansion;  /* Expansion string. */
} gsc_abbreviation_t;
typedef gsc_abbreviation_t *gsc_abbreviationref_t;

static gsc_abbreviation_t GSC_ABBREVIATIONS[] = {
  {'c', "close"},    {'g', "again"},  {'i', "inventory"},
  {'k', "attack"},   {'l', "look"},   {'p', "open"},
  {'q', "quit"},     {'r', "drop"},   {'t', "take"},
  {'x', "examine"},  {'y', "yes"},    {'z', "wait"},
  {'\0', NULL}
};


/*
 * gsc_expand_abbreviations()
 *
 * Expand a few common one-character abbreviations commonly found in other
 * game systems.
 */
static void
gsc_expand_abbreviations (char *buffer, int size)
{
  char *command, abbreviation;
  const char *expansion;
  gsc_abbreviationref_t entry;
  assert (buffer);

  /* Ignore anything that isn't a single letter command. */
  command = buffer + strspn (buffer, "\t ");
  if (!(strlen (command) == 1
        || (strlen (command) > 1 && isspace (command[1]))))
    return;

  /* Scan the abbreviations table for a match. */
  abbreviation = glk_char_to_lower ((unsigned char) command[0]);
  expansion = NULL;
  for (entry = GSC_ABBREVIATIONS; entry->expansion; entry++)
    {
      if (entry->abbreviation == abbreviation)
        {
          expansion = entry->expansion;
          break;
        }
    }

  /*
   * Give author-defined commands precedence over our conveniences.  Many
   * games use single letters as menu choices (battle/conversation menus); if
   * the game already recognises the raw input, leave it untouched rather than
   * expanding it (e.g. "c" -> "close", "k" -> "attack").  The probe matches
   * against the literal letter the player typed.
   */
  if (expansion)
    {
      char literal[2];

      literal[0] = command[0];
      literal[1] = '\0';
      if (scr_does_command_match (gsc_game, literal))
        return;
    }

  /*
   * If a match found, check for a fit, then replace the character with the
   * expansion string.
   */
  if (expansion)
    {
      if (strlen (buffer) + strlen (expansion) - 1 >= (unsigned int) size)
        return;

      memmove (command + strlen (expansion) - 1, command, strlen (command) + 1);
      memcpy (command, expansion, strlen (expansion));

      gsc_standout_string ("[");
      gsc_standout_char (abbreviation);
      gsc_standout_string (" -> ");
      gsc_standout_string (expansion);
      gsc_standout_string ("]\n");
    }
}


/*
 * os_read_line()
 *
 * Read and return a line of player input.
 */
scr_bool
os_read_line (scr_char *buffer, scr_int length)
{
  scr_int characters;
  assert (buffer && length > 0);

  /* If a help request is pending, provide a user hint. */
  gsc_output_provide_help_hint ();

  /*
   * Ensure normal style, update the status line, and issue an input prompt.
   */
  gsc_reset_glk_style ();
  gsc_status_notify ();

  /* Anything a meta-command that unwound the interpreter left to be said --
     "Ok." for a restore, the note that a turn was undone -- belongs above the
     prompt of the game it landed in. */
  gsc_meta_report ();

  /* The map follows the game: the ADRIFT 4 layout is centred on the room you
     are in and grows as you explore, so it is redrawn at every prompt -- and a
     restart, which reaches the front end no other way, is caught here first. */
  gsc_map_notice_restart ();
  gsc_map_redraw ();

#ifdef SPATTERLIGHT
  if (gsc_autorestored)
    /* The restored transcript already ends with the old prompt; skip
       printing another and just take input. */
    gsc_autorestored = FALSE;
  else
    {
      gsc_put_prompt (">");
      /* Autosave at every top-level prompt: after the prompt is printed (so
         the GUI snapshot ends with it) but before input is requested (so
         the archived windows carry no pending request and a restore
         re-enters cleanly right here). */
      gsc_autosave ();
    }
#else
  gsc_put_prompt (">");
#endif

  /* A walk set going by a click on the map supplies the next direction itself,
     in place of reading one from the player.  Echo it so the transcript reads
     as though it had been typed. */
  if (gsc_sc_walk_next (buffer, length))
    {
      glk_set_style (style_Input);
      glk_put_string ((char *) buffer);
      glk_set_style (style_Normal);
      gsc_put_literal ("\n");
      /* The prompt left the input colour in force for a line that is now not
         going to be read, so nothing else will put the output colour back
         before the turn's own text. */
      gsc_colour_echo (FALSE);
      return TRUE;
    }

  /*
   * If we have an input log to read from, use that until it is exhausted.
   * On end of file, close the stream and resume input from line requests.
   */
  if (gsc_readlog_stream)
    {
      glui32 chars;

      /* Get the next line from the log stream. */
      memset (buffer, 0, length);
      chars = glk_get_line_stream (gsc_readlog_stream, buffer, length);
      if (chars > 0)
        {
          /* Echo the line just read in input style. */
          glk_set_style (style_Input);
          gsc_put_buffer (buffer, chars);
          glk_set_style (style_Normal);
          /* As in the walk case above: no keyboard read follows to take the
             prompt's input colour back off. */
          gsc_colour_echo (FALSE);

          /* Return this line as player input. */
          return TRUE;
        }

      /*
       * We're at the end of the log stream.  Close it, and then continue
       * on to request a line from Glk.
       */
      glk_stream_close (gsc_readlog_stream, NULL);
      gsc_readlog_stream = NULL;
    }

  /*
   * No input log being read, or we just hit the end of file on one.  Revert
   * to normal line input; start by getting a new line from Glk.
   */
  characters = gsc_read_line (buffer, length - 1);
  assert (characters <= length);
  buffer[characters] = '\0';

  /*
   * If neither abbreviations nor local commands are enabled, use the data
   * read above without further massaging.
   */
  if (gsc_abbreviations_enabled || gsc_commands_enabled)
    {
      char *command;

      /*
       * If the first non-space input character is a quote, bypass all
       * abbreviation expansion and local command recognition, and use the
       * unadulterated input, less introductory quote.
       */
      command = buffer + strspn (buffer, "\t ");
      if (command[0] == GSC_QUOTED_INPUT)
        {
          /* Delete the quote with memmove(). */
          memmove (command, command + 1, strlen (command));
        }
      else
        {
          /* Check for, and expand, and abbreviated commands. */
          if (gsc_abbreviations_enabled)
            gsc_expand_abbreviations (buffer, length);

          /*
           * Check for standalone "help", then for Glk port special commands;
           * suppress the interpreter's use of this input for Glk commands by
           * returning FALSE.
           */
          if (gsc_commands_enabled)
            {
              int posn;

              posn = strspn (buffer, "\t ");
              gsc_note_help_request (buffer + posn);

              if (gsc_command_escape (buffer))
                {
                  gsc_output_silence_help_hints ();

                  /* UNDO / RESTORE / RESTART / QUIT happen here rather than in
                     the handler: the dispatcher above has since freed the line,
                     and all four may unwind out of the interpreter without
                     coming back.  A successful one never returns; the rest fall
                     through to another prompt.

                     Empty the line first.  It is the interpreter's own input
                     buffer, kept across calls so that "get lamp. go north"
                     runs as two commands, and an unwind skips the point where
                     the next read would have cleared it -- leaving the restored
                     or restarted game to parse "glk restart" as its first
                     command. */
                  memset (buffer, 0, length);
                  gsc_meta_perform ();
                  return FALSE;
                }

              /* A bare MAP shows the map pane, as it did in the ADRIFT 4
                 runner -- unless the game has a MAP command of its own, in
                 which case the game's wins and the pane is reached with
                 "glk map". */
              if (!gsc_map_taken
                  && scr_strcasecmp (buffer + posn, "map") == 0)
                {
                  gsc_map_toggle ();
                  gsc_output_silence_help_hints ();
                  return FALSE;
                }
            }
        }
    }

  /*
   * If there is an input log active, log this input string to it.  Note that
   * by logging here we get any abbreviation expansions but we won't log glk
   * special commands, nor any input read from a current open input log.
   */
  if (gsc_inputlog_stream)
    {
      glk_put_string_stream (gsc_inputlog_stream, buffer);
      glk_put_char_stream (gsc_inputlog_stream, '\n');
    }

  return TRUE;
}


/*
 * os_read_line_debug()
 *
 * Read and return a debugger command line.  There's no dedicated debugging
 * window, so this is just a call to the normal readline, with an additional
 * prompt.
 */
scr_bool
os_read_line_debug (scr_char *buffer, scr_int length)
{
  scr_bool status;

  gsc_output_silence_help_hints ();
  gsc_reset_glk_style ();
  gsc_put_literal ("[SCARIER debug]");
#ifdef SPATTERLIGHT
  /* A debugger prompt is mid-turn: not a state worth autosaving. */
  gsc_in_debug_read = TRUE;
#endif
  status = os_read_line (buffer, length);
#ifdef SPATTERLIGHT
  gsc_in_debug_read = FALSE;
#endif
  return status;
}


/*
 * gsc_get_choice_key()
 *
 * Wait for a keypress matching one of the uppercase characters in `choices`,
 * ignoring Glk special keys, and return it uppercased.
 */
static scr_char
gsc_get_choice_key (const char *choices)
{
  scr_char response;

  do
    {
      event_t event;

      /* Wait for a standard key, ignoring Glk special keys. */
      do
        {
          glk_request_char_event (gsc_main_window);
          gsc_event_wait (evtype_CharInput, &event);
        }
      while (event.val1 > UCHAR_MAX);
      response = glk_char_to_upper (event.val1);
    }
  while (response == '\0' || !strchr (choices, response));

  return response;
}


/*
 * os_confirm()
 *
 * Confirm a game action with a yes/no prompt.
 */
scr_bool
os_confirm (scr_int type)
{
  scr_char response;

  /*
   * Always allow game saves and hint display, and if we're reading from an
   * input log, allow everything no matter what, on the assumption that the
   * user knows what they are doing.
   */
  if (gsc_readlog_stream
      || type == SCR_CONF_SAVE || type == SCR_CONF_VIEW_HINTS)
    return TRUE;

  /* Ensure back to normal style, and update status. */
  gsc_reset_glk_style ();
  gsc_status_notify ();

  /* Prompt for the confirmation, based on the type. */
  if (type == GSC_CONF_SUBTLE_HINT)
    gsc_put_literal ("View the subtle hint for this topic");
  else if (type == GSC_CONF_UNSUBTLE_HINT)
    gsc_put_literal ("View the unsubtle hint for this topic");
  else if (type == GSC_CONF_CONTINUE_HINTS)
    gsc_put_literal ("Continue with hints");
  else
    {
      gsc_put_literal ("Do you really want to ");
      switch (type)
        {
        case SCR_CONF_QUIT:
          gsc_put_literal ("quit");
          break;
        case SCR_CONF_RESTART:
          gsc_put_literal ("restart");
          break;
        case SCR_CONF_SAVE:
          gsc_put_literal ("save");
          break;
        case SCR_CONF_RESTORE:
          gsc_put_literal ("restore");
          break;
        case SCR_CONF_VIEW_HINTS:
          gsc_put_literal ("view hints");
          break;
        default:
          gsc_put_literal ("do that");
          break;
        }
    }
  gsc_put_literal ("? ");

  /* Wait until 'yes' or 'no' entered. */
  response = gsc_get_choice_key ("YN");

  /* Echo the confirmation response, and a new line. */
  glk_set_style (style_Input);
  glk_put_string ((char *)(response == 'Y' ? "Yes" : "No"));
  glk_set_style (style_Normal);
  glk_put_char ('\n');

  /* Use a short delay on restarts, if confirmed. */
  if (type == SCR_CONF_RESTART && response == 'Y')
    gsc_short_delay ();

  /* Return TRUE if 'Y' was entered. */
  return (response == 'Y');
}


/*---------------------------------------------------------------------*/
/*  Glk port event functions                                           */
/*---------------------------------------------------------------------*/

/* Short delay before restarts; 1s, in 100ms segments. */
static const glui32 GSC_DELAY_TIMEOUT = 100;
static const glui32 GSC_DELAY_TIMEOUTS_COUNT = 10;

/*
 * gsc_short_delay()
 *
 * Delay for a short period; used before restarting a completed game, to
 * improve the display where 'r', or confirming restart, triggers an otherwise
 * immediate, and abrupt, restart.
 */
static void
gsc_short_delay (void)
{
  /* Ignore the call if the Glk doesn't have timers. */
  if (glk_gestalt (gestalt_Timer, 0))
    {
      glui32 timeout;

      /* Timeout in small chunks to minimize Glk jitter. */
      glk_request_timer_events (GSC_DELAY_TIMEOUT);
      for (timeout = 0; timeout < GSC_DELAY_TIMEOUTS_COUNT; timeout++)
        {
          event_t event;

          gsc_event_wait (evtype_Timer, &event);
        }
      glk_request_timer_events (0);
    }
}


/*
 * gsc_event_wait_2()
 * gsc_event_wait()
 *
 * Process Glk events until one of the expected type, or types, arrives.
 * Return the event of that type.
 */
static void
gsc_event_wait_2 (glui32 wait_type_1, glui32 wait_type_2, event_t * event)
{
  assert (event);

  do
    {
      glk_select (event);

      switch (event->type)
        {
        case evtype_Arrange:
        case evtype_Redraw:
          gsc_refresh_windows ();
          break;

        case evtype_MouseInput:
          /* A click on a room walks the player there, one room per turn.  The
             pending line request is cancelled so that os_read_line can issue
             the first step in its place; whatever the player had half-typed is
             discarded, as it is on the ADRIFT 5 side. */
          if (event->win == gsc_map_window && gsc_map != NULL
              && gsc_game != NULL)
            {
              map_view_t view;
              char here[16];
              const char *hit;

              scmap_view ((scr_gameref_t) gsc_game, &view);
              hit = map_hit (gsc_map, &view, &gsc_map_cam,
                             gsc_map_px_w, gsc_map_px_h,
                             (int) event->val1, (int) event->val2);
              snprintf (here, sizeof here, "%ld",
                        (long) gs_playerroom ((scr_gameref_t) gsc_game));
              if (hit != NULL && strcmp (hit, here) != 0)
                {
                  gsc_sc_walk_stop ();
                  snprintf (gsc_sc_walk_to, sizeof gsc_sc_walk_to, "%s", hit);
                  glk_cancel_line_event (gsc_main_window, event);
                  break;        /* now a LineInput event: the wait ends */
                }
              /* A click on empty map: re-arm and keep waiting. */
              if (glk_gestalt (gestalt_MouseInput, wintype_Graphics))
                glk_request_mouse_event (gsc_map_window);
            }
          break;
        }
    }
  while (!(event->type == wait_type_1 || event->type == wait_type_2));

#if defined(GLK_MODULE_GARGLK_FILE_RESOURCES) || defined(SPATTERLIGHT)
  /*
   * A completed command line ends a turn, so any graphic drawn before it is no
   * longer a candidate for redraw-after-clear on later turns.
   */
  if (event->type == evtype_LineInput)
    gsc_graphic_drawn_since_input = FALSE;
#endif

#ifdef GSC_HAVE_TITLE_WINDOW
  /* The player's first input dismisses any title/cover image window. */
  if (!gsc_seen_input
      && (event->type == evtype_LineInput || event->type == evtype_CharInput))
    {
      gsc_seen_input = TRUE;
      gsc_close_title_graphic ();
    }
#endif
}

static void
gsc_event_wait (glui32 wait_type, event_t * event)
{
  assert (event);

  gsc_event_wait_2 (wait_type, evtype_None, event);
}


/*---------------------------------------------------------------------*/
/*  Glk port file functions                                            */
/*---------------------------------------------------------------------*/

/*
 * os_open_file()
 *
 * Open a file for save or restore, and return a Glk stream for the opened
 * file.
 */
void *
os_open_file (scr_bool is_save)
{
  glui32 usage, fmode;
  frefid_t fileref;
  strid_t stream;

  usage = fileusage_SavedGame | fileusage_BinaryMode;
  fmode = is_save ? filemode_Write : filemode_Read;

  fileref = glk_fileref_create_by_prompt (usage, fmode, 0);
  if (!fileref)
    return NULL;

  if (!is_save && !glk_fileref_does_file_exist (fileref))
    {
      glk_fileref_destroy (fileref);
      return NULL;
    }

  stream = glk_stream_open_file (fileref, fmode, 0);
  glk_fileref_destroy (fileref);

  return stream;
}


/*
 * os_write_file()
 * os_read_file()
 *
 * Write/read the given buffered data to/from the open Glk stream.
 */
void
os_write_file (void *opaque, const scr_byte *buffer, scr_int length)
{
  strid_t stream = (strid_t) opaque;
  assert (opaque && buffer);

  glk_put_buffer_stream (stream, (char *) buffer, length);
}

scr_int
os_read_file (void *opaque, scr_byte *buffer, scr_int length)
{
  strid_t stream = (strid_t) opaque;
  assert (opaque && buffer);

  return glk_get_buffer_stream (stream, (char *) buffer, length);
}


/*
 * os_close_file()
 *
 * Close the opened Glk stream.
 */
void
os_close_file (void *opaque)
{
  strid_t stream = (strid_t) opaque;
  assert (opaque);

  glk_stream_close (stream, NULL);
}


/*---------------------------------------------------------------------*/
/*  main() and options parsing                                         */
/*---------------------------------------------------------------------*/

/* Loading message flush delay timeout. */
static const glui32 GSC_LOADING_TIMEOUT = 100;

/* Enumerated game end options. */
enum gsc_end_option { GAME_RESTART, GAME_UNDO, GAME_QUIT };

/*
 * The following value needs to be passed between the startup_code and main
 * functions.
 */
static const char *gsc_game_message = NULL;


/*
 * gsc_callback()
 *
 * Callback function for reading in game and restore file data; fills a
 * buffer with TAF or TAS file data from a Glk stream, and returns the byte
 * count.
 */
static scr_int
gsc_callback (void *opaque, scr_byte *buffer, scr_int length)
{
  strid_t stream = (strid_t) opaque;
  assert (stream);

  return glk_get_buffer_stream (stream, (char *) buffer, length);
}


/*
 * gsc_get_ending_option()
 *
 * Offer the option to restart, undo, or quit.  Returns the selected game
 * end option.  Called on game completion.
 */
static enum gsc_end_option
gsc_get_ending_option (void)
{
  const char *echo;
  enum gsc_end_option option;

  /* Ensure back to normal style, and update status. */
  gsc_reset_glk_style ();
  gsc_status_notify ();

  /* Prompt for restart, undo, or quit, and wait for one of the three. */
  gsc_put_literal ("\nWould you like to RESTART, UNDO a turn, or QUIT? ");
  switch (gsc_get_choice_key ("RUQ"))
    {
    case 'R':
      echo = "Restart";
      option = GAME_RESTART;
      break;
    case 'U':
      echo = "Undo";
      option = GAME_UNDO;
      break;
    default:
      echo = "Quit";
      option = GAME_QUIT;
      break;
    }

  /* Echo the confirmation response, and a new line. */
  glk_set_style (style_Input);
  glk_put_string ((char *) echo);
  glk_set_style (style_Normal);
  glk_put_char ('\n');

  return option;
}


/*
 * gsc_apply_known_game_assists()
 *
 * Hardcoded per-game assist defaults.  A few catalogued ADRIFT 4.0 games are
 * unwinnable, or have whole goal chains unreachable, in the faithful Runner
 * behaviour because of the exact authoring accidents the opt-in assists were
 * written for:
 *
 *  - The Town of Azra: a 3.9 game upgraded to the 4.0 file format, leaving
 *    every character's Accuracy and Agility at 0, so under the 4.0
 *    accuracy>agility hit test no attack ever lands and every combat-gated
 *    goal is closed.
 *  - To hell & beyond: the same unconfigured combat, plus progression move
 *    tasks whose "To:" combo was left unset -- faithfully ignored, the player
 *    never leaves the mansion and neither ending is reachable.
 *  - The X-Files: A New Beginning: completable, but the move summoning Dean
 *    when the player pushes his diner's buzzer has an unset "To:" combo, so
 *    the diner's owner (and all his conversation) never appears in the game.
 *  - HYPER Battle System: completable, but the move bringing the Flare Rat
 *    into the Attack Menu when the player attacks has the same unset "To:"
 *    combo, so the opponent is never visibly present during its own battle
 *    (cosmetic only -- the fight is driven by variables, not presence).
 *
 * For these known games the matching assists default to on, applied at game
 * start; "glk combatassist off" / "glk moveassist off" still turn them off,
 * and a one-line notice is printed at startup (see gsc_main).  True 3.9/3.8-
 * signature games (e.g. Villains and Kings) are deliberately NOT listed:
 * their combat is repaired unconditionally by the engine's legacy hit model.
 *
 * Games are recognised by the TAF's GameName and GameAuthor, compared
 * case-insensitively, so every release of a game is covered (the two known
 * Town of Azra releases differ only in CompileDate).  Author strings are the
 * TAF's raw Windows-1252 bytes.
 */
typedef const struct
{
  const char * const game_name;    /* TAF GameName. */
  const char * const game_author;  /* TAF GameAuthor. */
  const scr_bool combat_assist;    /* Default combat assist on. */
  const scr_bool move_assist;      /* Default move assist on. */
  const char * const reason;       /* Startup notice: why assists are on. */
} gsc_game_assist_t;

static gsc_game_assist_t GSC_GAME_ASSIST_TABLE[] = {
  {"The Town of Azra", "S. P. Tencza", TRUE, FALSE,
   "This game's combat cannot be won as authored"},
  {"To hell & beyond", "Steingr\xedmur J\xf3nsson", TRUE, TRUE,
   "This game cannot be completed as authored"},
  {"The X-Files: A New Beginning", "Superbone Ali", FALSE, TRUE,
   "A character in this game never appears as authored"},
  /* The GameName is the game's <wait>-animated title screen with the tags
     stripped, hence the run-together "1.1Copyright". */
  {"HYPER Battle System Version 1.1Copyright 2002 Seciden Mencarde",
   "Seciden Mencarde", FALSE, TRUE,
   "A character in this game never appears as authored"},
  {NULL, NULL, FALSE, FALSE, NULL}
};

/* Which assists were switched on automatically, and the matched table row's
   reason wording, for the startup notice. */
static scr_bool gsc_combat_assist_auto = FALSE;
static scr_bool gsc_move_assist_auto = FALSE;
static const char *gsc_assist_auto_reason = NULL;

static void
gsc_apply_known_game_assists (scr_game game)
{
  const char *name, *author;
  gsc_game_assist_t *entry;

  name = scr_get_game_name (game);
  author = scr_get_game_author (game);
  if (!name || !author)
    return;

  for (entry = GSC_GAME_ASSIST_TABLE; entry->game_name; entry++)
    {
      if (scr_strcasecmp (name, entry->game_name) == 0
          && scr_strcasecmp (author, entry->game_author) == 0)
        {
          if (entry->combat_assist && !scr_get_combat_assist ())
            {
              scr_set_combat_assist (TRUE);
              gsc_combat_assist_auto = TRUE;
            }
          if (entry->move_assist && !scr_get_move_assist ())
            {
              scr_set_move_assist (TRUE);
              gsc_move_assist_auto = TRUE;
            }
          if (gsc_combat_assist_auto || gsc_move_assist_auto)
            gsc_assist_auto_reason = entry->reason;
          break;
        }
    }
}


/*
 * gsc_hash_game_stream()
 *
 * Set gsc_game_key from the contents of the game file, and rewind the stream
 * so that the loader still sees it whole.
 *
 * This is only ever a name to hang a settings file off, never a claim about
 * the file's contents, so a 64-bit FNV-1a of the bytes with the length hung
 * on the end is ample: the whole population it has to keep apart is one
 * player's game folder.  It is deliberately not the Treaty of Babel IFID --
 * that is a published identifier, and inventing a private one that looks like
 * it would be worse than an obviously local name.
 */
static void
gsc_hash_game_stream (strid_t stream)
{
  static const unsigned long long fnv_offset = 0xcbf29ce484222325ULL;
  static const unsigned long long fnv_prime = 0x100000001b3ULL;
  unsigned long long hash = fnv_offset;
  unsigned long length = 0;
  char buffer[4096];
  glui32 count;

  do
    {
      glui32 i;

      count = glk_get_buffer_stream (stream, buffer, (glui32) sizeof buffer);
      for (i = 0; i < count; i++)
        hash = (hash ^ (unsigned char) buffer[i]) * fnv_prime;
      length += count;
    }
  while (count == sizeof buffer);

  glk_stream_set_position (stream, 0, seekmode_Start);
  snprintf (gsc_game_key, sizeof gsc_game_key, "%016llx%08lx", hash, length);
}


/*
 * gsc_startup_code()
 * gsc_main
 *
 * Together, these functions take the place of the original main().  The
 * first one is called from the platform-specific startup_code(), to parse
 * and generally handle options.  The second is called from glk_main, and
 * does the real work of running the game.
 */
static int
gsc_startup_code (strid_t game_stream, strid_t restore_stream,
                  scr_uint trace_flags, scr_bool enable_debugger,
                  scr_bool stable_random, const scr_char *locale)
{
  winid_t window = NULL;
  assert (game_stream);

  /* Open a temporary Glk main window. */
#ifdef SPATTERLIGHT
  /* Not when an autorestore is coming: during one the host has already
     rebuilt its windows from its GUI snapshot, and a window this process
     opens and closes takes the host's restored window of the same peer id
     down with it (the host reuses an existing peer on open, but a close is
     unconditional).  See gsc_autorestore_wanted. */
  if (!gsc_autorestore_wanted ())
#endif
    window = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
  if (window)
    {
      /* Clear and initialize the temporary window. */
      glk_window_clear (window);
      glk_set_window (window);
      glk_set_style (style_Normal);

      /*
       * Display a brief loading game message; here we have to use a timeout
       * to ensure that the text is flushed to Glk.
       */
      gsc_put_literal ("Loading game...\n");
      if (glk_gestalt (gestalt_Timer, 0))
        {
          event_t event;

          glk_request_timer_events (GSC_LOADING_TIMEOUT);
          do
            {
              glk_select (&event);
            }
          while (event.type != evtype_Timer);
          glk_request_timer_events (0);
        }
    }

  /* If the Glk libarary does not support unicode, disable it. */
  if (!gsc_has_unicode || !glk_gestalt (gestalt_Unicode, 0))
    gsc_unicode_enabled = FALSE;

  /*
   * If a locale was requested, set it in the core interpreter now.  This
   * locale will preempt any auto-detected one found from inspecting the
   * game on creation.  After game creation, the Glk locale is synchronized
   * to the core interpreter's locale.
   */
  if (locale)
    scr_set_locale (locale);

  /*
   * Set tracing flags, then try to create a SCARIER game reference from the
   * TAF file.  Since we need this in our call from glk_main, we have to keep
   * it in a module static variable.  If we can't open the TAF file, then
   * we'll set the pointer to NULL, and complain about it later in main.
   * Passing the message string around like this is a nuisance...
   */
  scr_set_trace_flags (trace_flags);

  /*
   * Select portable, predictable random number generation *before* loading the
   * game.  Game creation (run_create -> gs_create) draws random initial event
   * times (scr_randomint, scgamest.cpp), so reseeding only after the load would
   * leave those initial times -- and hence the whole event schedule -- governed
   * by the unseeded, time-based RNG, making event-heavy games nondeterministic
   * run to run even with determinism mode on.
   */
  if (stable_random)
    {
      scr_set_portable_random (TRUE);
      scr_reseed_random_sequence (1);
    }

  /* Name the game file, while the stream is still open and at its start: both
     loaders below want the file from the beginning, and only one of them (the
     ADRIFT 5 one) finds an IFID inside it. */
  gsc_hash_game_stream (game_stream);

  /*
   * ADRIFT 5 detection.  ADRIFT 5 games are zlib-compressed XML (optionally
   * Blorb-wrapped) and unrelated to the ADRIFT <=4 TAF format the scare engine
   * reads.  a5model_load returns NULL cleanly for a non-ADRIFT-5 file, so we
   * try it first; on success we run the dedicated a5 turn loop (gsc_a5_main)
   * and skip the scare path entirely.  a5model_load reads the file by path, so
   * this requires gsc_game_path to have been set by the startup code.
   */
  if (gsc_game_path[0] != '\0')
    {
      gsc_a5_adv = a5model_load (gsc_game_path);
      if (gsc_a5_adv)
        {
          gsc_is_a5 = TRUE;
          gsc_game = NULL;
          gsc_game_message = NULL;
          /* Unlike ADRIFT 4, where the palette is a Runner preference the .taf
             knows nothing about, an ADRIFT 5 adventure carries the author's
             own colours; colour mode uses those. */
          gsc_colour_background = gsc_a5_adv->bg_colour;
          gsc_colour_output = gsc_a5_adv->output_colour;
          gsc_colour_input = gsc_a5_adv->input_colour;
#ifdef GSC_HAVE_ZCOLORS
          /* An adventure that chose its own colours starts in them, as "-c"
             would; asked here, while the loading window is still up to measure
             the theme against. */
          if (gsc_colour_detect (window))
            gsc_colour_startup = TRUE;
#endif
          glk_stream_close (game_stream, NULL);
          if (restore_stream)
            glk_stream_close (restore_stream, NULL);
          if (window)
            glk_window_close (window, NULL);
#ifdef GARGLK
          if (gsc_a5_adv->title && gsc_a5_adv->title[0])
            {
              /* The title may carry ADRIFT markup (Trapped's is
                 "<centre><b>'Trapped'  by Driftwood</b></centre>"); render it
                 down to plain text before handing it to the host UI, exactly as
                 the in-game title Display does (see a5run.cpp). */
              char *tp = a5text_render_plain (gsc_a5_adv->title);
              garglk_set_story_name (tp);
              garglk_set_story_title (tp);
              free (tp);
            }
#endif
          return TRUE;
        }
    }

  gsc_game = scr_game_from_callback (gsc_callback, game_stream);
  if (!gsc_game)
    {
      gsc_game = NULL;
      gsc_game_message = "Unable to load an Adrift game from the"
                         " requested file.";
    }
  else
    gsc_game_message = NULL;
  glk_stream_close (game_stream, NULL);

  /*
   * If the game was created successfully and there is a restore stream, try
   * to immediately restore the game from that stream.
   */
  if (gsc_game && restore_stream)
    {
      if (!scr_load_game_from_callback (gsc_game, gsc_callback, restore_stream))
        {
          scr_free_game (gsc_game);
          gsc_game = NULL;
          gsc_game_message = "Unable to restore this Adrift game from the"
                             " requested file.";
        }
      else
        gsc_game_message = NULL;
    }
  if (restore_stream)
    glk_stream_close (restore_stream, NULL);

  /* If successful, set game debugging and synchronize to the core's locale. */
  if (gsc_game)
    {
      scr_set_game_debugger_enabled (gsc_game, enable_debugger);
      gsc_set_locale (scr_get_locale ());

      /* Default the assists on for known broken games, before the game's
         battle_start() reads the combat-assist flag. */
      gsc_apply_known_game_assists (gsc_game);

#ifdef GSC_HAVE_ZCOLORS
      /* A game whose text was written for the Runner's pane starts in the
         Runner's palette, as "-c" would; asked here, while the loading window
         is still up to measure the theme against. */
      if (gsc_colour_detect (window))
        gsc_colour_startup = TRUE;
#endif
    }

  /* Close the temporary window. */
  if (window)
    glk_window_close (window, NULL);

  /* Set title of game, and pass it to the host UI via wintitle().  gsc_game is
     NULL when the load/restore above failed (gsc_game_message is set instead),
     and scr_get_game_name would dereference it -- guard as the debugger/locale
     block above does. */
#ifdef GARGLK
    if (gsc_game)
      {
        garglk_set_story_name(scr_get_game_name(gsc_game));
        garglk_set_story_title(scr_get_game_name(gsc_game));
      }
#endif

  /* Game set up, perhaps successfully. */
  return TRUE;
}

static void
gsc_main (void)
{
  scr_bool is_running;
  int autorestore = FALSE;

#ifdef SPATTERLIGHT
  /* A failed game load has no state to restore onto, and needs a window to
     report itself in -- take the normal path and print the complaint. */
  autorestore = gsc_game != NULL && gsc_autorestore_wanted ();
#endif

  /* Ensure SCARIER internal types have the right sizes. */
  if (!(sizeof (scr_byte) == 1 && sizeof (scr_char) == 1
        && sizeof (scr_uint) >= 4 && sizeof (scr_int) >= 4
        && sizeof (scr_uint) <= 8 && sizeof (scr_int) <= 8))
    {
      gsc_fatal ("GLK: Types sized incorrectly, recompilation is needed");
      glk_exit ();
    }

  gsc_hint_window_styles ();

  /* Create the Glk window, and set its stream as the current one.  An
     autorestore adopts the archived windows below instead: opening any here
     would cost the player the host's restored ones (see
     gsc_autorestore_wanted). */
  if (!autorestore)
    {
#ifdef GSC_HAVE_ZCOLORS
      /* Starting in colour ("-c", or a game that needs its palette to be
         read): publish Normal colours before the first open so the windows
         snapshot them; gsc_colour_startup_apply then enables zcolors without
         tearing the tree down again. */
      if (gsc_colour_startup)
        {
          gsc_colour_set_normal_hints (TRUE);
          gsc_colour_hints_preapplied = TRUE;
        }
#endif
      gsc_open_main_window ();

      /* If there's a problem with the game file, complain now. */
      if (!gsc_game)
        {
          assert (gsc_game_message);
          gsc_header_string ("Glk SCARIER Error\n\n");
          gsc_normal_string (gsc_game_message);
          gsc_normal_char ('\n');
          glk_exit ();
        }

      gsc_open_status_window ();

      /* Into the game's palette before a word is printed. */
      gsc_colour_startup_apply ();
    }

  /* Does the game define a MAP verb of its own?  If so it keeps it, and the
     map pane is reached with "glk map" instead. */
  gsc_map_taken = scmap_command_taken ((scr_gameref_t) gsc_game);

  /* Mention any assists switched on automatically for this known game (see
     gsc_apply_known_game_assists), and how to get faithful behaviour back.
     Not on an autorestore: the note is already in the restored transcript,
     and there is no window to print it to yet. */
  if (!autorestore && (gsc_combat_assist_auto || gsc_move_assist_auto))
    {
      gsc_normal_char ('[');
      gsc_normal_string (gsc_assist_auto_reason
                         ? gsc_assist_auto_reason
                         : "This game cannot be completed as authored");
      gsc_normal_string (", so ");
      if (gsc_combat_assist_auto && gsc_move_assist_auto)
        gsc_normal_string ("combat assist and move assist have");
      else if (gsc_combat_assist_auto)
        gsc_normal_string ("combat assist has");
      else
        gsc_normal_string ("move assist has");
      gsc_normal_string (" been enabled.  Type ");
      if (gsc_combat_assist_auto)
        gsc_standout_string ("glk combatassist off");
      if (gsc_combat_assist_auto && gsc_move_assist_auto)
        gsc_normal_string (" and ");
      if (gsc_move_assist_auto)
        gsc_standout_string ("glk moveassist off");
      gsc_normal_string (" to restore the original ADRIFT Runner"
                         " behaviour.]\n\n");
    }

#ifdef SPATTERLIGHT
  /* When a Spatterlight autosave exists, replace the whole state -- engine
     and Glk library both -- with the saved one before entering the
     interpreter loop.  The game was already created at startup; loading the
     saved state marks the player's room seen, so run_main_loop skips the
     intro and drops straight to the command prompt, where os_read_line
     skips one prompt print (the restored transcript already ends with it).
     The app restores the window contents from its own GUI snapshot; no
     window has been opened above, so the restored library supplies them. */
  if (autorestore)
    {
      std::string data;
      bool restored = scarier_autosave_read_game (&data)
                      && gsc_sc_apply_all (data)
                      && scarier_autosave_restore_library ();
      if (!restored)
        {
          /* Bad autosave (now deleted): restart in a fresh process rather
             than continue from a polluted boot. */
          scarier_autosave_discard ();
          win_reset ();
          exit (0);
        }
      /* The app resumes any interrupted sound and restores the graphics
         window pixels itself; the engine must not replay them. */
      scr_note_resources_synced (gsc_game);
      /* ...and the blank line the engine prints before every prompt is in
         the restored transcript too (the autosave was taken between it and
         the prompt), so skip that one reprint as well. */
      scr_note_autorestored ();
      glk_set_window (gsc_main_window);
      glk_set_style (style_Normal);
      gsc_autorestored = TRUE;
    }
#endif

  /* Bring back the map if the player left this game with one.  Before the
     intro rather than after it, unlike the ADRIFT 5 path: there is no title
     page here to keep the screen to itself, and opening the pane first spares
     the opening text being laid out twice.  It usually opens a moment later
     anyway -- the starting room is not marked seen until it has been
     described, so there is nothing to draw yet and the first prompt's redraw
     is what actually reveals it.  An autorestore keeps the archived pane
     instead, whatever it was. */
  if (!autorestore)
    gsc_map_auto_reveal ();

  /* Repeat the game until no more restarts requested. */
  is_running = TRUE;
  while (is_running)
    {
#ifdef GSC_HAVE_TITLE_WINDOW
      /* Each (re)start replays the intro, so allow the title window again
         -- except on the autorestore pass, whose title-window state was
         just recovered from the archive. */
#ifdef SPATTERLIGHT
      if (!gsc_autorestored)
#endif
        gsc_seen_input = FALSE;
#endif
      /* Run the game until it ends, or the user quits. */
      gsc_status_notify ();
      scr_interpret_game (gsc_game);

      /*
       * If the game did not complete, the user quit explicitly, so leave the
       * game repeat loop.
       */
      if (!scr_has_game_completed (gsc_game))
        break;

      /*
       * If reading from an input log, close it now.  We need to request a
       * user selection, probably modal, and after that we probably don't
       * want the follow-on readlog data being used as game input.
       */
      if (gsc_readlog_stream)
        {
          glk_stream_close (gsc_readlog_stream, NULL);
          gsc_readlog_stream = NULL;
        }

      /*
       * Get user selection of restart, undo a turn, or quit completed game.
       * If undo is unavailable (this should not be possible), degrade to
       * restart.
       */
      switch (gsc_get_ending_option ())
        {
        case GAME_RESTART:
          gsc_short_delay ();
          scr_restart_game (gsc_game);
          break;

        case GAME_UNDO:
          if (scr_is_game_undo_available (gsc_game))
            {
              scr_undo_game_turn (gsc_game);
              gsc_normal_string ("The previous turn has been undone.\n");
            }
          else
            {
              gsc_normal_string ("Sorry, no undo is available.\n");
              gsc_short_delay ();
              scr_restart_game (gsc_game);
            }
          break;

        case GAME_QUIT:
          is_running = FALSE;
          break;
        }
    }

  /* All done -- release game resources. */
  map_free (gsc_map);
  gsc_map = NULL;
  gsc_map_screen_drop ();
  scr_free_game (gsc_game);

  /* Close any open transcript, input log, and/or read log. */
  if (gsc_transcript_stream)
    {
      glk_stream_close (gsc_transcript_stream, NULL);
      gsc_transcript_stream = NULL;
    }
  if (gsc_inputlog_stream)
    {
      glk_stream_close (gsc_inputlog_stream, NULL);
      gsc_inputlog_stream = NULL;
    }
  if (gsc_readlog_stream)
    {
      glk_stream_close (gsc_readlog_stream, NULL);
      gsc_readlog_stream = NULL;
    }
}


/*---------------------------------------------------------------------*/
/*  Linkage between Glk entry/exit calls and the real interpreter      */
/*---------------------------------------------------------------------*/

/*
 * Safety flags, to ensure we always get startup before main, and that
 * we only get a call to main once.
 */
static int gsc_startup_called = FALSE,
           gsc_main_called = FALSE;

/*---------------------------------------------------------------------*/
/*  ADRIFT 5 Glk driver                                                */
/*                                                                     */
/*  A minimal text-buffer turn loop over the a5 engine (a5run_*).  The */
/*  a5 engine produces plain UTF-8 text; meta-commands (quit, restart, */
/*  save, restore) are handled here at the host level since the engine */
/*  does not interpret them itself.                                    */
/*---------------------------------------------------------------------*/

/*
 * gsc_a5_put_string()
 *
 * Print a NUL-terminated UTF-8 string from the a5 engine to the current Glk
 * window, decoding to Unicode code points so non-ASCII text (smart quotes,
 * accented letters) renders correctly.  Falls back to raw output if the Glk
 * library has no Unicode support.
 */
static void
gsc_a5_put_string (const char *string)
{
  const unsigned char *p = (const unsigned char *) string;

  if (string == NULL)
    return;
  if (string[0] != '\0' && gsc_main_window
      && glk_stream_get_current () == glk_window_get_stream (gsc_main_window))
    gsc_main_window_empty = FALSE;
  if (!gsc_unicode_enabled)
    {
      glk_put_string ((char *) string);
      return;
    }

  while (*p)
    {
      glui32 c = *p++;
      int extra;

      if (c < 0x80)
        extra = 0;
      else if ((c & 0xe0) == 0xc0)
        { c &= 0x1f; extra = 1; }
      else if ((c & 0xf0) == 0xe0)
        { c &= 0x0f; extra = 2; }
      else if ((c & 0xf8) == 0xf0)
        { c &= 0x07; extra = 3; }
      else
        { c = '?'; extra = 0; }   /* invalid lead byte */

      while (extra-- > 0 && (*p & 0xc0) == 0x80)
        c = (c << 6) | (*p++ & 0x3f);

      glk_put_char_uni (c);
    }
}

/*
 * gsc_a5_put_prompt()
 *
 * The ADRIFT 5 loop's "> ", in the adventure's own input colour when colour
 * mode is on -- gsc_put_prompt's counterpart for the engine that writes its
 * text as UTF-8.  `before` is whatever leads up to the prompt (the blank line
 * every prompt sits on, plus the question the end-of-game banner asks); it
 * stays in the output colour, since only the prompt itself belongs to the
 * player's typing.
 */
static void
gsc_a5_put_prompt (const char *before)
{
  gsc_a5_put_string (before);
  gsc_colour_echo (TRUE);
  gsc_a5_put_string ("> ");
}

/*
 * gsc_a5_start_real_time()
 *
 * Decide whether this session runs the game's TimeBased events in real time,
 * and arm (or disarm) the 1-second Glk timer accordingly.  The real Runner
 * ticks TimeBased events off a wall-clock timer (tmrEvents_Tick); the
 * engine's default is a deterministic substitute -- one tick per input line
 * -- which headless harnesses and walkthrough replays depend on.  So real
 * time is used only when the Glk has timers and line-input echo control (a
 * tick must be able to cancel pending input cleanly), when determinism mode
 * is off, and when the game defines TimeBased events at all.  The engine
 * flag lives on the run: call again after every a5run_new.
 */
static void
gsc_a5_start_real_time (a5_run_t *run)
{
#ifdef GLK_MODULE_LINE_ECHO
  gsc_a5_real_time = glk_gestalt (gestalt_Timer, 0)
                     && glk_gestalt (gestalt_LineInputEcho, 0)
                     && a5run_has_time_events (run);
#if defined(SPATTERLIGHT)
  /* Determinism (testing) mode keeps the reproducible per-turn model. */
  if (gli_determinism)
    gsc_a5_real_time = FALSE;
#endif
#else
  gsc_a5_real_time = FALSE;
#endif
  a5run_set_real_time (run, gsc_a5_real_time);
  glk_request_timer_events (gsc_a5_real_time ? 1000 : 0);
}

#ifdef GSC_HAVE_UNPUT
/*
 * gsc_unput_tail()
 *
 * Take the ASCII string `s` back off the end of the main window, but only if
 * it is still exactly what is there: garglk_unput_string_count_uni is a
 * case-insensitive TAIL compare that removes nothing unless the whole string
 * matches, so failure always degrades to "leave the text alone".  It
 * retracts from the CURRENT output stream, so point that at the main window
 * first and put it back after.  Returns TRUE when the full string was
 * removed.  (Same retract the geas frontends use; see
 * questglk-common.inc unput_window_tail.)
 */
static int
gsc_unput_tail (const char *s)
{
  glui32 ubuf[16];
  size_t length = strlen (s), index_;
  strid_t saved;
  glui32 got;

  if (length == 0 || length >= sizeof ubuf / sizeof *ubuf
      || gsc_main_window == NULL)
    return FALSE;
  for (index_ = 0; index_ < length; index_++)
    ubuf[index_] = (glui32) (unsigned char) s[index_];
  ubuf[length] = 0;
  saved = glk_stream_get_current ();
  glk_set_window (gsc_main_window);
  got = garglk_unput_string_count_uni (ubuf);
  glk_stream_set_current (saved);
  return got == length;
}
#endif /* GSC_HAVE_UNPUT */

/*
 * gsc_a5_sound_marks_only()
 *
 * True when a turn text carries no visible output -- nothing but positional
 * A5_SOUND_MARK / A5_WAIT_MARK spans and whitespace (a sound-only commit's
 * spans keep the text non-empty where it used to render to "").  True for ""
 * itself, so a caller can use this as its whole output-less test.
 */
static int
gsc_a5_sound_marks_only (const char *text)
{
  const char *p;

  if (text == NULL)
    return TRUE;
  for (p = text; *p != '\0'; p++)
    {
      if (*p == A5_SOUND_MARK || *p == A5_WAIT_MARK)
        {
          const char *e = strchr (p + 1, *p);
          if (e == NULL)
            return FALSE;
          p = e;
        }
      else if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
        return FALSE;
    }
  return TRUE;
}

/*
 * gsc_a5_await_line()
 *
 * Wait for line input on the main window, servicing resize redraws and, in
 * real-time mode, the 1-second TimeBased event tick.  A tick that produces
 * output cancels the pending input request (echo is off, so the cancel is
 * clean -- Glk forbids printing to a window with a live line request),
 * retracts the stale prompt where the host supports it (ending its line
 * otherwise), shows the tick's commit, reprints the prompt, and re-requests
 * the line pre-seeded with whatever the player had already typed.  Exactly
 * one of buf/ubuf is non-NULL, matching the pending request's buffer.
 * Returns TRUE when line input completed, FALSE when a tick ended the game
 * (the pending request has been cancelled and the end-of-game text already
 * shown).
 */
static int
gsc_a5_await_line (event_t *event, char *buf, int bufsize,
                   glui32 *ubuf, glui32 ucap)
{
  for (;;)
    {
      glk_select (event);
      switch (event->type)
        {
        case evtype_Arrange:
        case evtype_Redraw:
          gsc_refresh_windows ();
          break;

        case evtype_MouseInput:
          /* A click on a room walks the player there (Map.vb imgMap_MouseDown:
             Player.WalkTo = node; DoWalk()).  Cancel the pending line request
             and let gsc_a5_read_line issue the first step instead. */
          if (event->win == gsc_map_window && gsc_a5_run != NULL)
            {
              map_view_t view;
              a5_state_t *st = a5run_state (gsc_a5_run);
              const char *hit, *here;

              gsc_a5_map_view (st, &view);
              hit = map_hit (gsc_map, &view, &gsc_map_cam,
                               gsc_map_px_w, gsc_map_px_h,
                               (int) event->val1, (int) event->val2);
              here = a5state_player_location (st);
              if (hit != NULL && (here == NULL || strcmp (hit, here) != 0))
                {
                  gsc_a5_walk_stop ();
                  snprintf (gsc_a5_walk_to, sizeof gsc_a5_walk_to, "%s", hit);
                  gsc_a5_walk_clicked = TRUE;
                  glk_cancel_line_event (gsc_main_window, event);
                  return FALSE;
                }
              /* A click on empty map: re-arm and keep waiting. */
              if (glk_gestalt (gestalt_MouseInput, wintype_Graphics))
                glk_request_mouse_event (gsc_map_window);
            }
          break;

        case evtype_Timer:
          if (gsc_a5_real_time && gsc_a5_run != NULL)
            {
              char *text = a5run_time_tick (gsc_a5_run);

              if (text == NULL)
                break;                          /* silent tick */
              if (gsc_a5_sound_marks_only (text))
                {
                  /* An output-less commit: at most sounds to start or stop
                     (their positional marks are all the text holds, so none
                     have fired yet and the sweep plays them all), and
                     possibly a silent score change for the status line
                     (a separate window, so no need to touch the pending
                     input request).  A <wait> with no visible output to
                     pace is dropped -- it must not stall the player's
                     pending line input. */
                  gsc_a5_show_media (gsc_a5_run);
                  gsc_a5_status (gsc_a5_run);
                  free (text);
                  break;
                }

              {
                event_t cancel;

                cancel.val1 = 0;
                glk_cancel_line_event (gsc_main_window, &cancel);
                /* Take the now-dangling "> " prompt back off the window --
                   the cancel already removed any typed text (echo is off in
                   real-time mode), so the prompt is the tail and the tick's
                   text reads as a clean continuation, with the one true
                   prompt reprinted below.  Best-effort: if the tail has
                   moved on, end the prompt's line instead, as before. */
#ifdef GSC_HAVE_UNPUT
                if (!gsc_unput_tail ("\n> "))
#endif
                  gsc_a5_put_string ("\n");
                gsc_a5_display (text);
                free (text);
                gsc_a5_show_media (gsc_a5_run);
                gsc_a5_status (gsc_a5_run);
                if (a5run_is_over (gsc_a5_run))
                  return FALSE;
                gsc_a5_put_prompt ("\n");
#ifdef SPATTERLIGHT
                /* The tick changed game state while we sat at the prompt;
                   refresh the autosave (it skips itself when the
                   autosave-on-timer preference is off). */
                gsc_autosave ();
#endif
                if (ubuf != NULL)
                  glk_request_line_event_uni (gsc_main_window, ubuf, ucap,
                                              cancel.val1);
                else
                  glk_request_line_event (gsc_main_window, buf,
                                          (glui32) (bufsize - 1), cancel.val1);
              }
            }
          break;

        case evtype_LineInput:
          if (event->win == gsc_main_window)
            {
#if defined(GLK_MODULE_GARGLK_FILE_RESOURCES) || defined(SPATTERLIGHT)
              /* A completed command line ends a turn (see gsc_event_wait_2). */
              gsc_graphic_drawn_since_input = FALSE;
#endif
#ifdef GSC_HAVE_TITLE_WINDOW
              /* The player's first input dismisses any title/cover window. */
              if (!gsc_seen_input)
                {
                  gsc_seen_input = TRUE;
                  gsc_close_title_graphic ();
                }
#endif
              return TRUE;
            }
          break;
        }
    }
}

static int
gsc_a5_read_line_raw (char *buf, int bufsize)
{
  event_t event;
  int n = 0, done;

  /* If an input log is being read back ("glk readlog on"), take the next
     line from it instead of the keyboard, echoing it in input style.  Log
     lines are the raw UTF-8 bytes "glk inputlog on" wrote, so they round-trip
     through the byte stream unchanged.  On end of file, close the stream and
     fall through to a normal line request. */
  if (gsc_readlog_stream)
    {
      glui32 chars;

      memset (buf, 0, (size_t) bufsize);
      chars = glk_get_line_stream (gsc_readlog_stream, buf, (glui32) bufsize);
      if (chars > 0)
        {
          while (chars > 0 && (buf[chars - 1] == '\n' || buf[chars - 1] == '\r'))
            chars--;
          buf[chars] = '\0';

          glk_set_style (style_Input);
          gsc_a5_put_string (buf);
          glk_set_style (style_Normal);
          gsc_a5_put_string ("\n");
          /* This path returns without ever reaching the gsc_colour_echo pair
             below, so the prompt's input colour has to be taken off here. */
          gsc_colour_echo (FALSE);
          return (int) chars;
        }

      glk_stream_close (gsc_readlog_stream, NULL);
      gsc_readlog_stream = NULL;
    }

  /* In real-time mode take over input echo: a TimeBased tick may cancel and
     re-issue the pending request, and with auto-echo every cancel would
     commit a spurious input line to the window.  The completed command is
     echoed manually below instead.

     Set it BOTH ways, every time.  Echo mode is per-window library state that
     a Spatterlight autosave archives and re-applies (TempWindow), so a
     one-sided "turn it off" leaves the restored process obeying the SAVED
     mode rather than this one: autosave with real-time on (echo off), then
     relaunch with real-time off -- determinism/testing mode, say -- and the
     library echo would stay off while the manual echo below is skipped,
     which loses every typed command from the transcript for the rest of the
     session. */
#ifdef GLK_MODULE_LINE_ECHO
  glk_set_echo_line_event (gsc_main_window, gsc_a5_real_time ? 0 : 1);
#endif

  /* The author's input colour for what the player types, whether the echo is
     Glk's or the manual one below. */
  gsc_colour_echo (TRUE);

  if (gsc_unicode_enabled)
    {
      const glui32 cap = 256;
      glui32 *unicode = (glui32 *) gsc_malloc (cap * sizeof (*unicode));
      glui32 i;

      memset (unicode, 0, cap * sizeof (*unicode));
      glk_request_line_event_uni (gsc_main_window, unicode, cap, 0);
      done = gsc_a5_await_line (&event, NULL, 0, unicode, cap);

      if (done)
        for (i = 0; i < event.val1; i++)
          {
            glui32 c = unicode[i];

            if (c < 0x80 && n < bufsize - 1)
              buf[n++] = (char) c;
            else if (c < 0x800 && n < bufsize - 2)
              {
                buf[n++] = (char) (0xc0 | (c >> 6));
                buf[n++] = (char) (0x80 | (c & 0x3f));
              }
            else if (c < 0x10000 && n < bufsize - 3)
              {
                buf[n++] = (char) (0xe0 | (c >> 12));
                buf[n++] = (char) (0x80 | ((c >> 6) & 0x3f));
                buf[n++] = (char) (0x80 | (c & 0x3f));
              }
            else if (n < bufsize - 4)
              {
                buf[n++] = (char) (0xf0 | (c >> 18));
                buf[n++] = (char) (0x80 | ((c >> 12) & 0x3f));
                buf[n++] = (char) (0x80 | ((c >> 6) & 0x3f));
                buf[n++] = (char) (0x80 | (c & 0x3f));
              }
          }
      free (unicode);
    }
  else
    {
      glk_request_line_event (gsc_main_window, buf, bufsize - 1, 0);
      done = gsc_a5_await_line (&event, buf, bufsize, NULL, 0);
      n = done ? (int) event.val1 : 0;
    }

  buf[n] = '\0';
  if (gsc_a5_real_time && done)
    {
      /* Echo the completed command, as Glk's auto-echo would have. */
      glk_set_style (style_Input);
      gsc_a5_put_string (buf);
      glk_set_style (style_Normal);
      gsc_a5_put_string ("\n");
    }
  gsc_colour_echo (FALSE);
  return n;
}

/*
 * gsc_a5_read_line()
 *
 * A line of input for one turn.  While a map-click walk is in progress it
 * comes from the walk rather than the keyboard: the runner submits each step
 * as an ordinary direction command and re-walks at the end of every turn
 * (clsUserSession:863), so a click on a distant room walks there a room per
 * turn.  A click arriving while we wait cancels the pending line request and
 * starts a walk instead.
 */
static int
gsc_a5_read_line (char *buf, int bufsize)
{
  for (;;)
    {
      if (gsc_a5_walk_to[0] != '\0')
        {
          if (gsc_a5_walk_next (gsc_a5_run, buf, bufsize))
            {
              /* Echo the step as though the player had typed it. */
              glk_set_style (style_Input);
              gsc_a5_put_string (buf);
              glk_set_style (style_Normal);
              gsc_a5_put_string ("\n");
              /* No keyboard read follows to take the prompt's input colour
                 back off before the turn's own text. */
              gsc_colour_echo (FALSE);
              return (int) strlen (buf);
            }
          gsc_a5_walk_stop ();          /* arrived, blocked, or no route */
        }

      gsc_a5_walk_clicked = FALSE;
      {
        int n = gsc_a5_read_line_raw (buf, bufsize);

        /* A click cancelled the request: loop round and walk instead. */
        if (gsc_a5_walk_clicked)
          continue;
        return n;
      }
    }
}

/*
 * gsc_a5_popup_input()
 *
 * Answer the %PopUpInput[prompt, default]% text function -- ADRIFT's naming
 * prompts, typically a System <RunImmediately> task that asks for the
 * player's name before the title (The After School Special: SetProperty
 * Player CharacterProperName %PopUpInput["Please enter your name",
 * "Anonymous"]%).  The Runner pops a modal VB InputBox seeded with the
 * author's default (Global.vb:2296); Glk has no dialog, so ask in the story
 * window instead -- print the prompt, take one line -- and read an empty
 * answer as the default, which is what OK on an unedited box returns.
 * Returns a heap-allocated answer the engine takes ownership of, or NULL to
 * fall back to the default (see a5text.h a5_popup_cb).
 */
static char *
gsc_a5_popup_input (void * /*ctx*/, const char *prompt, const char *dflt)
{
  char input[1024];
  int saved_real_time, n;

  /* No window to ask in, or a silent boot (the autorestore below replays the
     intro only to reach the saved state, whose name the player already
     chose): take the default without troubling anyone. */
  if (gsc_main_window == NULL || gsc_a5_popup_silent)
    return NULL;

  gsc_a5_put_string ("\n");
  if (prompt != NULL && prompt[0] != '\0')
    gsc_a5_put_string (prompt);
  if (dflt != NULL && dflt[0] != '\0')
    {
      /* The InputBox arrives with the default already filled in; say what
         answering with an empty line will give. */
      gsc_a5_put_string (" [");
      gsc_a5_put_string (dflt);
      gsc_a5_put_string ("]");
    }
  gsc_a5_put_prompt ("\n");

  /* This runs inside the engine (mid text-render), so a TimeBased tick must
     not re-enter it: hold real-time mode off for the duration, which also
     hands the echo of the typed answer back to the library. */
  saved_real_time = gsc_a5_real_time;
  gsc_a5_real_time = FALSE;
  for (;;)
    {
      n = gsc_a5_read_line_raw (input, sizeof input);

      /* A prompt like this one is exactly where the game's own QUIT and UNDO
         are out of reach -- whatever is typed becomes the answer -- so the
         "glk ..." forms are recognised here.  Handled directly rather than
         through gsc_a5_command_escape: an answer to the game's question is not
         a command, and does not belong in the input log.  Any action asked for
         waits for the turn loop (gsc_a5_meta_perform), which is where the run
         may be replaced; quitting needs no such care. */
      if (gsc_commands_enabled && gsc_command_escape (input))
        {
          if (gsc_meta_pending == GSC_META_QUIT)
            glk_exit ();
          gsc_a5_put_prompt ("\n");
          continue;
        }
      break;
    }
  gsc_a5_real_time = saved_real_time;

  return n > 0 ? gsc_copy_string (input) : NULL;
}

/*
 * gsc_a5_match_command()
 *
 * Case-insensitively test whether the player input (after trimming surrounding
 * whitespace) equals the given word.  Both sides are folded, so the word can be
 * a mixed-case one taken from the game (a %PopUpChoice% option) as well as one
 * of the port's own lower-case meta-commands.
 */
static int
gsc_a5_match_command (const char *input, const char *command)
{
  while (*input == ' ' || *input == '\t')
    input++;
  while (*input && *command)
    {
      if (glk_char_to_lower ((unsigned char) *input)
          != glk_char_to_lower ((unsigned char) *command))
        return FALSE;
      input++;
      command++;
    }
  while (*input == ' ' || *input == '\t')
    input++;
  return *input == '\0' && *command == '\0';
}

/*
 * gsc_a5_popup_choice()
 *
 * Answer the %PopUpChoice[prompt, choice1, choice2]% text function -- ADRIFT's
 * two-way prompts, in practice the gender question a game asks before play
 * (Beagle2's System <RunImmediately> Autorun, "Are you Male or Female?", whose
 * answer a SetGender task turns into the Player's Gender property).  The Runner
 * puts up a modal Yes/No MsgBox where Yes yields the first choice and No the
 * second (Global.vb:2278); Glk has no dialog, so ask in the story window.
 * Since the choices carry the meaning and the buttons don't, name both in the
 * prompt and accept either the choice itself or its yes/no button.  Returns
 * non-zero for the first choice, 0 for the second, or negative to leave the
 * token unevaluated (see a5text.h a5_popup_choice_cb).
 */
static int
gsc_a5_popup_choice (void * /*ctx*/, const char *prompt,
                     const char *choice1, const char *choice2)
{
  char input[1024];
  int saved_real_time, picked;

  /* No window to ask in, or a silent boot (the autorestore below replays the
     opening only to reach the saved state, whose answer the player already
     gave): leave the question unasked, as an unattended Runner does. */
  if (gsc_main_window == NULL || gsc_a5_popup_silent)
    return -1;

  /* This runs inside the engine (mid text-render), so a TimeBased tick must
     not re-enter it: hold real-time mode off for the duration, which also
     hands the echo of the typed answer back to the library. */
  saved_real_time = gsc_a5_real_time;
  gsc_a5_real_time = FALSE;

  for (;;)
    {
      int n;

      gsc_a5_put_string ("\n");
      if (prompt != NULL && prompt[0] != '\0')
        gsc_a5_put_string (prompt);
      gsc_a5_put_string (" [yes = ");
      gsc_a5_put_string (choice1);
      gsc_a5_put_string (" / no = ");
      gsc_a5_put_string (choice2);
      gsc_a5_put_string ("]\n> ");

      n = gsc_a5_read_line_raw (input, sizeof input);

      /* "glk ..." is answered here as it is at a naming prompt, and for the
         same reason; see gsc_a5_popup_input.  The loop re-asks the question. */
      if (gsc_commands_enabled && gsc_command_escape (input))
        {
          if (gsc_meta_pending == GSC_META_QUIT)
            glk_exit ();
          continue;
        }

      /* An empty line takes the dialog's default button, Yes -- what Return on
         an untouched MsgBox gives.  It also ends the loop at end of input, so
         a readlog replay that runs dry cannot spin here. */
      if (n <= 0
          || gsc_a5_match_command (input, "yes")
          || gsc_a5_match_command (input, "y")
          || gsc_a5_match_command (input, choice1))
        {
          picked = TRUE;
          break;
        }
      if (gsc_a5_match_command (input, "no")
          || gsc_a5_match_command (input, "n")
          || gsc_a5_match_command (input, choice2))
        {
          picked = FALSE;
          break;
        }

      /* A MsgBox has no third answer; ask again rather than invent one. */
      gsc_a5_put_string ("Please answer yes or no.\n");
    }

  gsc_a5_real_time = saved_real_time;
  return picked;
}

/*
 * gsc_a5_command_escape()
 *
 * Handle the Glk port meta-layer for one completed a5 input line: note a
 * standalone "help" (so the next prompt can hint at "glk help"), intercept
 * "glk ..." command escapes, and append game-bound lines to any active input
 * log.  Returns TRUE when the line was consumed as a Glk command, FALSE when
 * it should be handed to the game.
 */
static int
gsc_a5_command_escape (char *input)
{
  if (gsc_commands_enabled)
    {
      char *command;

      command = input + strspn (input, "\t ");

      /* As in os_read_line, a leading quote bypasses command interception;
         here only for "glk ..." lines, so ADRIFT 5 commands that legitimately
         start with a quote (say, quoted speech) reach the game unchanged. */
      if (command[0] == GSC_QUOTED_INPUT
          && scr_strncasecmp (command + 1, "glk", strlen ("glk")) == 0)
        memmove (command, command + 1, strlen (command));
      else
        {
          gsc_note_help_request (command);

          if (gsc_command_escape (input))
            {
              gsc_output_silence_help_hints ();
              return TRUE;
            }
        }
    }

  /* Log this line to any active input log.  Glk commands are never logged,
     matching os_read_line. */
  if (gsc_inputlog_stream)
    {
      glk_put_string_stream (gsc_inputlog_stream, input);
      glk_put_char_stream (gsc_inputlog_stream, '\n');
    }

  return FALSE;
}

/*
 * gsc_a5_save()
 *
 * Serialise the a5 runtime state (a5run_save -> the Adrift 5 runner <Game> XML), then
 * zlib-deflate it to the Glk-prompted save file.  The zlib framing (RFC 1950,
 * no header/obfuscation) matches the Adrift 5 runner's FileIO.SaveState; a5run_save
 * emits the Runner's UTF-8 BOM + <?xml?> declaration and, crucially, no trailing
 * newline after </Game> (a trailing byte makes the Runner's XmlReader scan into the
 * inflate zero-padding and reject the save as "illegal hex value 0x00").  The file
 * is thus interoperable with the ADRIFT 5 Runner.
 */
static void
gsc_a5_save (a5_run_t *run)
{
  frefid_t fileref;
  strid_t stream;
  char *blob;
  uint8_t *zblob;
  size_t length = 0;
  uint32_t zlen = 0;

  fileref = glk_fileref_create_by_prompt (fileusage_SavedGame | fileusage_BinaryMode,
                                          filemode_Write, 0);
  if (!fileref)
    {
      gsc_a5_put_string ("Save cancelled.\n");
      return;
    }

  blob = a5run_save (run, &length);
  if (!blob)
    {
      glk_fileref_destroy (fileref);
      gsc_a5_put_string ("Save failed.\n");
      return;
    }

  zblob = a5_deflate ((const uint8_t *) blob, (uint32_t) length, &zlen);
  free (blob);
  if (!zblob)
    {
      glk_fileref_destroy (fileref);
      gsc_a5_put_string ("Save failed.\n");
      return;
    }

  stream = glk_stream_open_file (fileref, filemode_Write, 0);
  glk_fileref_destroy (fileref);
  if (!stream)
    {
      free (zblob);
      gsc_a5_put_string ("Save failed.\n");
      return;
    }

  glk_put_buffer_stream (stream, (char *) zblob, zlen);
  glk_stream_close (stream, NULL);
  free (zblob);
  gsc_a5_put_string ("Game saved.\n");
}

/*
 * gsc_a5_restore()
 *
 * Read a Glk-prompted save file and apply it to the run.  Sniffs the framing: a
 * zlib stream (0x78 header -- ADRIFT 5 Runner, or Scarier's own
 * new saves) is inflated first; a raw '<' (a pre-interop uncompressed Scarier
 * <SaveState> file) is handed to a5run_restore as-is.  Returns TRUE on success.
 */
static int
gsc_a5_restore (a5_run_t *run)
{
  frefid_t fileref;
  strid_t stream;
  char *buffer;
  glui32 capacity, total;
  int ok;

  fileref = glk_fileref_create_by_prompt (fileusage_SavedGame | fileusage_BinaryMode,
                                          filemode_Read, 0);
  if (!fileref)
    return FALSE;

  stream = glk_stream_open_file (fileref, filemode_Read, 0);
  glk_fileref_destroy (fileref);
  if (!stream)
    return FALSE;

  capacity = 65536;
  buffer = (char *) gsc_malloc (capacity);
  total = 0;
  for (;;)
    {
      glui32 got = glk_get_buffer_stream (stream, buffer + total, capacity - total);
      total += got;
      if (total < capacity)
        break;
      capacity *= 2;
      buffer = (char *) gsc_realloc (buffer, capacity);
    }
  glk_stream_close (stream, NULL);

  /* zlib stream? (0x78 0x01 / 0x9C / 0xDA).  Inflate to XML before restoring. */
  if (total >= 2 && (unsigned char) buffer[0] == 0x78
      && ((unsigned char) buffer[1] == 0x01
          || (unsigned char) buffer[1] == 0x9c
          || (unsigned char) buffer[1] == 0xda))
    {
      uint32_t xlen = 0;
      uint8_t *xml = a5_inflate ((const uint8_t *) buffer, total, &xlen);
      free (buffer);
      if (!xml)
        return FALSE;
      ok = a5run_restore (run, (const char *) xml, xlen);
      free (xml);
      return ok;
    }

  ok = a5run_restore (run, buffer, total);
  free (buffer);
  return ok;
}

/*---------------------------------------------------------------------*/
/*  ADRIFT 5 graphics + sound                                          */
/*                                                                     */
/*  The game file is a Blorb; its Pict/Snd resources are addressed by  */
/*  the same numbers the engine reports through the media side channel */
/*  (a5run_media_*).  We register the Blorb as the Glk resource map so */
/*  glk_image_draw / glk_schannel_play work by resource number.        */
/*---------------------------------------------------------------------*/

static int gsc_a5_graphics_ok = FALSE;
static int gsc_a5_sound_ok = FALSE;

/* One Glk sound channel per ADRIFT audio channel.  The Runner has exactly 8
   (clsSound.vb: Channels(7), numbered 1..8 in the <audio> tag; anything out of
   that range is a no-op); slot 0 here is simply never used. */
enum { GSC_A5_MAX_CHANNELS = 9 };
static schanid_t gsc_a5_channels[GSC_A5_MAX_CHANNELS];
/* The resource last started on each channel, so a repeated play of the same
   sound leaves it alone rather than restarting it (see gsc_a5_show_media). */
static glui32 gsc_a5_chan_sound[GSC_A5_MAX_CHANNELS];

/*
 * gsc_a5_stop_all_sounds()
 *
 * Silence every active ADRIFT sound channel.  Used when a game restarts: a
 * previous playthrough may have started looping music/effects (play_ext with a
 * 0xffffffff repeat count), and the fresh run only ever (re)starts the channels
 * it explicitly plays, so without this the old loop keeps sounding on any
 * channel the new intro does not touch.
 */
static void
gsc_a5_stop_all_sounds (void)
{
  int ch;

  if (!gsc_a5_sound_ok)
    return;
  for (ch = 0; ch < GSC_A5_MAX_CHANNELS; ch++)
    {
      if (gsc_a5_channels[ch] != NULL)
        glk_schannel_stop (gsc_a5_channels[ch]);
      gsc_a5_chan_sound[ch] = 0;
    }
}

/* Declared in glkstart.h, which is included further down (in the UNIX linkage
 * section); forward-declared here so the resource setup above it can open the
 * game file as a Glk stream for giblorb. */
extern "C" strid_t glkunix_stream_open_pathname (char *pathname,
                                                 glui32 textmode, glui32 rock);

/*
 * gsc_a5_init_resources()
 *
 * Probe for graphics/sound support and register the game Blorb as the Glk
 * resource map, so image/sound resources can be addressed by Blorb number.
 */
static void
gsc_a5_init_resources (void)
{
  strid_t stream;

  gsc_a5_graphics_ok = glk_gestalt (gestalt_Graphics, 0) != 0;
  gsc_a5_sound_ok = glk_gestalt (gestalt_Sound, 0) != 0;
  if ((!gsc_a5_graphics_ok && !gsc_a5_sound_ok) || gsc_game_path[0] == '\0')
    return;

  stream = glkunix_stream_open_pathname (gsc_game_path, FALSE, 0);
  if (stream != NULL && giblorb_set_resource_map (stream) != giblorb_err_None)
    {
      /* Not a Blorb (e.g. a raw .taf with no resources): no media. */
      glk_stream_close (stream, NULL);
      gsc_a5_graphics_ok = gsc_a5_sound_ok = FALSE;
    }
}

#ifdef SPATTERLIGHT
/*---------------------------------------------------------------------*/
/*  Spatterlight autosave/autorestore support                          */
/*                                                                     */
/*  The file/plist plumbing lives in scarier-autosave.mm (app target   */
/*  only); this section owns everything engine-side: the state         */
/*  containers written into autosave.glksave, the stash/recover of the */
/*  Glk object globals above, and the per-prompt gsc_autosave().       */
/*---------------------------------------------------------------------*/

/* autosave.glksave is a small line-framed container: a magic line naming the
   engine, then length-prefixed chunks.  Both engines carry their own state
   serialization plus the undo history, so UNDO still works across an
   autorestore (as Bocfel carries its save stacks in its autosave). */
static const char *const GSC_SC_CONTAINER_MAGIC = "SCARAUTO4\n";
static const char *const GSC_A5_CONTAINER_MAGIC = "SCARAUTO5\n";

static void
gsc_container_put_chunk (std::string &out, const char *data, size_t length)
{
  out += std::to_string (length);
  out += '\n';
  out.append (data, length);
}

/* Read "<decimal>\n<bytes>" at *pos; false on malformed framing. */
static bool
gsc_container_get_chunk (const std::string &data, size_t *pos,
                         std::string *chunk)
{
  size_t eol = data.find ('\n', *pos);
  if (eol == std::string::npos)
    return false;
  unsigned long length = strtoul (data.c_str () + *pos, NULL, 10);
  *pos = eol + 1;
  if (length > data.size () - *pos)
    return false;
  chunk->assign (data, *pos, length);
  *pos += length;
  return true;
}

static bool
gsc_container_get_count (const std::string &data, size_t *pos, long *count)
{
  size_t eol = data.find ('\n', *pos);
  if (eol == std::string::npos)
    return false;
  *count = strtol (data.c_str () + *pos, NULL, 10);
  *pos = eol + 1;
  return *count >= 0;
}

/* Serialization callbacks bridging the engines' byte streams to strings. */
static void
gsc_state_append (void *opaque, const scr_byte *buffer, scr_int length)
{
  ((std::string *) opaque)->append ((const char *) buffer, (size_t) length);
}

struct GscStateCursor
{
  const std::string *data;
  size_t pos;
};

static scr_int
gsc_state_read (void *opaque, scr_byte *buffer, scr_int length)
{
  GscStateCursor *cursor = (GscStateCursor *) opaque;
  size_t left = cursor->data->size () - cursor->pos;
  size_t bytes = (size_t) length < left ? (size_t) length : left;

  memcpy (buffer, cursor->data->data () + cursor->pos, bytes);
  cursor->pos += bytes;
  return (scr_int) bytes;
}

/*
 * gsc_sc_serialize_all()
 * gsc_sc_apply_all()
 *
 * The ADRIFT <=4 container: the engine's TAS-format state, the memo undo
 * ring (oldest first), and the one-turn-back undo buffer when available.
 */
static std::string
gsc_sc_serialize_all (void)
{
  const scr_gameref_t game = (scr_gameref_t) gsc_game;
  const scr_memo_setref_t memento = gs_get_memento (game);
  std::string engine_state, undo_game;
  scr_int ring_count, index_;

  scr_save_game_to_callback (gsc_game, gsc_state_append, &engine_state);
  if (engine_state.empty ())
    return std::string ();

  std::string out = GSC_SC_CONTAINER_MAGIC;
  gsc_container_put_chunk (out, engine_state.data (), engine_state.size ());

  ring_count = memo_get_undo_count (memento);
  out += std::to_string ((long) ring_count);
  out += '\n';
  for (index_ = 0; index_ < ring_count; index_++)
    {
      scr_int length = 0;
      const scr_byte *blob = memo_get_undo (memento, index_, &length);

      gsc_container_put_chunk (out, (const char *) blob, (size_t) length);
    }

  if (scr_save_undo_game_to_callback (gsc_game, gsc_state_append, &undo_game)
      && !undo_game.empty ())
    {
      out += "1\n";
      gsc_container_put_chunk (out, undo_game.data (), undo_game.size ());
    }
  else
    out += "0\n";
  return out;
}

static bool
gsc_sc_apply_all (const std::string &data)
{
  const scr_gameref_t game = (scr_gameref_t) gsc_game;
  const std::string magic = GSC_SC_CONTAINER_MAGIC;
  std::string chunk;
  long ring_count, has_undo_game, index_;

  if (data.compare (0, magic.size (), magic) != 0)
    return false;
  size_t pos = magic.size ();

  if (!gsc_container_get_chunk (data, &pos, &chunk))
    return false;
  {
    GscStateCursor cursor = { &chunk, 0 };
    if (!scr_load_game_from_callback (gsc_game, gsc_state_read, &cursor))
      return false;
  }

  /* A malformed or unreadable undo history just means no UNDO past the
     restore point; the restored game state above stays good. */
  if (!gsc_container_get_count (data, &pos, &ring_count))
    return true;
  for (index_ = 0; index_ < ring_count; index_++)
    {
      if (!gsc_container_get_chunk (data, &pos, &chunk))
        return true;
      memo_append_undo (gs_get_memento (game),
                        (const scr_byte *) chunk.data (),
                        (scr_int) chunk.size ());
    }
  if (!gsc_container_get_count (data, &pos, &has_undo_game))
    return true;
  if (has_undo_game && gsc_container_get_chunk (data, &pos, &chunk))
    {
      GscStateCursor cursor = { &chunk, 0 };
      scr_load_undo_game_from_callback (gsc_game, gsc_state_read, &cursor);
    }
  return true;
}

/*
 * gsc_a5_serialize_all()
 * gsc_a5_apply_all()
 *
 * The ADRIFT 5 container: the engine's save XML (a5run_save, RNG state
 * included), the last turn's composed output, and the undo snapshot stack
 * (oldest first) with its parallel turn texts.
 */
static std::string
gsc_a5_serialize_all (void)
{
  size_t length = 0;
  char *blob = a5run_save (gsc_a5_run, &length);
  const char *text;
  size_t text_length;
  int depth, i;

  if (blob == NULL)
    return std::string ();

  std::string out = GSC_A5_CONTAINER_MAGIC;
  gsc_container_put_chunk (out, blob, length);
  free (blob);

  a5run_get_turn_text (gsc_a5_run, &text, &text_length);
  gsc_container_put_chunk (out, text, text_length);

  depth = a5run_undo_depth (gsc_a5_run);
  out += std::to_string ((long) depth);
  out += '\n';
  for (i = 0; i < depth; i++)
    {
      const char *turn_text;
      size_t blob_length = 0, turn_length = 0;
      const char *undo_blob = a5run_undo_peek (gsc_a5_run, i, &blob_length,
                                               &turn_text, &turn_length);

      gsc_container_put_chunk (out, undo_blob, blob_length);
      gsc_container_put_chunk (out, turn_text, turn_length);
    }
  return out;
}

static bool
gsc_a5_apply_all (const std::string &data)
{
  const std::string magic = GSC_A5_CONTAINER_MAGIC;
  std::string chunk, turn_chunk;
  long depth, i;

  if (data.compare (0, magic.size (), magic) != 0)
    return false;
  size_t pos = magic.size ();

  if (!gsc_container_get_chunk (data, &pos, &chunk))
    return false;
  if (!a5run_restore (gsc_a5_run, chunk.data (), chunk.size ()))
    return false;

  /* As on the scare side, a truncated undo tail is not fatal. */
  if (!gsc_container_get_chunk (data, &pos, &chunk))
    return true;
  a5run_set_turn_text (gsc_a5_run, chunk.data (), chunk.size ());

  if (!gsc_container_get_count (data, &pos, &depth))
    return true;
  for (i = 0; i < depth; i++)
    {
      if (!gsc_container_get_chunk (data, &pos, &chunk)
          || !gsc_container_get_chunk (data, &pos, &turn_chunk))
        return true;
      a5run_undo_push_blob (gsc_a5_run, chunk.data (), chunk.size (),
                            turn_chunk.data (), turn_chunk.size ());
    }
  return true;
}

/*
 * gsc_stash_frontend_state()
 * gsc_recover_frontend_state()
 *
 * Capture the Glk object globals as serialization tags for the library
 * plist, and re-point them at the restored objects after the library state
 * has been rebuilt (called from scarier-autosave.mm between the library's
 * main and "late" restore passes).
 */
void
gsc_stash_frontend_state (ScarierGlkFrontendState *st)
{
  int ch;

  st->mainwintag = gsc_main_window ? gsc_main_window->tag : 0;
  st->statuswintag = gsc_status_window ? gsc_status_window->tag : 0;
  st->sidewintag = gsc_a5_side_window ? gsc_a5_side_window->tag : 0;
  st->mapwintag = gsc_map_window ? gsc_map_window->tag : 0;
  st->gfxwintag = gsc_graphics_window ? gsc_graphics_window->tag : 0;
  st->transcripttag = gsc_transcript_stream ? gsc_transcript_stream->tag : 0;
  st->inputlogtag = gsc_inputlog_stream ? gsc_inputlog_stream->tag : 0;
  st->readlogtag = gsc_readlog_stream ? gsc_readlog_stream->tag : 0;
  st->soundchanneltag = sound_channel ? sound_channel->tag : 0;
  for (ch = 0; ch < GSC_A5_MAX_CHANNELS; ch++)
    {
      st->a5_channeltags[ch] = gsc_a5_channels[ch]
                               ? gsc_a5_channels[ch]->tag : 0;
      st->a5_chan_sound[ch] = gsc_a5_chan_sound[ch];
    }
  st->seen_input = gsc_seen_input;
  st->title_image = gsc_title_image;
  st->title_offset = gsc_title_offset;
  st->title_length = gsc_title_length;
  st->map_shown = gsc_map_shown;
  st->map_at_top = gsc_map_at_top;
  st->map_zoom = gsc_map_zoom;
  st->colour_on = gsc_colour_enabled;

  /* The exact RNG state (which generator is active plus the xoshiro words),
     so a deterministic session's randomness continues where it left off. */
  {
    int usenative = 1;
    glui32 *words = NULL;
    int count = 0;

    erkyrath_random_get_detstate (&usenative, &words, &count);
    st->rng_usenative = usenative;
    for (ch = 0; ch < 4; ch++)
      st->rng_state[ch] = (count == 4 && words) ? words[ch] : 0;
  }
}

void
gsc_recover_frontend_state (const ScarierGlkFrontendState *st)
{
  int ch;

  gsc_main_window = gli_window_for_tag (st->mainwintag);
  gsc_status_window = gli_window_for_tag (st->statuswintag);
  gsc_a5_side_window = gli_window_for_tag (st->sidewintag);
  gsc_map_window = gli_window_for_tag (st->mapwintag);
  gsc_graphics_window = gli_window_for_tag (st->gfxwintag);
  gsc_transcript_stream = gli_stream_for_tag (st->transcripttag);
  gsc_inputlog_stream = gli_stream_for_tag (st->inputlogtag);
  gsc_readlog_stream = gli_stream_for_tag (st->readlogtag);
  sound_channel = gli_schan_for_tag (st->soundchanneltag);
  for (ch = 0; ch < GSC_A5_MAX_CHANNELS; ch++)
    {
      gsc_a5_channels[ch] = gli_schan_for_tag (st->a5_channeltags[ch]);
      gsc_a5_chan_sound[ch] = st->a5_chan_sound[ch];
    }
  gsc_seen_input = st->seen_input;
  gsc_title_image = (glui32) st->title_image;
  gsc_title_offset = (scr_int) st->title_offset;
  gsc_title_length = (scr_int) st->title_length;
  /* The app-side image cache does not survive a relaunch; re-load the title
     image chunk so a post-restore resize can still redraw the cover. */
  if (gsc_graphics_window != NULL && gsc_title_image != 0
      && gsc_title_length > 0 && gli_game_path != NULL
      && !win_findimage ((int) gsc_title_image))
    win_loadimage ((int) gsc_title_image, gli_game_path,
                   (int) gsc_title_offset, (int) gsc_title_length);
  gsc_map_shown = st->map_shown;
  gsc_map_at_top = st->map_at_top;
  gsc_map_zoom = st->map_zoom;
  /* The restored streams still carry the zcolors colour mode set on them, and
     the restored windows their black background, so taking the flag back is
     all it takes to pick the mode up where it left off.  (An autosave written
     before this field existed decodes as 0, which is what those sessions
     restored as anyway.) */
  gsc_colour_enabled = st->colour_on;
  /* A map that was wanted but still waiting for somewhere to draw was archived
     closed, and looks exactly like one the player shut; what the map would do
     from a standing start -- the stored preference, or failing that the
     layout the game shipped -- is what tells the two apart, so the wait
     survives the autosave. */
  {
    int pref = gsc_map_pref_read (NULL);

    gsc_map_want = gsc_map_shown || pref == 1
                   || (pref < 0 && gsc_map_default_shown ());
  }
  /* The restored map window holds the app's snapshot pixels, not ours:
     force the next redraw to flush the whole surface. */
  gsc_map_screen_drop ();

  /* Restore the RNG to its saved position.  The silent autorestore boot
     re-seeded and may have drawn from it; this puts it back exactly where
     the autosave left it.  In native (non-deterministic) mode the flag
     keeps the native generator selected and the words are inert. */
  if (st->rng_usenative >= 0)
    {
      glui32 words[4] = { st->rng_state[0], st->rng_state[1],
                          st->rng_state[2], st->rng_state[3] };
      erkyrath_random_set_detstate (st->rng_usenative, words, 4);
    }
  /* Not stashed: gsc_map_taken, assist flags and locale are re-derived at
     startup; the walk-in-progress state is deliberately dropped (a restored
     session simply stops walking); gsc_map itself is authored data (ADRIFT
     5, reloaded at boot) or rebuilt every redraw (ADRIFT 4); and the restart
     count gsc_map_notice_restart watches is per-process on both sides of the
     comparison, so a fresh process starts it and the engine's at zero
     together. */
}

/* The on-disk game path, for the autosave directory's file-signature hash. */
const char *
gsc_autosave_game_path (void)
{
  return gsc_game_path;
}

/*
 * gsc_autosave()
 *
 * Save the whole game state (engine container + Glk library plist), then
 * ask the window server to snapshot the GUI under the same tag.  Called at
 * every top-level command prompt, after the prompt is printed but before
 * line input is requested, and again after a real-time tick that changed
 * state and reprinted the prompt.  Skips prompts that would not restore
 * coherently: engine-level ones (a pending disambiguation), the pre-intro
 * name/gender prompts, and the debugger's.
 */
static void
gsc_autosave (void)
{
  std::string state;

  if (gsc_in_debug_read || !scarier_autosave_wanted ())
    return;
  if (gsc_is_a5)
    {
      if (gsc_a5_run == NULL || a5run_is_over (gsc_a5_run)
          || a5run_input_pending (gsc_a5_run))
        return;
      state = gsc_a5_serialize_all ();
    }
  else
    {
      const scr_gameref_t game = (scr_gameref_t) gsc_game;

      if (gsc_game == NULL || !scr_is_game_running (gsc_game))
        return;
      /* Not seen the player's room yet = still inside the startup block
         (the "Please enter your name" prompt): resuming there would replay
         the intro over the restored transcript. */
      if (!gs_room_seen (game, gs_playerroom (game)))
        return;
      state = gsc_sc_serialize_all ();
    }
  if (!state.empty ())
    scarier_autosave_write (state);
}

#endif /* SPATTERLIGHT */


/*
 * gsc_a5_draw_image()
 *
 * Draw Blorb Pict resource `number` centred on its own line in window `win`.
 * Writes straight to `win`'s stream (not the current window), so an image
 * inside a <window> span and its surrounding blank lines both land in the
 * same window regardless of which window is currently selected for output.
 * Returns TRUE if an image was actually drawn.
 */
static int
gsc_a5_draw_image (winid_t win, glui32 number)
{
  glui32 width = 0, height = 0;
  strid_t str;

  if (!gsc_a5_graphics_ok || number == 0 || win == NULL)
    return FALSE;
  if (!glk_image_get_info (number, &width, &height))
    return FALSE;
  str = glk_window_get_stream (win);
  glk_window_flow_break (win);
  glk_put_char_stream (str, '\n');
  glk_image_draw (win, number, imagealign_InlineCenter, 0);
  glk_put_char_stream (str, '\n');
  return TRUE;
}

/*
 * gsc_a5_span_style()
 *
 * Pick the Glk style for a text span given its alignment and weight/oblique
 * nesting depths.  Glk styles don't combine, so alignment wins over character
 * styles: centered+bold keeps User2 (weight-hinted); right uses Note with no
 * bold/italic combo.  Unaligned bold+italic (or bold+underline) maps to Alert;
 * italic or underline alone to Emphasized; bold alone to Subheader (as the
 * ADRIFT 4 path does in gsc_set_glk_style).  Underline marks are distinct for
 * a future CSS path; for now they share italic's Glk styles.
 *
 * `input_colour` says the span's ink is the input colour (a <c> span, or the
 * game title, which the Runner Displays as one).  That ink only exists in
 * colour mode, so with colours off such a span falls back to Emphasized --
 * the same stand-in gsc_set_glk_style() gives the ADRIFT 4 path -- rather
 * than coming out indistinguishable from body text.  Alignment and bold keep
 * their precedence over the stand-in, as they do there.
 */
static glui32
gsc_a5_span_style (int center_depth, int right_depth,
                   int bold_depth, int italic_depth, int underline_depth,
                   int input_colour)
{
  const int oblique = italic_depth > 0 || underline_depth > 0;

  if (right_depth > 0)
    return style_Note;
  if (center_depth > 0)
    return bold_depth > 0 ? style_User2 : style_User1;
  if (bold_depth > 0 && oblique)
    return style_Alert;
  if (oblique)
    return style_Emphasized;
  if (bold_depth > 0)
    return style_Subheader;
  if (input_colour && !gsc_colour_visible ())
    return style_Emphasized;
  return style_Normal;
}

/*
 * gsc_a5_open_side_window()
 *
 * Return the author-defined secondary output window, opening it lazily the
 * first time the game routes text to one (ADRIFT 5 <window NAME>).  It splits
 * the main story window, taking ~40% of the width on the right, as a text
 * buffer -- Alien Diver's "Status" pane (ship-repair progress and command
 * reference).  Once open it is kept for the rest of the session, like the
 * official Runner's additional windows.  Returns NULL if Glk cannot split
 * (the caller then leaves output in the main window).
 */
static winid_t
gsc_a5_open_side_window (void)
{
  if (gsc_a5_side_window == NULL)
    {
      gsc_a5_side_window = glk_window_open (gsc_main_window,
                                            winmethod_Right
                                              | winmethod_Proportional,
                                            40, wintype_TextBuffer, 0);
#ifdef GSC_HAVE_ZCOLORS
      /* A window opened in colour mode starts in the theme's background: it
         takes the game's only from a clear made with the colours in force,
         and there is nothing in it yet to lose to one. */
      if (gsc_colour_enabled && gsc_a5_side_window)
        {
          gsc_colour_apply (gsc_a5_side_window, GSC_COLOUR_NONE);
          glk_window_clear (gsc_a5_side_window);
        }
#endif
    }
  return gsc_a5_side_window;
}

/*
 * gsc_a5_colour_top()
 *
 * The colour in force given a stack of nested colour spans: the innermost one
 * that names a colour.  A span that names none -- <font size=20> with no
 * colour attribute -- is transparent to colour, so the enclosing <font
 * colour="red"> still applies inside it, as it does in the Runner.
 */
static glui32
gsc_a5_colour_top (const glui32 *stack, int depth)
{
  while (depth > 0)
    {
      if (stack[depth - 1] != GSC_COLOUR_NONE)
        return stack[depth - 1];
      depth--;
    }
  return GSC_COLOUR_NONE;
}


/*
 * gsc_a5_colour_top_is_input()
 *
 * TRUE when the colour in force is the input colour -- i.e. the span
 * gsc_a5_colour_top() picked out is a <c> rather than a <font colour>.  Skips
 * colourless <font> spans the same way, so both answers describe one span.
 */
static int
gsc_a5_colour_top_is_input (const glui32 *stack, const int *is_input,
                            int depth)
{
  while (depth > 0)
    {
      if (stack[depth - 1] != GSC_COLOUR_NONE)
        return is_input[depth - 1];
      depth--;
    }
  return FALSE;
}


/*
 * gsc_a5_display()
 *
 * Present one turn's text.  The engine runs in interactive mode (see
 * a5text.h), so the text carries presentation marks: draw each embedded
 * image at its marked position, pause for a keypress at each <waitkey>
 * mark and for a timed delay at each <wait N> mark, clear the story window
 * at each <cls> mark, show <center> spans in the centered style_User1
 * (hinted for centered justification before the window opened), <right>
 * spans in RightFlush style_Note, <b>/<i> spans in bold/italic, and -- in
 * colour mode -- <font colour> and <c> spans in their colours.  This is the
 * same presentation the official Runner's output pane gives, e.g., Anno
 * 1700's intro: credits and cover image, "Press any key", then a cleared
 * screen for the opening narrative.

 */
static void
gsc_a5_display (const char *text)
{
  const char *p = text, *seg = text;
  int center_depth = 0, right_depth = 0, bold_depth = 0, italic_depth = 0,
      underline_depth = 0;
  glui32 colour_stack[GSC_MAX_STYLE_NESTING];
  int colour_is_input[GSC_MAX_STYLE_NESTING];
  int colour_depth = 0;

  /* The window a run of text is currently going to: the main story window, or
     an author-defined side window between an A5_WINDOW_MARK span and its
     A5_ENDWINDOW_MARK.  A <cls> inside the span clears that side window. */
  winid_t cur_window = gsc_main_window;

  if (text == NULL)
    return;

  glk_set_window (gsc_main_window);
  gsc_colour_apply (gsc_main_window, GSC_COLOUR_NONE);

  while (TRUE)
    {
      if (*p != '\0' && *p != A5_CLS_MARK && *p != A5_WAITKEY_MARK
          && *p != A5_IMG_MARK && *p != A5_CENTER_MARK
          && *p != A5_ENDCENTER_MARK && *p != A5_BOLD_MARK
          && *p != A5_ENDBOLD_MARK && *p != A5_ITALIC_MARK
          && *p != A5_ENDITALIC_MARK && *p != A5_UNDERLINE_MARK
          && *p != A5_ENDUNDERLINE_MARK && *p != A5_RIGHT_MARK
          && *p != A5_ENDRIGHT_MARK && *p != A5_WINDOW_MARK
          && *p != A5_ENDWINDOW_MARK && *p != A5_SOUND_MARK
          && *p != A5_COMMIT_MARK && *p != A5_WAIT_MARK
          && *p != A5_COLOUR_MARK && *p != A5_ENDCOLOUR_MARK)
        {
          p++;
          continue;
        }

      /* Flush the plain text run before this mark (or before the end). */
      if (p > seg)
        {
          size_t n = (size_t) (p - seg);
          char *chunk = (char *) gsc_malloc (n + 1);
          memcpy (chunk, seg, n);
          chunk[n] = '\0';
          gsc_a5_put_string (chunk);
          free (chunk);
        }
      if (*p == '\0')
        break;

      if (*p == A5_CLS_MARK)
        {
          glk_window_clear (cur_window);
          if (cur_window == gsc_main_window)
            gsc_main_window_empty = TRUE;
        }
      else if (*p == A5_WINDOW_MARK)
        {
          /* Side-window span opens: \022<name>\022.  Route text to the side
             window (opened lazily, right split); fall back to the main window
             if Glk cannot split.  The name delimits the span but is unused --
             this build routes every <window> to the one side window. */
          const char *e = strchr (p + 1, A5_WINDOW_MARK);
          winid_t w = gsc_a5_open_side_window ();

          cur_window = w != NULL ? w : gsc_main_window;
          glk_set_window (cur_window);
          gsc_colour_apply (cur_window,
                            gsc_a5_colour_top (colour_stack, colour_depth));
          if (e != NULL)
            p = e;
        }
      else if (*p == A5_ENDWINDOW_MARK)
        {
          /* Side-window span closes: text returns to the main story window. */
          cur_window = gsc_main_window;
          glk_set_window (cur_window);
          gsc_colour_apply (cur_window,
                            gsc_a5_colour_top (colour_stack, colour_depth));
        }
      else if (*p == A5_WAITKEY_MARK)
        {
          event_t event;

          /* Bring the status line up to date before pausing. */
          if (gsc_a5_run)
            gsc_a5_status (gsc_a5_run);
          glk_request_char_event (gsc_main_window);
          gsc_event_wait (evtype_CharInput, &event);
          /* gsc_a5_status leaves the main window selected; restore the span's
             routing window so text after the pause stays in it. */
          glk_set_window (cur_window);
        }
      else if (*p == A5_COMMIT_MARK)
        {
          /* Display-commit boundary with a dangling span (a5text.h): each
             commit is its own Source2HTML parse in the Runner, so a <center>,
             <right>, <b>, or <i> left open there ends now -- Death Shack's
             Introduction never closes its <center>, and the first room
             description (the next commit) must come out left-aligned, not
             centered. */
          center_depth = right_depth = bold_depth = italic_depth
            = underline_depth = 0;
          colour_depth = 0;
          glk_set_style (gsc_a5_span_style (0, 0, 0, 0, 0, FALSE));
          gsc_colour_apply (cur_window, GSC_COLOUR_NONE);
        }
      else if (*p == A5_COLOUR_MARK)
        {
          /* Colour span opens: \027<value>\027, the value being a colour name
             or hex triplet, the reserved token "input" for <c> (which the
             Runner draws in its input colour), or empty for a <font> tag with
             no colour of its own. */
          const char *e = strchr (p + 1, A5_COLOUR_MARK);

          if (e != NULL)
            {
              char token[32];
              size_t n = (size_t) (e - (p + 1));

              if (n >= sizeof token)
                n = sizeof token - 1;
              memcpy (token, p + 1, n);
              token[n] = '\0';

              if (colour_depth < GSC_MAX_STYLE_NESTING)
                {
                  const int is_input = strcmp (token, "input") == 0;

                  colour_is_input[colour_depth] = is_input;
                  colour_stack[colour_depth++]
                    = is_input ? gsc_colour_input
                               : gsc_colour_lookup (token);
                  gsc_colour_apply (cur_window,
                                    gsc_a5_colour_top (colour_stack,
                                                       colour_depth));
                  /* Colour spans carry a style too, for the colourless case
                     (see gsc_a5_span_style); with colours on this re-asserts
                     the style already in force. */
                  glk_set_style (gsc_a5_span_style (center_depth, right_depth,
                                                    bold_depth, italic_depth,
                                                    underline_depth,
                                                    gsc_a5_colour_top_is_input
                                                      (colour_stack,
                                                       colour_is_input,
                                                       colour_depth)));
                }
              p = e;
            }
        }
      else if (*p == A5_ENDCOLOUR_MARK)
        {
          if (colour_depth > 0)
            colour_depth--;
          gsc_colour_apply (cur_window,
                            gsc_a5_colour_top (colour_stack, colour_depth));
          glk_set_style (gsc_a5_span_style (center_depth, right_depth,
                                            bold_depth, italic_depth,
                                            underline_depth,
                                            gsc_a5_colour_top_is_input
                                              (colour_stack, colour_is_input,
                                               colour_depth)));
        }
      else if (*p == A5_CENTER_MARK || *p == A5_ENDCENTER_MARK
               || *p == A5_RIGHT_MARK || *p == A5_ENDRIGHT_MARK
               || *p == A5_BOLD_MARK || *p == A5_ENDBOLD_MARK
               || *p == A5_ITALIC_MARK || *p == A5_ENDITALIC_MARK
               || *p == A5_UNDERLINE_MARK || *p == A5_ENDUNDERLINE_MARK)
        {
          /* Alignment or character-style span boundary: like the Runner, only
             the style changes -- the game's own line breaks around an aligned
             span delimit the paragraph.  A <b> inside a <center> gets the
             weight-hinted centered style (User2); elsewhere bold is Subheader
             and italic/underline are Emphasized (Alert when combined with
             bold).  Right alignment uses style_Note and wins over character
             styles. */
          if (*p == A5_CENTER_MARK)
            center_depth++;
          else if (*p == A5_ENDCENTER_MARK && center_depth > 0)
            center_depth--;
          else if (*p == A5_RIGHT_MARK)
            right_depth++;
          else if (*p == A5_ENDRIGHT_MARK && right_depth > 0)
            right_depth--;
          else if (*p == A5_BOLD_MARK)
            bold_depth++;
          else if (*p == A5_ENDBOLD_MARK && bold_depth > 0)
            bold_depth--;
          else if (*p == A5_ITALIC_MARK)
            italic_depth++;
          else if (*p == A5_ENDITALIC_MARK && italic_depth > 0)
            italic_depth--;
          else if (*p == A5_UNDERLINE_MARK)
            underline_depth++;
          else if (*p == A5_ENDUNDERLINE_MARK && underline_depth > 0)
            underline_depth--;
          glk_set_style (gsc_a5_span_style (center_depth, right_depth,
                                            bold_depth, italic_depth,
                                            underline_depth,
                                            gsc_a5_colour_top_is_input
                                              (colour_stack, colour_is_input,
                                               colour_depth)));
        }
      else if (*p == A5_WAIT_MARK)
        {
          /* Timed-pause slot: \026<seconds>\026.  Run the same cancelable
             timer delay the ADRIFT 4 path gives its <wait x.x> tag, so
             Death Shack's letter-at-a-time title cards play out in real
             time (an empty or unparsable argument pauses not at all). */
          const char *e = strchr (p + 1, A5_WAIT_MARK);
          if (e != NULL)
            {
              char arg[16];
              size_t n = (size_t) (e - (p + 1));
              if (n >= sizeof arg)
                n = sizeof arg - 1;
              memcpy (arg, p + 1, n);
              arg[n] = '\0';
              gsc_handle_wait_tag (arg);
              p = e;
            }
        }
      else if (*p == A5_SOUND_MARK)
        {
          /* Sound slot: \024<media event index>\024.  Fire it here, at the
             tag's place in the text -- before any later <waitkey>, so e.g. a
             sting cued ahead of a keypress-paced cutscene starts immediately
             (Pervert Action Crisis' maid encounter). */
          const char *e = strchr (p + 1, A5_SOUND_MARK);
          if (e != NULL)
            {
              if (gsc_a5_run != NULL)
                gsc_a5_media_fire (gsc_a5_run, (int) atol (p + 1));
              p = e;
            }
        }
      else
        {
          /* Image slot: \006<Blorb resource number>\006. */
          const char *e = strchr (p + 1, A5_IMG_MARK);
          if (e != NULL)
            {
              gsc_a5_draw_image (cur_window, (glui32) atol (p + 1));
              p = e;
            }
        }
      seg = ++p;
    }

  /* A dangling span must not bleed into prompts and later turns. */
  if (center_depth > 0 || right_depth > 0
      || bold_depth > 0 || italic_depth > 0 || underline_depth > 0)
    glk_set_style (style_Normal);
  /* Nor a dangling colour span: the prompt and the player's input follow. */
  if (colour_depth > 0)
    gsc_colour_apply (cur_window, GSC_COLOUR_NONE);
  /* Likewise a dangling <window> span: prompts and input echo belong in the
     main story window. */
  if (cur_window != gsc_main_window)
    glk_set_window (gsc_main_window);
}

/*
 * gsc_a5_sound_event()
 *
 * Execute one collected sound event: start, stop or pause its channel.
 */
static void
gsc_a5_sound_event (const a5_media_event_t *m)
{
  int ch = m->channel;
  int trace = getenv ("A5_TRACE_MEDIA") != NULL;

  if (!gsc_a5_sound_ok)
    {
      if (trace)
        fprintf (stderr, "[a5 media] kind=%d ch=%d (no sound support)\n",
                 m->kind, ch);
      return;
    }
  /* The Runner has channels 1..8; a channel outside that range makes
     the whole tag a no-op there (clsSound.vb), so ignore it here too. */
  if (ch < 1 || ch >= GSC_A5_MAX_CHANNELS)
    {
      if (trace)
        fprintf (stderr, "[a5 media] ignore kind=%d ch=%d (range)\n",
                 m->kind, ch);
      return;
    }
  if (gsc_a5_channels[ch] == NULL)
    gsc_a5_channels[ch] = glk_schannel_create ((glui32) ch);
  if (gsc_a5_channels[ch] == NULL)
    return;
  if (m->kind == A5_MEDIA_SOUND_STOP)
    {
      if (trace)
        fprintf (stderr, "[a5 media] stop ch=%d\n", ch);
      glk_schannel_stop (gsc_a5_channels[ch]);
      gsc_a5_chan_sound[ch] = 0;
    }
  else if (m->kind == A5_MEDIA_SOUND_PAUSE)
    {
      if (trace)
        fprintf (stderr, "[a5 media] pause ch=%d\n", ch);
      glk_schannel_pause (gsc_a5_channels[ch]);
    }
  else if (m->number > 0)
    {
      /* Playing the sound a channel is already playing leaves it
         alone in the Runner ("just leave as is", clsSound.vb) -- a
         room description that embeds its background music must not
         restart the track every time the room is re-shown.  Only a
         different sound (re)starts the channel. */
      if ((glui32) m->number == gsc_a5_chan_sound[ch])
        {
          if (trace)
            fprintf (stderr, "[a5 media] keep ch=%d snd=%d (already "
                     "playing)\n", ch, m->number);
          glk_schannel_unpause (gsc_a5_channels[ch]);
        }
      else
        {
          if (trace)
            fprintf (stderr, "[a5 media] play ch=%d snd=%d loop=%d\n",
                     ch, m->number, m->loop);
          glk_schannel_play_ext (gsc_a5_channels[ch],
                                 (glui32) m->number,
                                 m->loop ? 0xffffffffu : 1, 0);
          gsc_a5_chan_sound[ch] = (glui32) m->number;
        }
    }
}

/*
 * gsc_a5_media_fire()
 *
 * Fire the sound event behind a positional A5_SOUND_MARK in the turn text --
 * at its place in the display, ahead of any later <waitkey> pause, the way
 * the Runner acts on an <audio> tag the moment its DisplayText reaches it.
 * Flags the event shown so gsc_a5_show_media's after-the-turn sweep does not
 * replay it.
 */
static void
gsc_a5_media_fire (a5_run_t *run, int idx)
{
  const a5_media_event_t *m = a5run_media_get (run, idx);

  if (m == NULL || m->kind == A5_MEDIA_IMAGE)
    return;
  gsc_a5_sound_event (m);
  a5run_media_note_shown (run, idx);
}

/*
 * gsc_a5_show_media()
 *
 * Present the media events the engine collected for the turn just rendered:
 * start/stop sounds on their channels (images are drawn inline at their text
 * marks by gsc_a5_display, and sounds whose text reached the display already
 * fired at their own positional marks -- this sweep catches events whose
 * rendering was dropped, e.g. a deduped repeat response).  Returns the number
 * of images the turn embedded, so the caller can decide whether to fall back
 * to the cover.
 */
static int
gsc_a5_show_media (a5_run_t *run)
{
  int n = a5run_media_count (run), i, images = 0;
  int trace = getenv ("A5_TRACE_MEDIA") != NULL;

  for (i = 0; i < n; i++)
    {
      const a5_media_event_t *m = a5run_media_get (run, i);

      if (m->kind == A5_MEDIA_IMAGE)
        {
          /* Images are drawn inline at their text marks by gsc_a5_display;
             count them here only so the intro's cover-art fallback knows an
             image was already part of the intro. */
          if (m->number > 0)
            images++;
        }
      else if (m->shown)
        {
          if (trace)
            fprintf (stderr, "[a5 media] skip kind=%d ch=%d (fired at its "
                     "mark)\n", m->kind, m->channel);
        }
      else
        gsc_a5_sound_event (m);
    }
  return images;
}

/*
 * gsc_a5_undo_look()
 *
 * Re-show the player's surroundings after a successful UNDO, the way the v4
 * engine reprints the room name -- here the full room view, separated from
 * the "undone" line by a blank line.
 */
static void
gsc_a5_undo_look (a5_run_t *run)
{
  char *look = a5run_look (run);

  if (look != NULL && look[0] != '\0')
    {
      gsc_a5_put_string ("\n");
      gsc_a5_display (look);
      gsc_a5_show_media (run);
    }
  free (look);
}

/*
 * gsc_a5_show_cover()
 *
 * Draw the Blorb frontispiece (cover) image.  Used only when a game's intro
 * does not itself embed any image, so the cover is not shown twice.
 */
static void
gsc_a5_show_cover (void)
{
  giblorb_map_t *map;
  giblorb_result_t chunk;
  const unsigned char *p;

  if (!gsc_a5_graphics_ok)
    return;
  map = giblorb_get_resource_map ();
  if (map == NULL)
    return;
  if (giblorb_load_chunk_by_type (map, giblorb_method_Memory, &chunk,
                                  giblorb_make_id ('F', 's', 'p', 'c'), 0)
      != giblorb_err_None)
    return;
  if (chunk.length >= 4 && chunk.data.ptr != NULL)
    {
      p = (const unsigned char *) chunk.data.ptr;
      gsc_a5_draw_image (gsc_main_window,
                         ((glui32) p[0] << 24) | ((glui32) p[1] << 16)
                         | ((glui32) p[2] << 8) | (glui32) p[3]);
    }
  giblorb_unload_chunk (map, chunk.chunknum);
}

/*
 * gsc_a5_present_intro_media()
 *
 * Show the intro's media; if the intro embedded no image of its own, fall back
 * to the cover so a game with only a frontispiece still shows it.
 */
static void
gsc_a5_present_intro_media (a5_run_t *run)
{
  if (gsc_a5_show_media (run) == 0)
    gsc_a5_show_cover ();
}

/*
 * gsc_a5_printed_width()
 * gsc_a5_next_char()
 *
 * The grid cells gsc_a5_put_string() fills for a UTF-8 string, and the start of
 * the character following the one a string points at.  With Unicode output each
 * multi-byte sequence prints as a single cell; without it the bytes go out as
 * they are, one cell each.  Stepping is by whole sequences either way, so that
 * a truncated tail stays well-formed UTF-8.
 */
static glui32
gsc_a5_printed_width (const char *string)
{
  const unsigned char *p = (const unsigned char *) string;
  glui32 width = 0;

  if (!gsc_unicode_enabled)
    return (glui32) strlen (string);

  for (; *p != '\0'; p++)
    {
      if ((*p & 0xc0) != 0x80)
        width++;
    }
  return width;
}

static const char *
gsc_a5_next_char (const char *string)
{
  const unsigned char *p = (const unsigned char *) string + 1;

  while ((*p & 0xc0) == 0x80)
    p++;
  return (const char *) p;
}

static const gsc_status_writer_t GSC_A5_STATUS_WRITER = {
  gsc_a5_printed_width, gsc_a5_put_string, gsc_a5_next_char
};


/*
 * gsc_a5_status()
 *
 * Redraw the one-line status window: the current room name at the left, and the
 * score (when the game keeps a Score variable) plus move count at the right.
 */
static void
gsc_a5_status (a5_run_t *run)
{
  glui32 width;
  char *room;
  char right[96];
  long score, maxscore;
  glui32 room_end;

  if (!gsc_status_begin (&width))
    return;

  /* Right-hand score / moves. */
  score = a5run_score (run);
  maxscore = a5run_maxscore (run);
  if (maxscore > 0)
    snprintf (right, sizeof right, "Score: %ld/%ld  Moves: %d",
              score, maxscore, a5run_turns (run));
  else if (score != 0)
    snprintf (right, sizeof right, "Score: %ld  Moves: %d",
              score, a5run_turns (run));
  else
    snprintf (right, sizeof right, "Moves: %d", a5run_turns (run));

  /* Room name at the left. */
  glk_window_move_cursor (gsc_status_window, 1, 0);
  room = a5run_location_name (run);
  room_end = 1;
  if (room)
    {
      gsc_a5_put_string (room);
      room_end += gsc_a5_printed_width (room);
      free (room);
    }

  /* Right-justified score/moves. */
  gsc_status_put_right (width, room_end, right, &GSC_A5_STATUS_WRITER);

  gsc_status_end ();
}


/*---------------------------------------------------------------------*/
/*  ADRIFT 5 map window                                                */
/*---------------------------------------------------------------------*/

/*
 * The map view callbacks: what a5map needs to know about this run.  They read
 * the same state the runner's map does -- the player's seen-set, room short
 * names, and the restriction-checked exits.
 */
static int
gsc_a5_map_seen (void *ctx, const char *lockey)
{
  a5_state_t *st = (a5_state_t *) ctx;
  int li = a5state_location_index (st, lockey);

  return li >= 0 && st->loc_seen != NULL && st->loc_seen[li];
}

/*
 * Room labels.  a5text_location_short_plain allocates, but a5map wants a
 * borrowed string, so cache the names for the lifetime of one redraw.
 */
#define GSC_A5_MAP_NAMES 512
static char *gsc_a5_map_name_cache[GSC_A5_MAP_NAMES];
static const char *gsc_a5_map_name_key[GSC_A5_MAP_NAMES];
static int gsc_a5_map_n_names = 0;

static void
gsc_a5_map_names_clear (void)
{
  int i;

  for (i = 0; i < gsc_a5_map_n_names; i++)
    free (gsc_a5_map_name_cache[i]);
  gsc_a5_map_n_names = 0;
}

static const char *
gsc_a5_map_name (void *ctx, const char *lockey)
{
  a5_state_t *st = (a5_state_t *) ctx;
  char *name;
  int i;

  for (i = 0; i < gsc_a5_map_n_names; i++)
    if (gsc_a5_map_name_key[i] == lockey)
      return gsc_a5_map_name_cache[i];

  name = a5text_location_short_plain (st, lockey);
  if (name == NULL)
    return NULL;
  /* Map labels are drawn outside the story window, where the turn loop's own
     strip pass never reaches: a room whose name lost a tag would otherwise
     carry the stripped-tag sentinel into the box. */
  a5text_strip_pres_marks (name);
  if (gsc_a5_map_n_names >= GSC_A5_MAP_NAMES)
    {
      /* Cache full (a huge page): draw this one and drop it. */
      static char scratch[128];
      snprintf (scratch, sizeof scratch, "%s", name);
      free (name);
      return scratch;
    }
  gsc_a5_map_name_key[gsc_a5_map_n_names] = lockey;
  gsc_a5_map_name_cache[gsc_a5_map_n_names] = name;
  gsc_a5_map_n_names++;
  return name;
}

static const char *
gsc_a5_map_exit_dest (void *ctx, const char *lockey, int dir)
{
  a5_state_t *st = (a5_state_t *) ctx;

  return a5restr_exit_in_direction (st, a5state_player_key (st), lockey,
                                    map_dirs[dir], NULL);
}

static int
gsc_a5_map_ever_blocked (void *ctx, const char *lockey, int dir)
{
  a5_state_t *st = (a5_state_t *) ctx;

  return a5restr_ever_blocked (st, lockey, map_dirs[dir]);
}

static void
gsc_a5_map_view (a5_state_t *st, map_view_t *view)
{
  /* The runner draws its map only after PrepareForNextTurn has emptied the
     per-turn route memo, so a route un-blocked late in the turn is re-checked
     fresh by the draw; drop the memo here to match.  (The turn loop clears it
     again at the next command, so entries seeded by drawing never leak into
     game-visible restriction checks.) */
  a5restr_route_cache_clear (st);
  view->seen = gsc_a5_map_seen;
  view->name = gsc_a5_map_name;
  view->exit_dest = gsc_a5_map_exit_dest;
  view->ever_blocked = gsc_a5_map_ever_blocked;
  view->ctx = st;
}

/*
 * gsc_map_current()
 *
 * The map to draw, and the room to centre it on, for whichever engine is
 * running.  For ADRIFT 5 that is the map parsed at load; for ADRIFT 4 there is
 * nothing to parse, so the layout is recomputed here -- it is derived from the
 * room you are in and the rooms you have seen, both of which change as you
 * play, and the runner likewise recomputed it every time it drew.
 *
 * Returns FALSE if there is no map to show.
 */
static int
gsc_map_current (map_view_t *view, const char **player, char *keybuf,
                 int keysize)
{
  if (gsc_is_a5)
    {
      a5_state_t *st;

      if (gsc_a5_run == NULL || gsc_map == NULL)
        return FALSE;
      st = a5run_state (gsc_a5_run);
      gsc_a5_map_view (st, view);
      *player = a5state_player_location (st);
      return TRUE;
    }

  if (gsc_game == NULL)
    return FALSE;

  scmap_view ((scr_gameref_t) gsc_game, view);
  map_free (gsc_map);
  gsc_map = scmap_build ((scr_gameref_t) gsc_game, view);
  if (gsc_map == NULL)
    return FALSE;

  snprintf (keybuf, (size_t) keysize, "%ld",
            (long) gs_playerroom ((scr_gameref_t) gsc_game));
  *player = keybuf;
  return TRUE;
}

/*
 * gsc_map_worth_opening()
 *
 * Whether opening the pane right now would show the player anything.
 *
 * ADRIFT 5 games routinely run their title, credits and options screens from a
 * staging room the map hides (Location <Hide>), and a page whose only seen room
 * is hidden renders as an empty rectangle -- The Axe of Kolt spends three
 * screens there before the game proper begins.  Rather than put up a blank
 * pane, opening waits; every redraw asks again, so the map arrives with the
 * first real room.
 *
 * ADRIFT 4 hides its map the same way, but a step earlier: the layout is
 * derived from the room you are standing in, and from a room the author flagged
 * HideOnMap the runner drew nothing at all (Form29.drawmap bails, so scmap_build
 * hands back no map).  The PK Girl opens in such a room and plays its whole
 * introduction there.  That is the same "wait for a real room" case, not a
 * failure, so it defers too.
 *
 * A map that cannot be built for any other reason answers TRUE, because that is
 * a different complaint and one the pane makes for itself: the game may have no
 * map to show, or ADRIFT 4's layout may have given up ("too complex"), and the
 * player asking for the map deserves to be told so.
 */
static int
gsc_map_worth_opening (void)
{
  map_view_t view;
  const char *ploc = NULL;
  char keybuf[16];

  if (!gsc_map_current (&view, &ploc, keybuf, sizeof keybuf))
    return gsc_is_a5 || !gsc_map_available () || scmap_failed () != 0;
  return map_has_content (gsc_map, &view, ploc);
}

/*
 * gsc_map_redraw()
 *
 * Rasterise the map and blit it into the graphics window.  Glk has no drawing
 * primitives beyond a filled rectangle, so -- as in the Comprehend port -- the
 * page is rendered into an RGB surface and flushed as run-length-encoded
 * horizontal spans, which is far cheaper than one fill_rect per pixel.
 *
 * Only the rows that differ from what is already on screen (gsc_map_screen) are
 * flushed, so a turn that leaves the map alone -- most of them -- sends nothing
 * at all.
 */
static void
gsc_map_redraw (void)
{
  map_surface_t *surf;
  map_view_t view;
  const char *ploc = NULL;
  char keybuf[16];
  glui32 w, h;
  int x, y;

  /* A map that was asked for while there was nothing on it: try again now
     that the game has moved on.  (gsc_map_show comes straight back here, but
     with gsc_map_shown set, so this runs once.) */
  if (gsc_map_want && !gsc_map_shown && gsc_map_worth_opening ())
    gsc_map_show ();

  if (!gsc_map_window)
    return;

  glk_window_get_size (gsc_map_window, &w, &h);
  if (w == 0 || h == 0)
    return;

  /* The map is drawn in the story's colours: the buffer's normal style
     supplies the background and text colour.  Colour mode publishes those as
     Normal stylehints (and rebuilds windows so they stick); libraries that
     ignore author styles leave measure on the theme, matching the transcript.
     A Glk that cannot measure leaves the palette at its black-on-white
     default.  The measurement comes from gsc_normal_measure's cache -- the
     map redraws at every prompt, and each miss is two synchronous
     round-trips to the application. */
  {
    glui32 bg, fg;

    if (gsc_normal_measure (&fg, &bg))
      {
        map_set_palette (bg, fg);
        /* So the clear below, and any exposed edge, match the surface. */
        glk_window_set_background_color (gsc_map_window, bg);
      }
  }

  /* Nothing to draw: an ADRIFT 4 layout that gave up, or a player standing in a
     room hidden from the map -- where the runner showed an empty map too.  Wipe
     the pane rather than leave the last one up; it will come back by itself. */
  if (!gsc_map_current (&view, &ploc, keybuf, sizeof keybuf))
    {
      glk_window_clear (gsc_map_window);
      gsc_map_screen_drop ();
      return;
    }

  surf = map_surface_new ((int) w, (int) h);
  if (surf == NULL)
    return;

  map_frame (gsc_map, &view, ploc, surf, gsc_map_zoom, &gsc_map_cam);
  map_render (gsc_map, &view, ploc, &gsc_map_cam, surf);
  gsc_map_px_w = surf->w;
  gsc_map_px_h = surf->h;
  if (gsc_is_a5)
    gsc_a5_map_names_clear ();

  /* The window was resized under us, so the pixels we think are on screen are
     not the ones that are. */
  if (gsc_map_screen != NULL
      && (gsc_map_screen->w != surf->w || gsc_map_screen->h != surf->h))
    gsc_map_screen_drop ();

  for (y = 0; y < surf->h; y++)
    {
      const glui32 *row = &surf->px[(size_t) y * surf->w];

      /* Untouched row: what is on screen is already right. */
      if (!gsc_map_full_flush && gsc_map_screen != NULL
          && memcmp (row, &gsc_map_screen->px[(size_t) y * surf->w],
                     (size_t) surf->w * sizeof row[0]) == 0)
        continue;

      x = 0;
      while (x < surf->w)
        {
          glui32 c = row[x];
          int x0 = x;

          do
            x++;
          while (x < surf->w && row[x] == c);
          glk_window_fill_rect (gsc_map_window, c,
                                x0, y, (glui32) (x - x0), 1);
        }
    }

  /* These pixels are the screen now. */
  map_surface_free (gsc_map_screen);
  gsc_map_screen = surf;
  gsc_map_full_flush = FALSE;

  /* Arm the click that walks the player to a room.  Mouse requests are
     one-shot, so this is re-armed after every redraw. */
  if (glk_gestalt (gestalt_MouseInput, wintype_Graphics))
    glk_request_mouse_event (gsc_map_window);
}

/*
 * gsc_sc_walk_next()
 *
 * The next step of a map-click walk in an ADRIFT 4 game, as the direction
 * command that makes it.  The route only runs through rooms already visited,
 * and only along exits whose restrictions are currently satisfied, so it can
 * always be walked.  Gives up on arrival, when no route remains, or when the
 * last step did not actually move us -- something in the game blocked it.
 */
static int
gsc_sc_walk_next (scr_char *buffer, scr_int length)
{
  map_view_t view;
  char from[16];
  const scr_char *word;
  int dir;

  if (gsc_sc_walk_to[0] == '\0' || gsc_game == NULL)
    return FALSE;
  if (++gsc_sc_walk_steps > GSC_SC_WALK_MAX)
    {
      gsc_sc_walk_stop ();
      return FALSE;
    }

  snprintf (from, sizeof from, "%ld",
            (long) gs_playerroom ((scr_gameref_t) gsc_game));
  if (strcmp (from, gsc_sc_walk_to) == 0
      || (gsc_sc_walk_last[0] != '\0' && strcmp (from, gsc_sc_walk_last) == 0))
    {
      gsc_sc_walk_stop ();
      return FALSE;
    }

  scmap_view ((scr_gameref_t) gsc_game, &view);
  dir = map_walk_step (&view, from, gsc_sc_walk_to);
  word = (dir >= 0) ? lib_direction_name (dir) : NULL;
  if (word == NULL)
    {
      gsc_sc_walk_stop ();
      return FALSE;
    }

  snprintf (buffer, (size_t) length, "%s", word);
  snprintf (gsc_sc_walk_last, sizeof gsc_sc_walk_last, "%s", from);
  return TRUE;
}

/*
 * gsc_a5_walk_next()
 *
 * The next step of a map-click walk, as the game command that makes it (the
 * localized direction word, so the game's own parser takes it).  Returns FALSE
 * when the walk is over -- we have arrived, no route remains, or the last step
 * did not move us (something blocked the way), which is DoWalk's
 * sLastPosition bail-out.
 */
static int
gsc_a5_walk_next (a5_run_t *run, char *buf, int bufsize)
{
  map_view_t view;
  a5_state_t *st;
  const char *from, *word;
  int dir;

  if (gsc_a5_walk_to[0] == '\0' || run == NULL)
    return FALSE;
  if (a5run_is_over (run))
    return FALSE;                       /* the walk ended the game */
  if (++gsc_a5_walk_steps > GSC_A5_WALK_MAX)
    return FALSE;

  st = a5run_state (run);
  from = a5state_player_location (st);
  if (from == NULL)
    return FALSE;

  if (strcmp (from, gsc_a5_walk_to) == 0)
    return FALSE;                       /* arrived */
  if (gsc_a5_walk_last[0] != '\0' && strcmp (from, gsc_a5_walk_last) == 0)
    return FALSE;                       /* the last step did not move us */

  gsc_a5_map_view (st, &view);
  dir = map_walk_step (&view, from, gsc_a5_walk_to);
  if (dir < 0)
    return FALSE;

  word = a5parse_direction_name (map_dirs[dir]);
  if (word == NULL)
    return FALSE;

  snprintf (buf, (size_t) bufsize, "%s", word);
  snprintf (gsc_a5_walk_last, sizeof gsc_a5_walk_last, "%s", from);
  return TRUE;
}

/*
 * gsc_map_available()
 *
 * Whether this game has a map at all.  In ADRIFT 5 that is settled at load:
 * it either shipped map data or it did not.  In ADRIFT 4 every game with
 * rooms has one, unless the author switched it off -- and gsc_map itself is
 * no guide there, being built lazily at the first redraw.  Whether there is
 * anything to draw *right now* is a separate question (map_has_content).
 */
static int
gsc_map_available (void)
{
  if (gsc_is_a5)
    return gsc_map != NULL;
  return gsc_game != NULL && scmap_available ((scr_gameref_t) gsc_game);
}

/*
 * gsc_map_default_shown()
 *
 * Whether this game would start with the map showing if the player had never
 * said otherwise: what the author's shipped Runner layout asks for in ADRIFT
 * 5, and nothing at all in ADRIFT 4, whose Runner opened with the map closed
 * however the last game left it.
 */
static int
gsc_map_default_shown (void)
{
  return gsc_is_a5 && gsc_a5_adv != NULL && gsc_a5_adv->map_pane_open == 1;
}

/*
 * gsc_map_pref_ref()
 *
 * A fileref for this game's remembered map choice, or NULL when there is
 * nothing to remember or nowhere to keep it.
 *
 * The ADRIFT 5 Runner keeps the player's window layout per game, in
 * RunnerLayout-<IFID>.xml, and writes it out on quit; that file is also what
 * overrides the layout an author shipped inside the Blorb, because the Runner
 * only unpacks the shipped one when no file for that IFID exists yet
 * (Blorb.vb:343).  This is the same store, under the same key -- two bytes,
 * '1' or '0' for whether the map is wanted and 't' or 'r' for where it goes,
 * rather than a window layout.
 *
 * Games the Runner has no key for get one of ours (gsc_game_key): ADRIFT 4
 * never had an IFID, and its Runner kept the map as a global View-menu
 * setting in the registry rather than per game, so there is nothing to be
 * compatible with -- and a few ADRIFT 5 .taf files were built without an
 * <ifid> block too.
 *
 * Games with no map and hosts with no graphics have nothing to record, and
 * must not leave a file behind for it.
 */
static frefid_t
gsc_map_pref_ref (void)
{
  const char *key;
  char name[128];

  if (!gsc_map_available ())
    return NULL;
  if (!glk_gestalt (gestalt_Graphics, 0)
      || !glk_gestalt (gestalt_DrawImage, wintype_Graphics))
    return NULL;

  if (gsc_is_a5 && gsc_a5_adv != NULL
      && gsc_a5_adv->ifid != NULL && gsc_a5_adv->ifid[0] != '\0')
    key = gsc_a5_adv->ifid;
  else if (gsc_game_key[0] != '\0')
    key = gsc_game_key;
  else
    return NULL;

  snprintf (name, sizeof name, "scarier-map-%s", key);
  return glk_fileref_create_by_name (fileusage_Data | fileusage_TextMode,
                                     name, 0);
}

/*
 * gsc_map_pref_read()
 *
 * The player's remembered choice for this game: 1 to show the map, 0 to hide
 * it, -1 when they have never said.  Where they last put it is returned
 * through at_top on the same terms -- 1 for the top band, 0 for the pane at
 * the right, -1 for never said -- since files written before the map could be
 * moved hold only the first byte.
 */
static int
gsc_map_pref_read (int *at_top)
{
  frefid_t fileref;
  int value = -1;

  if (at_top != NULL)
    *at_top = -1;

  fileref = gsc_map_pref_ref ();
  if (fileref == NULL)
    return -1;

  if (glk_fileref_does_file_exist (fileref))
    {
      strid_t stream = glk_stream_open_file (fileref, filemode_Read, 0);

      if (stream)
        {
          glui32 c = glk_get_char_stream (stream);

          if (c == '0' || c == '1')
            value = (int) (c - '0');

          c = glk_get_char_stream (stream);
          if (at_top != NULL && (c == 't' || c == 'r'))
            *at_top = (c == 't');

          glk_stream_close (stream, NULL);
        }
    }

  glk_fileref_destroy (fileref);
  return value;
}

/*
 * gsc_map_pref_write()
 *
 * Remember that the player asked for the map to be shown or hidden in this
 * game, and where they had it, so that the next session opens the way they
 * left it.  The position is recorded even when the map is off, so that turning
 * it back on later still puts it where they last had it.
 *
 * Only a game whose map the player has actually moved away from its default
 * gets a file; one they have put back where it started has theirs removed
 * again.  So the common case -- ADRIFT 4, where the map starts hidden and
 * most players leave it that way -- costs nothing at all, and the files that
 * do accumulate are only for games the player made a decision about.
 */
static void
gsc_map_pref_write (int shown, int at_top)
{
  frefid_t fileref;
  strid_t stream;

  fileref = gsc_map_pref_ref ();
  if (fileref == NULL)
    return;

  if (!shown == !gsc_map_default_shown () && !at_top)
    {
      if (glk_fileref_does_file_exist (fileref))
        glk_fileref_delete_file (fileref);
      glk_fileref_destroy (fileref);
      return;
    }

  stream = glk_stream_open_file (fileref, filemode_Write, 0);
  if (stream)
    {
      glk_put_char_stream (stream, (unsigned char) (shown ? '1' : '0'));
      glk_put_char_stream (stream, (unsigned char) (at_top ? 't' : 'r'));
      glk_stream_close (stream, NULL);
    }
  glk_fileref_destroy (fileref);
}

/*
 * gsc_map_show()
 *
 * Open the map pane, in whichever position gsc_map_at_top asks for: the
 * standard pane to the right of the story, or a band across the top of the
 * screen.  The top band splits the root window, not the story, so that it
 * sits above the status line and spans the display's full width.
 */
static void
gsc_map_show (void)
{
  if (!gsc_map_available ())
    {
      gsc_normal_string ("This game has no map.\n");
      return;
    }

  if (!glk_gestalt (gestalt_Graphics, 0)
      || !glk_gestalt (gestalt_DrawImage, wintype_Graphics))
    {
      gsc_normal_string ("This interpreter cannot display the map.\n");
      return;
    }

  if (gsc_map_at_top)
    gsc_map_window = glk_window_open (glk_window_get_root (),
                                         winmethod_Above
                                         | winmethod_Proportional,
                                         30, wintype_Graphics, 0);
  else
    gsc_map_window = glk_window_open (gsc_main_window,
                                         winmethod_Right
                                         | winmethod_Proportional,
                                         40, wintype_Graphics, 0);
  if (!gsc_map_window)
    {
      gsc_normal_string ("Sorry, the map window could not be opened.\n");
      return;
    }
  gsc_map_shown = TRUE;
  gsc_map_screen_drop ();       /* a fresh window holds nothing */
  gsc_map_redraw ();

  /* The ADRIFT 4 layout can give up ("Cannot draw map - too complex.",
     Form29.showmapfail).  Say so here, once, rather than at every prompt: the
     pane stays open, because a layout that fails from one room may well succeed
     from the next. */
  if (!gsc_is_a5 && gsc_map == NULL && scmap_failed () != 0)
    gsc_normal_string ("Sorry, this game's map is too complex to draw.\n");

  glk_set_window (gsc_main_window);
}

/*
 * gsc_map_hide()
 *
 * Close the map pane, dropping the record of what was drawn there.
 */
static void
gsc_map_hide (void)
{
  if (gsc_map_window)
    {
      glk_window_close (gsc_map_window, NULL);
      gsc_map_window = NULL;
    }
  gsc_map_shown = FALSE;
  gsc_map_screen_drop ();
}

/*
 * gsc_map_set()
 *
 * Show or hide the map pane at the player's request, and remember the choice
 * for this game.  The window is opened on first use (and only for games that
 * actually have map data), then kept, so toggling is cheap.
 *
 * Remembering matters because a game can ask to start with its map showing
 * (gsc_map_auto_reveal); the player's own last word has to outlast the
 * session, or "glk map off" would be undone by the next launch.
 */
static void
gsc_map_set (int shown)
{
  int deferred = FALSE;

  if (shown)
    {
      if (!gsc_map_shown)
        {
          if (gsc_map_worth_opening ())
            gsc_map_show ();
          else
            {
              deferred = TRUE;
              gsc_normal_string ("There is nothing to put on the map from"
                                 " here; it will open by itself as soon as"
                                 " there is.\n");
            }
        }
    }
  else if (gsc_map_shown)
    {
      gsc_map_hide ();
      gsc_normal_string ("Map hidden.\n");
    }
  else if (gsc_map_want)
    /* Called off before it ever got to open. */
    gsc_normal_string ("Map hidden.\n");

  /* What actually happened, not what was asked for: opening can fail outright
     (no map, no graphics), and a pane that cannot exist is not a preference
     worth keeping.  A deferred open is the opposite case -- the player did
     ask, and the map is on its way. */
  gsc_map_want = gsc_map_shown || deferred;
  gsc_map_pref_write (gsc_map_want, gsc_map_at_top);
}

/*
 * gsc_map_toggle()
 *
 * Show the map pane if it is hidden, hide it if it is shown -- or cancel it if
 * it is merely on its way.
 */
static void
gsc_map_toggle (void)
{
  gsc_map_set (!(gsc_map_shown || gsc_map_want));
}

/*
 * gsc_map_auto_reveal()
 *
 * Open the map pane at the start of a game that asks for it.
 *
 * ADRIFT 5 games can carry the author's own Runner window layout in their
 * Blorb (a5blorb_find_layout), and run500.exe starts with whatever panes that
 * layout has open -- which is the whole of why a handful of games "come with
 * the map already showing" and the rest do not.  There is no authored
 * map-visibility setting, and ADRIFT 4 has no equivalent at all: run400's map
 * was a View-menu toggle stored globally in the registry, never per game.
 *
 * A choice the player made themselves wins over the author's, exactly as the
 * Runner's own saved RunnerLayout-<IFID>.xml wins over the shipped one -- in
 * either direction, so a player who once opened the map in this game gets it
 * back too.  That part applies to ADRIFT 4 as well, which is why this runs on
 * both paths: the game has nothing to say about the map, but the player does.
 * Silent throughout: a host that cannot draw a map, or a game with none, just
 * gets no pane.
 */
static void
gsc_map_auto_reveal (void)
{
  int pref, at_top;

  if (gsc_map_shown)
    return;

  gsc_map_want = FALSE;
  if (!gsc_map_available ())
    return;
  if (!glk_gestalt (gestalt_Graphics, 0)
      || !glk_gestalt (gestalt_DrawImage, wintype_Graphics))
    return;

  pref = gsc_map_pref_read (&at_top);

  /* Where the map goes is remembered whether or not it is opened now, so that
     a later "glk map on" puts it back where the player last had it. */
  if (at_top >= 0)
    gsc_map_at_top = at_top;

  if (pref < 0)
    {
      /* Never asked: follow the layout the author shipped, if any. */
      if (!gsc_is_a5 || gsc_a5_adv == NULL || gsc_a5_adv->map_pane_open != 1)
        return;
    }
  else if (pref == 0)
    return;

  /* Wanted -- but an opening played out from a room the map hides has nothing
     to put in the pane, so the map may have to wait for the first real room
     (gsc_map_worth_opening, and the retry in gsc_map_redraw). */
  gsc_map_want = TRUE;
  if (gsc_map_worth_opening ())
    gsc_map_show ();
}

/*
 * gsc_map_notice_restart()
 *
 * Take the map pane down for a replayed ADRIFT 4 opening, and let it be decided
 * again from scratch.
 *
 * The ADRIFT 5 path does this where the restart happens (gsc_a5_restart_run);
 * ADRIFT 4 has nowhere to do it, because the restart never surfaces here.  Both
 * RESTART at the prompt and the restart offered when the game ends are handled
 * inside scr_interpret_game, which just replays the opening -- so the pane goes
 * into the new game still showing the old one's layout, and for a game whose
 * opening plays out in a room the map hides, like The PK Girl, that means the
 * empty rectangle gsc_map_worth_opening exists to keep off the screen.  Watch
 * the interpreter's restart count for a change instead (run_get_restart_count).
 *
 * The zoom goes with the old run for the same reason it does in ADRIFT 5: the
 * new one is back to a single room, which a scale chosen for a whole map would
 * show far too close in.
 *
 * Called at the first sign of the replayed opening -- its first line of output,
 * or failing that its first prompt -- so that the opening is laid out at the
 * width it will keep.  Opening and closing windows moves the current stream
 * about (gsc_map_show leaves output pointed at the story window), which the
 * caller mid-print is not expecting, so put it back.
 */
static void
gsc_map_notice_restart (void)
{
  strid_t stream;
  scr_int restarts;

  if (gsc_is_a5 || gsc_game == NULL)
    return;

  restarts = run_get_restart_count ();
  if (restarts == gsc_map_restarts)
    return;
  gsc_map_restarts = restarts;

  stream = glk_stream_get_current ();
  gsc_map_hide ();
  gsc_map_zoom = 0;
  gsc_map_auto_reveal ();
  glk_stream_set_current (stream);
}

/*
 * gsc_map_place()
 *
 * Put the map pane at the top of the screen ("glk map top", for games with
 * wide maps) or back at the right of the story ("glk map right").  A map
 * already shown in the other position is closed and reopened in the new one.
 */
static void
gsc_map_place (int at_top)
{
  if (gsc_map_shown && gsc_map_at_top == at_top)
    {
      gsc_normal_string (at_top
                         ? "The map is already at the top of the screen.\n"
                         : "The map is already at the right.\n");
      return;
    }

  gsc_map_hide ();
  gsc_map_at_top = at_top;

  /* Asking where the map goes is asking for a map -- through gsc_map_set, so
     that the request is remembered and honoured later even if there is nothing
     to draw from where the player is standing. */
  gsc_map_set (TRUE);
}

/*
 * gsc_command_map()
 *
 * "glk map [on|off]".  Always available, even for the rare game that defines a
 * MAP command of its own (see gsc_map_taken).
 */
static void
gsc_command_map (const char *argument)
{
  if (gsc_is_a5 ? gsc_a5_run == NULL : gsc_game == NULL)
    return;

  if (strlen (argument) == 0)
    {
      gsc_map_toggle ();
      return;
    }
  /* Explicit on/off go through gsc_map_set even when they change nothing, so
     that "glk map off" in a game that opens with its map showing is recorded
     as a veto rather than shrugged off as a no-op. */
  if (scr_strcasecmp (argument, "on") == 0)
    {
      gsc_map_set (TRUE);
    }
  else if (scr_strcasecmp (argument, "off") == 0)
    {
      gsc_map_set (FALSE);
    }
  else if (scr_strcasecmp (argument, "top") == 0
           || scr_strcasecmp (argument, "above") == 0)
    {
      gsc_map_place (TRUE);
    }
  else if (scr_strcasecmp (argument, "right") == 0
           || scr_strcasecmp (argument, "side") == 0)
    {
      gsc_map_place (FALSE);
    }
  else if (scr_strncasecmp (argument, "zoom", 4) == 0
           && (argument[4] == '\0' || argument[4] == ' '
               || argument[4] == '\t'))
    {
      /* "glk map zoom ..." is "glk zoom ..." by another name. */
      gsc_command_zoom (argument + 4 + strspn (argument + 4, "\t "));
    }
  else
    gsc_command_usage ("map");
}

/*
 * gsc_command_zoom()
 *
 * "glk zoom [in | out | auto]".  Plain "glk zoom" zooms in, and "default" is a
 * synonym for "auto".  A manual zoom is kept until "auto" puts the map back
 * to fitting itself to its window; meanwhile the view pans to keep the
 * player on-screen (map_frame).
 */
static void
gsc_command_zoom (const char *argument)
{
  int in, scale, stepped;

  if (gsc_is_a5 ? gsc_a5_run == NULL : gsc_game == NULL)
    return;

  /* "glk map zoom help" arrives here directly; the dispatcher catches the
     plain "glk zoom help" before the handler is ever called. */
  if (scr_strcasecmp (argument, "help") == 0)
    {
      gsc_command_help ("zoom");
      return;
    }

  if (scr_strcasecmp (argument, "auto") == 0
      || scr_strcasecmp (argument, "default") == 0)
    {
      if (gsc_map_zoom == 0)
        gsc_normal_string ("The map is already zooming to fit its window.\n");
      else
        {
          gsc_map_zoom = 0;
          gsc_map_redraw ();
          gsc_normal_string ("Map zoom returned to automatic.\n");
        }
      return;
    }

  if (strlen (argument) == 0 || scr_strcasecmp (argument, "in") == 0)
    in = TRUE;
  else if (scr_strcasecmp (argument, "out") == 0)
    in = FALSE;
  else
    {
      gsc_command_usage ("zoom");
      return;
    }

  if (!gsc_map_shown)
    {
      gsc_normal_string ("The map is not open.  Use ");
      gsc_standout_string ("glk map on");
      gsc_normal_string (" to open it first.\n");
      return;
    }

  /* Step from the manual zoom, or from wherever the automatic fit last
     landed; a map with nothing drawn yet steps from the runner's default. */
  scale = gsc_map_zoom > 0 ? gsc_map_zoom : gsc_map_cam.scale;
  if (scale < MAP_SCALE_MIN)
    scale = 10;
  stepped = map_zoom_step (scale, in ? 1 : -1);
  if (stepped == scale)
    {
      gsc_normal_string (in ? "The map is already at its maximum zoom.\n"
                            : "The map is already at its minimum zoom.\n");
      return;
    }
  gsc_map_zoom = stepped;
  gsc_map_redraw ();
}


/*
 * gsc_a5_try_undo()
 * gsc_a5_try_restore()
 *
 * The UNDO and RESTORE actions shared by the running-game prompt and the
 * end-of-game banner.  Each performs the action, reports it, and brings the
 * live-state panes up to date, returning TRUE on success.  The undo failure
 * message is left to the callers, which word it differently.
 */
static int
gsc_a5_try_undo (a5_run_t *run)
{
  if (!a5run_undo (run))
    return FALSE;

  gsc_a5_put_string ("The previous turn has been undone.\n");
  gsc_a5_undo_look (run);
  gsc_a5_status (run);
  gsc_map_redraw ();
  return TRUE;
}

static int
gsc_a5_try_restore (a5_run_t *run)
{
  if (!gsc_a5_restore (run))
    {
      gsc_a5_put_string ("Restore failed.\n");
      return FALSE;
    }

  /* Don't let a later UNDO jump back across the restore boundary. */
  a5run_undo_forget (run);
  gsc_a5_put_string ("Game restored.\n");
  return TRUE;
}


/*
 * gsc_a5_present_intro()
 *
 * Show a fresh run's opening: the intro text (paged by gsc_a5_display, so
 * <cls>/<waitkey> marks in it work as they do in the official Runner), then
 * any cover media, then the live-state panes.
 */
static void
gsc_a5_present_intro (a5_run_t *run)
{
  char *text = a5run_intro (run);

  gsc_a5_display (text);
  free (text);
  gsc_a5_present_intro_media (run);
  gsc_a5_status (run);
  gsc_map_redraw ();
}


/*
 * gsc_a5_restart_run()
 *
 * Replace the run with a new one on the same adventure and replay its
 * opening.  Reached both from RESTART at the prompt and from RESTART at the
 * end-of-game banner; `run` aliases gsc_a5_run, so redraws follow it across
 * the swap.  Does not return if the new run cannot be allocated.
 */
static void
gsc_a5_restart_run (a5_run_t *&run)
{
  a5run_free (run);
  run = a5run_new (gsc_a5_adv);
  if (!run)
    {
      gsc_a5_put_string ("Out of memory restarting game.\n");
      glk_exit ();
    }
  gsc_a5_stop_all_sounds ();
  gsc_a5_start_real_time (run);

  /* Take the map down for the replayed opening and put it back afterwards,
     for the same two reasons the first reveal waits: the title screen should
     have the screen to itself, and what comes back should be decided afresh
     -- by the player's remembered choice, or failing that by the layout the
     game shipped.  A hand-set zoom goes with the old run: the new one is back
     to a single room, which a scale chosen for a whole map would show far too
     close in. */
  gsc_map_hide ();
  gsc_map_zoom = 0;

  glk_window_clear (gsc_main_window);
  gsc_main_window_empty = TRUE;
  gsc_a5_present_intro (run);
  gsc_map_auto_reveal ();
}


/*
 * gsc_a5_meta_perform()
 *
 * Carry out a pending meta-command on the a5 engine, called from the top of
 * the turn loop -- the one place the run may be thrown away and rebuilt.  That
 * is also why a command typed at a %PopUpInput% prompt waits: the engine is
 * then partway through rendering a turn on the very run a restart would free.
 *
 * Each action does what the a5 loop's own command of that name does,
 * unconfirmed as those are.  Returns TRUE if the game should stop.
 */
static int
gsc_a5_meta_perform (a5_run_t *&run)
{
  switch (gsc_meta_take ())
    {
    case GSC_META_QUIT:
      return TRUE;

    case GSC_META_RESTART:
      gsc_a5_restart_run (run);
      break;

    case GSC_META_UNDO:
      if (!gsc_a5_try_undo (run))
        gsc_a5_put_string (a5run_turns (run) == 0
                           ? "You can't undo what hasn't been done.\n"
                           : "Sorry, no more undo is available.\n");
      break;

    case GSC_META_RESTORE:
      gsc_a5_try_restore (run);
      break;

    default:
      break;
    }

  return FALSE;
}


/*
 * gsc_a5_main()
 *
 * Run the ADRIFT 5 game in a single text-buffer window: print the intro, then
 * loop reading commands and printing each turn's output, handling the host
 * meta-commands and end-of-game.
 */
static void
gsc_a5_main (void)
{
  /* Alias the run through gsc_a5_run so resize redraws always see the
     current run, including across restarts. */
  a5_run_t *&run = gsc_a5_run;
  char *text;
  char input[1024];
  int autorestore = FALSE;

#ifdef SPATTERLIGHT
  autorestore = gsc_autorestore_wanted ();
#endif

  gsc_hint_window_styles ();

  /* An autorestore adopts the archived windows further down instead: opening
     any here would cost the player the host's restored ones (see
     gsc_autorestore_wanted). */
  if (!autorestore)
    {
#ifdef GSC_HAVE_ZCOLORS
      /* Starting in colour ("-c", or an adventure that needs its palette to
         be read): publish Normal colours before the first open so the windows
         snapshot them; gsc_colour_startup_apply then enables zcolors without
         tearing the tree down again. */
      if (gsc_colour_startup)
        {
          gsc_colour_set_normal_hints (TRUE);
          gsc_colour_hints_preapplied = TRUE;
        }
#endif
      gsc_open_main_window ();
      gsc_open_status_window ();

      /* Into the adventure's own palette (already parsed off the header by the
         startup code) before the title page is drawn. */
      gsc_colour_startup_apply ();
    }

  /* Present turns interactively: keep <cls>/<waitkey>/<img> as positional
     marks in the turn text (a5text.h) so gsc_a5_display can page an intro
     like the official Runner -- title page, keypress, screen clear. */
  a5text_set_interactive (TRUE);

  /* Ask the player %PopUpInput% naming prompts, the story window standing in
     for the Runner's modal InputBox; unasked, they evaluate to their default.
     Likewise %PopUpChoice% gender prompts for its Yes/No MsgBox; unasked,
     those stay unevaluated. */
  a5text_set_popup_cb (gsc_a5_popup_input, NULL);
  a5text_set_popup_choice_cb (gsc_a5_popup_choice, NULL);

  /* Register the game Blorb for image/sound resources. */
  gsc_a5_init_resources ();

  if (!gsc_a5_adv)
    {
      gsc_a5_put_string ("No ADRIFT 5 game loaded.\n");
      glk_exit ();
    }

  /* The "Adventure Upgrade" bracket-correction question (pre-5.0.26 file with
     AND-then-OR restriction sequences): rather than interrupt the player with a
     dialog, resolve it silently.  The default is NO (matching an unattended
     ADRIFT 5 Runner -- the sequence is read verbatim); a hard-wired per-game
     allow-list forces YES for the few games that need the correction to play or
     score correctly (a5model_upgrade_forced_yes, keyed on the Babel IFID). */
  if (a5model_upgrade_pending (gsc_a5_adv))
    {
      a5model_upgrade_answer (gsc_a5_adv, a5model_upgrade_forced_yes (gsc_a5_adv));
      gsc_a5_adv->upgrade_silent = 1;   /* suppress the dialog prose in the intro */
    }

  run = a5run_new (gsc_a5_adv);
  if (!run)
    {
      gsc_a5_put_string ("Out of memory loading ADRIFT 5 game.\n");
      glk_exit ();
    }
  gsc_a5_start_real_time (run);

  /* The map is authored data on the adventure, so it outlives restarts. */
  if (gsc_map == NULL)
    {
      gsc_map = a5map_load (gsc_a5_adv);
      gsc_map_taken = a5map_command_taken (gsc_a5_adv);
    }

#ifdef SPATTERLIGHT
  /* When a Spatterlight autosave exists, boot the world silently to where a
     manual RESTORE would find it (the intro's task and RNG side effects
     included, its text discarded), then replace the whole state -- engine
     and Glk library both -- with the saved one.  The app restores the
     window contents from its own GUI snapshot; the loop below skips one
     prompt print (the restored transcript already ends with it).  No window
     has been opened yet -- the restored library supplies them all. */
  if (autorestore)
    {
      std::string data;
      bool restored;

      gsc_a5_popup_silent = TRUE;
      text = a5run_intro (run);
      gsc_a5_popup_silent = FALSE;
      free (text);
      restored = scarier_autosave_read_game (&data)
                 && gsc_a5_apply_all (data)
                 && scarier_autosave_restore_library ();
      if (!restored)
        {
          /* Bad autosave (now deleted).  The silent boot above already
             consumed the game's intro, so restart in a fresh process
             rather than continue from a polluted state. */
          scarier_autosave_discard ();
          win_reset ();
          exit (0);
        }
      glk_set_window (gsc_main_window);
      glk_set_style (style_Normal);
      /* Re-arm or cancel the real-time timer per the current preferences
         (the library's late pass re-armed whatever interval was archived);
         then repaint the live-state panes.  Sounds are the app's business:
         it resumes them from its own snapshot, so none are replayed here. */
      gsc_a5_start_real_time (run);
      gsc_a5_status (run);
      gsc_map_redraw ();
      gsc_autorestored = TRUE;
    }
  else
    {
#endif
  gsc_a5_present_intro (run);
  /* After the intro, so the title page is not sharing the screen with a map.
     An autorestore takes the archived pane instead, whatever it was. */
  gsc_map_auto_reveal ();
#ifdef SPATTERLIGHT
    }
#endif

  /* An adventure the runner refuses outright -- today, one with no locations at
     all -- stops here rather than dropping the player into a world with no
     room to stand in.  The real Runner shows its window and the game title,
     then an "ADRIFT Error" dialog carrying this same text, which is why the
     check comes after the intro (a5model_load_error).  Scarier has no dialog,
     so the story window stands in for it, as it does for the Adventure-Upgrade
     question. */
  {
    const char *load_err = a5model_load_error (gsc_a5_adv);
    if (load_err != NULL)
      {
        gsc_a5_put_string ("\n");
        gsc_a5_put_string (load_err);
        gsc_a5_put_string ("\n");
        glk_exit ();
      }
  }

  for (;;)
    {
      /* Any "glk undo/restore/restart/quit" happens here, before the state of
         play is read: this is the first moment the run may safely be replaced,
         whether the command was typed at the prompt below or at a naming
         prompt the engine opened mid-turn. */
      if (gsc_a5_meta_perform (run))
        break;

      if (a5run_is_over (run))
        /* The game has ended -- by the last command, or by a real-time
           TimeBased tick that fired while awaiting input.  The engine has
           already emitted the win/lose/score block (whose banner offers
           restart / restore / quit / undo); honour all four here.  UNDO and
           RESTORE revert to a running state and resume play; RESTART rebuilds
           the game. */
        {
          int resumed = 0;

          /* A step of a map walk may have ended the game; the walk must not
             then answer the restart/restore/undo/quit prompt for us. */
          gsc_a5_walk_stop ();

          for (;;)
            {
              /* The banner's four answers, however they were asked for: typed
                 as themselves, or reached with "glk ..." (which is how they are
                 still available to a game that has taken the words for its
                 own).  Anything else is ignored, and the banner asks again. */
              gsc_meta_t answer = GSC_META_NONE;

              gsc_a5_put_prompt ("\nPlease enter RESTART, RESTORE, UNDO or QUIT.\n");
              if (gsc_a5_read_line (input, sizeof input) == 0)
                continue;

              if (gsc_a5_command_escape (input))
                answer = gsc_meta_take ();
              else if (gsc_a5_match_command (input, "quit")
                       || gsc_a5_match_command (input, "q"))
                answer = GSC_META_QUIT;
              else if (gsc_a5_match_command (input, "restart"))
                answer = GSC_META_RESTART;
              else if (gsc_a5_match_command (input, "undo"))
                answer = GSC_META_UNDO;
              else if (gsc_a5_match_command (input, "restore"))
                answer = GSC_META_RESTORE;

              if (answer == GSC_META_QUIT)
                {
                  a5run_free (run);
                  glk_exit ();
                }
              if (answer == GSC_META_RESTART)
                break;
              if (answer == GSC_META_UNDO)
                {
                  if (gsc_a5_try_undo (run))
                    {
                      resumed = 1;
                      break;
                    }
                  gsc_a5_put_string ("Sorry, no undo is available.\n");
                }
              if (answer == GSC_META_RESTORE)
                {
                  if (gsc_a5_try_restore (run))
                    {
                      resumed = 1;
                      break;
                    }
                }
            }
          if (!resumed)
            gsc_a5_restart_run (run);
        }

      /* If a "help" request was noted last turn, hint at "glk help". */
      gsc_output_provide_help_hint ();

#ifdef SPATTERLIGHT
      if (gsc_autorestored)
        /* The restored transcript already ends with the old prompt; skip
           printing another and just take input. */
        gsc_autorestored = FALSE;
      else
        {
          gsc_a5_put_prompt ("\n");
          /* Autosave at every top-level prompt: after the prompt is printed
             (so the GUI snapshot ends with it) but before input is
             requested (so the archived windows carry no pending request and
             a restore re-enters cleanly right here). */
          gsc_autosave ();
        }
#else
      gsc_a5_put_prompt ("\n");
#endif
      if (gsc_a5_read_line (input, sizeof input) == 0)
        continue;

      /* Handle "glk ..." command escapes and input logging. */
      if (gsc_a5_command_escape (input))
        continue;

      if (gsc_a5_match_command (input, "quit")
          || gsc_a5_match_command (input, "q"))
        break;

      if (gsc_a5_match_command (input, "restart"))
        {
          gsc_a5_restart_run (run);
          continue;
        }

      /* MAP toggles the map pane.  The runner's map is host chrome rather than
         a game command, so authors are free to use MAP themselves; when they
         do, theirs wins and the pane is reached with "glk map". */
      if (gsc_map != NULL && !gsc_map_taken
          && gsc_a5_match_command (input, "map"))
        {
          gsc_map_toggle ();
          continue;
        }

      if (gsc_a5_match_command (input, "save"))
        {
          gsc_a5_save (run);
          continue;
        }

      if (gsc_a5_match_command (input, "restore"))
        {
          gsc_a5_try_restore (run);
          continue;
        }

      if (gsc_a5_match_command (input, "undo"))
        {
          if (!gsc_a5_try_undo (run))
            {
              if (a5run_turns (run) == 0)
                gsc_a5_put_string ("You can't undo what hasn't been done.\n");
              else
                gsc_a5_put_string ("Sorry, no more undo is available.\n");
            }
          continue;
        }

      /* Push the pre-turn state onto the undo stack (before a5run_input,
         which increments the turn counter on entry). */
      a5run_snapshot (run);

      text = a5run_input (run, input);
      gsc_a5_display (text);
      free (text);
      gsc_a5_show_media (run);
      gsc_a5_status (run);
      gsc_map_redraw ();
      /* An ended game is handled at the top of the loop. */
    }

  /* Clear the run alias before freeing: `run` references gsc_a5_run, which a
     resize/redraw dispatched during teardown would otherwise dereference. */
  {
    a5_run_t *dead = run;
    run = NULL;
    a5run_free (dead);
  }
  gsc_a5_map_names_clear ();
  map_free (gsc_map);
  gsc_map = NULL;
  gsc_map_screen_drop ();
  glk_exit ();
}

/*
 * glk_main()
 *
 * Main entry point for Glk.  Here, all startup is done, and we call our
 * function to run the game, or to report errors if gsc_game_message is set.
 */
void
glk_main (void)
{
  assert (gsc_startup_called && !gsc_main_called);
  gsc_main_called = TRUE;

  /* ADRIFT 5 games run the dedicated a5 turn loop; everything else (ADRIFT
   * <=4) uses the scare engine. */
  if (gsc_is_a5)
    gsc_a5_main ();
  else
    gsc_main ();
}


/*---------------------------------------------------------------------*/
/*  Glk linkage relevant only to the UNIX platform                     */
/*---------------------------------------------------------------------*/
#ifdef TRUE

extern "C" {
#include "glkstart.h"
}

/*
 * Glk arguments for UNIX versions of the Glk interpreter.
 */
glkunix_argumentlist_t glkunix_arguments[] = {
  {(char *) "-nc", glkunix_arg_NoValue,
   (char *) "-nc        No local handling for Glk special commands"},
  {(char *) "-na", glkunix_arg_NoValue,
   (char *) "-na        Turn off abbreviation expansions"},
  {(char *) "-nu", glkunix_arg_NoValue,
   (char *) "-nu        Turn off any use of Unicode output"},
#ifdef GSC_HAVE_ZCOLORS
  {(char *) "-c", glkunix_arg_NoValue,
   (char *) "-c         Start in the game's own Adrift colours"},
#endif
#ifdef LINUX_GRAPHICS
  {(char *) "-ng", glkunix_arg_NoValue,
   (char *) "-ng        Turn off attempts at game graphics"},
#endif
  {(char *) "-r", glkunix_arg_ValueFollows,
   (char *) "-r FILE    Restore from FILE on starting the game"},
  {(char *) "", glkunix_arg_ValueCanFollow,
   (char *) "filename   game to run"},
  {NULL, glkunix_arg_End, NULL}
};


/*
 * glkunix_startup_code()
 *
 * Startup entry point for UNIX versions of Glk interpreter.  Glk will call
 * glkunix_startup_code() to pass in arguments.  On startup, parse arguments
 * and open a Glk stream to the game, then call the generic gsc_startup_code()
 * to build a game from the stream.  On error, set the message in
 * gsc_game_message; the core gsc_main() will report it when it's called.
 */
int
glkunix_startup_code (glkunix_startup_t * data)
{
  int argc = data->argc;
  scr_char **argv = data->argv;
  int argv_index;
  scr_char *restore_from;
  const scr_char *locale;
  strid_t game_stream, restore_stream;
  scr_uint trace_flags;
  scr_bool enable_debugger, stable_random;
  assert (!gsc_startup_called);
  gsc_startup_called = TRUE;

#ifdef GARGLK
  garglk_set_program_name("SCARIER " SCARIER_VERSION);
  garglk_set_program_info("SCARIER " SCARIER_VERSION
      " by Simon Baldwin and Mark J. Tilford");
#endif

  /* Handle command line arguments. */
  restore_from = NULL;
  for (argv_index = 1;
       argv_index < argc && argv[argv_index][0] == '-'; argv_index++)
    {
      if (strcmp (argv[argv_index], "-nc") == 0)
        {
          gsc_commands_enabled = FALSE;
          continue;
        }
      if (strcmp (argv[argv_index], "-na") == 0)
        {
          gsc_abbreviations_enabled = FALSE;
          continue;
        }
      if (strcmp (argv[argv_index], "-nu") == 0)
        {
          gsc_unicode_enabled = FALSE;
          continue;
        }
#ifdef GSC_HAVE_ZCOLORS
      /* Only offered where the Glk library has the zcolors extension, exactly
         as "glk colour" is -- accepting it elsewhere would promise a palette
         that could never arrive. */
      if (strcmp (argv[argv_index], "-c") == 0)
        {
          gsc_colour_startup = TRUE;
          continue;
        }
#endif
#ifdef LINUX_GRAPHICS
      if (strcmp (argv[argv_index], "-ng") == 0)
        {
          gsclinux_graphics_enabled = FALSE;
          continue;
        }
#endif
      if (strcmp (argv[argv_index], "-r") == 0)
        {
          restore_from = argv[++argv_index];
          continue;
        }
      return FALSE;
    }

  /* On invalid usage, set a complaint message and return. */
  if (argv_index != argc - 1)
    {
      gsc_game = NULL;
      if (argv_index < argc - 1)
        gsc_game_message = "More than one game file"
                           " was given on the command line.";
      else
        gsc_game_message = "No game file was given on the command line.";
      return TRUE;
    }

  /* Remember the game file path; the ADRIFT 5 loader (a5model_load) reads the
   * file directly by path rather than through a Glk stream. */
  snprintf (gsc_game_path, sizeof gsc_game_path, "%s", argv[argv_index]);

  /* Open a stream to the TAF file, complain if this fails. */
  game_stream = glkunix_stream_open_pathname (argv[argv_index], FALSE, 0);
  if (!game_stream)
    {
      gsc_game = NULL;
      gsc_game_message = "Unable to open the requested game file.";
      return TRUE;
    }
  else
    gsc_game_message = NULL;

  /*
   * If a restore requested, open a stream to the TAF (TAS) file, and
   * again, complain if this fails.
   */
  if (restore_from)
    {
      restore_stream = glkunix_stream_open_pathname (restore_from, FALSE, 0);
      if (!restore_stream)
        {
          glk_stream_close (game_stream, NULL);
          gsc_game = NULL;
          gsc_game_message = "Unable to open the requested restore file.";
          return TRUE;
        }
      else
        gsc_game_message = NULL;
    }
  else
    restore_stream = NULL;

  /* Set SCARIER trace flags and other general setup from the environment. */
  if (getenv ("SCR_TRACE_FLAGS"))
    trace_flags = strtoul (getenv ("SCR_TRACE_FLAGS"), NULL, 0);
  else
    trace_flags = 0;
  enable_debugger = (getenv ("SCR_DEBUGGER_ENABLED") != NULL);
#if defined (SPATTERLIGHT)
  stable_random = gli_determinism;
#else
  stable_random = (getenv ("SCR_STABLE_RANDOM_ENABLED") != NULL);
#endif
  locale = getenv ("SCR_LOCALE");

#ifdef LINUX_GRAPHICS
  /* Note the path to the game file for graphics extraction. */
  gsclinux_game_file = argv[argv_index];
#endif

#ifdef GLK_MODULE_GARGLK_FILE_RESOURCES
#ifdef _WIN32
    const char *sep = "/\\";
#else
    const char *sep = "/";
#endif
    const char *slash = find_last_of(argv[argv_index], sep);
    if (slash == NULL) {
      snprintf(gamefile, sizeof gamefile, "%s", argv[argv_index]);
    } else {
      snprintf(gamefile, sizeof gamefile, "%s", slash + 1);
  }
#endif

#ifdef GARGLK
  glkunix_set_base_file(argv[argv_index]);
#endif

  /* Use the generic startup code to complete startup. */
  return gsc_startup_code (game_stream, restore_stream, trace_flags,
                           enable_debugger, stable_random, locale);
}
#endif /* __unix */


/*---------------------------------------------------------------------*/
/*  Glk linkage relevant only to the Windows platform                  */
/*---------------------------------------------------------------------*/
#ifdef GARGLK
#undef _WIN32
#endif

#ifdef _WIN32

#include <windows.h>

#include "WinGlk.h"
#include "resource.h"

/* Windows constants and external definitions. */
static const unsigned int GSCWIN_GLK_INIT_VERSION = 0x601;
extern int InitGlk (unsigned int iVersion);

/*
 * WinMain()
 *
 * Entry point for all Glk applications.
 */
int WINAPI
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
         LPSTR lpCmdLine, int nCmdShow)
{
  /* Attempt to initialize both the Glk library and SCARIER. */
  if (!(InitGlk (GSCWIN_GLK_INIT_VERSION) && winglk_startup_code (lpCmdLine)))
    return 0;

  /* Run the application; no return from this routine. */
  glk_main ();
  glk_exit ();
  return 0;
}


/*
 * winglk_startup_code()
 *
 * Startup entry point for Windows versions of Glk interpreter.
 */
int
winglk_startup_code (const char *cmdline)
{
  const char *filename, *locale;
  frefid_t fileref;
  strid_t game_stream;
  scr_uint trace_flags;
  scr_bool enable_debugger, stable_random;
  assert (!gsc_startup_called);
  gsc_startup_called = TRUE;

  /* Set up application and window. */
  winglk_app_set_name ("Scarier");
  winglk_set_menu_name ("&Scarier");
  winglk_window_set_title ("Scarier Adrift Interpreter");
  winglk_set_about_text ("Windows Scarier " SCARIER_VERSION);
  winglk_set_gui (IDI_SCARIER);
  glk_stylehint_set (wintype_TextGrid, style_Normal, stylehint_ReverseColor, 1);

  /* Open a stream to the game. */
  filename = winglk_get_initial_filename (cmdline,
                             "Select an Adrift game to run",
                             "Adrift Files (.taf)|*.taf;All Files (*.*)|*.*||");
  if (!filename)
    return 0;

  fileref = winglk_fileref_create_by_name (fileusage_BinaryMode
                                           | fileusage_Data,
                                           (char *) filename, 0, 0);
  if (!fileref)
    return 0;

  game_stream = glk_stream_open_file (fileref, filemode_Read, 0);
  glk_fileref_destroy (fileref);
  if (!game_stream)
    return 0;

  /* Set trace, debugger, and portable random flags. */
  if (getenv ("SCR_TRACE_FLAGS"))
    trace_flags = strtoul (getenv ("SCR_TRACE_FLAGS"), NULL, 0);
  else
    trace_flags = 0;
  enable_debugger = (getenv ("SCR_DEBUGGER_ENABLED") != NULL);
  stable_random = (getenv ("SCR_STABLE_RANDOM_ENABLED") != NULL);
  locale = getenv ("SCR_LOCALE");

  /* Use the generic startup code to complete startup. */
  return gsc_startup_code (game_stream, NULL, trace_flags,
                           enable_debugger, stable_random, locale);
}
#endif /* _WIN32 */
