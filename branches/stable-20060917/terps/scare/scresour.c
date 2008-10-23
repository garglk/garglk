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
 * o ...
 */

#include <assert.h>
#include <string.h>

#include "scare.h"
#include "scprotos.h"
#include "scgamest.h"


/* Assorted definitions and constants. */
static const sc_char	NUL			= '\0';


/*
 * res_has_sound()
 * res_has_graphics()
 *
 * Return TRUE if the game uses sound or graphics.
 */
sc_bool
res_has_sound (sc_gamestateref_t gamestate)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2];
	sc_bool			has_sound;
	assert (gs_is_gamestate_valid (gamestate));

	vt_key[0].string = "Globals";
	vt_key[1].string = "Sound";
	has_sound = prop_get_boolean (bundle, "B<-ss", vt_key);
	return has_sound;
}

sc_bool
res_has_graphics (sc_gamestateref_t gamestate)
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2];
	sc_bool			has_graphics;
	assert (gs_is_gamestate_valid (gamestate));

	vt_key[0].string = "Globals";
	vt_key[1].string = "Graphics";
	has_graphics = prop_get_boolean (bundle, "B<-ss", vt_key);
	return has_graphics;
}


/*
 * res_set_resource()
 * res_clear_resource()
 * res_compare_resource()
 *
 * Convenience functions to set, clear, and compare resource fields.
 */
static void
res_set_resource (sc_resourceref_t resource, sc_char *name,
		sc_int32 offset, sc_int32 length)
{
	resource->name   = name;
	resource->offset = offset;
	resource->length = length;
}

void
res_clear_resource (sc_resourceref_t resource)
{
	res_set_resource (resource, "", 0, 0);
}

sc_bool
res_compare_resource (sc_resourceref_t from, sc_resourceref_t with)
{
	return (!strcmp (from->name, with->name)
			&& from->offset == with->offset
			&& from->length == with->length);
}


/*
 * res_handle_resource()
 *
 * General helper for handling graphics and sound resources.  Supplied
 * with a partial key to the node containing resources, it identifies what
 * resource is appropriate, and sets this as the requested resource in the
 * gamestate, for later use on sync'ing, using the handler appropriate for
 * the game version.
 *
 * The partial format is something like "sis" (the bit to follow I<- or
 * S<- in prop_get), and the partial key is guaranteed to contain at least
 * strlen(partial_format) elements.
 *
 */
void
res_handle_resource (sc_gamestateref_t gamestate,
		const sc_char *partial_format, sc_vartype_t vt_partial[])
{
	sc_prop_setref_t	bundle	= gamestate->bundle;
	sc_vartype_t		vt_key[2], *vt_full;
	sc_int32		partial_length, resource_start_offset;
	sc_bool			embedded;
	sc_char			*format;
	assert (gs_is_gamestate_valid (gamestate));
	assert (partial_format != NULL && vt_partial != NULL);

	/*
	 * Get the global offset for all resources.  For version 3.9 games
	 * this should be zero.  For version 4.0 games, it's the start of
	 * resource data in the TAF file where resources are embedded.
	 */
	vt_key[0].string = "ResourceOffset";
	resource_start_offset = prop_get_integer (bundle, "I<-s", vt_key);

	/*
	 * Get the flag that indicated embedded resources.  For version 3.9
	 * games this should be false.  If not set, offset and length are
	 * forced to zero for interface functions.
	 */
	vt_key[0].string = "Globals";
	vt_key[1].string = "Embedded";
	embedded = prop_get_boolean (bundle, "B<-ss", vt_key);

	/*
	 * Allocate a format for use with properties calls, five characters
	 * longer than the partial passed in.  Build a key one element larger
	 * than the partial supplied, and copy over all supplied elements.
	 */
	partial_length = strlen (partial_format);
	format = sc_malloc (partial_length + 5);

	vt_full = sc_malloc ((partial_length + 1) * sizeof (vt_partial[0]));
	memcpy (vt_full, vt_partial, partial_length * sizeof (vt_partial[0]));

	/* Search for sound resources, and offer if found. */
	if (res_has_sound (gamestate))
	    {
		sc_char		*soundfile;
		sc_int32	soundoffset, soundlen;

		/* Get soundfile property from the node supplied. */
		vt_full[partial_length].string = "SoundFile";
		strcpy (format, "S<-");
		strcat (format, partial_format);
		strcat (format, "s");
		soundfile = prop_get_string (bundle, format, vt_full);

		/* If a sound is defined, handle it. */
		if (!sc_strempty (soundfile))
		    {
			if (embedded)
			    {
				/* Retrieve offset and length. */
				vt_full[partial_length].string = "SoundOffset";
				strcpy (format, "I<-");
				strcat (format, partial_format);
				strcat (format, "s");
				soundoffset = prop_get_integer
						(bundle, format, vt_full)
							+ resource_start_offset;

				vt_full[partial_length].string = "SoundLen";
				strcpy (format, "I<-");
				strcat (format, partial_format);
				strcat (format, "s");
				soundlen = prop_get_integer
						(bundle, format, vt_full);
			    }
			else
			    {
				/* Coerce offset and length to zero. */
				soundoffset	= 0;
				soundlen	= 0;
			    }

			/*
			 * If the sound is the special "##", latch stop,
			 * otherwise note details to play on sync.
			 */
			if (!strcmp (soundfile, "##"))
			    {
				gamestate->stop_sound = TRUE;
				res_clear_resource
						(&gamestate->requested_sound);
			    }
			else
			    {
				res_set_resource (&gamestate->requested_sound,
						soundfile, soundoffset,
						soundlen);
			    }
		    }
	    }

	/* Now do the same thing for graphics resources. */
	if (res_has_graphics (gamestate))
	    {
		sc_char		*graphicfile;
		sc_int32	graphicoffset, graphiclen;

		/* Get graphicfile property from the node supplied. */
		vt_full[partial_length].string = "GraphicFile";
		strcpy (format, "S<-");
		strcat (format, partial_format);
		strcat (format, "s");
		graphicfile = prop_get_string (bundle, format, vt_full);

		/* If a graphic is defined, handle it. */
		if (!sc_strempty (graphicfile))
		    {
			if (embedded)
			    {
				/* Retrieve offset and length. */
				vt_full[partial_length].string =
								"GraphicOffset";
				strcpy (format, "I<-");
				strcat (format, partial_format);
				strcat (format, "s");
				graphicoffset = prop_get_integer
						(bundle, format, vt_full)
							+ resource_start_offset;

				vt_full[partial_length].string = "GraphicLen";
				strcpy (format, "I<-");
				strcat (format, partial_format);
				strcat (format, "s");
				graphiclen = prop_get_integer
						(bundle, format, vt_full);
			    }
			else
			    {
				/* Coerce offset and length to zero. */
				graphicoffset	= 0;
				graphiclen	= 0;
			    }

			/* Graphics resource retrieved, note to show on sync. */
			res_set_resource (&gamestate->requested_graphic,
						graphicfile, graphicoffset,
						graphiclen);
		    }
	    }

	/* Free allocated memory. */
	sc_free (format);
	sc_free (vt_full);
}


/*
 * res_sync_resources()
 *
 * Bring resources into line with the gamestate; called on undo, restart,
 * restore, and so on.
 */
void
res_sync_resources (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	/* Deal with any latched sound stop first. */
	if (gamestate->stop_sound)
	    {
		if (gamestate->sound_active)
		    {
			if_update_sound ("", 0, 0, FALSE);
			gamestate->sound_active = FALSE;

			res_clear_resource (&gamestate->playing_sound);
		    }
		gamestate->stop_sound = FALSE;
	    }

	/* Look for a change of sound, and pass to interface on change. */
	if (!res_compare_resource (&gamestate->playing_sound,
					&gamestate->requested_sound))
	    {
		sc_char		*name, *clean_name;
		sc_bool		is_looping;

		/* If the sound name ends '##', this is a looping sound. */
		name = gamestate->requested_sound.name;
		is_looping = !strcmp (name + strlen (name) - 2, "##");

		clean_name = sc_malloc (strlen (name) + 1);
		strcpy (clean_name, name);
		if (is_looping)
			clean_name[strlen (clean_name) - 2] = NUL;

		if_update_sound (clean_name,
				gamestate->requested_sound.offset,
				gamestate->requested_sound.length,
				is_looping);
		gamestate->playing_sound = gamestate->requested_sound;
		gamestate->sound_active = TRUE;

		sc_free (clean_name);
	    }

	/* Look for a change of graphic, and pass to interface on change. */
	if (!res_compare_resource (&gamestate->displayed_graphic,
					&gamestate->requested_graphic))
	    {
		if_update_graphic (gamestate->requested_graphic.name,
				gamestate->requested_graphic.offset,
				gamestate->requested_graphic.length);
		gamestate->displayed_graphic = gamestate->requested_graphic;
	    }
}


/*
 * res_cancel_resources()
 *
 * Turn off sound and graphics, and reset the gamestate's tracking of
 * resources in use to match.  Called on game restart or restore.
 */
void
res_cancel_resources (sc_gamestateref_t gamestate)
{
	assert (gs_is_gamestate_valid (gamestate));

	/* Request that everything stops and clears. */
	gamestate->stop_sound = FALSE;
	res_clear_resource (&gamestate->requested_sound);
	res_clear_resource (&gamestate->requested_graphic);

	/* Synchronize to have the above take effect. */
	res_sync_resources (gamestate);
}
