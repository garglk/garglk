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
 * The output side of the printfilter: tag dissection and entity
 * unescaping, the filter passes (variables, then ALRs), pf_filter() and
 * friends, and pf_flush().
 */

#include "scprintf_internal.h"

/*
 * pf_output_text()
 *
 * Edit the tag-stripped text element passed in, substituting &lt; &gt;
 * +percent+ with < > %, then send to the OS-specific output functions.
 */
static void
pf_output_text (const scr_char *string)
{
  scr_int index_;
  std::string buffer;

  /* Optimize away the allocation and copy if possible. */
  if (!(strstr (string, ENTITY_LESSTHAN)
        || strstr (string, ENTITY_GREATERTHAN)
        || strstr (string, ENTITY_PERCENT)))
    {
      if_print_string (string);
      return;
    }

  /*
   * Copy characters from the string into the buffer, replacing any &..;
   * elements by their single-character equivalents.  We also replace any
   * +percent+ elements by percent characters; apparently an undocumented
   * Adrift Runner extension.
   */
  buffer.reserve (strlen (string));
  for (index_ = 0; string[index_] != NUL; index_++)
    {
      if (scr_strncasecmp (string + index_,
                          ENTITY_LESSTHAN, ENTITY_LENGTH) == 0)
        {
          buffer.push_back (LESSTHAN);
          index_ += ENTITY_LENGTH - 1;
        }
      else if (scr_strncasecmp (string + index_,
                               ENTITY_GREATERTHAN, ENTITY_LENGTH) == 0)
        {
          buffer.push_back (GREATERTHAN);
          index_ += ENTITY_LENGTH - 1;
        }
      else if (scr_strncasecmp (string + index_,
                               ENTITY_PERCENT, PERCENT_LENGTH) == 0)
        {
          buffer.push_back (PERCENT);
          index_ += PERCENT_LENGTH - 1;
        }
      else
        buffer.push_back (string[index_]);
    }

  /* Print the rebuilt string. */
  if_print_string (buffer.c_str ());
}


/*
 * pf_output_tag()
 *
 * Output an HTML-like tag element to the OS-specific tag handling function.
 */
static void
pf_output_tag (const scr_char *contents)
{
  const scr_html_tags_t *entry;
  const scr_char *argument;

  /* For a simple <br> tag, just print out a newline. */
  if (scr_compare_word (contents, "br", 2))
    {
      if_print_character ('\n');
      return;
    }

  /*
   * Search for the name in the HTML tags table.  It should be a full match,
   * that is, the character after the matched name must be space or NUL.
   * The <bgcolour="xyz"> tag is the exception; here the terminator is '='.
   * So is <wait>: run400 (Proc at 47A82C) tests Left(LCase(tag), 5) =
   * "<wait" and takes Val() of the rest, so "<wait3>" is a three-second
   * pause exactly like "<wait 3>" (Through time writes the spaceless form
   * throughout).  <waitkey> sits earlier in the table and still wins.
   */
  for (entry = HTML_TAGS_TABLE; entry->name; entry++)
    {
      if (scr_strncasecmp (contents, entry->name, entry->length) == 0)
        {
          scr_char next;

          next = contents[entry->length];
          /*
           * <waitkey> is an exact-string test in run400 (47A779 compares
           * the whole tag with "<waitkey>"); anything else starting "<wait"
           * is a timed <wait> whose Val() of the remainder decides the
           * delay, so "<waitkey 4>" is a zero-second wait and NOT a
           * keypress pause.  Vardock Bates (4.00) writes one after the
           * Jhave wall; run400 does not stop there (transcript 2026-08-29).
           */
          if (entry->tag == SCR_TAG_WAITKEY)
            {
              if (next == NUL)
                break;
              continue;
            }
          if (next == NUL || scr_isspace (next)
              || (entry->tag == SCR_TAG_BGCOLOUR && next == '=')
              || entry->tag == SCR_TAG_WAIT)
            break;
        }
    }

  /* If not matched, output an unknown tag with contents as its argument. */
  if (!entry->name)
    {
      if_print_tag (SCR_TAG_UNKNOWN, contents);
      return;
    }

  /*
   * Find the argument by skipping the tag name and any spaces.  Again, for
   * <bgcolour="xyz">, make a special case, passing the complete contents as
   * argument (to match <font colour=...> for the client.
   */
  argument = contents;
  argument += (entry->tag != SCR_TAG_BGCOLOUR) ? entry->length : 0;
  while (scr_isspace (argument[0]))
    argument++;
  if_print_tag (entry->tag, argument);
}


/*
 * pf_output_untagged()
 *
 * Break apart HTML-like string into normal text elements, and HTML-like
 * tags.
 */
static void
pf_output_untagged (const scr_char *string)
{
  scr_char *untagged, *contents, *cursor;
  const scr_char *marker;

  /*
   * Optimize away the allocation and copy if possible.  We need to check
   * here both for tags and for entities; only if neither occurs is it safe
   * to output the string directly.
   */
  if (!strchr (string, LESSTHAN)
      && !(strstr (string, ENTITY_LESSTHAN)
           || strstr (string, ENTITY_GREATERTHAN)
           || strstr (string, ENTITY_PERCENT)))
    {
      if_print_string (string);
      return;
    }

  /*
   * Create a general temporary string, and alias it to both untagged text
   * and the tag name, for sharing inside the loop.
   */
  std::vector<scr_char> temporary (strlen (string) + 1);
  untagged = contents = temporary.data ();

  /* Run through the string looking for <...> tags. */
  marker = string;
  for (cursor = (scr_char *) strchr (marker, LESSTHAN);
       cursor; cursor = (scr_char *) strchr (marker, LESSTHAN))
    {
      scr_char close;

      /* Handle characters up to the tag start; untagged text. */
      memcpy (untagged, marker, cursor - marker);
      untagged[cursor - marker] = NUL;
      pf_output_text (untagged);

      /* Catch and ignore completely empty tags. */
      if (cursor[1] == GREATERTHAN)
        {
          marker = cursor + 2;
          continue;
        }

      /*
       * Get the text within the tag, reusing the temporary buffer.  If this
       * fails, allow the remainder of the line to be delivered as a tag;
       * unknown, probably.
       */
      if (sscanf (cursor, "<%[^>]%c", contents, &close) != 2
          || close != GREATERTHAN)
        {
          if (sscanf (cursor, "<%[^>]", contents) != 1)
            {
              scr_error ("pf_output_untagged: mismatched '%c'\n", LESSTHAN);
              if_print_character (LESSTHAN);
              marker = cursor + 1;
              continue;
            }
        }

      /* Output tag, and advance marker over the <...> tag. */
      if (!scr_strempty (contents))
        pf_output_tag (contents);
      marker = cursor + strlen (contents) + 1;
      marker += (marker[0] == GREATERTHAN) ? 1 : 0;
    }

  /* Output any remaining string text; the temporary buffer frees itself. */
  pf_output_text (marker);
}


/*
 * pf_filter_internal()
 *
 * Filters an output string, interpolating variables and replacing ALR's.  If
 * any filtering was done, returns an allocated string that the caller needs
 * to free; otherwise, return NULL.
 *
 * Bundle may be NULL, requesting that the function suppress ALR replacements,
 * and do only variables; used for game info strings.
 *
 * How many times the Runner walks the ALR list is a version split, and both
 * halves are measured rather than argued (harness/make_400_alrprobe.py and
 * harness/make_39_alrprobe.py, one task per cell, run under run400.exe and
 * run390.exe in Wine on 2026-08-24):
 *
 *                      ALRs                       run390     run400
 *    "AAA."      AAA -> qAAA                      qAAA.      qqAAA.
 *    "EEE."      EEE -> EEE EEE                   EEE EEE.   EEE EEE EEE EEE.
 *    "RRR."      PPPP -> QQ, RRR -> PPPP          PPPP.      QQ.
 *    "UUU."      WWWWW -> done, VVVV -> WWWWW,
 *                UUU -> VVVV                      VVVV.      done.
 *    "MMM."      MM -> short, MMM -> long         long.      long.
 *    "ZZ ZZ."    ZZ -> z                          z z.       z z.
 *
 * Both versions make ONE pass down the length-sorted list, replacing every
 * occurrence of each original; a chain only runs in the direction of the walk,
 * which is what stops run390's "RRR." at "PPPP." (PPPP was already behind the
 * cursor when RRR produced it).  What 4.0 adds is not a second pass but
 * RECURSION: it filters each replacement before splicing it in, so "UUU."
 * runs all three hops and "RRR." both inside the one pass.  The two routines
 * are read out in full above pf_alr_walk(); run390's is the loop at
 * loc_45BD43, the tail of its output filter Proc_2_28_45CBD0
 * (run390_3.bas:55465), and run400's is Proc_21_20_44C7DC.
 *
 * That distinction is measurable, and measured: a repeated pass would let a
 * replacement combine with the text around it to spell some other ALR's
 * original, and the recursion cannot.  See pf_alr_walk() for Qui a tue Dana's
 * "Vous vous deplacez in.".
 *
 * A self-containing ALR is held to one expansion per walk not by a retirement
 * flag -- SCARE's, which this used to keep -- but by 4.0's whole-text equality
 * test: filtering [Okay.  I put ] for the ALR [I put ] -> [Okay.  I put ]
 * finds the original, sees the text IS the replacement and hands it back
 * untouched.  Every other ALR fires whenever its original is in front of the
 * cursor.  Measured, with [TOKEN] -> [tok] and [zebra] -> [TOKEN] over
 * "TOKEN zebra.":
 *
 *                                                 run390     run400
 *    "TOKEN zebra."                               tok TOKEN. tok tok.
 *
 * SCARE retired every ALR that fired, which gives run390's answer for a
 * version 4.0 game.
 *
 * All of the above is one walk over the whole turn's text at flush.  Version
 * 4.0 gives ONE KIND OF TEXT a second walk of its own: a completing task's
 * CompleteText and AdditionalMessage, filtered again as the task prints them
 * (see pf_refilter() below, and the measurement in
 * harness/make_400_alrsrcprobe.py).  Every cell of the probe above is a task
 * CompleteText, so that second walk is where run400's extra "q" and its third
 * and fourth "EEE" come from -- one recursive walk of "AAA." gives "qAAA.",
 * and walking that again gives "qqAAA.".  That is the humbug (4.00) divergence
 * this all started from -- its "[I put ] -> [Okay.  I put ]" is
 * self-containing, so task 80's CompleteText answers `Put sweet on plinth`
 * with "Okay.  Okay.  I put the sweet on the plinth."
 * (runner_probes/humbug.run400.txt:841) while the library's own put, in the
 * same transcript, says "Okay.  I put the watch onto the rectangular table."
 * with a single "Okay.".
 *
 * Scarier deliberately does not reproduce that multiplication (deviation
 * policy): pf_replace_alr() leaves a self-containing ALR's original alone
 * where it already sits inside a copy of its replacement, so every walk still
 * runs -- variables still freeze where they did -- but "Okay." prints once.
 *
 * Versions 3.8 and 3.7 cannot reach any of this: neither schema in
 * sctafpar.cpp carries an ALRs section, so those games have no ALRs at all.
 */
static scr_char *
pf_filter_internal (const scr_char *string,
                    scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  scr_int alr_count, iteration;
  scr_bool alr_single_pass, alr_pass_done;
  std::string current;
  scr_bool have_current;
  assert (string && vars);

  if (pf.trace)
    scr_trace ("Printfilter: initial \"%s\"\n", string);

  /* If including ALRs, create a common set of ALR application flags. */
  if (bundle)
    {
      scr_vartype_t vt_key;

      /* Obtain a count of ALRs. */
      vt_key.string = "ALRs";
      alr_count = prop_get_child_count (bundle, "I<-s", &vt_key);

      /*
       * What a walk of the ALR list is -- the measured version split
       * described above.  Both versions get one walk; 4.0's filters each
       * replacement recursively on the way in, 3.9's does not.
       */
      alr_single_pass = prop_get_taf_version (bundle) < TAF_VERSION_400;
    }
  else
    {
      /* Not including ALRs, so set alr count to 0. */
      alr_count = 0;
      alr_single_pass = FALSE;
    }
  alr_pass_done = FALSE;

  /*
   * Loop for a sort-of arbitrary number of passes; probably enough.
   * 'have_current' tracks whether any replacement has produced a 'current'
   * string yet; until then we work on the input 'string'.
   */
  have_current = FALSE;
  for (iteration = 0; iteration < ITERATION_LIMIT; iteration++)
    {
      scr_int inner_iteration;
      scr_bool changed;
      scr_char *intermediate;

      /* Note whether this iteration changes anything. */
      changed = FALSE;

      for (inner_iteration = 0;
           inner_iteration < ITERATION_LIMIT; inner_iteration++)
        {
          /*
           * Interpolate variables.  If any changes were made, adopt the
           * interpolated version as current, freeing the returned char*.
           * Work on the current string, if any, otherwise the input string.
           */
          intermediate
            = pf_interpolate_vars (have_current ? current.c_str () : string,
                                   vars);
          if (intermediate)
            {
              current = intermediate;
              scr_free (intermediate);
              have_current = TRUE;
              changed = TRUE;
              if (pf.trace)
                {
                  scr_trace ("Printfilter: interpolated [%ld,%ld] \"%s\"\n",
                            iteration, inner_iteration, current.c_str ());
                }
            }
          else
            break;
        }

      /* If we have ALRs to process, search out and replace all findable. */
      if (alr_count > 0 && !alr_pass_done)
        {
          /*
           * Both versions walk the list exactly once for the whole filter --
           * not once per iteration of the loop we are in.  What differs is
           * what a walk does with a replacement: 4.0 filters it recursively
           * before splicing it in, 3.9 splices it verbatim.  See
           * pf_alr_walk().
           */
          alr_pass_done = TRUE;

          intermediate
            = pf_replace_alrs (have_current ? current.c_str () : string,
                               vars, bundle, alr_count, !alr_single_pass);
          if (intermediate)
            {
              current = intermediate;
              scr_free (intermediate);
              have_current = TRUE;
              changed = TRUE;
              if (pf.trace)
                {
                  scr_trace ("Printfilter: replaced [%ld,-] \"%s\"\n",
                            iteration, current.c_str ());
                }
            }
        }

      /* If nothing changed this iteration, stop now. */
      if (!changed)
        break;
    }

  /* Return an allocated current, or NULL if nothing changed. */
  return have_current ? scr_strdup (current) : nullptr;
}


/*
 * pf_filter()
 *
 * A facet of pf_filter_internal().  Filter an output string, interpolating
 * variables and replacing ALR's.  Returns an allocated string that the caller
 * needs to free.
 */
scr_char *
pf_filter (const scr_char *string,
           scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  scr_char *current;

  /* Filter this string, including ALRs replacements. */
  current = pf_filter_internal (string, vars, bundle);

  /* Our contract is to return an allocated string; copy if required. */
  if (!current)
    {
      current = scr_strdup (string);
    }

  return current;
}


/*
 * pf_filter_for_info()
 *
 * A facet of pf_filter_internal().  Filters output, interpolating variables
 * only (no ALR replacement), and returns the resulting string to the caller.
 * Used on informational strings such as the game title and author.  Returns
 * an allocated string that the caller needs to free.
 */
scr_char *
pf_filter_for_info (const scr_char *string, scr_var_setref_t vars)
{
  scr_char *current;

  /* Filter this string, excluding ALRs replacements. */
  current = pf_filter_internal (string, vars, nullptr);

  /* Our contract is to return an allocated string; copy if required. */
  if (!current)
    {
      current = scr_strdup (string);
    }

  return current;
}


/*
 * pf_filter_buffer()
 *
 * Filter the buffer in place, leaving alone any frozen prefix the Runner has
 * already printed.  Returns TRUE if the text changed.
 */
scr_bool
pf_filter_buffer (scr_filterref_t filter,
                  scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  scr_char *filtered;

  if (filter->frozen >= filter->buffer.size ())
    return FALSE;

  filtered = pf_filter_internal (filter->buffer.c_str () + filter->frozen,
                                 vars, bundle);
  if (!filtered)
    return FALSE;

  /* The note cannot say where its text went if filtering moved it. */
  if (filter->printed_to > (scr_int) filter->frozen
      && strlen (filtered) != filter->buffer.size () - filter->frozen)
    filter->printed_to = -1;
  filter->buffer.replace (filter->frozen, std::string::npos, filtered);
  scr_free (filtered);
  return TRUE;
}


/*
 * pf_flush()
 *
 * Filter buffered data, interpolating variables and replacing ALR's, and
 * send the resulting string to the output channel.
 */
void
pf_flush (scr_filterref_t filter,
          scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  assert (pf_is_valid (filter));
  assert (vars && bundle);

  /* See if there is any buffered data to flush. */
  if (!filter->buffer.empty ())
    {
      /*
       * Filter the buffered string unless checkpointing already has, then
       * print it untagged.
       */
      if (filter->needs_filtering)
        pf_filter_buffer (filter, vars, bundle);

      filter->printed.append (filter->buffer);
      pf_output_untagged (filter->buffer.c_str ());

      /* Remove buffered data. */
      filter->buffer.clear ();
      filter->needs_filtering = FALSE;
    }
  filter->frozen = 0;
  filter->printed_to = -1;

  /* Reset new sentence and mute flags. */
  filter->new_sentence = FALSE;
  filter->is_muted = FALSE;
}
