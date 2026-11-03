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
 * os_glk_a5.cpp: the ADRIFT 5 Glk driver -- output, line input, popups,
 * save/restore, the Blorb media map, undo, and the gsc_a5_main turn loop.
 * Split out of os_glk.cpp; see os_glk_internal.h.
 */

#include "os_glk_internal.h"

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
void
gsc_a5_put_string (const char *string)
{
  const unsigned char *p = (const unsigned char *) string;

  if (string == nullptr)
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
 * removed.  (Same retract the Question frontends use; see
 * questglk-common.cc unput_window_tail.)
 */
static int
gsc_unput_tail (const char *s)
{
  glui32 ubuf[16];
  size_t length = strlen (s), index_;
  strid_t saved;
  glui32 got;

  if (length == 0 || length >= sizeof ubuf / sizeof *ubuf
      || gsc_main_window == nullptr)
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

  if (text == nullptr)
    return TRUE;
  for (p = text; *p != '\0'; p++)
    {
      if (*p == A5_SOUND_MARK || *p == A5_WAIT_MARK)
        {
          const char *e = strchr (p + 1, *p);
          if (e == nullptr)
            return FALSE;
          p = e;
        }
      else if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
        return FALSE;
    }
  return TRUE;
}

/* Whether the line being awaited is a command, which a click on a room of
   the map may replace with a walk: set by gsc_a5_read_line around its read. */
static int gsc_a5_line_walkable = FALSE;

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
      if (gsc_service_window_event (event))
        continue;
      switch (event->type)
        {
        case evtype_MouseInput:
          /* A click on a room walks the player there: the pending line
             request is cancelled, and gsc_a5_read_line issues the first step
             instead.  Only at a prompt that takes commands: a line read for
             a popup question is an answer, which a walk could neither give
             nor follow, so there the click only works the map's pan and zoom
             buttons. */
          if (!gsc_a5_line_walkable)
            gsc_map_click_view (event);
          else if (gsc_map_click (event))
            return FALSE;
          break;

        case evtype_Timer:
          if (gsc_a5_real_time && gsc_a5_run != nullptr)
            {
              char *text = a5run_time_tick (gsc_a5_run);

              if (text == nullptr)
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
                if (ubuf != nullptr)
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

/* Whether the line just read is echoed by us rather than by the library:
   set by every gsc_a5_read_line_raw. */
static int gsc_a5_manual_echo = FALSE;

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
      glui32 chars = gsc_readlog_line (buf, (glui32) bufsize);

      if (chars > 0)
        {
          while (chars > 0 && (buf[chars - 1] == '\n' || buf[chars - 1] == '\r'))
            chars--;
          buf[chars] = '\0';

          /* This path returns without ever reaching the gsc_colour_echo pair
             below; the echo takes the prompt's input colour off itself. */
          gsc_echo_input (gsc_a5_put_string, buf, TRUE);
          return (int) chars;
        }
    }

  /* Take over input echo wherever the library lets us: the pending request
     can be cancelled, by a TimeBased tick that has something to print or by
     a click on the map that starts a walk, and with auto-echo every cancel
     would commit a spurious input line to the window, ending the prompt's
     line.  The completed command is echoed manually below instead.

     Set it every time.  Echo mode is per-window library state that a
     Spatterlight autosave archives and re-applies (TempWindow), and an
     autosave from before the echo was always ours may hold it switched on.

     A library without line echo control (cheapglk) has no echo state to set,
     and warns about every call.  There the library echoes, except in
     real-time mode, which has always echoed for itself. */
  gsc_a5_manual_echo = gsc_a5_real_time;
#ifdef GLK_MODULE_LINE_ECHO
  if (glk_gestalt (gestalt_LineInputEcho, 0))
    {
      glk_set_echo_line_event (gsc_main_window, 0);
      gsc_a5_manual_echo = TRUE;
    }
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
      done = gsc_a5_await_line (&event, nullptr, 0, unicode, cap);

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
      done = gsc_a5_await_line (&event, buf, bufsize, nullptr, 0);
      n = done ? (int) event.val1 : 0;
    }

  buf[n] = '\0';
  if (gsc_a5_manual_echo && done)
    /* Echo the completed command, as Glk's auto-echo would have. */
    gsc_echo_input (gsc_a5_put_string, buf, TRUE);
  else
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
              /* Echo the step as though the player had typed it.  The first
                 step of a walk follows a line request the click cancelled,
                 which took the prompt's input colour off again; and where
                 the library echoed that cancelled line, the prompt's line
                 has ended too, so the step gets a prompt of its own. */
              if (gsc_a5_walk_clicked)
                {
                  if (gsc_a5_manual_echo)
                    gsc_colour_echo (TRUE);
                  else
                    gsc_a5_put_prompt ("");
                  gsc_a5_walk_clicked = FALSE;
                }
              gsc_echo_input (gsc_a5_put_string, buf, TRUE);
              return (int) strlen (buf);
            }
          gsc_a5_walk_stop ();          /* arrived, blocked, or no route */
        }

      gsc_a5_walk_clicked = FALSE;
      {
        int n;

        gsc_a5_line_walkable = TRUE;
        n = gsc_a5_read_line_raw (buf, bufsize);
        gsc_a5_line_walkable = FALSE;

        /* A click cancelled the request: loop round and walk instead. */
        if (gsc_a5_walk_clicked)
          continue;
        return n;
      }
    }
}

/*
 * gsc_a5_popup_status()
 *
 * Bring the status line up to date before a popup blocks for its answer, as
 * a <waitkey> does: the question is asked mid-turn, and the window would
 * otherwise show the room and score from the last prompt until it is
 * answered.  gsc_a5_status leaves the main window selected, so the stream the
 * popup is about to print to is put back.
 */
static void
gsc_a5_popup_status (void)
{
  strid_t current;

  if (gsc_a5_run == nullptr)
    return;
  current = glk_stream_get_current ();
  gsc_a5_status (gsc_a5_run);
  if (current != nullptr)
    glk_stream_set_current (current);
}


/*
 * gsc_a5_popup_replayed()
 * gsc_a5_popup_answered()
 *
 * A game closed at a popup question comes back at it: the autosave written
 * there (gsc_a5_autosave_popup) holds the state the turn started from, the
 * command, and the answers its earlier questions were given, and the
 * relaunch runs the command again.  Nothing of the turn has been printed
 * yet -- its text is displayed once the engine returns -- so all there is to
 * keep quiet is the questions already answered.  gsc_a5_popup_replayed hands
 * back the next recorded answer; when none is left the replay is over, and
 * the question being asked is the one on screen, which *resumed tells the
 * caller not to print again.
 */
static int
gsc_a5_popup_replayed (std::string *answer, int *resumed)
{
  *resumed = FALSE;
  if (gsc_a5_popup_replay == GSC_A5_POPUP_ELSEWHERE)
    return FALSE;
  if (gsc_a5_replay_answers.empty ())
    {
      gsc_a5_popup_replay = GSC_A5_POPUP_ELSEWHERE;
      *resumed = TRUE;
      return FALSE;
    }
  *answer = gsc_a5_replay_answers.front ();
  gsc_a5_replay_answers.erase (gsc_a5_replay_answers.begin ());
  gsc_a5_popup_answers.push_back (*answer);
  return TRUE;
}

static void
gsc_a5_popup_answered (const std::string &answer)
{
  gsc_a5_popup_answers.push_back (answer);
}

/* Save the game at the question just printed. */
static void
gsc_a5_popup_autosave (void)
{
#ifdef SPATTERLIGHT
  gsc_a5_autosave_popup ();
#endif
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
  int saved_real_time, n, resumed;
  std::string replayed;

  /* No window to ask in, or a silent boot (the autorestore below replays the
     intro only to reach the saved state, whose name the player already
     chose): take the default without troubling anyone. */
  if (gsc_main_window == nullptr || gsc_a5_popup_silent)
    return nullptr;

  /* An answer is recorded as "=" and the text typed, the default as "". */
  if (gsc_a5_popup_replayed (&replayed, &resumed))
    return replayed.empty ()
           ? nullptr : gsc_copy_string (replayed.c_str () + 1);

  gsc_a5_popup_status ();

  if (!resumed)
    {
      gsc_a5_put_string ("\n");
      if (prompt != nullptr && prompt[0] != '\0')
        gsc_a5_put_string (prompt);
      if (dflt != nullptr && dflt[0] != '\0')
        {
          /* The InputBox arrives with the default already filled in; say
             what answering with an empty line will give. */
          gsc_a5_put_string (" [");
          gsc_a5_put_string (dflt);
          gsc_a5_put_string ("]");
        }
      gsc_a5_put_prompt ("\n");
      gsc_a5_popup_autosave ();
    }

  /* This runs inside the engine (mid text-render), so a TimeBased tick must
     not re-enter it: hold real-time mode off for the duration. */
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

  gsc_a5_popup_answered (n > 0 ? std::string ("=") + input : std::string ());
  return n > 0 ? gsc_copy_string (input) : nullptr;
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
  int saved_real_time, picked, resumed;
  std::string replayed;

  /* No window to ask in, or a silent boot (the autorestore below replays the
     opening only to reach the saved state, whose answer the player already
     gave): leave the question unasked, as an unattended Runner does. */
  if (gsc_main_window == nullptr || gsc_a5_popup_silent)
    return -1;

  /* Recorded as "1" for the first choice and "0" for the second. */
  if (gsc_a5_popup_replayed (&replayed, &resumed))
    return replayed != "0";

  gsc_a5_popup_status ();

  /* This runs inside the engine (mid text-render), so a TimeBased tick must
     not re-enter it: hold real-time mode off for the duration. */
  saved_real_time = gsc_a5_real_time;
  gsc_a5_real_time = FALSE;

  for (;;)
    {
      int n;

      if (resumed)
        resumed = FALSE;
      else
        {
          gsc_a5_put_string ("\n");
          if (prompt != nullptr && prompt[0] != '\0')
            gsc_a5_put_string (prompt);
          gsc_a5_put_string (" [yes = ");
          gsc_a5_put_string (choice1);
          gsc_a5_put_string (" / no = ");
          gsc_a5_put_string (choice2);
          gsc_a5_put_string ("]\n> ");
          gsc_a5_popup_autosave ();
        }

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
  gsc_a5_popup_answered (picked ? "1" : "0");
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
  glk_stream_close (stream, nullptr);
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
  glk_stream_close (stream, nullptr);

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
/*  glk_image_draw / glk_schannel_play work by resource number.  On    */
/*  Spatterlight, media files beside the game are loaded under numbers */
/*  of their own (gsc_a5_resolve_media).                               */
/*---------------------------------------------------------------------*/

int gsc_a5_graphics_ok = FALSE;
int gsc_a5_sound_ok = FALSE;
/* Whether the game is a Blorb registered as the resource map. */
static int gsc_a5_have_blorb = FALSE;

/* One Glk sound channel per ADRIFT audio channel.  The Runner has exactly 8
   (clsSound.vb: Channels(7), numbered 1..8 in the <audio> tag; anything out of
   that range is a no-op); slot 0 here is simply never used. */
schanid_t gsc_a5_channels[GSC_A5_MAX_CHANNELS];
/* The resource playing on each channel, so a repeated play of the same sound
   leaves it alone rather than restarting it (see gsc_a5_show_media).  It is
   cleared when the sound finishes (see gsc_a5_sound_finished). */
glui32 gsc_a5_chan_sound[GSC_A5_MAX_CHANNELS];

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
      if (gsc_a5_channels[ch] != nullptr)
        glk_schannel_stop (gsc_a5_channels[ch]);
      gsc_a5_chan_sound[ch] = 0;
    }
}

/* Rock of the stream behind the resource map, so that an autorestore can
 * tell it from the game's other streams (see gsc_a5_rebind_resources). */
#define GSC_A5_BLORB_STREAM_ROCK 0x424c5242 /* 'BLRB' */

/*
 * gsc_a5_init_resources()
 *
 * Probe for graphics/sound support and register the game Blorb as the Glk
 * resource map, so image/sound resources can be addressed by Blorb number.
 *
 * The game is reopened here by path rather than kept from startup, because
 * giblorb_set_resource_map() takes ownership of the stream it is given and the
 * startup stream has other work to do first.  Reopening through Glk (not
 * fopen) is what keeps this working on hosts with no C library filesystem,
 * such as Emglken, whose Glk implements glkunix_stream_open_pathname over its
 * own VFS and permits it outside glkunix_startup_code.  How a path becomes
 * a stream on each host is os_glk_platform.cpp's business.
 */
#ifdef SPATTERLIGHT
/*
 * gsc_a5_resolve_media()
 *
 * The engine's media resolver (a5run_set_media_resolver).  A src the Blorb
 * holds keeps its <FileMappings> number.  Anything else -- every src of a raw
 * .taf, or one a Blorb names but never bundled -- is looked for as a file
 * beside the game, by its last path component, the way os_play_sound finds
 * an ADRIFT 4 game's unembedded sounds; the Runner itself opens the author's
 * full path, which exists only on the author's machine.
 */
static int
gsc_a5_resolve_media (void * /* ctx */, const char *src, int is_image,
                      int mapped)
{
  glui32 id;

  if (mapped > 0 && gsc_a5_have_blorb)
    return mapped;
  if (is_image ? !gsc_a5_graphics_ok : !gsc_a5_sound_ok)
    return mapped;
  id = gsc_load_external_resource (src, !is_image);
  return id != 0 ? (int) id : mapped;
}
#endif

static void
gsc_a5_init_resources (void)
{
  strid_t stream;

  gsc_a5_graphics_ok = glk_gestalt (gestalt_Graphics, 0) != 0;
  gsc_a5_sound_ok = glk_gestalt (gestalt_Sound, 0) != 0;
  gsc_a5_have_blorb = FALSE;
#ifdef SPATTERLIGHT
  /* Media files beside the game are usable with or without a Blorb, so
     losing the resource map below leaves graphics and sound on here. */
  a5run_set_media_resolver (gsc_a5_resolve_media, nullptr);
#endif
  if ((!gsc_a5_graphics_ok && !gsc_a5_sound_ok) || gsc_game_path[0] == '\0')
    return;

  stream = gsc_open_game_path (gsc_game_path, GSC_A5_BLORB_STREAM_ROCK);
  if (stream == nullptr)
    {
      /* The file is unreachable by path (a host that cannot reopen it, or a
         game that has moved since startup): no resource map, so no media. */
#ifndef SPATTERLIGHT
      gsc_a5_graphics_ok = gsc_a5_sound_ok = FALSE;
#endif
      return;
    }
  if (giblorb_set_resource_map (stream) != giblorb_err_None)
    {
      /* Not a Blorb (e.g. a raw .taf with no resources): no media. */
      glk_stream_close (stream, nullptr);
#ifndef SPATTERLIGHT
      gsc_a5_graphics_ok = gsc_a5_sound_ok = FALSE;
#endif
      return;
    }
  gsc_a5_have_blorb = TRUE;
}

#ifdef SPATTERLIGHT
/*
 * gsc_a5_rebind_resources()
 *
 * Register the resource map again after an autorestore.  Replacing the Glk
 * library closes every stream there was, the one the map reads the Blorb
 * through included, and the map is not part of what the library restores: it
 * would go on reading through the freed stream, at the first picture or sound
 * the app has not already cached.
 *
 * The restored library brings back its own copy of that stream, opened on
 * wherever the game was when it was saved.  That one is closed rather than
 * adopted -- the game may have moved since -- and the file opened afresh, so
 * exactly one such stream is carried from one autosave to the next.
 */
static void
gsc_a5_rebind_resources (void)
{
  strid_t stream, next;

  if (!gsc_a5_have_blorb)
    return;
  gsc_a5_have_blorb = FALSE;
  if (giblorb_unset_resource_map () != giblorb_err_None)
    return;

  for (stream = glk_stream_iterate (nullptr, nullptr); stream != nullptr;
       stream = next)
    {
      next = glk_stream_iterate (stream, nullptr);
      if (glk_stream_get_rock (stream) == GSC_A5_BLORB_STREAM_ROCK)
        glk_stream_close (stream, nullptr);
    }

  stream = gsc_open_game_path (gsc_game_path, GSC_A5_BLORB_STREAM_ROCK);
  if (stream == nullptr)
    return;
  if (giblorb_set_resource_map (stream) != giblorb_err_None)
    {
      glk_stream_close (stream, nullptr);
      return;
    }
  gsc_a5_have_blorb = TRUE;
}
#endif


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

/*
 * gsc_a5_undo_command()
 *
 * UNDO at the prompt, however it was asked for: take the turn back, or say
 * why not.
 */
static void
gsc_a5_undo_command (a5_run_t *run)
{
  if (!gsc_a5_try_undo (run))
    gsc_a5_put_string (gsc_undo_refusal (a5run_turns (run)));
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
  char *text;

  gsc_a5_waitkey_count = 0;
  text = a5run_intro (run);
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
      gsc_a5_undo_command (run);
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
 * gsc_a5_main_setup()
 *
 * Everything gsc_a5_main does before the opening is played: the windows
 * (unless an autorestore is about to supply them), the engine's presentation
 * hooks, the game's resources, the run itself and the map.  Does not return
 * if the run cannot be allocated.
 */
static void
gsc_a5_main_setup (a5_run_t *&run, int autorestore)
{
  gsc_hint_window_styles ();

  /* An autorestore adopts the archived windows further down instead: opening
     any here would cost the player the host's restored ones (see
     gsc_autorestore_wanted). */
  if (!autorestore)
    {
      /* Starting in colour ("-c", or an adventure that needs its palette to
         be read) publishes its colours before the first open. */
      gsc_colour_startup_prepare ();
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
  a5text_set_popup_cb (gsc_a5_popup_input, nullptr);
  a5text_set_popup_choice_cb (gsc_a5_popup_choice, nullptr);

  /* Register the game Blorb for image/sound resources. */
  gsc_a5_init_resources ();

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
  if (gsc_map == nullptr)
    {
      gsc_map = a5map_load (gsc_a5_adv);
      gsc_map_taken = a5map_command_taken (gsc_a5_adv);
    }
}


/*
 * gsc_a5_main_opening()
 *
 * Bring the new run to its first prompt: play the opening, or -- on a
 * Spatterlight autorestore -- put the saved state in its place.  Does not
 * return for an adventure the runner refuses outright.
 */
static void
gsc_a5_main_opening (a5_run_t *&run, int autorestore)
{
#ifdef SPATTERLIGHT
  /* When a Spatterlight autosave exists, boot the world silently to where a
     manual RESTORE would find it (the intro's task and RNG side effects
     included, its text discarded), then replace the whole state -- engine
     and Glk library both -- with the saved one.  The app restores the
     window contents from its own GUI snapshot; the loop below skips one
     prompt print (the restored transcript already ends with it).  No window
     has been opened yet -- the restored library supplies them all. */
  if (autorestore && gsc_a5_autosave_at_boot_popup ())
    {
      /* Closed at a question the opening itself asks, before any of it was
         shown: there is no state to load, only the windows and the answers
         given so far (which gsc_a5_apply_all takes), so play the opening
         for real.  It stops at the question already on screen. */
      gsc_autorestore_replace_state (gsc_a5_apply_all);
      gsc_a5_rebind_resources ();
      glk_set_window (gsc_main_window);
      glk_set_style (style_Normal);
      gsc_a5_start_real_time (run);
      gsc_a5_popup_context = GSC_A5_POPUP_BOOT;
      gsc_a5_present_intro (run);
      gsc_a5_popup_context = GSC_A5_POPUP_ELSEWHERE;
      gsc_a5_popup_replay = GSC_A5_POPUP_ELSEWHERE;
      gsc_map_auto_reveal ();
    }
  else if (autorestore)
    {
      char *text;

      gsc_a5_popup_silent = TRUE;
      text = a5run_intro (run);
      gsc_a5_popup_silent = FALSE;
      free (text);
      /* The silent boot above already consumed the game's intro, so a bad
         autosave restarts in a fresh process rather than continue from a
         polluted state. */
      gsc_autorestore_replace_state (gsc_a5_apply_all);
      gsc_a5_rebind_resources ();
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
#else
  (void) autorestore;
#endif
  /* Only this first opening can be closed at one of its questions and come
     back: a relaunch reaches it again by starting the game, which is not
     where a RESTART's opening starts from. */
  gsc_a5_popup_answers.clear ();
  gsc_a5_popup_context = GSC_A5_POPUP_BOOT;
  gsc_a5_present_intro (run);
  gsc_a5_popup_context = GSC_A5_POPUP_ELSEWHERE;
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
    if (load_err != nullptr)
      {
        gsc_a5_put_string ("\n");
        gsc_a5_put_string (load_err);
        gsc_a5_put_string ("\n");
        glk_exit ();
      }
  }
}


/*
 * gsc_a5_game_over()
 *
 * The game has ended -- by the last command, or by a real-time TimeBased tick
 * that fired while awaiting input.  The engine has already emitted the
 * win/lose/score block (whose banner offers restart / restore / quit / undo);
 * honour all four here.  UNDO and RESTORE revert to a running state and
 * resume play; RESTART rebuilds the game; QUIT does not return.
 */
static void
gsc_a5_game_over (a5_run_t *&run)
{
  char input[1024];
  int resumed = 0;

  /* A step of a map walk may have ended the game; the walk must not then
     answer the restart/restore/undo/quit prompt for us. */
  gsc_a5_walk_stop ();

  for (;;)
    {
      /* The banner's four answers, however they were asked for: typed as
         themselves, or reached with "glk ..." (which is how they are still
         available to a game that has taken the words for its own).  Anything
         else is ignored, and the banner asks again.  This does not go
         through gsc_a5_meta_perform: here whether UNDO or RESTORE worked
         decides whether the banner is left, QUIT has no turn loop to return
         to, and a failed UNDO is answered as gsc_main's banner answers it,
         not with the mid-game gsc_undo_refusal. */
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


/*
 * gsc_a5_next_command()
 *
 * Print the prompt (and autosave at it) and take the next command into
 * `input`, from the player or from the turn an autorestore is resuming.
 * Returns FALSE if there is nothing to act on and the loop should come round
 * again.
 */
static int
gsc_a5_next_command (char *input, int size)
{
#ifdef SPATTERLIGHT
  if (gsc_autorestored)
    {
      /* The restored transcript already ends with the old prompt; skip
         printing another and just take input. */
      gsc_autorestored = FALSE;
      gsc_a5_note_turn_start ();
    }
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
  if (gsc_a5_popup_replay == GSC_A5_POPUP_TURN
      && !gsc_a5_replay_command.empty ()
      && gsc_a5_replay_command.size () < (size_t) size)
    {
      /* Resuming at a question this command's turn asked: the command
         is in the restored transcript, and is run again rather than
         read. */
      memcpy (input, gsc_a5_replay_command.c_str (),
              gsc_a5_replay_command.size () + 1);
      gsc_a5_replay_command.clear ();
      return TRUE;
    }

  gsc_a5_popup_replay = GSC_A5_POPUP_ELSEWHERE;
  gsc_a5_waitkey_skip = 0;
  return gsc_a5_read_line (input, size) != 0;
}


/* What gsc_a5_host_command() made of a line of input. */
typedef enum
{
  GSC_A5_COMMAND_GAME,          /* Not the host's: a turn for the engine. */
  GSC_A5_COMMAND_HANDLED,       /* Dealt with; back to the prompt. */
  GSC_A5_COMMAND_QUIT           /* Leave the turn loop. */
} gsc_a5_command_t;

/*
 * gsc_a5_host_command()
 *
 * Take the commands the front end answers itself rather than hand to the
 * engine: the "glk ..." escapes, and QUIT, RESTART, MAP, SAVE, RESTORE and
 * UNDO.
 */
static gsc_a5_command_t
gsc_a5_host_command (a5_run_t *&run, char *input)
{
  /* Handle "glk ..." command escapes and input logging. */
  if (gsc_a5_command_escape (input))
    return GSC_A5_COMMAND_HANDLED;

  if (gsc_a5_match_command (input, "quit")
      || gsc_a5_match_command (input, "q"))
    return GSC_A5_COMMAND_QUIT;

  if (gsc_a5_match_command (input, "restart"))
    {
      gsc_a5_restart_run (run);
      return GSC_A5_COMMAND_HANDLED;
    }

  /* MAP toggles the map pane.  The runner's map is host chrome rather than
     a game command, so authors are free to use MAP themselves; when they
     do, theirs wins and the pane is reached with "glk map". */
  if (gsc_map != nullptr && !gsc_map_taken
      && gsc_a5_match_command (input, "map"))
    {
      gsc_map_toggle ();
      return GSC_A5_COMMAND_HANDLED;
    }

  if (gsc_a5_match_command (input, "save"))
    {
      gsc_a5_save (run);
      return GSC_A5_COMMAND_HANDLED;
    }

  if (gsc_a5_match_command (input, "restore"))
    {
      gsc_a5_try_restore (run);
      return GSC_A5_COMMAND_HANDLED;
    }

  if (gsc_a5_match_command (input, "undo"))
    {
      gsc_a5_undo_command (run);
      return GSC_A5_COMMAND_HANDLED;
    }

  return GSC_A5_COMMAND_GAME;
}


/*
 * gsc_a5_play_turn()
 *
 * Give the engine one command and present what it makes of it.
 */
static void
gsc_a5_play_turn (a5_run_t *&run, const char *input)
{
  char *text;

  /* Push the pre-turn state onto the undo stack (before a5run_input,
     which increments the turn counter on entry). */
  a5run_snapshot (run);

  gsc_a5_popup_command = input;
  gsc_a5_popup_answers.clear ();
  gsc_a5_popup_context = GSC_A5_POPUP_TURN;
  gsc_a5_waitkey_count = 0;
  text = a5run_input (run, input);
  gsc_a5_popup_replay = GSC_A5_POPUP_ELSEWHERE;
  /* Still the turn's while its text is paged out: a <waitkey> pause in
     it is saved as a question is (gsc_a5_autosave_waitkey). */
  gsc_a5_display (text);
  gsc_a5_popup_context = GSC_A5_POPUP_ELSEWHERE;
  free (text);
  gsc_a5_show_media (run);
  gsc_a5_status (run);
  gsc_map_redraw ();
}


/*
 * gsc_a5_main_shutdown()
 *
 * Release the run and the map once the turn loop is left.
 */
static void
gsc_a5_main_shutdown (a5_run_t *&run)
{
  /* Clear the run alias before freeing: `run` references gsc_a5_run, which a
     resize/redraw dispatched during teardown would otherwise dereference. */
  {
    a5_run_t *dead = run;
    run = nullptr;
    a5run_free (dead);
  }
  gsc_a5_map_names_clear ();
  map_free (gsc_map);
  gsc_map = nullptr;
  gsc_map_screen_drop ();
}


/*
 * gsc_a5_main()
 *
 * Run the ADRIFT 5 game in a single text-buffer window: print the intro, then
 * loop reading commands and printing each turn's output, handling the host
 * meta-commands and end-of-game.
 */
void
gsc_a5_main (void)
{
  /* Alias the run through gsc_a5_run so resize redraws always see the
     current run, including across restarts. */
  a5_run_t *&run = gsc_a5_run;
  char input[1024];
  int autorestore = FALSE;

#ifdef SPATTERLIGHT
  autorestore = gsc_autorestore_wanted ();
#endif

  gsc_a5_main_setup (run, autorestore);
  gsc_a5_main_opening (run, autorestore);

  for (;;)
    {
      gsc_a5_command_t command;

      /* Any "glk undo/restore/restart/quit" happens here, before the state of
         play is read: this is the first moment the run may safely be replaced,
         whether the command was typed at the prompt below or at a naming
         prompt the engine opened mid-turn. */
      if (gsc_a5_meta_perform (run))
        break;

      if (a5run_is_over (run))
        gsc_a5_game_over (run);

      /* If a "help" request was noted last turn, hint at "glk help". */
      gsc_output_provide_help_hint ();

      if (!gsc_a5_next_command (input, sizeof input))
        continue;

      command = gsc_a5_host_command (run, input);
      if (command == GSC_A5_COMMAND_QUIT)
        break;
      if (command == GSC_A5_COMMAND_HANDLED)
        continue;

      /* An ended game is handled at the top of the loop. */
      gsc_a5_play_turn (run, input);
    }

  gsc_a5_main_shutdown (run);
  glk_exit ();
}
