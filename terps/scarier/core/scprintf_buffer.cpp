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
 * The pending-output buffer: what pf_buffer_*() accumulates between
 * flushes, the printed-so-far and reference bookkeeping, and the Runner
 * paragraph, break and join rules that decide how the next text meets
 * what is already there.
 */

#include "scprintf_internal.h"

/*
 * pf_append_string()
 *
 * Append a string to the filter buffer.
 */
static void
pf_append_string (scr_filterref_t filter, const scr_char *string)
{
  /* std::string handles growth (amortized) and termination for us. */
  filter->buffer.append (string);
}


/*
 * pf_take_printed()
 * pf_get_printed()
 * pf_set_printed()
 * pf_buffer_printed()
 *
 * Take, and clear, the text flushed out since the last take; and buffer such
 * text again.  The text is already filtered, so the buffer is checkpointed
 * first and the replay appended after it unfiltered, to keep its ALRs from
 * applying a second time.  pf_get_printed() and pf_set_printed() peek at and
 * put back the text still to be taken, for an autosave: at a prompt it is the
 * finished turn's output, which the next line's undo snapshot takes.
 */
std::string
pf_take_printed (scr_filterref_t filter)
{
  std::string text;
  assert (pf_is_valid (filter));

  text.swap (filter->printed);
  return text;
}

const std::string &
pf_get_printed (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));
  return filter->printed;
}

void
pf_set_printed (scr_filterref_t filter, const std::string &text)
{
  assert (pf_is_valid (filter));
  filter->printed = text;
}

void
pf_buffer_printed (scr_filterref_t filter,
                   scr_var_setref_t vars, scr_prop_setref_t bundle,
                   const std::string &text)
{
  assert (pf_is_valid (filter));

  if (filter->is_muted || text.empty ())
    return;

  pf_checkpoint (filter, vars, bundle);
  filter->buffer.append (text);
  filter->new_sentence = FALSE;
  filter->auto_break_at = -1;
  filter->hard_break_at = -1;
  filter->reference_at = -1;
}


/*
 * pf_checkpoint()
 *
 * Filter buffered data, interpolating variables and replacing ALR's, and
 * store the result back in the buffer.  This allows a string to be inter-
 * polated in between main flushes; used to update buffered text with variable
 * values before those values are updated by task actions.
 */
void
pf_checkpoint (scr_filterref_t filter,
               scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  assert (pf_is_valid (filter));
  assert (vars && bundle);

  /* See if there is any buffered data to filter. */
  if (!filter->buffer.empty ())
    {
      /*
       * Filter the buffered string, and place the filtered result, if any,
       * back into the filter buffer.
       */
      if (filter->needs_filtering)
        pf_filter_buffer (filter, vars, bundle);

      /* Note the buffer as filtered, to avoid pointless filtering. */
      filter->needs_filtering = FALSE;
    }
}


/*
 * pf_print_so_far()
 *
 * Filter the turn's text buffered so far and freeze it, as the Runner does
 * when it prints that text there and then: later checkpoints, task refilters
 * and the flush filter only what is buffered after it.  The text stays in the
 * buffer, so the paragraph-spacing helpers still see it, and mute and
 * sentence state are untouched.
 *
 * The one caller is the room exits list; lib_print_room_exits() has the
 * Runner addresses.
 */
void
pf_print_so_far (scr_filterref_t filter,
                 scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  assert (pf_is_valid (filter));
  assert (vars && bundle);

  if (filter->is_muted)
    return;

  if (filter->needs_filtering)
    pf_filter_buffer (filter, vars, bundle);
  filter->needs_filtering = FALSE;
  filter->frozen = filter->buffer.size ();
  filter->printed_to = (scr_int) filter->frozen;
  if (filter->auto_break_at > (scr_int) filter->frozen)
    filter->auto_break_at = -1;
}


/*
 * pf_refilter()
 *
 * Filter buffered data the way pf_checkpoint() does, but leave it marked as
 * still needing filtering, so that the flush at the end of the turn filters
 * it once more.
 *
 * This is what a completing version 4.0 task does to the turn's output, and
 * it is why some text comes out with its ALRs applied twice.  Measured under
 * run400.exe in Wine on 2026-08-24 with harness/make_400_alrsrcprobe.py,
 * whose ALRs include the self-containing "ball -> qball" (transcripts
 * runner_probes/alrsrc.run400.b.txt, runner_probes/alrsrc.run400.c.txt and
 * runner_probes/alrsrc.run400.d.txt):
 *
 *    look       "LONG ball ..." (room, no task)      -> LONG qball
 *    zulu       task CompleteText                    -> You take the qqball.
 *    victor     CompleteText "CT n=%n% TXT %w%.",
 *               its action then setting n = 9        -> CT n=9 TXT qqball.
 *    uniform    CompleteText "CTU ball.", an action
 *               running zulu, AdditionalMessage
 *               "AMU ball."     -> CTU qqqball.  You take the qqqball.
 *                                  AMU qqball.
 *    tango      CompleteText "CTT ball." and
 *               ShowRoomDesc    -> CTT qqball. / LONG qqball ... qqAAA
 *
 * Every one of those falls out of one rule: at the end of a task that
 * completes, the Runner runs its output filter over the whole of the turn's
 * buffered text, and the turn's own flush runs it once more.  So the "qqq" in
 * the uniform cell is the inner task's pass, the outer task's pass and the
 * flush; "AMU" misses the inner one because it was printed after it; and the
 * room description tango prints is caught by tango's pass just like tango's
 * own CompleteText.
 *
 * The variables go the same way -- the pass interpolates them where it finds
 * them, which is why "CT n=%n%" comes out as the value the task's own action
 * has just set, and why "%w%" (holding "ball") comes out with both walks
 * applied to its value.
 *
 * Scarier keeps the extra walks but deliberately not their "qqq": a
 * self-containing ALR does not re-expand text it has already expanded (see
 * pf_replace_alr()), since the repeats are a display accident.
 */
void
pf_refilter (scr_filterref_t filter,
             scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  assert (pf_is_valid (filter));
  assert (vars && bundle);

  if (!filter->buffer.empty () && filter->needs_filtering)
    {
      const size_t length = filter->buffer.size ();
      const scr_bool at_end = filter->auto_break_at >= 0
                              && (size_t) filter->auto_break_at == length;

      if (pf_filter_buffer (filter, vars, bundle))
        {
          /*
           * Keep the note of our own trailing newline pointing at the end of
           * the rewritten text, so pf_undo_auto_break() can still take it
           * back.
           */
          if (at_end && pf_text_ends_with_break (filter->buffer.c_str ()))
            filter->auto_break_at = (scr_int) filter->buffer.size ();
        }
    }
}


/*
 * pf_get_buffer()
 * pf_transfer_buffer()
 *
 * Return the raw, unfiltered, buffered text.  Returns NULL if no buffered
 * data available.  Transferring the buffer transfers ownership of the buffer
 * string to the caller, who is then responsible for freeing it.
 *
 * The second function is an optimization to avoid allocations and copying
 * in client code.
 */
const scr_char *
pf_get_buffer (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  /*
   * Return the visible buffer if it holds any text, otherwise NULL.  Text
   * before the barrier, if one is set, is invisible here (pf_hide_prefix()).
   */
  if (filter->buffer.size () > filter->hidden)
    return filter->buffer.c_str () + filter->hidden;
  else
    return nullptr;
}

scr_char *
pf_transfer_buffer (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  /*
   * If the filter holds text, hand the caller a freshly allocated copy (which
   * they scr_free) and reset the filter to an empty state.
   */
  if (!filter->buffer.empty ())
    {
      scr_char *retval;

      /* Copy out the buffered text for the caller to own. */
      retval = scr_strdup (filter->buffer);

      /* Clear all filter fields down to empty values. */
      filter->buffer.clear ();
      filter->frozen = 0;
      filter->printed_to = -1;
      filter->new_sentence = FALSE;
      filter->is_muted = FALSE;
      filter->needs_filtering = FALSE;

      /*
       * Deliberately not resetting auto_break_at: task running transfers the
       * buffer out, runs task actions, then prepends it back, and the note of
       * our own trailing newline has to survive that round trip.  It stays
       * meaningless while the buffer is empty, since it can only match a
       * buffer length equal to the transferred text's own.
       */

      /* Return the allocated buffered text. */
      return retval;
    }
  else
    return nullptr;
}


/*
 * pf_empty()
 *
 * Empty any text currently buffered in the filter.
 */
void
pf_empty (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  /* Free any allocation, and return the filter to initialization state. */
  filter->buffer.clear ();
  pf_reset_state (filter);
}


/*
 * pf_buffer_reference_line()
 * pf_buffer_reference()
 * pf_rebuffer_reference()
 *
 * Buffer one of the 4.0 Runner's bracketed reference lines -- "(a trophy)"
 * for a pronoun, "(look)" for "again" -- as its own line, and note where it
 * ends.  The Runner builds each of these as
 * "(" & text & ")" & vbCrLf and appends the turn's response after it
 * unchanged, so a task text that opens with "<br>" still puts a blank line
 * between the reference and itself.  Unmarked, the line's newline would make
 * pf_buffer_paragraph() swallow that leading break as if it were one of
 * Scarier's own terminators.  Only that one collapse is switched off: the
 * hidden-prefix barrier would also stop a walk announcement joining the
 * paragraph (see pf_buffer_join()), which the Runner still does.
 *
 * The line is printed there and then: run400's Proc_21_19_47B568 is the
 * window printer, and its first act is the output filter Proc_21_20_44C7DC
 * (47A4DF), the %tag% pass then the ALR walk, so the line reads the
 * variables as they stand BEFORE the command runs; run390 prints its echo
 * through Proc_2_28_45CBD0, which is the 3.9 output filter itself.  The
 * turn's own text is filtered later, at a completing 4.0 task's refilter
 * and at the flush, by which time an action may have changed a variable
 * the antecedent reads.  Measured on The Tempest (tempest7.taf 4.00,
 * run400x runner_transcripts/tempest7.txt T138): `ask girl her name` with
 * the girl's Name "[GIRL=%miranda%]" echoes "(A pretty girl)" -- miranda 0
 * through ALR [GIRL=0] -- and the task's own action then sets miranda 2,
 * which a flush-time filter turned into "(Miranda)".  So the line is
 * filtered and frozen as it is buffered, the way lib_print_room_exits()
 * does for the room block (pf_print_so_far()); pf_rebuffer_reference()
 * puts back a line that was already printed and filtered, without
 * filtering it again.
 */
static void
pf_buffer_reference_line (scr_filterref_t filter, const scr_char *text)
{
  pf_buffer_string (filter, "(");
  pf_buffer_string (filter, text);
  pf_buffer_string (filter, ")\n");
}

void
pf_buffer_reference (scr_filterref_t filter, const scr_char *text,
                     scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  assert (pf_is_valid (filter));
  assert (text && vars && bundle);

  pf_buffer_reference_line (filter, text);
  pf_print_so_far (filter, vars, bundle);
  if (!filter->is_muted)
    filter->reference_at = (scr_int) filter->buffer.size ();
}

void
pf_rebuffer_reference (scr_filterref_t filter, const scr_char *text)
{
  assert (pf_is_valid (filter));
  assert (text);

  pf_buffer_reference_line (filter, text);
  if (!filter->is_muted)
    {
      filter->frozen = filter->buffer.size ();
      filter->printed_to = (scr_int) filter->frozen;
      filter->reference_at = (scr_int) filter->buffer.size ();
    }
}


/*
 * pf_leading_reference()
 *
 * The bracketed reference line the turn opened with, "(to Nobody)\n", when
 * that is all the buffer holds ahead of pf_buffer_reference()'s mark; empty
 * otherwise.  The Runner prints the echo straight to the window (47B568)
 * while the turn's answer builds in its own string, so a later "Which X."
 * prompt that replaces the answer leaves the echo standing.
 */
std::string
pf_leading_reference (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  if (filter->reference_at <= 0
      || (size_t) filter->reference_at > filter->buffer.size ()
      || filter->buffer[0] != '(')
    return std::string ();
  return filter->buffer.substr (0, (size_t) filter->reference_at);
}


/*
 * pf_hide_prefix()
 * pf_reveal_prefix()
 *
 * Hide everything buffered so far from the paragraph-spacing helpers, and put
 * it back.  pf_hide_prefix() returns the barrier that was in force, which the
 * caller hands to pf_reveal_prefix() when it is done, so that the pair nests.
 *
 * Version 4.0 task running keeps the turn's text in the buffer while a task's
 * actions run, because a task that an action completes filters that text along
 * with its own (see pf_refilter()).  Pre-4.0 task running instead transfers
 * the text out and prepends it back afterwards, and the spacing helpers there
 * see an empty buffer for the duration.  Hiding rather than transferring keeps
 * that -- text run by an action opens its paragraph the same way under both --
 * so the version split stays confined to when the filter runs.
 */
size_t
pf_hide_prefix (scr_filterref_t filter)
{
  size_t previous;

  assert (pf_is_valid (filter));

  previous = filter->hidden;
  filter->hidden = filter->buffer.size ();

  /*
   * pf_transfer_buffer() clears these two as a side effect of resetting the
   * filter, and pf_prepend_string() does not put them back; match it.
   */
  filter->new_sentence = FALSE;
  filter->is_muted = FALSE;

  return previous;
}

void
pf_reveal_prefix (scr_filterref_t filter, size_t previous)
{
  assert (pf_is_valid (filter));

  filter->hidden = previous;
}


/*
 * pf_buffer_string()
 * pf_buffer_character()
 * pf_buffer_integer()
 *
 * Add a string, a single character, and a decimal integer, to the
 * printfilter buffer.  If muted, these functions do nothing.
 */
void
pf_buffer_string (scr_filterref_t filter, const scr_char *string)
{
  assert (pf_is_valid (filter));
  assert (string);

  /* Ignore the call if the printfilter is muted. */
  if (!filter->is_muted)
    {
      size_t noted;

      /*
       * A pending join runs this text straight on from the last text, two
       * spaces between, as the Runner's string concatenation would.
       */
      if (filter->join_pending)
        {
          filter->join_pending = FALSE;
          pf_undo_auto_break (filter);
          pf_buffer_pspace (filter);
        }

      /* Note append start, then append the string to the buffer. */
      noted = filter->buffer.size ();
      pf_append_string (filter, string);

      /* Adjust the first character of the appended string if flagged. */
      if (filter->new_sentence && noted < filter->buffer.size ())
        filter->buffer[noted] = scr_toupper (filter->buffer[noted]);

      /* Clear new sentence, and note as currently needing filtering. */
      filter->needs_filtering = TRUE;
      filter->new_sentence = FALSE;

      /*
       * Anything buffered invalidates a note of our own trailing newline,
       * and equally the two notes of a break the Runner keeps.  All three are
       * positions, and a position only means anything while it is still the
       * end of the buffer; left standing, a stale one collides with any later
       * buffer that happens to reach exactly that length.  Measured on
       * circus.taf: the room heading three turns earlier left hard_break_at
       * at 53, and "You get no reply from the videotape.\n" is 53 characters
       * too, so the walk announcement that should have joined it broke
       * instead.
       */
      filter->auto_break_at = -1;
      filter->hard_break_at = -1;
      filter->reference_at = -1;
    }
}

void
pf_buffer_character (scr_filterref_t filter, scr_char character)
{
  scr_char buffer[2];
  assert (pf_is_valid (filter));

  buffer[0] = character;
  buffer[1] = NUL;
  pf_buffer_string (filter, buffer);
}

void
pf_buffer_integer (scr_filterref_t filter, scr_int value)
{
  scr_char buffer[32];

  snprintf (buffer, sizeof (buffer), "%ld", value);
  pf_buffer_string (filter, buffer);
}


/*
 * pf_text_ends_with_break()
 * pf_text_leads_with_break()
 *
 * Return TRUE if text ends with / begins with a line break -- either a literal
 * newline or a "<br>" tag (the unfiltered buffer holds tags verbatim).
 */
scr_bool
pf_text_ends_with_break (const scr_char *text)
{
  scr_int length = strlen (text);

  if (length > 0 && text[length - 1] == '\n')
    return TRUE;
  if (length >= 4 && !scr_strncasecmp (text + length - 4, "<br>", 4))
    return TRUE;
  return FALSE;
}

static scr_bool
pf_text_leads_with_break (const scr_char *text)
{
  return text[0] == '\n' || !scr_strncasecmp (text, "<br>", 4);
}


/*
 * pf_text_ends_with_newline()
 *
 * TRUE if the text ends in a literal newline, as opposed to a "<br>" tag.
 * The distinction is the whole of pf_buffer_paragraph()'s test: a newline in
 * the buffer is one Scarier put there to end a section of its own, while a
 * "<br>" still standing at the end is one the author wrote, and the Runner
 * has it too.
 */
static scr_bool
pf_text_ends_with_newline (const scr_char *text)
{
  scr_int length = strlen (text);

  return length > 0 && text[length - 1] == '\n';
}


/*
 * pf_buffer_paragraph()
 *
 * Buffer a block of text that conventionally begins with its own line break(s)
 * for spacing -- Adrift event and atmosphere texts typically start with "<br>"
 * or "<br><br>".  The Runner relies on those leading breaks alone for
 * paragraph spacing, whereas Scarier also terminates the preceding room
 * description, exits list, NPC announcement and task text with a newline of
 * its own.  To avoid a doubled blank line, a single leading break is dropped
 * from the text -- but only when the break it would double is one of ours.
 *
 * The buffer says which it is.  Tags are translated at filter time, not here,
 * so an author's trailing "<br>" is still standing verbatim at the end of the
 * buffer, while every break Scarier supplies is a literal newline.  A trailing
 * "<br>" is therefore the author's, the Runner has it too, and run400 really
 * does print a blank line between it and the next paragraph's "<br>" -- see
 * Ghost town's "...but follows you anyway.<br>" in
 * runner_probes/ghosttown.run400.txt 586-592.  The one literal newline that is
 * not ours is 4.0's "Time passes...\n" (runner_probes/patient7.run400.txt
 * 81-87), which pf_buffer_hard_break() records, and the 4.0 bracketed
 * reference line, which pf_buffer_reference() records.  Measured over the
 * whole transcript archive: harness/sweep_wine_breaks.py, and the write-up
 * under "Ported 2026-09-07: a leading <br> is collapsed only against a break
 * of Scarier's own".
 */
static void
pf_buffer_paragraph (scr_filterref_t filter, const scr_char *string)
{
  const scr_char *buffered;

  assert (pf_is_valid (filter));
  assert (string);

  buffered = pf_get_buffer (filter);
  if (buffered
      && !filter->join_pending
      && pf_text_ends_with_newline (buffered)
      && pf_text_leads_with_break (string)
      && !(filter->hard_break_at >= 0
           && (size_t) filter->hard_break_at == filter->buffer.size ())
      && !(filter->reference_at >= 0
           && (size_t) filter->reference_at == filter->buffer.size ()))
    {
      /* Skip exactly one leading break -- a literal newline or a "<br>" tag. */
      string += (*string == '\n') ? 1 : 4;
    }
  pf_buffer_string (filter, string);
}


/*
 * pf_buffer_paragraph_line()
 *
 * Buffer a paragraph and terminate it with a line break, but only add one if
 * the paragraph did not already end with a break of its own -- the trailing
 * counterpart to the leading-break collapse above.
 *
 * Task and event texts are terminated with a newline so the next thing printed
 * starts on a fresh line.  Authors who end their text with "<br>" get that line
 * break as well, and the doubled break shows as a blank line the Adrift runner
 * does not print.  "The Warlord, The Princess & The Bulldog" builds its
 * inventory from one task per object, each with text like "*<tab>your uniform
 * (worn)<br>", so every item in the listing was followed by a blank line.
 */
void
pf_buffer_paragraph_line (scr_filterref_t filter, const scr_char *string)
{
  const scr_char *buffered;

  pf_buffer_paragraph (filter, string);

  filter->auto_break_at = -1;
  buffered = pf_get_buffer (filter);
  if (!buffered || !pf_text_ends_with_break (buffered))
    {
      size_t before = filter->buffer.size ();

      pf_buffer_character (filter, '\n');

      /*
       * Note the terminator as ours, so that pf_undo_auto_break() can take it
       * back.  Muting makes the call above a no-op, hence the length test.
       */
      if (filter->buffer.size () > before)
        filter->auto_break_at = (scr_int) filter->buffer.size ();
    }
}


/*
 * pf_undo_auto_break()
 *
 * Take back the line terminator that the last pf_buffer_paragraph_line()
 * supplied of its own accord, if it is still the last thing in the buffer.
 * Returns TRUE if one was removed.
 *
 * The pre-4.0 Runners do not terminate a task's text: whatever is printed
 * next either supplies its own separator or runs straight on from where the
 * text stopped.  (4.0 does terminate it, which is why the one caller so far,
 * task_print_end_game_message(), asks for this on a pre-4.0 game only.)
 * Scarier instead ends each section with a newline, which is right for almost
 * every caller but wrong for the ones that the Runner butts up against the
 * text with nothing in between.  Measured live in run380 finishing
 * microwaveman.taf, where the winning task's "You win the game." and the
 * game's WinText appear as one run-on line, "You win the game.You have
 * destroyed Coffee Man...".  Only that section's own terminator is taken
 * back; a break the author wrote is left alone, which is why the position is
 * recorded rather than a flag.
 */
scr_bool
pf_undo_auto_break (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  if (filter->auto_break_at >= 0
      && (size_t) filter->auto_break_at == filter->buffer.size ()
      && !filter->buffer.empty ()
      && filter->buffer[filter->buffer.size () - 1] == '\n')
    {
      filter->buffer.erase (filter->buffer.size () - 1);
      filter->auto_break_at = -1;
      if (filter->frozen > filter->buffer.size ())
        filter->frozen = filter->buffer.size ();
      if (filter->printed_to > (scr_int) filter->buffer.size ())
        filter->printed_to = (scr_int) filter->buffer.size ();
      return TRUE;
    }

  return FALSE;
}


/*
 * pf_note_trailing_auto_break()
 *
 * Record a newline already at the end of the buffer as a terminator of our
 * own, as if pf_buffer_paragraph_line() had just supplied it, so that
 * pf_ends_with_double_space() can look through it at the text the Runner
 * really assembled.  Needed for the task ShowRoomDesc block: the pre-4.0
 * Runners store no break after a room description -- the next piece is glued
 * on by the two-space separator logic -- so the newline Scarier's room
 * printer adds there stands where the Runner's string simply stopped.
 * Measured live on superliam.taf in run380 (2026-08-31): tasks 23 ("press
 * button") and 24 ("up") show rooms whose Long ends in two spaces, and the
 * real Runner suppresses both AdditionalMessages ("a cat runs out of a
 * door...", "X1 raising his gunlike arms...") where Scarier printed them.
 */
void
pf_note_trailing_auto_break (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  if (!filter->buffer.empty ()
      && filter->buffer[filter->buffer.size () - 1] == '\n')
    filter->auto_break_at = (scr_int) filter->buffer.size ();
}


/*
 * pf_buffer_answer_break()
 *
 * End a library answer.  The Runner has none of these: its handlers append
 * their sentence to the turn's one string and stop there, and whatever comes
 * next in the turn -- an event's text, an NPC's walk line, a battle strike --
 * follows after pspace()'s two spaces.  We give each answer a line of its
 * own, so the terminator has to be recorded as OURS, for pf_buffer_join_line()
 * to take back when something does follow.
 *
 * troll (3.90) is the plainest measurement: `drop tankard` is "You drop the
 * tankard.  Your guts rumble.  Your throat is sore." on one line, the two
 * hunger events running straight on from the library's own answer
 * (runner_transcripts/troll.txt t29), and t31, t37, t43, t45 and t57 say the
 * same after `drop`, `get` and `take ... from`.
 *
 * Not every newline the library buffers is one of ours.  The room heading's
 * is the Runner's too (lib_print_room_name, pf_buffer_hard_break), and the
 * room description block's terminator is already noted where the contents
 * lister can take it back on its own terms (lib_print_room_contents); those
 * sites keep their own spelling.
 */
void
pf_buffer_answer_break (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  pf_buffer_character (filter, '\n');
  pf_note_trailing_auto_break (filter);
}


/*
 * pf_ends_with_double_space()
 *
 * TRUE if the text buffered so far ends in two spaces, ignoring a trailing
 * newline that pf_buffer_paragraph_line() supplied of its own accord.
 *
 * The Runners assemble a turn's output by concatenating onto one string, and
 * separate the pieces with two spaces.  Before adding that separator they ask
 * whether the string already ends in one, so an author's own trailing spaces
 * are not doubled up.  run380 wrote the test for the task's AdditionalMessage
 * the wrong way round -- see task_run_task_unrestricted() -- and that is the
 * only caller so far.  The auto-break is skipped because it is ours and not
 * the Runner's: it stands where the Runner's string simply stopped.
 */
scr_bool
pf_ends_with_double_space (scr_filterref_t filter)
{
  size_t length;

  assert (pf_is_valid (filter));

  length = filter->buffer.size ();
  if (filter->auto_break_at >= 0
      && (size_t) filter->auto_break_at == length
      && length > 0
      && filter->buffer[length - 1] == '\n')
    length--;

  return length >= 2
         && filter->buffer[length - 1] == ' '
         && filter->buffer[length - 2] == ' ';
}


/*
 * pf_text_trailing_breaks()
 * pf_buffer_paragraph_break()
 *
 * Ensure that whatever is buffered next opens a new paragraph: if the buffer
 * already holds text, top it up with breaks until it ends in a blank line.
 * Does nothing on an empty buffer, so a paragraph never opens with leading
 * whitespace.
 *
 * This exists for the inline room name.  With the 3.9+ "Room names in
 * descriptions" Appearance option ticked -- which is how the archive was
 * measured -- the Runner does print the name into the transcript, and puts a
 * break in front of it: the blank line this tops the buffer up to is the
 * Runner's own leading break landing after text that already ended in one.
 * (Pre-3.9 there is no heading at all, and lib_describe_player_room() never
 * calls this.)
 */
static scr_int
pf_text_trailing_breaks (const scr_char *text)
{
  scr_int length, count;

  count = 0;
  length = strlen (text);
  while (length > 0)
    {
      if (text[length - 1] == '\n')
        {
          length -= 1;
          count++;
        }
      else if (length >= 4 && !scr_strncasecmp (text + length - 4, "<br>", 4))
        {
          length -= 4;
          count++;
        }
      else
        break;
    }

  return count;
}

void
pf_buffer_paragraph_break (scr_filterref_t filter)
{
  const scr_char *buffered;
  scr_int breaks;

  assert (pf_is_valid (filter));

  /*
   * Nothing buffered means nothing to separate from -- but look through the
   * hidden barrier before deciding that.  A 4.0 task's actions run with the
   * turn's text hidden (pf_hide_prefix), so a task an action runs shows its
   * room into what looks like an empty buffer, and the heading lost its
   * break; the Runner has no barrier and puts its own break in regardless.
   * Measured: baroo t120/t121, blood t30/t70, cursed t76/t134, thepkgirl
   * t125/t177/t257 and vendetta t113 all break before a task-driven room
   * heading where Scarier ran it onto the line above.
   */
  buffered = pf_get_buffer (filter);
  if (!buffered || scr_strempty (buffered))
    {
      if (filter->hidden == 0 || filter->buffer.empty ())
        return;
      buffered = filter->buffer.c_str ();
    }

  for (breaks = pf_text_trailing_breaks (buffered); breaks < 2; breaks++)
    pf_buffer_character (filter, '\n');
}


/*
 * pf_buffer_ends_with_two_spaces()
 * pf_buffer_join()
 *
 * Append a string as a continuation of the current output line, with the
 * Adrift runner's two-space sentence separator.  The runner builds a turn's
 * output as one paragraph joined with two spaces; our section printers
 * instead terminate each section with a newline of their own.  To join text
 * onto the previous section the way the runner does -- event look text runs
 * on after the room's character lines, and an NPC walk announcement runs on
 * after whatever the turn has printed -- remove a single terminating newline
 * first, then separate with two spaces unless the text before it ends with
 * an author's own break.
 *
 * 3.9 and 4.0 factor that out into a sub, pspace (run390 loc_45A99E, run400
 * Proc_21_50_44A9F4 @ General.bas:10341), which is
 *
 *   If buf <> "" And Right(buf, 2) <> "  " And Right(buf, 1) <> Chr(10)
 *      And Right(buf, 4) <> "<br>" Then buf = buf & "  "
 *
 * -- so text that already ends in the separator does not get a second one.
 * The two older runners write the test out inline at each site and it is
 * shorter, with no such clause:
 *
 *   run370 loc_4395AA / run380 loc_441740
 *      If Right(buf, 1) <> Chr(10) And Len(buf) > 0 Then buf = buf & "  "
 *
 * so a 3.7 or 3.8 join onto text ending in two spaces really does make four.
 * Deliberate deviation: every version joins with pspace's guard here, so no
 * caller makes four spaces (see npc_announce()).  The "<br>" clause is not
 * split either, both being folded into pf_text_ends_with_break(), because no
 * game has yet shown the difference.
 */
static scr_bool
pf_buffer_ends_with_two_spaces (scr_filterref_t filter)
{
  const size_t length = filter->buffer.size ();

  return length >= filter->hidden + 2
         && filter->buffer[length - 1] == ' '
         && filter->buffer[length - 2] == ' ';
}

void
pf_buffer_join (scr_filterref_t filter, const scr_char *string)
{
  assert (pf_is_valid (filter));
  assert (string);

  if (!filter->is_muted && filter->buffer.size () > filter->hidden)
    {
      if (filter->buffer.back () == '\n')
        {
          /*
           * A newline the Runner's own string carries stays, and pspace
           * then adds no separator to text ending in Chr(10).
           */
          if (filter->hard_break_at >= 0
              && (size_t) filter->hard_break_at == filter->buffer.size ())
            {
              pf_buffer_string (filter, string);
              return;
            }
          filter->buffer.pop_back ();
        }

      if (filter->buffer.size () > filter->hidden
          && !pf_text_ends_with_break (filter->buffer.c_str ())
          && !pf_buffer_ends_with_two_spaces (filter))
        pf_append_string (filter, "  ");
    }
  pf_buffer_string (filter, string);
}


/*
 * pf_buffer_tentative_join()
 * pf_retract_tentative_join()
 *
 * pf_buffer_join()'s separator for text that may never come: the
 * first appends "  " when buffered text would need it and says whether it
 * did; the second takes it back again if nothing followed it.  For a caller
 * that can only tell whether some code prints by running it -- the buffer
 * is filtered and moved about while tasks run, so a length taken before is
 * no guide to where their text starts.
 */
scr_bool
pf_buffer_tentative_join (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  if (filter->is_muted || filter->buffer.size () <= filter->hidden
      || pf_text_ends_with_break (filter->buffer.c_str ())
      || filter->buffer.back () == ' ')
    return FALSE;

  pf_append_string (filter, "  ");
  return TRUE;
}

void
pf_retract_tentative_join (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  const size_t length = filter->buffer.size ();
  if (length >= filter->hidden + 2
      && filter->buffer.compare (length - 2, 2, "  ") == 0)
    filter->buffer.resize (length - 2);
}


/*
 * pf_buffer_pspace()
 *
 * The pspace() sub by itself, for a caller that needs the separator without
 * pf_buffer_join()'s removal of a trailing newline: append the two-space
 * separator unless the buffer is empty or already ends with "  ", Chr(10)
 * or "<br>" (run390 @42C920).  run390's win path calls it unconditionally
 * before appending the game's WinText (execute_task loc_43F255), where the
 * accumulated text still ends with the winning task's unterminated text --
 * see task_print_end_game_message().  The caller is responsible for taking
 * back our own section terminator first (pf_undo_auto_break), since the
 * Runner's string never had one.
 */
void
pf_buffer_pspace (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  if (filter->is_muted)
    return;

  if (filter->buffer.size () > filter->hidden
      && !pf_text_ends_with_break (filter->buffer.c_str ())
      && !pf_buffer_ends_with_two_spaces (filter))
    pf_append_string (filter, "  ");
}


/*
 * pf_has_hidden_prefix()
 * pf_buffer_join_open()
 * pf_buffer_join_line()
 *
 * pf_buffer_join_open() is the separator half on its own, for text a caller
 * assembles piece by piece rather than handing over as one string -- a battle
 * blow, say, which is a name, a verb, another name and a full stop.  Call it
 * once, before the first piece.
 *
 * pf_buffer_join_line() buffers a task text the way run400 assembles it: onto
 * the end of the turn's one string after pspace(), and not as a section of its
 * own.  Our terminator on the text before it is taken back -- through the
 * hidden barrier, which the Runner does not have -- and the two-space gap put
 * in its place, then the text is buffered and terminated as
 * pf_buffer_paragraph_line() would.  Where there is no terminator of ours to
 * take back the separator is still pspace()'s to decide, since the Runner
 * calls it ahead of every text regardless: after an author's break or a
 * Runner's own newline it adds nothing, but after a string the Runner left
 * unterminated it adds the two spaces just the same.  That is the ending's
 * bare "[Press any key to end]", which haunt T84 runs into when a walk's task
 * ends the game mid-tick and the Clock chime follows -- "...end]  You hear the
 * chiming of the grandfather clock." (runner_transcripts/haunt.rtf).  A text
 * that opens with a break of its own is buffered unchanged: the break there
 * reads the same either way.
 *
 * Measured with the ALR source probe (p4SRC.taf, run400,
 * runner_probes/alrsrc.run400.c.txt and runner_probes/alrsrc.run400.d.txt):
 * `uniform`, CompleteText "CTU ball.", an action running zulu ("You take the
 * ball."), AdditionalMessage "AMU ball.", answers on ONE line "CTU qqqball.
 * You take the qqqball.  AMU qqball."  It matters beyond the line structure
 * because the ALR pass sees the joined string: the_pk_girl's Original `done
 * soon."  The toaster is now on` spans task 832's CompleteText and that of
 * task 699, which 832's action runs (runner_probes/thepkgirl.run400.site.txt
 * T156).
 */
scr_bool
pf_has_hidden_prefix (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  return filter->hidden > 0;
}

void
pf_buffer_join_open (scr_filterref_t filter)
{
  const size_t length = filter->buffer.size ();

  assert (pf_is_valid (filter));

  if (filter->is_muted)
    return;

  if (filter->auto_break_at >= 0
      && (size_t) filter->auto_break_at == length
      && length > 1
      && filter->buffer[length - 1] == '\n')
    {
      filter->buffer.pop_back ();
      filter->auto_break_at = -1;
      if (filter->hidden > filter->buffer.size ())
        filter->hidden = filter->buffer.size ();
      if (filter->frozen > filter->buffer.size ())
        filter->frozen = filter->buffer.size ();
      if (filter->printed_to > (scr_int) filter->buffer.size ())
        filter->printed_to = (scr_int) filter->buffer.size ();

      if (!pf_text_ends_with_break (filter->buffer.c_str ())
          && !(filter->buffer.size () >= 2
               && filter->buffer[filter->buffer.size () - 1] == ' '
               && filter->buffer[filter->buffer.size () - 2] == ' '))
        pf_append_string (filter, "  ");
    }
  else
    pf_buffer_pspace (filter);
}

void
pf_buffer_join_line (scr_filterref_t filter, const scr_char *string)
{
  assert (pf_is_valid (filter));
  assert (string);

  if (!filter->is_muted && !pf_text_leads_with_break (string))
    pf_buffer_join_open (filter);

  pf_buffer_paragraph_line (filter, string);
}


/*
 * pf_buffer_join_pending()
 * pf_clear_join_pending()
 *
 * Arrange for the NEXT text buffered, whatever prints it, to be joined onto
 * the text already there: our own trailing auto-break is taken back and the
 * Runner's two-space separator put in its place (pf_buffer_pspace), and only
 * then is the new text appended.  Nothing happens if nothing follows, which
 * is what makes this a note rather than an immediate join: the caller does
 * not yet know whether anything will.  Clear the note when the turn's
 * dispatch is over.
 *
 * The one user is the 4.0 put refusal that does not claim its command: run400
 * prints "The rock is too big to fit inside the slot." and then lets a
 * matching task answer the same line, concatenated onto the refusal --
 * "The rock is too big to fit inside the slot.  PUTBIG." (arena probe PUT7,
 * runner_probes/put7.run400.b.txt, 2026-09-05; Zack Smackfoot's `put knife in
 * slot`, runner_transcripts/zacksmackfoot.txt).  If no task claims, the
 * refusal stands alone on its own line, exactly as buffered.
 */
void
pf_buffer_join_pending (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  filter->join_pending = TRUE;
}

void
pf_clear_join_pending (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  filter->join_pending = FALSE;
}


/*
 * pf_buffer_hard_break()
 *
 * Note that the newline the buffer currently ends with is one the Runner
 * keeps in its output string, not a section terminator of ours, so that
 * pf_buffer_join() runs the joined text on after it rather than in place of
 * it.  Anything buffered afterwards makes the note stale by position.
 */
void
pf_buffer_hard_break (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  if (!filter->is_muted && !filter->buffer.empty ()
      && filter->buffer.back () == '\n')
    filter->hard_break_at = (scr_int) filter->buffer.size ();
}


/*
 * pf_prepend_string()
 *
 * Add a string to the front of the printfilter buffer, rather than to the
 * end.  Generally less efficient than an append, these are for use by task
 * running code, which needs to run task actions and then prepend the task's
 * completion text.  If muted, this function does nothing.
 */
void
pf_prepend_string (scr_filterref_t filter, const scr_char *string)
{
  assert (pf_is_valid (filter));
  assert (string);

  /* Ignore the call if the printfilter is muted. */
  if (!filter->is_muted)
    {
      /*
       * Prepending leaves the tail of the buffer alone, so a note of our own
       * trailing newline survives it -- including down the empty-buffer path
       * below, which routes through pf_buffer_string() and would clear it.
       */
      const scr_int auto_break_at = filter->auto_break_at;
      const scr_int printed_to = filter->printed_to;
      const size_t length = filter->buffer.size ();

      if (!filter->buffer.empty ())
        {
          /* Take a copy of the current buffered string. */
          std::string copy = filter->buffer;

          /* Now restart buffering with the input string passed in. */
          filter->buffer.clear ();
          pf_append_string (filter, string);

          /* Append the string saved above; the copy frees itself. */
          pf_append_string (filter, copy.c_str ());

          /* Adjust the first character of the prepended string if flagged. */
          if (filter->new_sentence)
            filter->buffer[0] = scr_toupper (filter->buffer[0]);

          /*
           * Clear new sentence, and note as currently needing filtering.  The
           * prepended text leads any frozen prefix, so filter the lot again,
           * as pf_hoist_tail() does.
           */
          filter->needs_filtering = TRUE;
          filter->new_sentence = FALSE;
          filter->frozen = 0;
        }
      else
        /* No data, so the call is equivalent to a normal buffer. */
        pf_buffer_string (filter, string);

      filter->auto_break_at = auto_break_at;
      filter->printed_to = printed_to < 0 ? -1
                           : printed_to + (scr_int) (filter->buffer.size ()
                                                     - length);
    }
}


/*
 * pf_buffer_length()
 * pf_hoist_tail()
 *
 * pf_hoist_tail() moves everything buffered from FROM on to the FRONT of the
 * buffer, leaving what came before it in order behind.  It exists for
 * run400's implicit take: the "(Taking the hat first)" line is printed
 * straight to the Runner's textbox, where the rest of a turn's library text
 * is assembled in a string and shown when the turn ends, so a take asked for
 * by the SECOND clause of a put list still comes out above the first
 * clause's answer.  The Runner's own scrollback for `put coin in box and hat
 * in desk and hat in box` opens with the take line and only then prints "The
 * coin is too big to fit inside the box." (p4AND,
 * runner_probes/and.run400.c.txt, 2026-09-08).
 *
 * The buffer only ever moves within itself, so offsets recorded past the
 * moved text still address the same characters; the trailing-newline note is
 * dropped, since the text that ended the buffer no longer does.
 */
size_t
pf_buffer_length (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  return filter->buffer.size ();
}

void
pf_hoist_tail (scr_filterref_t filter, size_t from)
{
  assert (pf_is_valid (filter));

  if (!filter->is_muted
      && from > 0 && from < filter->buffer.size ())
    {
      const std::string tail = filter->buffer.substr (from);

      filter->buffer.erase (from);
      filter->buffer.insert (0, tail);
      filter->auto_break_at = -1;
      /*
       * Unfiltered text now leads the buffer, which a prefix cannot keep
       * apart; filter the lot again.
       */
      filter->frozen = 0;
      filter->printed_to = -1;
      filter->needs_filtering = TRUE;
    }
}


/*
 * pf_buffer_tail()
 *
 * Everything buffered from a length pf_buffer_length() returned earlier, for
 * a caller that cuts it out with pf_truncate() and puts it back elsewhere.
 */
std::string
pf_buffer_tail (scr_filterref_t filter, size_t from)
{
  assert (pf_is_valid (filter));

  return from < filter->buffer.size () ? filter->buffer.substr (from)
                                       : std::string ();
}


/*
 * pf_truncate()
 *
 * Cut the buffer back to a length pf_buffer_length() returned earlier,
 * dropping everything buffered since.  Notes that pointed past the cut go
 * with the text they pointed into.
 */
void
pf_truncate (scr_filterref_t filter, size_t length)
{
  assert (pf_is_valid (filter));

  if (length >= filter->buffer.size ())
    return;

  filter->buffer.erase (length);
  if (filter->auto_break_at > (scr_int) length)
    filter->auto_break_at = -1;
  if (filter->hard_break_at > (scr_int) length)
    filter->hard_break_at = -1;
  if (filter->reference_at > (scr_int) length)
    filter->reference_at = -1;
  if (filter->hidden > length)
    filter->hidden = length;
  if (filter->frozen > length)
    filter->frozen = length;
  if (filter->printed_to > (scr_int) length)
    filter->printed_to = (scr_int) length;
  filter->new_sentence = FALSE;
  filter->join_pending = FALSE;
}


/*
 * pf_cut_tail()
 *
 * Remove the buffered text from offset "from" on and return it, for a caller
 * that has to drop something printed before it and then put it back.
 */
std::string
pf_cut_tail (scr_filterref_t filter, size_t from)
{
  assert (pf_is_valid (filter));

  if (from >= filter->buffer.size ())
    return std::string ();

  const std::string tail = filter->buffer.substr (from);
  pf_truncate (filter, from);
  return tail;
}


/*
 * pf_printed_to()
 * pf_restore_printed_to()
 *
 * pf_printed_to() is the buffer offset of the last pf_print_so_far(), where
 * the Runner's own string restarted after it printed the text before, or -1
 * if there is none to trust.  Unlike the frozen prefix it survives a task's
 * actions running with the buffer transferred out: pf_restore_printed_to()
 * hands back a note that pf_transfer_buffer() dropped, once the transferred
 * text is prepended again -- it leads the buffer, so the note's offset holds.
 * A note set since, on text printed in between, is the later print, and so
 * wins.
 */
scr_int
pf_printed_to (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  return filter->printed_to;
}

void
pf_restore_printed_to (scr_filterref_t filter, scr_int note)
{
  assert (pf_is_valid (filter));

  if (filter->printed_to < 0
      && note >= 0 && (size_t) note <= filter->buffer.size ())
    filter->printed_to = note;
}


/*
 * pf_erase_note()
 * pf_erase_prefix()
 * pf_erase()
 *
 * Cut the span [FROM, TO) out of the buffer and keep what follows it, for a
 * caller that has to drop text the Runner overwrote while it was still in the
 * turn's string.  Notes that pointed into the span go with it; notes past it
 * move back with their text.
 */
static void
pf_erase_note (scr_int *note, size_t from, size_t to)
{
  if (*note < 0 || (size_t) *note <= from)
    return;
  *note = (size_t) *note >= to ? *note - (scr_int) (to - from) : -1;
}

static void
pf_erase_prefix (size_t *prefix, size_t from, size_t to)
{
  if (*prefix <= from)
    return;
  *prefix = *prefix >= to ? *prefix - (to - from) : from;
}

void
pf_erase (scr_filterref_t filter, size_t from, size_t to)
{
  assert (pf_is_valid (filter));

  if (to > filter->buffer.size ())
    to = filter->buffer.size ();
  if (from >= to)
    return;

  filter->buffer.erase (from, to - from);
  pf_erase_note (&filter->auto_break_at, from, to);
  pf_erase_note (&filter->hard_break_at, from, to);
  pf_erase_note (&filter->reference_at, from, to);
  pf_erase_note (&filter->printed_to, from, to);
  pf_erase_prefix (&filter->hidden, from, to);
  pf_erase_prefix (&filter->frozen, from, to);
}


/*
 * pf_new_sentence()
 *
 * Tells the printfilter to force the next non-space character to uppercase.
 * Ignored if the printfilter is muted.
 */
void
pf_new_sentence (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  if (!filter->is_muted)
    filter->new_sentence = TRUE;
}


/*
 * pf_mute()
 * pf_clear_mute()
 *
 * A muted printfilter ignores all new text additions.
 */
void
pf_mute (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  filter->is_muted = TRUE;
}

void
pf_clear_mute (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  filter->is_muted = FALSE;
}


/*
 * pf_buffer_tag()
 *
 * Insert an HTML-like tag into the buffered output data.  The call is ignored
 * if the printfilter is muted.
 */
void
pf_buffer_tag (scr_filterref_t filter, scr_int tag)
{
  const scr_html_tags_t *entry;
  assert (pf_is_valid (filter));

  /* Search the tags table for this tag. */
  for (entry = HTML_TAGS_TABLE; entry->name; entry++)
    {
      if (tag == entry->tag)
        break;
    }

  /* If found, output the equivalent string, enclosed in '<>' characters. */
  if (entry->name)
    {
      pf_buffer_character (filter, LESSTHAN);
      pf_buffer_string (filter, entry->name);
      pf_buffer_character (filter, GREATERTHAN);
    }
  else
    scr_error ("pf_buffer_tag: invalid tag, %ld\n", tag);
}
