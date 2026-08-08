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
 * o Is the whole interpolation and ALR passes thing right?  There's no
 *   documentation on it, and it's not intuitively implemented in Adrift.
 *
 * o Is dissecting HTML tags the right thing to do?
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "scarier.h"
#include "scprotos.h"


/*
 * pf_strdup()
 *
 * Copy a std::string into a freshly scr_malloc'ed C string, so callers that
 * expect to scr_free() the result keep working unchanged.  Used as the boundary
 * between the std::string accumulators below and the engine's char* contract.
 */
static scr_char *
pf_strdup (const std::string &string)
{
  scr_char *buffer = (scr_char *) scr_malloc (string.size () + 1);
  memcpy (buffer, string.c_str (), string.size () + 1);
  return buffer;
}


/* Assorted definitions and constants. */
static const scr_uint PRINTFILTER_MAGIC = 0xb4736417;
enum
{ ITERATION_LIMIT = 32
};
static const scr_char NUL = '\0';
static const scr_char LESSTHAN = '<';
static const scr_char GREATERTHAN = '>';
static const scr_char PERCENT = '%';
static const scr_char *const ENTITY_LESSTHAN = "&lt;",
                     *const ENTITY_GREATERTHAN = "&gt;",
                     *const ENTITY_PERCENT = "+percent+";
enum
{ ENTITY_LENGTH = 4,
  PERCENT_LENGTH = 9
};
static const scr_char *const ESCAPES = "<>%&+";
static const scr_char *const WHITESPACE = "\t\n\v\f\r ";

/* Trace flag, set before running. */
static scr_bool pf_trace = FALSE;


/*
 * Table tying HTML-like tag strings to enumerated tag types.  Since it's
 * scanned sequentially by strncmp(), it's ordered so that longer strings
 * come before shorter ones.  The <br> tag is missing because this is
 * handled separately, as a simple put of '\n'.
 */
typedef struct
{
  const scr_char *const name;
  const scr_int length;
  const scr_int tag;
} scr_html_tags_t;

static const scr_html_tags_t HTML_TAGS_TABLE[] = {
  {"bgcolour", 8, SCR_TAG_BGCOLOR}, {"bgcolor", 7, SCR_TAG_BGCOLOR},
  {"waitkey", 7, SCR_TAG_WAITKEY},
  {"center", 6, SCR_TAG_CENTER}, {"/center", 7, SCR_TAG_ENDCENTER},
  {"centre", 6, SCR_TAG_CENTER}, {"/centre", 7, SCR_TAG_ENDCENTER},
  {"right", 5, SCR_TAG_RIGHT}, {"/right", 6, SCR_TAG_ENDRIGHT},
  {"font", 4, SCR_TAG_FONT}, {"/font", 5, SCR_TAG_ENDFONT},
  {"wait", 4, SCR_TAG_WAIT}, {"cls", 3, SCR_TAG_CLS},
  {"i", 1, SCR_TAG_ITALICS}, {"/i", 2, SCR_TAG_ENDITALICS},
  {"b", 1, SCR_TAG_BOLD}, {"/b", 2, SCR_TAG_ENDBOLD},
  {"u", 1, SCR_TAG_UNDERLINE}, {"/u", 2, SCR_TAG_ENDUNDERLINE},
  {"c", 1, SCR_TAG_COLOR}, {"/c", 2, SCR_TAG_ENDCOLOR},
  {NULL, 0, SCR_TAG_UNKNOWN}
};

/*
 * Printfilter structure definition.  It defines a buffer for output,
 * associated size and length, a note of any conversion to apply to the next
 * buffered character, and a flag to let the filter ignore incoming text.
 */
typedef struct scr_filter_s
{
  scr_uint magic;
  std::string buffer;
  scr_bool new_sentence;
  scr_bool is_muted;
  scr_bool needs_filtering;
} scr_filter_t;


/*
 * pf_is_valid()
 *
 * Return TRUE if pointer is a valid printfilter, FALSE otherwise.
 */
static scr_bool
pf_is_valid (scr_filterref_t filter)
{
  return filter && filter->magic == PRINTFILTER_MAGIC;
}


/*
 * pf_create()
 *
 * Create and return a new printfilter.
 */
scr_filterref_t
pf_create (void)
{
  static scr_bool initialized = FALSE;

  scr_filterref_t filter;

  /* On first call only, verify the string lengths in the table. */
  if (!initialized)
    {
      const scr_html_tags_t *entry;

      /* Compare table lengths with string lengths. */
      for (entry = HTML_TAGS_TABLE; entry->name; entry++)
        {
          if (entry->length != (scr_int) strlen (entry->name))
            {
              scr_fatal ("pf_create:"
                        " table string length is wrong for \"%s\"\n",
                        entry->name);
            }
        }

      initialized = TRUE;
    }

  /* Create a new printfilter; 'buffer' default-constructs empty. */
  filter = new scr_filter_t ();
  filter->magic = PRINTFILTER_MAGIC;
  filter->new_sentence = FALSE;
  filter->is_muted = FALSE;
  filter->needs_filtering = FALSE;

  return filter;
}


/*
 * pf_destroy()
 *
 * Destroy a printfilter and free its allocated memory.
 */
void
pf_destroy (scr_filterref_t filter)
{
  assert (pf_is_valid (filter));

  /* Poison the magic, then delete (frees the std::string buffer too). */
  filter->magic = 0;
  delete filter;
}


/*
 * pf_interpolate_vars()
 *
 * Replace %...% elements in a string by their variable values.  If any
 * variables were interpolated, returns an allocated string with replacements
 * done, otherwise returns NULL.
 *
 * If a %...% element exists that is not a variable, then it's left in as is.
 * Similarly, an unmatched (single) % in a string is also left as is.  There
 * appears to be no facility in the file format for escaping literal '%'
 * characters, and since some games have strings with this character in them,
 * this is probably all that can be done.
 */
static scr_char *
pf_interpolate_vars (const scr_char *string, scr_var_setref_t vars)
{
  std::string buffer;
  std::vector<scr_char> name;
  const scr_char *marker, *cursor;
  scr_bool buffer_used, is_interpolated;

  /*
   * Begin with an empty buffer (and "unused" flag, mirroring the original lazy
   * allocation), an unallocated name buffer, and a clear interpolation flag.
   */
  buffer_used = FALSE;
  is_interpolated = FALSE;

  /* Run through the string looking for variables. */
  marker = string;
  for (cursor = strchr (marker, PERCENT);
       cursor; cursor = strchr (marker, PERCENT))
    {
      scr_int type;
      scr_vartype_t vt_rvalue;
      scr_char close;

      /*
       * Append up to the percent character to the buffer (amortized O(1)), and
       * note the buffer as now in use.  If not yet done, allocate a name buffer
       * guaranteed long enough.
       */
      buffer.append (marker, cursor - marker);
      buffer_used = TRUE;
      if (name.empty ())
        name.resize (strlen (string) + 1);

      /*
       * Get the variable name, and from that, the value.  If we encounter a
       * mismatched '%' or unknown variable, skip it.
       */
      if (sscanf (cursor, "%%%[^%]%c", name.data (), &close) != 2
          || close != PERCENT
          || !var_get (vars, name.data (), &type, &vt_rvalue))
        {
          buffer.append (cursor, 1);
          marker = cursor + 1;
          continue;
        }

      /* Get variable value and append to the string. */
      switch (type)
        {
        case VAR_INTEGER:
          {
            scr_char value[32];

            snprintf (value, sizeof(value), "%ld", vt_rvalue.integer);
            buffer.append (value);
            break;
          }

        case VAR_STRING:
          buffer.append (vt_rvalue.string);
          break;

        default:
          scr_fatal ("pf_interpolate_vars: invalid variable type, %ld\n", type);
        }

      /* Advance over the %...% variable name, and note success. */
      marker = cursor + strlen (name.data ()) + 2;
      is_interpolated = TRUE;
    }

  /*
   * If we used the buffer and interpolated into it, append the remainder of
   * the string and return it.  If we didn't interpolate successfully (the
   * input contained a rogue '%' character), throw out the buffer as it will be
   * the same as our input, and return NULL.
   */
  if (buffer_used && is_interpolated)
    {
      buffer.append (marker);
      return pf_strdup (buffer);
    }
  return NULL;
}


/*
 * pf_replace_alr()
 *
 * Helper for pf_replace_alrs().  Replace one ALR found in the string with
 * its equivalent.  If any replacement was made, the rebuilt string is handed
 * back in 'out' and TRUE returned; otherwise 'out' is untouched and FALSE
 * returned.
 */
static scr_bool
pf_replace_alr (const scr_char *string,
                std::string &out, scr_int alr, scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[3];
  const scr_char *marker, *cursor, *original, *replacement;

  /* Retrieve the ALR original string, set replacement to NULL for now. */
  vt_key[0].string = "ALRs";
  vt_key[1].integer = alr;
  vt_key[2].string = "Original";
  original = prop_get_string (bundle, "S<-sis", vt_key);
  replacement = NULL;

  /* Ignore pathological empty originals. */
  if (original[0] == NUL)
    return FALSE;

  /* Run through the marker string looking for things to replace. */
  std::string result;
  marker = string;
  for (cursor = strstr (marker, original);
       cursor; cursor = strstr (marker, original))
    {
      /* Optimize by retrieving the replacement string only on demand. */
      if (!replacement)
        {
          vt_key[2].string = "Replacement";
          replacement = prop_get_string (bundle, "S<-sis", vt_key);
        }

      /* Append the text up to the match, then the replacement. */
      result.append (marker, cursor - marker);
      result.append (replacement);

      /* Advance over the original. */
      marker = cursor + strlen (original);
    }

  /*
   * If a replacement was made, append any trailing text and hand the rebuilt
   * string back through 'out'.  If nothing matched, leave 'out' untouched.
   */
  if (replacement)
    {
      result.append (marker);
      out = std::move (result);
    }
  return replacement != NULL;
}


/*
 * pf_replace_alrs()
 *
 * Replace any ALRs found in the string with their equivalents.  If any
 * ALRs were replaced, returns an allocated string with replacements done,
 * otherwise returns NULL.
 */
static scr_char *
pf_replace_alrs (const scr_char *string, scr_prop_setref_t bundle,
                 scr_bool alr_applied[], scr_int alr_count)
{
  scr_int index_;
  std::string current;
  const scr_char *marker;
  scr_bool replaced;

  /*
   * 'marker' is the string we replace into; it starts as the input and, once
   * any ALR fires, points into 'current' (the std::string holding the rebuilt
   * text).  std::string's amortized growth removes the need for the old two-
   * buffer alternation.
   */
  marker = string;
  replaced = FALSE;

  /* Run through each ALR that exists. */
  for (index_ = 0; index_ < alr_count; index_++)
    {
      scr_vartype_t vt_key[3];
      scr_int alr;
      std::string rebuilt;

      /*
       * Ignore ALR indexes that have already been applied.  This prevents
       * endless loops in ALR replacement.
       */
      if (alr_applied[index_])
        continue;

      /*
       * Get the actual ALR number for the ALR.  This comes from the index
       * that we sorted earlier by length of original string.  Try replacing
       * that ALR in the current marker string.
       */
      vt_key[0].string = "ALRs2";
      vt_key[1].integer = index_;
      vt_key[2].string = "ALRIndex";
      alr = prop_get_integer (bundle, "I<-sis", vt_key);

      if (pf_replace_alr (marker, rebuilt, alr, bundle))
        {
          /*
           * The string was altered.  Adopt the rebuilt text as the current
           * string and re-point marker into it for the next ALR iteration.
           * pf_replace_alr finished reading marker before returning, so moving
           * rebuilt into current (which marker may have pointed into) is safe.
           */
          current = std::move (rebuilt);
          marker = current.c_str ();
          replaced = TRUE;

          /* Note this ALR as "used up", and unavailable for future passes. */
          alr_applied[index_] = TRUE;
        }
    }

  /* Return the rebuilt string if any replacement was made, else NULL. */
  return replaced ? pf_strdup (current) : NULL;
}


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
   */
  for (entry = HTML_TAGS_TABLE; entry->name; entry++)
    {
      if (scr_strncasecmp (contents, entry->name, entry->length) == 0)
        {
          scr_char next;

          next = contents[entry->length];
          if (next == NUL || scr_isspace (next)
              || (entry->tag == SCR_TAG_BGCOLOR && next == '='))
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
  argument += (entry->tag != SCR_TAG_BGCOLOR) ? entry->length : 0;
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
 * The way Adrift does this is somewhat obscure, but the following seems to
 * replicate it well enough for most practical purposes (it's unlikely that
 * any game assumes or relies on anything not covered by this):
 *
 *  repeat some number of times
 *    repeat some number of times
 *      interpolate variables
 *    repeat [some number of times?]
 *      for each ALR unused so far this pass
 *        search the current string for the ALR original
 *        if found
 *          replace this ALR in the current string
 *          mark this ALR as used
 *    until no more changes in the current string
 *
 */
static scr_char *
pf_filter_internal (const scr_char *string,
                    scr_var_setref_t vars, scr_prop_setref_t bundle)
{
  scr_int alr_count, iteration;
  std::string current;
  scr_bool have_current;
  std::vector<scr_bool> alr_applied;
  assert (string && vars);

  if (pf_trace)
    scr_trace ("Printfilter: initial \"%s\"\n", string);

  /* If including ALRs, create a common set of ALR application flags. */
  if (bundle)
    {
      scr_vartype_t vt_key;

      /* Obtain a count of ALRs. */
      vt_key.string = "ALRs";
      alr_count = prop_get_child_count (bundle, "I<-s", &vt_key);

      /*
       * Create a new set of ALR application flags.  These are used to ensure
       * that a given ALR is applied only once on a given pass.  If the game
       * has no ALRs, leave the flag set empty.
       */
      if (alr_count > 0)
        alr_applied.assign (alr_count, FALSE);
    }
  else
    {
      /* Not including ALRs, so set alr count to 0. */
      alr_count = 0;
    }

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

      /* Note whether this iteration changes anything, to check for no change. */
      changed = FALSE;

      for (inner_iteration = 0;
           inner_iteration < ITERATION_LIMIT; inner_iteration++)
        {
          /*
           * Interpolate variables.  If any changes were made, adopt the
           * interpolated version as current, freeing the returned char*.
           * Work on the current string, if any, otherwise the input string.
           */
          intermediate = pf_interpolate_vars (have_current ? current.c_str ()
                                                           : string, vars);
          if (intermediate)
            {
              current = intermediate;
              scr_free (intermediate);
              have_current = TRUE;
              changed = TRUE;
              if (pf_trace)
                {
                  scr_trace ("Printfilter: interpolated [%ld,%ld] \"%s\"\n",
                            iteration, inner_iteration, current.c_str ());
                }
            }
          else
            break;
        }

      /* If we have ALRs to process, search out and replace all findable. */
      if (alr_count > 0)
        {
          /* Replace ALRs until no more ALRs can be found. */
          inner_iteration = 0;
          while (TRUE)
            {
              /*
               * Replace ALRs, and adopt current as for variables above.
               * Leave the loop when ALR replacements stop.  Again, work on
               * the current string if any, otherwise the input string.
               */
              intermediate = pf_replace_alrs (have_current ? current.c_str ()
                                                           : string,
                                              bundle, alr_applied.data (),
                                              alr_count);
              if (intermediate)
                {
                  current = intermediate;
                  scr_free (intermediate);
                  have_current = TRUE;
                  changed = TRUE;
                  if (pf_trace)
                    {
                      scr_trace ("Printfilter: replaced [%ld,%ld] \"%s\"\n",
                                iteration, inner_iteration, current.c_str ());
                    }
                }
              else
                break;
              inner_iteration++;
            }
        }

      /* If nothing changed this iteration, stop now. */
      if (!changed)
        break;
    }

  /* Return an allocated current, or NULL if nothing changed. */
  return have_current ? pf_strdup (current) : NULL;
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
      current = (decltype(current)) scr_malloc (strlen (string) + 1);
      memcpy (current, string, strlen (string) + 1);
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
  current = pf_filter_internal (string, vars, NULL);

  /* Our contract is to return an allocated string; copy if required. */
  if (!current)
    {
      current = (decltype(current)) scr_malloc (strlen (string) + 1);
      memcpy (current, string, strlen (string) + 1);
    }

  return current;
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
       * Filter the buffered string, then print it untagged.  Remember to free
       * the filtered version.  If filtering made no difference, or if the
       * buffer was already filtered by, say, checkpointing, just print the
       * original buffer untagged instead.
       */
      if (filter->needs_filtering)
        {
          scr_char *filtered;

          filtered = pf_filter_internal (filter->buffer.c_str (), vars, bundle);
          if (filtered)
            {
              pf_output_untagged (filtered);
              scr_free (filtered);
            }
          else
            pf_output_untagged (filter->buffer.c_str ());
        }
      else
        pf_output_untagged (filter->buffer.c_str ());

      /* Remove buffered data. */
      filter->buffer.clear ();
      filter->needs_filtering = FALSE;
    }

  /* Reset new sentence and mute flags. */
  filter->new_sentence = FALSE;
  filter->is_muted = FALSE;
}


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
        {
          scr_char *filtered;

          filtered = pf_filter_internal (filter->buffer.c_str (), vars, bundle);
          if (filtered)
            {
              filter->buffer.assign (filtered);
              scr_free (filtered);
            }
        }

      /* Note the buffer as filtered, to avoid pointless filtering. */
      filter->needs_filtering = FALSE;
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

  /* Return the buffer if it holds any text, otherwise NULL. */
  if (!filter->buffer.empty ())
    return filter->buffer.c_str ();
  else
    return NULL;
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
      retval = pf_strdup (filter->buffer);

      /* Clear all filter fields down to empty values. */
      filter->buffer.clear ();
      filter->new_sentence = FALSE;
      filter->is_muted = FALSE;
      filter->needs_filtering = FALSE;

      /* Return the allocated buffered text. */
      return retval;
    }
  else
    return NULL;
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
  filter->new_sentence = FALSE;
  filter->is_muted = FALSE;
  filter->needs_filtering = FALSE;
}


/*
 * pf_buffer_string()
 * pf_buffer_character()
 *
 * Add a string, and a single character, to the printfilter buffer.  If muted,
 * these functions do nothing.
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

      /* Note append start, then append the string to the buffer. */
      noted = filter->buffer.size ();
      pf_append_string (filter, string);

      /* Adjust the first character of the appended string if flagged. */
      if (filter->new_sentence && noted < filter->buffer.size ())
        filter->buffer[noted] = scr_toupper (filter->buffer[noted]);

      /* Clear new sentence, and note as currently needing filtering. */
      filter->needs_filtering = TRUE;
      filter->new_sentence = FALSE;
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
 * pf_buffer_paragraph()
 *
 * Buffer a block of text that conventionally begins with its own line break(s)
 * for spacing -- Adrift event and atmosphere texts typically start with "<br>"
 * or "<br><br>".  The Adrift runner relies on those leading breaks alone for
 * paragraph spacing, whereas SCARIER also terminates the preceding room
 * description and contents with a newline of its own.  To avoid a doubled blank
 * line, if the buffer already ends with a break and this text leads with one,
 * drop a single leading break from the text before appending it.
 */
void
pf_buffer_paragraph (scr_filterref_t filter, const scr_char *string)
{
  const scr_char *buffered;

  assert (pf_is_valid (filter));
  assert (string);

  buffered = pf_get_buffer (filter);
  if (buffered
      && pf_text_ends_with_break (buffered)
      && pf_text_leads_with_break (string))
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

  buffered = pf_get_buffer (filter);
  if (!buffered || !pf_text_ends_with_break (buffered))
    pf_buffer_character (filter, '\n');
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

          /* Clear new sentence, and note as currently needing filtering. */
          filter->needs_filtering = TRUE;
          filter->new_sentence = FALSE;
        }
      else
        /* No data, so the call is equivalent to a normal buffer. */
        pf_buffer_string (filter, string);
    }
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


/*
 * pf_strip_tags_common()
 *
 * Strip HTML-like tags from a string.  Used to process strings used in ways
 * other than being passed to if_print_string(), for example room names and
 * status lines.  It ignores all tags except <br>, which it replaces with
 * a newline if requested by allow_newlines.
 */
static void
pf_strip_tags_common (scr_char *string, scr_bool allow_newlines)
{
  scr_char *marker, *cursor;

  /* Run through the string looking for <...> tags. */
  marker = string;
  for (cursor = (scr_char *) strchr (marker, LESSTHAN);
       cursor; cursor = (scr_char *) strchr (marker, LESSTHAN))
    {
      scr_char *tag_end;

      /* Locate tag end, and break if unterminated. */
      tag_end = strchr (cursor, GREATERTHAN);
      if (!tag_end)
        break;

      /* If the tag is <br>, replace with newline if requested. */
      if (allow_newlines)
        {
          if (tag_end - cursor == 3
              && scr_strncasecmp (cursor + 1, "br", 2) == 0)
            *cursor++ = '\n';
        }

      /* Remove the tag from the string, then advance input. */
      memmove (cursor, tag_end + 1, strlen (tag_end));
      marker = cursor;
    }
}


/*
 * pf_strip_tags()
 * pf_strip_tags_for_hints()
 *
 * Public interfaces to pf_strip_tags_common().  The hints version will
 * allow <br> tags to map into newlines in hints strings.
 */
void
pf_strip_tags (scr_char *string)
{
  pf_strip_tags_common (string, FALSE);
}

void
pf_strip_tags_for_hints (scr_char *string)
{
  pf_strip_tags_common (string, TRUE);
}


/*
 * pf_escape()
 *
 * Escape <, >, and % characters in the input string.  Used to filter player
 * input prior to storing in referenced text.
 *
 * Adrift offers no escapes for & and + escapes, so for these we convert to
 * the character itself followed by a space.  The return string is malloc'ed,
 * so the caller needs to remember to free it.
 */
scr_char *
pf_escape (const scr_char *string)
{
  const scr_char *marker, *cursor;
  std::string buffer;

  /* Run through the string looking for <, >, %, or other escapes. */
  marker = string;
  for (cursor = marker + strcspn (marker, ESCAPES);
       cursor[0] != NUL; cursor = marker + strcspn (marker, ESCAPES))
    {
      const scr_char *escape;
      scr_char escape_buffer[3];

      /* Extend buffer to hold the string so far. */
      if (cursor > marker)
        buffer.append (marker, cursor - marker);

      /* Determine the appropriate character escape. */
      if (cursor[0] == LESSTHAN)
        escape = ENTITY_LESSTHAN;
      else if (cursor[0] == GREATERTHAN)
        escape = ENTITY_GREATERTHAN;
      else if (cursor[0] == PERCENT)
        escape = ENTITY_PERCENT;
      else
        {
          /*
           * No real escape available, so fake, badly, by appending a space
           * for cases where we've encountered a character entity; leave
           * others untouched.
           */
          escape_buffer[0] = cursor[0];
          if (scr_strncasecmp (cursor,
                              ENTITY_LESSTHAN, ENTITY_LENGTH) == 0
              || scr_strncasecmp (cursor,
                                 ENTITY_GREATERTHAN, ENTITY_LENGTH) == 0
              || scr_strncasecmp (cursor,
                                 ENTITY_PERCENT, PERCENT_LENGTH) == 0)
            {
              escape_buffer[1] = ' ';
              escape_buffer[2] = NUL;
            }
          else
            escape_buffer[1] = NUL;
          escape = escape_buffer;
        }

      buffer.append (escape);

      /* Pass over character escaped and continue. */
      cursor++;
      marker = cursor;
    }

  /* Add all remaining characters to the buffer. */
  if (cursor > marker)
    buffer.append (marker, cursor - marker);

  return pf_strdup (buffer);
}


/*
 * pf_compare_words()
 *
 * Matches multiple words from words in string.  Returns the extent of
 * the match if the string matched, 0 otherwise.
 */
static scr_int
pf_compare_words (const scr_char *string, const scr_char *words)
{
  scr_int word_posn, posn;

  /* None expected, but skip leading space. */
  for (word_posn = 0; scr_isspace (words[word_posn]) && words[word_posn] != NUL;)
    word_posn++;

  /* Match characters from words with the string at position. */
  posn = 0;
  while (TRUE)
    {
      /* Any character mismatch means no words match. */
      if (scr_tolower (words[word_posn]) != scr_tolower (string[posn]))
        return 0;

      /* Move to next character in each. */
      word_posn++;
      posn++;

      /*
       * If at space, advance over whitespace in words list.  Stop when we
       * hit the end of the words list.
       */
      while (scr_isspace (words[word_posn]) && words[word_posn] != NUL)
        word_posn++;
      if (words[word_posn] == NUL)
        break;

      /*
       * About to match another word, so advance over whitespace in the
       * current string too.
       */
      while (scr_isspace (string[posn]) && string[posn] != NUL)
        posn++;
    }

  /*
   * We reached the end of words.  If we're at the end of the match string,
   * or at spaces, we've matched.
   */
  if (scr_isspace (string[posn]) || string[posn] == NUL)
    return posn;

  /* More text after the match, so it's not quite a match. */
  return 0;
}


/*
 * pf_filter_input()
 *
 * Applies synonym changes to a player input string, and returns the resulting
 * string to the caller, or NULL if no synonym changes were needed.  The
 * return string is malloc'ed, so the caller needs to remember to free it.
 */
scr_char *
pf_filter_input (const scr_char *string, scr_prop_setref_t bundle)
{
  scr_vartype_t vt_key[3];
  scr_int synonym_count;
  std::string buffer;
  scr_bool modified;
  const scr_char *current;
  scr_int offset;
  assert (string && bundle);

  if (pf_trace)
    scr_trace ("Printfilter: input \"%s\"\n", string);

  /* Obtain a count of synonyms. */
  vt_key[0].string = "Synonyms";
  synonym_count = prop_get_child_count (bundle, "I<-s", vt_key);

  /*
   * 'current' is the active string -- the input until a synonym fires, then
   * the 'buffer' copy (basic copy-on-write).  'offset' is our position within
   * it; we work by offset because std::string edits may reallocate.
   */
  modified = FALSE;
  current = string;

  /* Loop over each word in the string. */
  offset = strspn (current, WHITESPACE);
  while (current[offset] != NUL)
    {
      scr_int index_, extent;

      /* Search for a synonym match at this index into the buffer. */
      extent = 0;
      for (index_ = 0; index_ < synonym_count; index_++)
        {
          const scr_char *original;

          /* Retrieve the synonym original string. */
          vt_key[0].string = "Synonyms";
          vt_key[1].integer = index_;
          vt_key[2].string = "Original";
          original = prop_get_string (bundle, "S<-sis", vt_key);

          /* Compare the original at this point. */
          extent = pf_compare_words (current + offset, original);
          if (extent > 0)
            break;
        }

      /*
       * If a synonym found was, index_ indicates it, and extent shows how
       * much of the buffer to replace with it.
       */
      if (index_ < synonym_count && extent > 0)
        {
          const scr_char *replacement;
          scr_int length;

          /*
           * If not yet copied, copy the input string into the buffer now and
           * switch 'current' to it.  The offset is unchanged since the buffer
           * starts as an exact copy.  More basic copy-on-write.
           */
          if (!modified)
            {
              buffer = string;
              modified = TRUE;
            }

          /* Find the replacement text for this synonym. */
          vt_key[0].string = "Synonyms";
          vt_key[1].integer = index_;
          vt_key[2].string = "Replacement";
          replacement = prop_get_string (bundle, "S<-sis", vt_key);
          length = strlen (replacement);

          /* Splice the replacement in for the matched extent. */
          buffer.replace (offset, extent, replacement, length);
          current = buffer.c_str ();

          /* Adjust offset to skip over the replacement. */
          offset += length;

          if (pf_trace)
            scr_trace ("Printfilter: synonym \"%s\"\n", buffer.c_str ());
        }
      else
        {
          /* If no match, advance over the unmatched word. */
          offset += strcspn (current + offset, WHITESPACE);
        }

      /* Set offset to the next word start. */
      offset += strspn (current + offset, WHITESPACE);
    }

  /* Return the final string, or NULL if no synonym replacements. */
  return modified ? pf_strdup (buffer) : NULL;
}


/*
 * pf_debug_trace()
 *
 * Set filter tracing on/off.
 */
void
pf_debug_trace (scr_bool flag)
{
  pf_trace = flag;
}
