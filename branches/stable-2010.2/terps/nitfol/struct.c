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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

    The author can be reached at nitfol@deja.com
*/
#include "nitfol.h"


glui32 fillstruct(strid_t stream, const unsigned *info, glui32 *dest,
		  glui32 (*special)(strid_t))
{
  unsigned char buffer[4];
  unsigned e;
  glui32 len = 0;;

  for(e = 0; info[e]; e++) {
    if(info[e] == 0x8000) {
      *dest++ = special(stream);
      len++;
    }
    else if(info[e] > 4) {
      unsigned i;
      for(i = 0; i < info[e]; i++) {
	*dest++ = glk_get_char_stream(stream);
	len++;
      }
    } else {
      glk_get_buffer_stream(stream, (char *) buffer, info[e]);

      switch(info[e]) {
      case 1: *dest = MSBdecode1(buffer); break;
      case 2: *dest = MSBdecode2(buffer); break;
      case 3: *dest = MSBdecode3(buffer); break;
      case 4: *dest = MSBdecode4(buffer); break;
      }
      dest++;
      len+=info[e];
    }
  }
  return len;
}

glui32 emptystruct(strid_t stream, const unsigned *info, const glui32 *src)
{
  unsigned char buffer[4];
  unsigned e;
  glui32 len = 0;;

  for(e = 0; info[e]; e++) {
    if(info[e] > 4) {
      unsigned i;
      for(i = 0; i < info[e]; i++) {
	glk_put_char_stream(stream, *src++);
	len++;
      }
    } else {
      switch(info[e]) {
      case 1: MSBencode1(buffer, *src); break;
      case 2: MSBencode2(buffer, *src); break;
      case 3: MSBencode3(buffer, *src); break;
      case 4: MSBencode4(buffer, *src); break;
      }

      w_glk_put_buffer_stream(stream, (char *) buffer, info[e]);
      
      src++;
      len++;
    }
  }
  return len;
}
