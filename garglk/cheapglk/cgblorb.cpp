/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson, Andrew Plotkin.                  *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

/* cgblorb.c: Blorb functions for Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html

    Portions of this file are copyright (c) 1998-2016, Andrew Plotkin
    It is distributed under the MIT license; see the "LICENSE" file.
*/

#include <stdio.h>
#include "glk.h"
#include "garglk.h"
#include "gi_blorb.h"

/* We'd like to be able to deal with game files in Blorb files, even
   if we never load a sound or image. We'd also like to be able to
   deal with Data chunks. So we're willing to set a map here. */

static giblorb_map_t *blorbmap = 0; /* NULL */

#ifdef GARGLK
static strid_t blorbfile = NULL;
#endif

giblorb_err_t giblorb_set_resource_map(strid_t file)
{
  giblorb_err_t err;
  
#ifdef GARGLK
  /* For the moment, we only allow file-streams, because the resource
      loaders expect a FILE*. This could be changed, but I see no
      reason right now. */
  if (file->type != strtype_File)
      return giblorb_err_NotAMap;
#endif

  err = giblorb_create_map(file, &blorbmap);
  if (err) {
    blorbmap = 0; /* NULL */
    return err;
  }
  
#ifdef GARGLK
  blorbfile = file;
#endif

  return giblorb_err_None;
}

giblorb_map_t *giblorb_get_resource_map()
{
  return blorbmap;
}

#ifdef GARGLK
void giblorb_get_resource(glui32 usage, glui32 resnum,
    FILE **file, long *pos, long *len, glui32 *type)
{
    giblorb_err_t err;
    giblorb_result_t blorbres;

    *file = NULL;
    *pos = 0;

    if (!blorbmap)
        return;

    err = giblorb_load_resource(blorbmap, giblorb_method_FilePos,
            &blorbres, usage, resnum);
    if (err)
        return;

    *file = blorbfile->file;
    if (pos)
        *pos = blorbres.data.startpos;
    if (len)
        *len = blorbres.length;
    if (type)
        *type = blorbres.chunktype;
}
#endif
