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

#include <cstdio>
#include <vector>

#include "glk.h"
#include "garglk.h"
#include "gi_blorb.h"

/* We'd like to be able to deal with game files in Blorb files, even
   if we never load a sound or image. We'd also like to be able to
   deal with Data chunks. So we're willing to set a map here. */

static giblorb_map_t *blorbmap = 0; /* NULL */

#ifdef GARGLK
static strid_t blorbfile = nullptr;
#endif

giblorb_err_t giblorb_set_resource_map(strid_t file)
{
  giblorb_err_t err;
  
#ifdef GARGLK
  if (file->type != strtype_File && file->type != strtype_Memory) {
      return giblorb_err_NotAMap;
  }

  if (file->type == strtype_Memory && file->unicode) {
      return giblorb_err_NotAMap;
  }

  if (blorbmap != nullptr) {
      giblorb_destroy_map(blorbmap);
      blorbmap = nullptr;
  }

  if (blorbfile != nullptr) {
      glk_stream_close(blorbfile, nullptr);
      blorbfile = nullptr;
  }
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
bool giblorb_copy_resource(glui32 usage, glui32 resnum, glui32 &type, std::vector<unsigned char> &buf)
{
    if (blorbmap == nullptr) {
        return false;
    }

    giblorb_result_t blorbres;
    auto err = giblorb_load_resource(blorbmap, giblorb_method_FilePos, &blorbres, usage, resnum);
    if (err != giblorb_err_None) {
        return false;
    }

    auto pos = blorbres.data.startpos;
    auto len = blorbres.length;

    try {
        buf.resize(len);
    } catch (const std::bad_alloc &) {
        return false;
    }

    switch (blorbfile->type) {
    case strtype_File:
        if (std::fseek(blorbfile->file, pos, SEEK_SET) == -1 ||
            std::fread(buf.data(), len, 1, blorbfile->file) != 1) {
            return false;
        }
        break;
    case strtype_Memory:
        std::copy(blorbfile->buf + pos, blorbfile->buf + pos + len, buf.begin());
        break;
    default:
        return false;
    }

    type = blorbres.chunktype;

    return true;
}
#endif
