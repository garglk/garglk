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

/*
 * Module notes:
 *
 * o Is the whole interpolation and ALR passes thing right?  There's
 *   no documentation on it, and it's not intuitively implemented in
 *   Adrift.
 *
 * o Is dissecting HTML tags the right thing to do?
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "scare.h"
#include "scprotos.h"


/* Assorted definitions and constants. */
static const sc_uint32	PRINTFILTER_MAGIC	= 0xB4736417;
static const sc_uint32	BUFFER_GROW_INCREMENT	= 32;
static const sc_char	PERCENT			= '%',
			NUL			= '\0',
			LESSTHAN		= '<',
			GREATERTHAN		= '>';

/* Trace flag, set before running. */
static sc_bool		pf_trace		= FALSE;

/* Enumerated variable types. */
enum {			VAR_INTEGER		= 'I',
			VAR_STRING		= 'S' };

/*
 * Table tying HTML-like tag strings to enumerated tag types.  Since it's
 * scanned sequentially by strncmp(), it's ordered so that longer strings
 * come before shorter ones.  The <br> tag is missing because this is
 * handled separately, as a simple put of '\n'.
 */
static const struct {
	const sc_char		*tag_string;
	const sc_uint32		tag;
} HTML_TAGS[] = {
	{ "bgcolour",  SC_TAG_BGCOLOR   },
	{ "bgcolor",   SC_TAG_BGCOLOR   },
	{ "waitkey",   SC_TAG_WAITKEY   },
	{ "center",    SC_TAG_CENTER    }, { "/center",  SC_TAG_ENDCENTER    },
	{ "centre",    SC_TAG_CENTER    }, { "/centre",  SC_TAG_ENDCENTER    },
	{ "right",     SC_TAG_RIGHT     }, { "/right",   SC_TAG_ENDRIGHT     },
	{ "font",      SC_TAG_FONT      }, { "/font",    SC_TAG_ENDFONT      },
	{ "wait",      SC_TAG_WAIT      },
	{ "cls",       SC_TAG_CLS       },
	{ "i",         SC_TAG_ITALICS   }, { "/i",       SC_TAG_ENDITALICS   },
	{ "b",         SC_TAG_BOLD      }, { "/b",       SC_TAG_ENDBOLD      },
	{ "u",         SC_TAG_UNDERLINE }, { "/u",       SC_TAG_ENDUNDERLINE },
	{ "c",         SC_TAG_SCOLOR    }, { "/c",       SC_TAG_ENDSCOLOR    },
	{ NULL,	       SC_TAG_UNKNOWN   }
};

/*
 * Printfilter structure definition.  It defines a buffer for output,
 * associated size and length, and a note of any conversion to apply to
 * the next buffered character.
 */
typedef struct sc_printfilter_s {
	sc_uint32		magic;
	sc_uint32		buffer_length;
	sc_uint32		buffer_size;
	sc_char			*buffer;
	sc_bool			new_sentence;
} sc_printfilter_t;


/*
 * pf_create()
 *
 * Create and return a new printfilter.
 */
sc_printfilterref_t
pf_create (void)
{
	sc_printfilterref_t	filter;

	/* Create a new printfilter. */
	filter = sc_malloc (sizeof (*filter));
	filter->magic		= PRINTFILTER_MAGIC;
	filter->buffer_length	= 0;
	filter->buffer_size	= 0;
	filter->buffer		= NULL;
	filter->new_sentence	= FALSE;

	/* Return the new printfilter. */
	return filter;
}


/*
 * pf_destroy()
 *
 * Destroy a printfilter and free its allocated memory.
 */
void
pf_destroy (sc_printfilterref_t filter)
{
	assert (filter != NULL && filter->magic == PRINTFILTER_MAGIC);

	/* Free any buffer space. */
	if (filter->buffer != NULL)
		sc_free (filter->buffer);

	/* Shred and free the printfilter. */
	memset (filter, 0, sizeof (*filter));
	sc_free (filter);
}


/*
 * pf_interpolate()
 *
 * Replace %...% elements in a string by their variable values.  Return
 * a malloc'ed string with replacements done, and set changed if any
 * variables were found and replaced.
 *
 * If a %...% element exists that is not a variable, then it's left in
 * as is.  Similarly, an unmatched (single) % in a string is also left
 * as is.  There appears to be no facility in the file format for escaping
 * literal % characters, and since some games have strings with this
 * character in them, this is probably all that can be done.
 */
static sc_char *
pf_interpolate (const sc_char *string, sc_var_setref_t vars,
		sc_bool *changed)
{
	sc_char		*obuf, *vptr;
	sc_char		*vname;
	const sc_char	*iptr;

	/* Initially, no changes made. */
	*changed = FALSE;

	/* Size vname to the same size as the input string. */
	vname = sc_malloc (strlen (string) + 1);

	/* Start with an empty output buffer. */
	obuf = sc_malloc (1);
	strcpy (obuf, "");

	/* Run through the string looking for variables. */
	iptr = string;
	for (vptr = strchr (iptr, PERCENT);
			vptr != NULL; vptr = strchr (iptr, PERCENT))
	    {
		sc_char		type;
		sc_vartype_t	vt_rvalue;

		/* Append buffer up to percent character. */
		obuf = sc_realloc (obuf, strlen (obuf) + vptr - iptr + 1);
		strncat (obuf, iptr, vptr - iptr);

		/* Get the variable name. */
		if (sscanf (vptr, "%%%[^%]%%", vname) != 1)
		    {
			/*
			 * Mismatched % -- append the single % character
			 * and continue.
			 */
			obuf = sc_realloc (obuf, strlen (obuf) + 2);
			strncat (obuf, vptr, 1);
			iptr = vptr + 1;
			continue;
		    }

		/* Get the variable value, expected to exist. */
		if (!var_get (vars, vname, &type, &vt_rvalue))
		    {
			/*
			 * The %...% substring wasn't a known variable
			 * name -- as above, append the single % character
			 * and continue.
			 */
			obuf = sc_realloc (obuf, strlen (obuf) + 2);
			strncat (obuf, vptr, 1);
			iptr = vptr + 1;
			continue;
		    }

		/* Get variable value and append to the string. */
		switch (type)
		    {
		    case VAR_INTEGER:
			{
			sc_char		cbuf[16];
			sprintf (cbuf, "%ld", vt_rvalue.integer);
			obuf = sc_realloc (obuf, strlen (obuf)
						+ strlen (cbuf) + 1);
			strcat (obuf, cbuf);
			break;
			}

		    case VAR_STRING:
			obuf = sc_realloc (obuf, strlen (obuf)
					+ strlen (vt_rvalue.string) + 1);
			strcat (obuf, vt_rvalue.string);
			break;

		    default:
			sc_fatal ("pf_interpolate: invalid type, %d\n", type);
		    }

		/* Advance iptr over the %...% variable name. */
		iptr = vptr + strlen (vname) + 2;

		/* Note that a variable was found. */
		*changed = TRUE;
	    }

	/* Append remainder of the string. */
	obuf = sc_realloc (obuf, strlen (obuf) + strlen (iptr) + 1);
	strcat (obuf, iptr);

	/* Free temporary vname, and return constructed string. */
	sc_free (vname);
	return obuf;
}


/*
 * pf_replace_alrs()
 *
 * Replace any ALRs found in the string with their equivalents.  Return
 * a malloc'ed string with replacements done, and set changed if any
 * variables were found and replaced.
 */
static sc_char *
pf_replace_alrs (const sc_char *string, sc_prop_setref_t bundle,
		sc_bool *changed, sc_bool alr_applied[])
{
	sc_vartype_t	vt_key[3], vt_rvalue;
	sc_int32	alr_count, index;
	sc_char		*obuf, *tbuf;

	/* Initially, no changes made. */
	*changed = FALSE;

	/* Get a count of ALRs. */
	vt_key[0].string = "ALRs";
	alr_count = (prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;

	/*
	 * Start with the output buffer being a copy of string, and the
	 * temporary buffer being an empty string.
	 */
	obuf = sc_malloc (strlen (string) + 1);
	strcpy (obuf, string);
	tbuf = sc_malloc (1);
	strcpy (tbuf, "");

	/* Run through each ALR that exists. */
	for (index = 0; index < alr_count; index++)
	    {
		sc_int32	alr;
		sc_char		*original, *replacement;
		sc_char		*aptr, *iptr, *sbuf;

		/* Ignore ALR indexes that have already been applied. */
		if (alr_applied[index])
			continue;

		/*
		 * Get the actual ALR number for the ALR.  This comes
		 * from the index that we sorted earlier by length of
		 * original string.
		 */
		vt_key[0].string = "ALRs2";
		vt_key[1].integer = index;
		vt_key[2].string = "ALRIndex";
		alr = prop_get_integer (bundle, "I<-sis", vt_key);

		/* Set this ALR in the properties key. */
		vt_key[0].string = "ALRs";
		vt_key[1].integer = alr;

		/* Retrieve the ALR original string, and NULL replacement. */
		vt_key[2].string = "Original";
		original = prop_get_string (bundle, "S<-sis", vt_key);
		replacement = NULL;

		/* Ignore pathological empty originals. */
		if (original[0] == NUL)
			continue;

		/* Switch output and temporary buffers. */
		sbuf = tbuf; tbuf = obuf; obuf = sbuf;

		/* Empty the output buffer string. */
		obuf = sc_realloc (obuf, 1);
		strcpy (obuf, "");

		/* Run through the temporary looking for things to replace. */
		iptr = tbuf;
		for (aptr = strstr (iptr, original);
				aptr != NULL; aptr = strstr (iptr, original))
		    {
			/* Append buffer up to original string start. */
			obuf = sc_realloc (obuf, strlen (obuf)
							+ aptr - iptr + 1);
			strncat (obuf, iptr, aptr - iptr);

			/*
			 * Optimize by retrieving the replacement string
			 * only on demand -- NULL means not yet retrieved.
			 */
			if (replacement == NULL)
			    {
				vt_key[2].string = "Replacement";
				replacement = prop_get_string
						(bundle, "S<-sis", vt_key);
			    }

			/* Append the replacement string. */
			obuf = sc_realloc (obuf, strlen (obuf)
						+ strlen (replacement) + 1);
			strcat (obuf, replacement);

			/* Advance iptr over the original ALR string found. */
			iptr = aptr + strlen (original);

			/* Note that a replacement was made. */
			alr_applied[index]	= TRUE;
			*changed		= TRUE;
		    }

		/* See if iptr advanced, that is, was ALR found? */
		if (iptr != tbuf)
		    {
			/* Append remainder of the string. */
			obuf = sc_realloc (obuf, strlen (obuf)
							+ strlen (iptr) + 1);
			strcat (obuf, iptr);
		    }
		else
		    {
			/*
			 * ALR not found, so optimize by just switching the
			 * buffers around again.
			 */
			sbuf = tbuf; tbuf = obuf; obuf = sbuf;
		    }
	    }

	/* Free temporary buffer memory. */
	sc_free (tbuf);

	/* Return constructed string. */
	return obuf;
}


/*
 * pf_output_text()
 *
 * Edit the tag-stripped text element passed in, substituting &lt; &gt;
 * with < >, then send to the OS-specific output functions.
 */
static void
pf_output_text (const sc_char *string)
{
	sc_uint32	index;
	sc_char		*copy;

	/* Do nothing if the string element is empty. */
	if (strlen (string) == 0)
		return;

	/* Copy the string. */
	copy = sc_malloc (strlen (string) + 1);
	strcpy (copy, string);

	/* Run along the copy replacing &..; elements by equivalents. */
	for (index = 0; copy[index] != NUL; index++)
	    {
		if (!sc_strncasecmp (copy + index, "&lt;", strlen("&lt;")))
		    {
			copy[index] = LESSTHAN;
			memmove (copy + index + 1, copy + index + 4,
					strlen (copy) - index - 3);
		    }
		else if (!sc_strncasecmp (copy + index, "&gt;", strlen("&gt;")))
		    {
			copy[index] = GREATERTHAN;
			memmove (copy + index + 1, copy + index + 4,
					strlen (copy) - index - 3);
		    }
	    }

	/* Print out, then free, the modified string copy. */
	if_print_string (copy);
	sc_free (copy);
}


/*
 * pf_output_tag()
 *
 * Output an HTML-like tag element to the OS-specific tag handling function.
 */
static void
pf_output_tag (const sc_char *tag)
{
	sc_uint16	index;
	sc_uint32	tag_code;
	const sc_char	*tag_arg;

	/* For a simple <br> tag, just print out a newline. */
	if (!sc_strcasecmp (tag, "br")
			&& (tag[2] == NUL || isspace (tag[2])))
	    {
		if_print_character ('\n');
		return;
	    }

	/* Look up the tag in the tags table. */
	tag_code = SC_TAG_UNKNOWN;
	tag_arg  = tag;
	for (index = 0; HTML_TAGS[index].tag_string != NULL; index++)
	    {
		if (!sc_strncasecmp (tag, HTML_TAGS[index].tag_string,
					strlen (HTML_TAGS[index].tag_string)))
		    {
			const sc_char	*next;

			/*
			 * Found a possible table match -- check that it's
			 * a full match, and if it is, set tag_code and
			 * tag_arg and break.
			 */
			next = tag + strlen (HTML_TAGS[index].tag_string);
			if (*next == NUL || isspace (*next))
			    {
				while (*next != NUL && isspace (*next))
					next++;
				tag_code = HTML_TAGS[index].tag;
				tag_arg  = next;
				break;
			    }
		    }
	    }

	/*
	 * Call tag output.  Tag_arg is "" if none, the tag argument if any,
	 * or the the complete tag if no match.
	 */
	if_print_tag (tag_code, tag_arg);
}


/*
 * pf_output_untagged()
 *
 * Break apart HTML-like string into normal text elements, and HTML-like
 * tags.
 */
static void
pf_output_untagged (const sc_char *string)
{
	sc_char		*temp, *text, *tag;
	sc_char		*tptr;
	const sc_char	*iptr;

	/* Create a general temporary string. */
	temp = sc_malloc (strlen (string) + 1);
	text = tag = temp;

	/* Run through the string looking for <...> tags. */
	iptr = string;
	for (tptr = strchr (iptr, LESSTHAN);
			tptr != NULL; tptr = strchr (iptr, LESSTHAN))
	    {
		/* Handle characters up to the tag start. */
		strncpy (text, iptr, tptr - iptr);
		text[tptr - iptr] = NUL;
		pf_output_text (text);

		/* Catch and ignore completely empty tags. */
		if (strchr (tptr, GREATERTHAN) == tptr + 1)
		    {
			iptr = tptr + 2;
			continue;
		    }

		/* Get the tag text. */
		if (sscanf (tptr, "<%[^>]>", tag) != 1)
		    {
			sc_error ("pf_output_untagged:"
					" mismatched '%c'\n", LESSTHAN);

			/* Output the single < character and continue. */
			if_print_character (LESSTHAN);
			iptr = tptr + 1;
			continue;
		    }

		/* Handle tag output. */
		pf_output_tag (tag);

		/* Advance iptr over the <...> tag. */
		iptr = tptr + strlen (tag) + 1;
		if (*iptr == GREATERTHAN)
			iptr++;
	    }

	/* Output any remaining string text. */
	pf_output_text (iptr);

	/* All done. */
	sc_free (temp);
}


/*
 * pf_filter()
 *
 * Filter output, interpolating variables and replacing ALR's, and return
 * the resulting string to the caller.  The string is malloc'ed, so the
 * caller needs to remember to free it.
 */
sc_char *
pf_filter (const sc_char *string,
		sc_var_setref_t vars, sc_prop_setref_t bundle)
{
	sc_vartype_t	vt_key[1], vt_rvalue;
	sc_char		*interpolated, *replaced;
	sc_int32	alr_count, index;
	sc_bool		changed, *alr_applied;
	assert (string != NULL && vars != NULL && bundle != NULL);

	/*
	 * Create and clear a new set of ALR application flags.  These
	 * are used to ensure that a given ALR is applied only once.
	 */
	vt_key[0].string = "ALRs";
	alr_count = (prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;
	alr_applied = sc_malloc (alr_count * sizeof (*alr_applied));

	for (index = 0; index < alr_count; index++)
		alr_applied[index] = FALSE;

	if (pf_trace)
		sc_trace ("Printfilter: initial \"%s\"\n", string);

	/* Interpolate until all variables are out of the string. */
	interpolated = pf_interpolate (string, vars, &changed);
	while (changed)
	    {
		sc_char		*target;

		if (pf_trace)
		    {
			sc_trace ("Printfilter:"
				" interpolation \"%s\"\n", interpolated);
		    }

		/* Repeat interpolation on last string returned. */
		target = interpolated;
		interpolated = pf_interpolate (target, vars, &changed);
		sc_free (target);
	    }

	if (pf_trace)
	    {
		sc_trace ("Printfilter:"
				" interpolated 1 \"%s\"\n", interpolated);
	    }

	/* Replace ALRs in the current interpolated string. */
	replaced = pf_replace_alrs (interpolated, bundle,
						&changed, alr_applied);
	sc_free (interpolated);

	if (pf_trace)
		sc_trace ("Printfilter: replaced 1 \"%s\"\n", replaced);

	/* Now do the variable interpolation loop again. */
	interpolated = pf_interpolate (replaced, vars, &changed);
	while (changed)
	    {
		sc_char		*target;

		if (pf_trace)
		    {
			sc_trace ("Printfilter:"
				" interpolation \"%s\"\n", interpolated);
		    }

		/* Repeat interpolation on last string returned. */
		target = interpolated;
		interpolated = pf_interpolate (target, vars, &changed);
		sc_free (target);
	    }

	/* No further need for the intermediate ALR replacement string. */
	sc_free (replaced);

	if (pf_trace)
	    {
		sc_trace ("Printfilter:"
				" interpolated 2 \"%s\"\n", interpolated);
	    }

	/*
	 * Replace ALRs a second time in the current interpolated string.
	 * TODO How many times are we supposed to repeat this cycle?
	 */
	replaced = pf_replace_alrs (interpolated, bundle,
						&changed, alr_applied);
	sc_free (interpolated);

	if (pf_trace)
	    {
		sc_trace ("Printfilter:"
				" replaced 2 [final] \"%s\"\n", replaced);
	    }

	/* Free ALR application flags, and return the final string. */
	sc_free (alr_applied);
	return replaced;
}


/*
 * pf_filter_for_info()
 *
 * Filter output, interpolating variables only (no ALR replacement), and
 * return the resulting string to the caller.  Used on informational strings
 * such as the game title and author.  The string is malloc'ed, so the
 * caller needs to remember to free it.
 */
sc_char *
pf_filter_for_info (const sc_char *string, sc_var_setref_t vars)
{
	sc_char		*interpolated;
	sc_bool		changed;
	assert (string != NULL && vars != NULL);

	if (pf_trace)
		sc_trace ("Printfilter: info initial \"%s\"\n", string);

	/* Interpolate until all variables are out of the string. */
	interpolated = pf_interpolate (string, vars, &changed);
	while (changed)
	    {
		sc_char		*target;

		if (pf_trace)
		    {
			sc_trace ("Printfilter:"
				" info interpolation \"%s\"\n", interpolated);
		    }

		/* Repeat interpolation on last string returned. */
		target = interpolated;
		interpolated = pf_interpolate (target, vars, &changed);
		sc_free (target);
	    }

	if (pf_trace)
	    {
		sc_trace ("Printfilter:"
				" info interpolated \"%s\"\n", interpolated);
	    }

	/* Return the interpolated string. */
	return interpolated;
}


/*
 * pf_flush()
 *
 * Filter buffered data, interpolating variables and replacing ALR's, and
 * send the resulting string to the output channel.
 */
void
pf_flush (sc_printfilterref_t filter,
		sc_var_setref_t vars, sc_prop_setref_t bundle)
{
	sc_char		*filtered;
	assert (filter != NULL && filter->magic == PRINTFILTER_MAGIC);
	assert (vars != NULL && bundle != NULL);

	/* See if there is any buffered data to flush. */
	if (filter->buffer_length > 0)
	    {
		/*
		 * Filter the buffered string, then print it untagged.
		 * Remember to free the filtered version.
		 */
		filtered = pf_filter (filter->buffer, vars, bundle);
		pf_output_untagged (filtered);
		sc_free (filtered);

		/* Remove buffered data by resetting length to zero. */
		filter->buffer_length = 0;
	    }
}


/*
 * pf_get_buffer()
 *
 * Return the raw, unfiltered, buffered text.  Primarily for use by the
 * debugger.  Returns NULL if no buffered data available.
 */
sc_char *
pf_get_buffer (sc_printfilterref_t filter)
{
	assert (filter != NULL && filter->magic == PRINTFILTER_MAGIC);

	/*
	 * Return buffer if filter length is greater than zero.  Note that
	 * this assumes that the buffer is a nul-terminated string.
	 */
	if (filter->buffer_length > 0)
	    {
		assert (filter->buffer[filter->buffer_length] == NUL);
		return filter->buffer;
	    }
	else
		return NULL;
}


/*
 * pf_buffer_string()
 * pf_buffer_character()
 *
 * Add a string, and a single character, to the printfilter buffer.
 */
void
pf_buffer_string (sc_printfilterref_t filter, const sc_char *string)
{
	sc_uint32	required, noted;
	assert (filter != NULL && filter->magic == PRINTFILTER_MAGIC);
	assert (string != NULL);

	/*
	 * Calculate the required buffer size to append string.  Remember
	 * to add one for the terminating NUL.
	 */
	required = filter->buffer_length + strlen (string) + 1;

	/* If this is more than the current buffer size, resize it. */
	if (required > filter->buffer_size)
	    {
		sc_uint32	new_size;

		/* Calculate the new malloc size, in increment chunks. */
		new_size = ((required + BUFFER_GROW_INCREMENT - 1)
					/ BUFFER_GROW_INCREMENT)
						* BUFFER_GROW_INCREMENT;

		/* Grow the buffer. */
		filter->buffer = sc_realloc (filter->buffer, new_size);
		filter->buffer_size = new_size;
	    }

	/* If empty, put a NUL into the buffer to permit strcat. */
	if (filter->buffer_length == 0)
		filter->buffer[0] = NUL;

	/* Note append start, then append the string to the buffer. */
	noted = filter->buffer_length;
	strcat (filter->buffer, string);
	filter->buffer_length += strlen (string);

	/* Adjust first character of appended string if flagged. */
	if (filter->new_sentence)
		filter->buffer[noted] = toupper (filter->buffer[noted]);
	filter->new_sentence = FALSE;
}

void
pf_buffer_character (sc_printfilterref_t filter, sc_char character)
{
	sc_char		buffer[2];
	assert (filter != NULL && filter->magic == PRINTFILTER_MAGIC);

	/* Create a short string and add this. */
	buffer[0] = character;
	buffer[1] = NUL;
	pf_buffer_string (filter, buffer);
}


/*
 * pf_new_sentence()
 *
 * Tells the printfilter to force the next non-space character to uppercase.
 */
void
pf_new_sentence (sc_printfilterref_t filter)
{
	assert (filter != NULL && filter->magic == PRINTFILTER_MAGIC);

	filter->new_sentence = TRUE;
}


/*
 * pf_buffer_tag()
 *
 * Insert an HTML-like tag into the buffered output data.
 */
void
pf_buffer_tag (sc_printfilterref_t filter, sc_uint32 tag)
{
	sc_uint16	index;
	assert (filter != NULL && filter->magic == PRINTFILTER_MAGIC);

	/* Find the tag in the tags table. */
	for (index = 0; HTML_TAGS[index].tag_string != NULL; index++)
	    {
		if (tag == HTML_TAGS[index].tag)
		    {
			/* Found a match, so output the equivalent string. */
			pf_buffer_character (filter, LESSTHAN);
			pf_buffer_string (filter,
						HTML_TAGS[index].tag_string);
			pf_buffer_character (filter, GREATERTHAN);
			return;
		    }
	    }

	/* Tag not found in the table. */
	sc_error ("pf_buffer_tag: invalid tag, %lu\n", tag);
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
pf_strip_tags_common (sc_char *string, sc_bool allow_newlines)
{
	sc_char		*iptr, *sptr, *eptr;

	/* Run through the string looking for <...> tags. */
	iptr = string;
	for (sptr = strchr (iptr, LESSTHAN);
			sptr != NULL; sptr = strchr (iptr, LESSTHAN))
	    {
		/* Locate tag end, and break if unterminated. */
		eptr = strchr (sptr, GREATERTHAN);
		if (eptr == NULL)
			break;

		/* If the tag is <br>, replace with newline if requested. */
		if (allow_newlines)
		    {
			if (eptr - sptr == 3
					&& !sc_strncasecmp (sptr + 1, "br", 2))
				*sptr++ = '\n';
		    }

		/* Remove the tag from the string, then advance input. */
		memmove (sptr, eptr + 1, strlen (eptr));
		iptr = sptr;
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
pf_strip_tags (sc_char *string)
{
	pf_strip_tags_common (string, FALSE);
}

void
pf_strip_tags_for_hints (sc_char *string)
{
	pf_strip_tags_common (string, TRUE);
}


/*
 * pf_compare_words()
 *
 * Matches multiple words from words in string.  Returns the extent of
 * the match if the string matched, 0 otherwise.
 */
static sc_uint16
pf_compare_words (const sc_char *string, const sc_char *words)
{
	sc_uint16	wpos, posn;

	/* None expected, but skip leading space. */
	for (wpos = 0; isspace (words[wpos]) && words[wpos] != NUL; )
		wpos++;

	/* Match characters from words with the string at position. */
	posn = 0;
	while (TRUE)
	    {
		/* Any character mismatch means no words match. */
		if (tolower (words[wpos]) != tolower (string[posn]))
			return 0;

		/* Move to next character in each. */
		wpos++; posn++;

		/*
		 * If at space, advance over whitespace in words list.  Stop
		 * when we hit the end of the words list.
		 */
		while (isspace (words[wpos]) && words[wpos] != NUL)
			wpos++;
		if (words[wpos] == NUL)
			break;

		/*
		 * About to match another word, so advance over whitespace
		 * in the current string too.
		 */
		while (isspace (string[posn])
				&& string[posn] != NUL)
			posn++;
	    }

	/*
	 * We reached the end of words.  If we're at the end of the match
	 * string, or at spaces, we've matched.
	 */
	if (isspace (string[posn])
			|| string[posn] == NUL)
		return posn;

	/* More text after the match, so it's not quite a match. */
	return 0;
}


/*
 * pf_filter_input()
 *
 * Apply synonym changes to a player input string, and return the resulting
 * string to the caller.  The string is malloc'ed, so the caller needs to
 * remember to free it.
 */
sc_char *
pf_filter_input (const sc_char *string, sc_prop_setref_t bundle)
{
	sc_vartype_t	vt_key[3], vt_rvalue;
	sc_int32	synonym_count, index;
	sc_uint16	windex;
	sc_char		*obuf;
	sc_uint16	obuflen;
	assert (string != NULL && bundle != NULL);

	if (pf_trace)
		sc_trace ("Printfilter: input synonyms \"%s\"\n", string);

	/* Count synonyms, and set invariant part of properties key. */
	vt_key[0].string = "Synonyms";
	synonym_count = (prop_get (bundle, "I<-s", &vt_rvalue, vt_key))
						? vt_rvalue.integer : 0;

	/* Start off with a copy of the input string. */
	obuflen = strlen (string) + 1;
	obuf = sc_malloc (obuflen);
	strcpy (obuf, string);

	/* Find the first string word. */
	for (windex = 0; obuf[windex] != NUL && isspace (obuf[windex]); )
		windex++;

	/* Loop over each word in the string. */
	while (obuf[windex] != NUL)
	    {
		sc_bool		matched;

		/* Loop over synonyms, looking for a match at this point. */
		matched = FALSE;
		for (index = 0; index < synonym_count; index++)
		    {
			sc_char		*original, *replacement;
			sc_uint16	extent;

			/* Set this synonym in the properties key. */
			vt_key[1].integer = index;

			/* Retrieve the synonym original string. */
			vt_key[2].string = "Original";
			original = prop_get_string (bundle,
							"S<-sis", vt_key);

			/* Compare the original at this point. */
			extent = pf_compare_words (obuf + windex, original);
			if (extent > 0)
			    {
				sc_uint16	length, last;

				/* Matched, so find replacement. */
				vt_key[2].string = "Replacement";
				replacement = prop_get_string (bundle,
							"S<-sis", vt_key);
				length = strlen (replacement);

				/*
				 * If necessary, grow the output buffer for
				 * the replacement.  At the same time, note
				 * the last character index for the move.
				 */
				if (length > extent)
				    {
					obuflen += length - extent;
					obuf = sc_realloc (obuf, obuflen);
					last = length;
				    }
				else
					last = extent;

				/* Insert the replacement string. */
				memmove (obuf + windex + length,
						obuf + windex + extent,
						obuflen - windex - last);
				memcpy (obuf + windex, replacement, length);

				/* Adjust the word index and break. */
				windex += length;
				matched = TRUE;
				break;
			    }
		    }

		/* If no match, advance over the unmatched word. */
		if (!matched)
		    {
			while (obuf[windex] != NUL && !isspace (obuf[windex]))
				windex++;
		    }

		/* Find the next word start. */
		while (obuf[windex] != NUL && isspace (obuf[windex]))
			windex++;
	    }
	while (obuf[windex] != NUL);

	if (pf_trace)
		sc_trace ("Printfilter: input filtered \"%s\"\n", obuf);

	/* Return the final string. */
	return obuf;
}


/*
 * pf_debug_trace()
 *
 * Set filter tracing on/off.
 */
void
pf_debug_trace (sc_bool flag)
{
	pf_trace = flag;
}
