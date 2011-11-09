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

    Portions of this file are copyright 1998-2004 by Andrew Plotkin.
    You may copy, distribute, and incorporate it into your own programs,
    by any means and under any conditions, as long as you do not modify it.
    You may also modify this file, incorporate it into your own programs,
    and distribute the modified version, as long as you retain a notice
    in your program or documentation which mentions my name and the URL
    shown above.
*/

#include <stdio.h>
#include "glk.h"
#include "garglk.h"
#include "gi_blorb.h"

static strid_t blorbfile = NULL;
static giblorb_map_t *blorbmap = NULL;

giblorb_err_t giblorb_set_resource_map(strid_t file)
{
    giblorb_err_t err;

    /* For the moment, we only allow file-streams, because the resource
       loaders expect a FILE*. This could be changed, but I see no
       reason right now. */
    if (file->type != strtype_File)
        return giblorb_err_NotAMap;

    err = giblorb_create_map(file, &blorbmap);
    if (err)
    {
        blorbmap = NULL;
        return err;
    }

    blorbfile = file;

    return giblorb_err_None;
}

giblorb_map_t *giblorb_get_resource_map()
{
    return blorbmap;
}

int giblorb_is_resource_map()
{
    return (blorbmap != NULL);
}

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
