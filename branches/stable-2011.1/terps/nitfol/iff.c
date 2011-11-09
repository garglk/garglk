/*  Nitfol - z-machine interpreter using Glk for output.
    Copyright (C) 1999  Evin Robertson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

    The author can be reached at nitfol@deja.com
*/
#include "nitfol.h"

BOOL iffgetchunk(strid_t stream, char *desttype, glui32 *ulength,
		 glui32 file_size)
{
  int i;
  glui32 c;
  unsigned char length[4];

  c = glk_stream_get_position(stream);
  if(c & 1) {
    glk_get_char_stream(stream);    /* Eat padding */
    c++;
  }

  if(glk_get_buffer_stream(stream, desttype, 4) != 4)
    return FALSE;
  if(glk_get_buffer_stream(stream, (char *) length, 4) != 4)
    return FALSE;

  *ulength = MSBdecode4(length);

  for(i = 0; i < 4; i++)
    if(desttype[i] < 0x20 || desttype[i] > 0x7e)
      return FALSE;

  c += 8;

  return ((c + *ulength) <= file_size);
}


BOOL ifffindchunk(strid_t stream, const char *type, glui32 *length, glui32 loc)
{
  char t[4];
  glui32 file_size;

  glk_stream_set_position(stream, 0, seekmode_End);
  file_size = glk_stream_get_position(stream);

  glk_stream_set_position(stream, loc, seekmode_Start);
  *length = 0;
  do {
    glk_stream_set_position(stream, *length, seekmode_Current);
    if(!iffgetchunk(stream, t, length, file_size))
      return FALSE;
   } while((t[0] != type[0]) || (t[1] != type[1]) ||
	   (t[2] != type[2]) || (t[3] != type[3]));

  return TRUE;
}


void iffputchunk(strid_t stream, const char *type, glui32 ulength)
{
  glui32 c;
  unsigned char length[4];

  c = glk_stream_get_position(stream);
  if(c & 1)
    glk_put_char_stream(stream, 0);  /* Spew padding */

  MSBencode4(length, ulength);

  w_glk_put_buffer_stream(stream, type, 4);
  w_glk_put_buffer_stream(stream, (char *) length, 4);
}


void iffpadend(strid_t stream)
{
  glui32 c;
  
  c = glk_stream_get_position(stream);
  if(c & 1)
    glk_put_char_stream(stream, 0);  /* Spew padding */
}
