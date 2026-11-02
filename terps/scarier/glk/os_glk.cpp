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
 * os_glk.cpp: the Glk front end's module state, utilities, event and file
 * functions, startup and the ADRIFT <=4 main loop.
 * The rest of the front end is split by topic into the other os_glk*.cpp
 * files; os_glk_internal.h lists them.
 *
 * Module notes:
 *
 * o The Glk interface makes no effort to set text colours, background
 *   colours, and so forth, and minimal effort to set fonts and other style
 *   effects.
 */

#include "os_glk_internal.h"

#if defined (SPATTERLIGHT)
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
int
gsc_autorestore_wanted (void)
{
  return scarier_autosave_exists ();
}
/* Set after a successful autorestore: the restored transcript already ends
 * with the old prompt, so the next prompt print is skipped once. */
int gsc_autorestored = 0;
/* Set around the debugger's read; a debug prompt is mid-turn, not a state
 * worth autosaving. */
int gsc_in_debug_read = 0;
#endif

#ifdef GLK_MODULE_GARGLK_FILE_RESOURCES
/* The game's file name, without its directory: what garglk_add_resource_from_file
   takes to find a chunk of it (os_glk_resources.cpp). */
char gsc_gamefile[1024];
#endif



/*---------------------------------------------------------------------*/
/*  Module variables, miscellaneous other stuff                        */
/*---------------------------------------------------------------------*/

/* Two windows, one for the main text, and one for a status line. */
winid_t gsc_main_window = nullptr;
winid_t gsc_status_window = nullptr;

/* Whether nothing has been printed to the main window since it was last
   cleared.  A window that is still empty can be cleared again for free, which
   is the one moment a background colour change can repaint the pane without
   costing the player any text; see SCR_TAG_BGCOLOUR. */
int gsc_main_window_empty = TRUE;

/* ADRIFT <=4, set while a relaunch runs a turn (or the opening) again to get
   back to the <waitkey> pause the game was closed at: the text, pictures and
   sounds on the way there are already on the restored screen, so none of it
   is put out a second time.  The pause is the gsc_sc_waitkey_skip'th since
   the last line was read, gsc_sc_waitkey_count being how many there have
   been.  See gsc_sc_autosave_waitkey(). */
int gsc_sc_silent = FALSE;
int gsc_sc_waitkey_count = 0;
int gsc_sc_waitkey_skip = 0;

/*
 * Transcript stream and input log.  These are NULL if there is no current
 * collection of these strings.
 */
strid_t gsc_transcript_stream = nullptr;
strid_t gsc_inputlog_stream = nullptr;

/* Input read log stream, for reading back an input log. */
strid_t gsc_readlog_stream = nullptr;

/* Options that may be turned off or set by command line flags. */
int gsc_commands_enabled = TRUE;
int gsc_abbreviations_enabled = TRUE;
int gsc_unicode_enabled = TRUE;

/* ADRIFT <=4 Runner default palette; the ADRIFT 5 loop overwrites these from
   the adventure when it loads one.  See gsc_set_colour for where the numbers
   come from. */
glui32 gsc_colour_background = 0x000000;
glui32 gsc_colour_output = 0x00ff00;
glui32 gsc_colour_input = 0xff3232;

/* Whether the game's own palette is in force ("glk colour", set by
   gsc_set_colour in os_glk_commands.cpp).  The status line picks its style by
   it too; where the Glk library has no zcolors extension the command is not
   offered and this stays FALSE. */
int gsc_colour_enabled = FALSE;
/* Whether colour mode should be on before the game prints a word: the "-c"
   command line switch, sparing the player a "glk colour on" at every launch, or
   a game that asks for its own palette by the colours it was written in (see
   gsc_colour_detect).  Acted on once the windows exist; see
   gsc_colour_startup_apply. */
int gsc_colour_startup = FALSE;
/* True when Normal colour stylehints were already set before the first window
   open (the -c path, and the detected one).  gsc_set_colour consumes this so it
   can skip a pointless rebuild of the just-opened tree. */
int gsc_colour_hints_preapplied = FALSE;
/* The colour last named on the story window (gsc_colour_apply), so that drawing
   the status bar in its own two colours can put the story's back afterwards --
   a library whose zcolors are global as well as per-stream would otherwise go
   on painting the story window in the bar's colours.  See gsc_status_end.  Only
   read while colour mode is on, and gsc_set_colour colours the story window
   before it turns the mode on, so it is always set by then. */
glui32 gsc_colour_main_fg = 0;

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
int gsc_normal_measure_state = -1;
static glui32 gsc_normal_measured_fg, gsc_normal_measured_bg;

/*
 * gsc_normal_measure()
 *
 * Report the story window's style_Normal text and background colour as the
 * library measures them -- the game palette when colour mode's stylehints
 * are being honoured, the theme when they are ignored.  Answers from the
 * cache above when it is warm; FALSE when the library cannot measure.
 */
scr_bool
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
scr_bool
gsc_colour_visible (void)
{
  glui32 fg, bg;

  if (!gsc_colour_enabled)
    return FALSE;
  if (gsc_main_window == nullptr)
    return TRUE;

  /* A library that cannot measure leaves no way to tell; believe the
     colours are being honoured. */
  if (!gsc_normal_measure (&fg, &bg))
    return TRUE;
  return fg == gsc_colour_output && bg == gsc_colour_background;
}

/* Adrift game to interpret. */
scr_game gsc_game = nullptr;

/* ADRIFT 5 game.  When the file loaded at startup is an ADRIFT 5 game,
 * gsc_a5_adv holds the parsed adventure and gsc_is_a5 is set; glk_main then
 * runs the dedicated a5 turn loop (gsc_a5_main) instead of the scare engine.
 * The game data itself comes off the startup Glk stream (a5model_load_buffer);
 * gsc_game_path is the on-disk path, kept for the things that reopen the file
 * later -- the Blorb resource map (gsc_a5_init_resources) and the autosave
 * directory's file signature. */
char gsc_game_path[2048];
a5_adventure_t *gsc_a5_adv = nullptr;
int gsc_is_a5 = FALSE;

/* A name for this game to file per-game settings under, for the games that
   carry no IFID to use instead: ADRIFT 4 has none at all, and a few ADRIFT 5
   games were built without one.  It is derived from the game file's contents
   (gsc_hash_game_stream), so it follows the game when the file is moved or
   renamed, and two games never share it the way two files both called
   "adventure.taf" would.  Empty until the startup code has read the file. */
char gsc_game_key[40];

/* Current a5 run, kept here so status redraws on window resize can reach it. */
a5_run_t *gsc_a5_run = nullptr;

/* Set when this session drives the game's TimeBased events off a wall-clock
   1-second Glk timer (the real Runner's tmrEvents_Tick) instead of the
   engine's deterministic one-tick-per-input substitute.  Decided per run by
   gsc_a5_start_real_time. */
int gsc_a5_real_time = FALSE;

/* Set while the opening is replayed only to reach a saved state (the
   Spatterlight autorestore boot), so a %PopUpInput% naming prompt in it is
   answered with its default instead of asking the player again. */
int gsc_a5_popup_silent = FALSE;
int gsc_a5_popup_context = GSC_A5_POPUP_ELSEWHERE;
std::string gsc_a5_popup_command;
std::vector<std::string> gsc_a5_popup_answers;
/* The context an autorestore is playing back to a question, if any, with
   the command to run again and the answers still to be given. */
int gsc_a5_popup_replay = GSC_A5_POPUP_ELSEWHERE;
std::string gsc_a5_replay_command;
std::vector<std::string> gsc_a5_replay_answers;
/* How many <waitkey> pauses the display of the turn in hand (or of the
   opening) has reached, and how many of them an autorestore's replay passes
   in silence: a game closed at one comes back at it.  See
   gsc_a5_autosave_waitkey(). */
int gsc_a5_waitkey_count = 0;
int gsc_a5_waitkey_skip = 0;

/* Author-defined secondary output window (ADRIFT 5 <window NAME>), opened
   lazily as a right-hand text buffer the first time the game routes text to
   one, then kept open like the official Runner.  This build supports a single
   side window (games such as Alien Diver use exactly one, "Status"), so every
   <window NAME> shares it regardless of NAME. */
winid_t gsc_a5_side_window = nullptr;


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
void
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
  glk_cancel_line_event (gsc_main_window, nullptr);
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
void
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

void
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

void
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
void *
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
void *
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
void
gsc_short_delay (void)
{
#ifdef SPATTERLIGHT
  /* Honour Spatterlight's delays preference, as gsc_handle_wait_tag does. */
  if (!gli_sa_delays)
    return;
#endif
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
void
gsc_event_wait_2 (glui32 wait_type_1, glui32 wait_type_2, event_t * event)
{
  assert (event);

  do
    {
      glk_select (event);
      if (gsc_service_window_event (event))
        continue;

      switch (event->type)
        {
        case evtype_MouseInput:
          /* A click on a room starts a walk, and the cancelled line request
             ends the wait as a LineInput event.  Only where a line is what
             is being waited for, which is only the scare engine (the a5
             loop reads its lines through gsc_a5_await_line): the other
             waits of both -- a <waitkey>, a <wait>, a popup choice -- have
             no line request to cancel, and there the click can only work
             the map's pan and zoom buttons.  So too at the name and gender
             questions a game opens with: the line read there is an answer,
             which the first step of a walk would be taken for. */
          if (wait_type_1 == evtype_LineInput
              || wait_type_2 == evtype_LineInput)
            {
              if (!gsc_is_a5)
                {
                  if (run_startup_prompt () != 0)
                    gsc_map_click_view (event);
                  else
                    gsc_map_click (event);
                }
            }
          else
            gsc_map_click_view (event);
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

void
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

#ifdef SPATTERLIGHT
  gsc_sc_note_mid_turn_input ();
#endif
  fileref = glk_fileref_create_by_prompt (usage, fmode, 0);
  if (!fileref)
    return nullptr;

  if (!is_save && !glk_fileref_does_file_exist (fileref))
    {
      glk_fileref_destroy (fileref);
      return nullptr;
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

  glk_stream_close (stream, nullptr);
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
const char *gsc_game_message = nullptr;


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
 * Hardcoded per-game assist defaults.  A few catalogued ADRIFT games are
 * unwinnable, or have whole goal chains unreachable, in the faithful Runner
 * behaviour because of the exact authoring accidents the opt-in assists were
 * written for.  Each row below was measured on the headless harness against
 * the game's walkthrough, with and without the assist (2026-09-26):
 *
 *  - Combat assist.  Every character's Accuracy and Agility left at 0, so
 *    under the 4.0 accuracy>agility hit test no attack ever lands.
 *    The Town of Azra (a 3.9 game upgraded to the 4.0 format) and The tunnels
 *    of Athylon cannot be won; in Enigma Creature every fight is an endless
 *    exchange of dodges, though the tasks around them still finish the game.
 *  - Move assist.  Move tasks whose "To:" combo was left unset, which the
 *    Runner ignores.  To hell & beyond (with combat too): the player never
 *    leaves the mansion.  The X-Files: A New Beginning: the move summoning
 *    Dean to his diner, so he and his conversation never appear.  HYPER
 *    Battle System: the move bringing the Flare Rat into the Attack Menu
 *    (cosmetic only -- the fight is driven by variables, not presence).
 *  - Repeat assist.  Pre-4.0 games where a finished task blocks a command
 *    the game needs again.  In The Long Journey Home and Inverness Castle a
 *    spent catch-all task answers every later command, even "quit", with
 *    "You have already done that."  The Vampire with a Conscience and The
 *    Merry Murders wall at 70/100 and 120/135 on the same class of bug, but
 *    each is a single bad field, so the patch table repairs them instead and
 *    they are not listed below: one notice, and no session-wide change to
 *    spent-task handling.  After "glk patches off" their walls are back, and
 *    "glk assist repeat on" is then the way past them.
 *  - Room assist.  A task left set to run in no room at all.  Space Run's
 *    ending is behind one; in The Hangover it is the doctor taking the fries
 *    (5/7 without, 6/7 with -- a separate bug still blocks the last point).
 *  - Capacity recompute.  No row needs it since 2026-10-17.  Welcome to
 *    Wonderland did: the 4.0 Runner's running total phantom-weighs the
 *    characters' held objects, so the game's only weapon, the ethereal knife,
 *    could not be taken.  Scarier no longer ports that (the weigh routine
 *    counts only real children, a deliberate deviation), so the knife is
 *    takeable and the row runs with the combat assist alone.  The switch
 *    stays as a user preference.
 *
 * Not listed, although their walkthrough rows once carried an assist: games
 * where it measured no difference (g7056, Ghoster, Noximion; combat in Space
 * Run) and Goldilocks, whose route the capacity switch breaks.  True
 * 3.9/3.8-signature games (e.g. Villains and Kings) are deliberately not
 * listed either: their combat is repaired unconditionally by the engine's
 * legacy hit model.
 *
 * For these known games the matching assists default to on, applied at game
 * start; "glk assist <name> off" still turns each one off, and a one-line notice is
 * printed at startup (see gsc_main).
 *
 * Games are recognised by the TAF's GameName and GameAuthor, compared
 * case-insensitively, so every release of a game is covered (the two known
 * Town of Azra releases differ only in CompileDate).  Author strings are the
 * TAF's raw Windows-1252 bytes.
 */
enum
{
  GSC_ASSIST_COMBAT = 1 << 0,
  GSC_ASSIST_MOVE = 1 << 1,
  GSC_ASSIST_REPEAT = 1 << 2,
  GSC_ASSIST_ROOM = 1 << 3,
  GSC_ASSIST_CAPACITY = 1 << 4
};

typedef const struct
{
  const char * const game_name;    /* TAF GameName. */
  const char * const game_author;  /* TAF GameAuthor. */
  const int assists;               /* GSC_ASSIST_* to default on. */
  const char * const reason;       /* Startup notice: why assists are on. */
} gsc_game_assist_t;

static gsc_game_assist_t GSC_GAME_ASSIST_TABLE[] = {
  {"The Town of Azra", "S. P. Tencza", GSC_ASSIST_COMBAT,
   "This game's combat cannot be won as authored"},
  {"The tunnels of Athylon", "Anonymous", GSC_ASSIST_COMBAT,
   "This game's combat cannot be won as authored"},
  {"Enigma Creature", "Matthew Moya", GSC_ASSIST_COMBAT,
   "This game's combat cannot be won as authored"},
  {"To hell & beyond", "Steingr\xedmur J\xf3nsson",
   GSC_ASSIST_COMBAT | GSC_ASSIST_MOVE,
   "This game cannot be completed as authored"},
  {"The X-Files: A New Beginning", "Superbone Ali", GSC_ASSIST_MOVE,
   "A character in this game never appears as authored"},
  /* The GameName is the game's <wait>-animated title screen with the tags
     stripped, hence the run-together "1.1Copyright". */
  {"HYPER Battle System Version 1.1Copyright 2002 Seciden Mencarde",
   "Seciden Mencarde", GSC_ASSIST_MOVE,
   "A character in this game never appears as authored"},
  {"The Long Journey Home", "Danny Chabino", GSC_ASSIST_REPEAT,
   "This game cannot be completed as authored"},
  {"Inverness Castle", "David Good", GSC_ASSIST_REPEAT,
   "This game cannot be completed as authored"},
  {"Space Run", "Matthew Moya", GSC_ASSIST_ROOM,
   "This game cannot be completed as authored"},
  {"The Hangover", "Red Conine", GSC_ASSIST_ROOM,
   "Part of this game cannot be reached as authored"},
  {"Welcome to Wonderland", "The Cheshire Cat (Michael Suhar)",
   GSC_ASSIST_COMBAT, "This game cannot be completed as authored"},
  {nullptr, nullptr, 0, nullptr}
};

/* Each assist's name for the startup notice, the command that turns it back
   off, and its getter and setter, in the order the notice lists them. */
typedef const struct
{
  const int flag;
  const char * const name;
  const char * const off_command;
  scr_bool (*const get_state) (void);
  void (*const set_state) (scr_bool);
} gsc_assist_switch_t;

static gsc_assist_switch_t GSC_ASSIST_SWITCHES[] = {
  {GSC_ASSIST_COMBAT, "combat assist", "glk assist combat off",
   scr_get_combat_assist, scr_set_combat_assist},
  {GSC_ASSIST_MOVE, "move assist", "glk assist move off",
   scr_get_move_assist, scr_set_move_assist},
  {GSC_ASSIST_REPEAT, "repeat assist", "glk assist repeat off",
   scr_get_repeat_assist, scr_set_repeat_assist},
  {GSC_ASSIST_ROOM, "room assist", "glk assist room off",
   scr_get_room_assist, scr_set_room_assist},
  {GSC_ASSIST_CAPACITY, "carrying capacity recompute", "glk assist capacity off",
   gsc_get_capacity, gsc_set_capacity},
  {0, nullptr, nullptr, nullptr, nullptr}
};

/* Which assists were switched on automatically (GSC_ASSIST_* bits), and the
   matched table row's reason wording, for the startup notice. */
static int gsc_assists_auto = 0;
static const char *gsc_assist_auto_reason = nullptr;

static void
gsc_apply_known_game_assists (scr_game game)
{
  const char *name, *author;
  gsc_game_assist_t *entry;
  gsc_assist_switch_t *assist;

  name = scr_get_game_name (game);
  author = scr_get_game_author (game);
  if (!name || !author)
    return;

  for (entry = GSC_GAME_ASSIST_TABLE; entry->game_name; entry++)
    {
      if (scr_strcasecmp (name, entry->game_name) == 0
          && scr_strcasecmp (author, entry->game_author) == 0)
        {
          for (assist = GSC_ASSIST_SWITCHES; assist->flag; assist++)
            {
              if ((entry->assists & assist->flag) && !assist->get_state ())
                {
                  assist->set_state (TRUE);
                  gsc_assists_auto |= assist->flag;
                }
            }
          if (gsc_assists_auto)
            gsc_assist_auto_reason = entry->reason;
          break;
        }
    }
}


/*
 * gsc_print_auto_assists()
 *
 * Print a list of the automatically enabled assists, as "A", "A and B" or
 * "A, B and C", using each one's name or its off command.
 */
static void
gsc_print_auto_assists (scr_bool commands)
{
  gsc_assist_switch_t *assist;
  int remaining = 0, printed = 0;

  for (assist = GSC_ASSIST_SWITCHES; assist->flag; assist++)
    remaining += (gsc_assists_auto & assist->flag) != 0;

  for (assist = GSC_ASSIST_SWITCHES; assist->flag; assist++)
    {
      if (!(gsc_assists_auto & assist->flag))
        continue;
      if (printed > 0)
        gsc_normal_string (remaining == 1 ? " and " : ", ");
      if (commands)
        gsc_standout_string (assist->off_command);
      else
        gsc_normal_string (assist->name);
      printed++;
      remaining--;
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
 * gsc_open_loading_window()
 *
 * Open the temporary Glk main window that shows "Loading game..." while the
 * game file is read, and return it; NULL if there is none.
 */
static winid_t
gsc_open_loading_window (void)
{
  winid_t window = nullptr;

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

  return window;
}


/*
 * gsc_load_adrift5()
 *
 * ADRIFT 5 detection.  ADRIFT 5 games are zlib-compressed XML (optionally
 * Blorb-wrapped) and unrelated to the ADRIFT <=4 TAF format the scare engine
 * reads.  a5model_load_buffer returns NULL cleanly for a non-ADRIFT-5 file, so
 * we try it first from the already-open Glk stream; on success gsc_a5_adv is
 * set, and the caller sets up the dedicated a5 turn loop (gsc_a5_main) and
 * skips the scare path entirely.
 *
 * Loading from the Glk stream (rather than fopen of gsc_game_path) is required
 * for hosts like Emglken that open the story via Dialog with FILESYSTEM=0.
 */
static void
gsc_load_adrift5 (strid_t game_stream)
{
  glui32 file_len;
  char *file_buf;
  glui32 got;

  glk_stream_set_position (game_stream, 0, seekmode_End);
  file_len = glk_stream_get_position (game_stream);
  glk_stream_set_position (game_stream, 0, seekmode_Start);

  if (file_len > 0)
    {
      file_buf = (char *) malloc (file_len);
      if (file_buf != nullptr)
        {
          got = glk_get_buffer_stream (game_stream, file_buf, file_len);
          /* Rewind for the ADRIFT <=4 path if A5 load fails. */
          glk_stream_set_position (game_stream, 0, seekmode_Start);
          if (got == file_len)
            gsc_a5_adv = a5model_load_buffer ((uint8_t *) file_buf, file_len);
          else
            free (file_buf);
        }
    }
}


/*
 * gsc_start_adrift5()
 *
 * The rest of startup for a game that gsc_load_adrift5() recognised: take
 * the adventure's colours, and close the streams and the loading window.
 */
static void
gsc_start_adrift5 (strid_t game_stream, strid_t restore_stream,
                   winid_t window)
{
  gsc_is_a5 = TRUE;
  gsc_game = nullptr;
  gsc_game_message = nullptr;
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
  glk_stream_close (game_stream, nullptr);
  if (restore_stream)
    glk_stream_close (restore_stream, nullptr);
  if (window)
    glk_window_close (window, nullptr);
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
}


/*
 * gsc_start_adrift4()
 *
 * The rest of startup for an ADRIFT 4 or earlier game: create the game from
 * the TAF stream, restore it from restore_stream if there is one, then close
 * the streams and the loading window.  On failure gsc_game is NULL and
 * gsc_game_message says why, for gsc_main() to report.
 */
static void
gsc_start_adrift4 (strid_t game_stream, strid_t restore_stream,
                   scr_bool enable_debugger, winid_t window)
{
  /* Patching happens as the game is read, so the setting has to be in place
     before it is; "glk patches off" then applies from the next load on. */
  scr_set_game_patches (gsc_patches_enabled);

  gsc_game = scr_game_from_callback (gsc_callback, game_stream);
  if (!gsc_game)
    {
      gsc_game = nullptr;
      gsc_game_message = "Unable to load an Adrift game from the"
                         " requested file.";
    }
  else
    gsc_game_message = nullptr;
  glk_stream_close (game_stream, nullptr);

  /*
   * If the game was created successfully and there is a restore stream, try
   * to immediately restore the game from that stream.
   */
  if (gsc_game && restore_stream)
    {
      if (!scr_load_game_from_callback (gsc_game, gsc_callback, restore_stream))
        {
          scr_free_game (gsc_game);
          gsc_game = nullptr;
          gsc_game_message = "Unable to restore this Adrift game from the"
                             " requested file.";
        }
      else
        gsc_game_message = nullptr;
    }
  if (restore_stream)
    glk_stream_close (restore_stream, nullptr);

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
    glk_window_close (window, nullptr);

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
}


/*
 * gsc_startup_code()
 * gsc_main
 *
 * Together, these functions take the place of the original main().  The
 * first one is called from the platform-specific startup_code()
 * (os_glk_platform.cpp), to parse and generally handle options.  The second
 * is called from glk_main, and does the real work of running the game.
 */
int
gsc_startup_code (strid_t game_stream, strid_t restore_stream,
                  scr_uint trace_flags, scr_bool enable_debugger,
                  scr_bool stable_random, const scr_char *locale)
{
  winid_t window;
  assert (game_stream);

  window = gsc_open_loading_window ();

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
   * Set tracing flags, then try to create a Scarier game reference from the
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
      scr_reseed_random_sequence (scr_default_random_seed ());
    }

  /* Name the game file, while the stream is still open and at its start: both
     loaders below want the file from the beginning, and only one of them (the
     ADRIFT 5 one) finds an IFID inside it. */
  gsc_hash_game_stream (game_stream);

  gsc_load_adrift5 (game_stream);
  if (gsc_a5_adv)
    gsc_start_adrift5 (game_stream, restore_stream, window);
  else
    gsc_start_adrift4 (game_stream, restore_stream, enable_debugger, window);

  /* Game set up, perhaps successfully. */
  return TRUE;
}

/*
 * gsc_main_open_windows()
 *
 * Create the Glk windows, and set the main one's stream as the current one.
 * Does not return if the game failed to load: the complaint about that is
 * printed here, in the window just opened.
 */
static void
gsc_main_open_windows (void)
{
  /* Starting in colour ("-c", or a game that needs its palette to be
     read) publishes its colours before the first open. */
  gsc_colour_startup_prepare ();
  gsc_open_main_window ();

  /* If there's a problem with the game file, complain now. */
  if (!gsc_game)
    {
      assert (gsc_game_message);
      gsc_header_string ("Glk Scarier Error\n\n");
      gsc_normal_string (gsc_game_message);
      gsc_normal_char ('\n');
      gsc_report_fatal (FALSE);
      glk_exit ();
    }

  gsc_open_status_window ();

  /* Into the game's palette before a word is printed. */
  gsc_colour_startup_apply ();
}


/*
 * gsc_main_startup_notes()
 *
 * Tell the player what the front end has changed about this game before it
 * starts: a corrected data bug, and any assists switched on for it.
 */
static void
gsc_main_startup_notes (void)
{
  /* Say so if this game was one the engine's patch table corrects, and how to
     play it as published instead. */
  if (scr_get_applied_game_patch ())
    {
      gsc_normal_string ("[A bug in this game's own data has been corrected: ");
      gsc_normal_string (scr_get_applied_game_patch ());
      gsc_normal_string (".  Type ");
      gsc_standout_string ("glk patches off");
      gsc_normal_string (" and reload the game to play it exactly as its"
                         " author left it.]\n\n");
    }

  /* Mention any assists switched on automatically for this known game (see
     gsc_apply_known_game_assists), and how to get faithful behaviour back. */
  if (gsc_assists_auto)
    {
      const scr_bool several = (gsc_assists_auto & (gsc_assists_auto - 1)) != 0;

      gsc_normal_char ('[');
      gsc_normal_string (gsc_assist_auto_reason
                         ? gsc_assist_auto_reason
                         : "This game cannot be completed as authored");
      gsc_normal_string (", so ");
      gsc_print_auto_assists (FALSE);
      gsc_normal_string (several ? " have" : " has");
      gsc_normal_string (" been enabled.  Type ");
      gsc_print_auto_assists (TRUE);
      gsc_normal_string (" to restore the original ADRIFT Runner"
                         " behaviour.]\n\n");
    }
}


#ifdef SPATTERLIGHT
/*
 * gsc_main_autorestore()
 *
 * When a Spatterlight autosave exists, replace the whole state -- engine
 * and Glk library both -- with the saved one before entering the
 * interpreter loop.  The game was already created at startup; loading the
 * saved state marks the player's room seen, so run_main_loop skips the
 * intro and drops straight to the command prompt, where os_read_line
 * skips one prompt print (the restored transcript already ends with it).
 * The app restores the window contents from its own GUI snapshot; no
 * window has been opened by gsc_main, so the restored library supplies them.
 */
static void
gsc_main_autorestore (void)
{
  gsc_autorestore_replace_state (gsc_sc_apply_all);
  if (gsc_sc_boot_replay ())
    {
      /* Closed at a pause in the opening: there is no state to have
         loaded, and the game starts over without showing anything until
         it is back there. */
      glk_set_window (gsc_main_window);
      glk_set_style (style_Normal);
      gsc_sc_silent = TRUE;
      return;
    }
  /* The app resumes any interrupted sound and restores the graphics
     window pixels itself; the engine must not replay them. */
  scr_note_resources_synced (gsc_game);
  /* ...and the blank line the engine prints before every prompt is in
     the restored transcript too (the autosave was taken between it and
     the prompt), so skip that one reprint as well. */
  scr_note_autorestored ();
#ifdef GSC_HAVE_TITLE_WINDOW
  /* An autosave from before title screens waited for their own key can
     hold a cover pane open over the story; the game is past its intro
     by now, so put the story window back on its own. */
  gsc_close_title_graphic ();
  gsc_seen_input = TRUE;
#endif
  glk_set_window (gsc_main_window);
  glk_set_style (style_Normal);
  gsc_autorestored = TRUE;
}
#endif


/*
 * gsc_main_game_over()
 *
 * The game has completed: get the user's selection of restart, undo a turn,
 * or quit, and act on it.  Returns FALSE if the game is not to be run again.
 */
static scr_bool
gsc_main_game_over (void)
{
  /*
   * If reading from an input log, close it now.  We need to request a
   * user selection, probably modal, and after that we probably don't
   * want the follow-on readlog data being used as game input.
   */
  if (gsc_readlog_stream)
    {
      glk_stream_close (gsc_readlog_stream, nullptr);
      gsc_readlog_stream = nullptr;
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
      return FALSE;
    }

  /* A restart or undo that ended in a fatal engine error leaves no game. */
  if (gsc_report_fatal (TRUE))
    return FALSE;

  return TRUE;
}


/*
 * gsc_main_shutdown()
 *
 * All done -- release game resources, and close any open transcript, input
 * log, and/or read log.
 */
static void
gsc_main_shutdown (void)
{
  map_free (gsc_map);
  gsc_map = nullptr;
  gsc_map_screen_drop ();
  scr_free_game (gsc_game);

  if (gsc_transcript_stream)
    {
      glk_stream_close (gsc_transcript_stream, nullptr);
      gsc_transcript_stream = nullptr;
    }
  if (gsc_inputlog_stream)
    {
      glk_stream_close (gsc_inputlog_stream, nullptr);
      gsc_inputlog_stream = nullptr;
    }
  if (gsc_readlog_stream)
    {
      glk_stream_close (gsc_readlog_stream, nullptr);
      gsc_readlog_stream = nullptr;
    }
}


void
gsc_main (void)
{
  scr_bool is_running;
  int autorestore = FALSE;

#ifdef SPATTERLIGHT
  /* A failed game load has no state to restore onto, and needs a window to
     report itself in -- take the normal path and print the complaint. */
  autorestore = gsc_game != nullptr && gsc_autorestore_wanted ();
#endif

  /* Ensure Scarier internal types have the right sizes. */
  if (!(sizeof (scr_byte) == 1 && sizeof (scr_char) == 1
        && sizeof (scr_uint) >= 4 && sizeof (scr_int) >= 4
        && sizeof (scr_uint) <= 8 && sizeof (scr_int) <= 8))
    {
      gsc_fatal ("GLK: Types sized incorrectly, recompilation is needed");
      glk_exit ();
    }

  gsc_hint_window_styles ();

  /* An autorestore adopts the archived windows below instead: opening any
     here would cost the player the host's restored ones (see
     gsc_autorestore_wanted). */
  if (!autorestore)
    gsc_main_open_windows ();

  /* Does the game define a MAP verb of its own?  If so it keeps it, and the
     map pane is reached with "glk map" instead. */
  gsc_map_taken = scmap_command_taken ((scr_gameref_t) gsc_game);

  /* Not on an autorestore: the notes are already in the restored transcript,
     and there is no window to print them to yet. */
  if (!autorestore)
    gsc_main_startup_notes ();

#ifdef SPATTERLIGHT
  if (autorestore)
    gsc_main_autorestore ();
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

      /* Stopped by a fatal engine error: say so, and leave. */
      if (gsc_report_fatal (TRUE))
        break;

      /*
       * If the game did not complete, the user quit explicitly, so leave the
       * game repeat loop.
       */
      if (!scr_has_game_completed (gsc_game))
        break;

      is_running = gsc_main_game_over ();
    }

  gsc_main_shutdown ();
}
