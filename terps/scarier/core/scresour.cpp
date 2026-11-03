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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include "scarier.h"
#include "sctypes.h"
#include "scutils.h"
#include "scprops.h"
#include "scgamest_api.h"
#include "scresour.h"
#include "scinterf.h"
#include "scgamest.h"


/*
 * res_has_sound()
 * res_has_graphics()
 *
 * Return TRUE if the game uses sound or graphics.
 */
scr_bool
res_has_sound (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  assert (gs_is_game_valid (game));

  return prop_get_global_boolean (bundle, "Sound");
}

scr_bool
res_has_graphics (scr_gameref_t game)
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  assert (gs_is_game_valid (game));

  return prop_get_global_boolean (bundle, "Graphics");
}


/*
 * res_set_resource()
 * res_clear_resource()
 * res_compare_resource()
 *
 * Convenience functions to set, clear, and compare resource fields.
 */
static void
res_set_resource (scr_resourceref_t resource, const scr_char *name,
                  scr_int offset, scr_int length)
{
  resource->name = name;
  resource->offset = offset;
  resource->length = length;
}

void
res_clear_resource (scr_resourceref_t resource)
{
  res_set_resource (resource, "", 0, 0);
}

scr_bool
res_compare_resource (scr_resourceref_t from, scr_resourceref_t with)
{
  return strcmp (from->name, with->name) == 0
         && from->offset == with->offset && from->length == with->length;
}


/*
 * The scratch state res_handle_resource() builds once and both of its
 * lookups share: the bundle, the format buffer and the key array (one
 * element longer than the caller's partial key, the last element being the
 * property name of the moment), and the two globals that decide whether an
 * offset and length are read or forced to zero.
 */
typedef struct scr_res_lookup_s
{
  scr_prop_setref_t bundle;
  scr_char *format;
  scr_int format_size;
  const scr_char *partial_format;
  scr_vartype_t *vt_full;
  scr_int partial_length;
  scr_bool embedded;
  scr_int resource_start_offset;
} scr_res_lookup_t;


/*
 * res_lookup_resource()
 *
 * Helper for res_handle_resource(), shared by its sound and graphics halves.
 * Reads the resource file name held under file_key; if one is defined,
 * passes it back with its offset (adjusted by the resource start) and
 * length, or with both forced to zero where resources are not embedded, and
 * returns TRUE.  Returns FALSE if no resource file is defined.
 */
static scr_bool
res_lookup_resource (const scr_res_lookup_t *lookup,
                     const scr_char *file_key, const scr_char *offset_key,
                     const scr_char *length_key,
                     const scr_char **file, scr_int *offset, scr_int *length)
{
  /* Get the file property from the node supplied. */
  lookup->vt_full[lookup->partial_length].string = file_key;
  snprintf (lookup->format, lookup->format_size,
            "S<-%ss", lookup->partial_format);
  *file = prop_get_string (lookup->bundle, lookup->format, lookup->vt_full);

  /* If no resource is defined, there is nothing more to find. */
  if (scr_strempty (*file))
    return FALSE;

  if (lookup->embedded)
    {
      /* Retrieve offset and length. */
      lookup->vt_full[lookup->partial_length].string = offset_key;
      snprintf (lookup->format, lookup->format_size,
                "I<-%ss", lookup->partial_format);
      *offset = prop_get_integer (lookup->bundle, lookup->format,
                                  lookup->vt_full)
                + lookup->resource_start_offset;

      lookup->vt_full[lookup->partial_length].string = length_key;
      *length = prop_get_integer (lookup->bundle, lookup->format,
                                  lookup->vt_full);
    }
  else
    {
      /* Coerce offset and length to zero. */
      *offset = 0;
      *length = 0;
    }

  return TRUE;
}


/*
 * res_handle_resource()
 *
 * General helper for handling graphics and sound resources.  Supplied with a
 * partial key to the node containing resources, it identifies what resource
 * is appropriate, and sets this as the requested resource in the game, for
 * later use on sync'ing, using the handler appropriate for the game version.
 *
 * The partial format is something like "sis" (the bit to follow I<- or S<-
 * in prop_get), and the partial key is guaranteed to contain at least
 * strlen(partial_format) elements.
 */
void
res_handle_resource (scr_gameref_t game,
                     const scr_char *partial_format,
                     const scr_vartype_t vt_partial[])
{
  const scr_prop_setref_t bundle = gs_get_bundle (game);
  scr_vartype_t vt_key[2], *vt_full;
  scr_int partial_length, format_size, resource_start_offset;
  scr_bool embedded;
  scr_char *format;
  scr_res_lookup_t lookup;
  assert (gs_is_game_valid (game));
  assert (partial_format && vt_partial);

  /*
   * Check for resources.  If this game doesn't use any, exit now to avoid the
   * overhead of pointless lookups and allocations.
   */
  if (!(res_has_sound (game) || res_has_graphics (game)))
    return;

  /*
   * Get the global offset for all resources.  For version 3.9 games this
   * should be zero.  For version 4.0 games, it's the start of resource data
   * in the TAF file where resources are embedded.
   */
  vt_key[0].string = "ResourceOffset";
  resource_start_offset = prop_get_integer (bundle, "I<-s", vt_key);

  /*
   * Get the flag that indicated embedded resources.  For version 3.9 games
   * this should be false.  If not set, offset and length are forced to zero
   * for interface functions.
   */
  vt_key[0].string = "Globals";
  vt_key[1].string = "Embedded";
  embedded = prop_get_boolean (bundle, "B<-ss", vt_key);

  /*
   * Allocate a format for use with properties calls, five characters longer
   * than the partial passed in: three for the leading "X<-", one for the
   * trailing "s", and one for the terminator.  Build a key one element larger
   * than the partial supplied, and copy over all supplied elements.
   */
  partial_length = strlen (partial_format);
  format_size = partial_length + 5;
  scr_owned_string owned_format ((scr_char *) scr_malloc (format_size));
  format = owned_format.get ();

  /*
   * Both scratch buffers are RAII-owned, so a prop_get_*() below throwing
   * (scr_fatal on a missing property) does not leak them.
   */
  std::unique_ptr<scr_vartype_t, scr_free_deleter> owned_vt_full
    ((scr_vartype_t *) scr_malloc ((partial_length + 1)
                                   * sizeof (vt_partial[0])));
  vt_full = owned_vt_full.get ();
  memcpy (vt_full, vt_partial, partial_length * sizeof (vt_partial[0]));

  /* Gather the shared lookup state for the two halves below. */
  lookup.bundle = bundle;
  lookup.format = format;
  lookup.format_size = format_size;
  lookup.partial_format = partial_format;
  lookup.vt_full = vt_full;
  lookup.partial_length = partial_length;
  lookup.embedded = embedded;
  lookup.resource_start_offset = resource_start_offset;

  /* Search for sound resources, and offer if found. */
  if (res_has_sound (game))
    {
      const scr_char *soundfile;
      scr_int soundoffset, soundlen;

      /* If a sound is defined, handle it. */
      if (res_lookup_resource (&lookup, "SoundFile", "SoundOffset", "SoundLen",
                               &soundfile, &soundoffset, &soundlen))
        {
          /*
           * If the sound is the special "##", latch stop, otherwise note
           * details to play on sync.
           */
          if (!strcmp (soundfile, "##"))
            {
              game->stop_sound = TRUE;
              res_clear_resource (&game->requested_sound);
            }
          else
            {
              res_set_resource (&game->requested_sound,
                                soundfile, soundoffset, soundlen);
            }
        }
    }

  /* Now do the same thing for graphics resources. */
  if (res_has_graphics (game))
    {
      const scr_char *graphicfile;
      scr_int graphicoffset, graphiclen;

      /* If a graphic is defined, note to show on sync. */
      if (res_lookup_resource (&lookup, "GraphicFile", "GraphicOffset",
                               "GraphicLen",
                               &graphicfile, &graphicoffset, &graphiclen))
        {
          res_set_resource (&game->requested_graphic,
                            graphicfile, graphicoffset, graphiclen);
        }
    }
}


/*
 * res_sync_resources()
 *
 * Bring resources into line with the game; called on undo, restart,
 * restore, and so on.
 */
void
res_sync_resources (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  /* Deal with any latched sound stop first. */
  if (game->stop_sound)
    {
      if (game->sound_active)
        {
          if_update_sound ("", 0, 0, FALSE);
          game->sound_active = FALSE;

          res_clear_resource (&game->playing_sound);
        }
      game->stop_sound = FALSE;
    }

  /* Look for a change of sound, and pass to interface on change. */
  if (!res_compare_resource (&game->playing_sound,
                             &game->requested_sound))
    {
      const scr_char *name;
      scr_bool is_looping;

      /* If the sound name ends '##', this is a looping sound. */
      name = game->requested_sound.name;
      is_looping = strlen (name) >= 2
                   && !strcmp (name + strlen (name) - 2, "##");

      scr_owned_string clean_name (scr_strdup (name));
      if (is_looping)
        clean_name.get ()[strlen (name) - 2] = NUL;

      if_update_sound (clean_name.get (),
                       game->requested_sound.offset,
                       game->requested_sound.length, is_looping);
      game->playing_sound = game->requested_sound;
      game->sound_active = TRUE;
    }

  /* Look for a change of graphic, and pass to interface on change. */
  if (!res_compare_resource (&game->displayed_graphic,
                             &game->requested_graphic))
    {
      if_update_graphic (game->requested_graphic.name,
                         game->requested_graphic.offset,
                         game->requested_graphic.length);
      game->displayed_graphic = game->requested_graphic;
    }
}


/*
 * res_cancel_resources()
 *
 * Turn off sound and graphics, and reset the game's tracking of resources in
 * use to match.  Called on game restart or restore.
 */
void
res_cancel_resources (scr_gameref_t game)
{
  assert (gs_is_game_valid (game));

  /* Request that everything stops and clears. */
  game->stop_sound = FALSE;
  res_clear_resource (&game->requested_sound);
  res_clear_resource (&game->requested_graphic);

  /* Synchronize to have the above take effect. */
  res_sync_resources (game);
}
