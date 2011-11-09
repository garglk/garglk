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

/* Link this in only if your glk supports sound */

#ifdef GLK_MODULE_SOUND

static schanid_t channel;

void init_sound(void)
{
  channel = glk_schannel_create(0);
}

void kill_sound(void)
{
  glk_schannel_destroy(channel);
}

static void set_volume(zword zvolume)
{
  glui32 vol;
  switch(zvolume) {
  case   1: vol = 0x02000; break;
  case   2: vol = 0x04000; break;
  case   3: vol = 0x06000; break;
  case   4: vol = 0x08000; break;
  case   5: vol = 0x0a000; break;
  case   6: vol = 0x0c000; break;
  case   7: vol = 0x0e000; break;
  case   8: vol = 0x10000; break;
  case 255: vol = 0x20000; break;
  default: n_show_error(E_SOUND, "illegal volume", zvolume);
  }
  glk_schannel_set_volume(channel, vol);
}


void check_sound(event_t moo) /* called from main event loop */
{
  if(moo.type == evtype_SoundNotify) {
    zword dummylocals[16];
    in_timer = TRUE;
    mop_call(moo.val2, 0, dummylocals, -2); /* add a special stack frame */
    decode();                              /* start interpreting the routine */
    in_timer = FALSE;
    exit_decoder = FALSE;
   }
}


void op_sound_effect(void)
{
  zword number = operand[0], effect = operand[1];
  zword volume = operand[2], routine = operand[3];
  glui32 repeats;

  if(!glk_gestalt(gestalt_Sound, 0))
    return;

  if(numoperands < 1)
    number = 1;
  if(numoperands < 2)
    effect = 2;
  if(numoperands < 3)
    volume = 8;
  if(numoperands < 4)
    routine = 0;

  repeats = (volume >> 8) & 0xff;
  if(repeats == 0xff)
    repeats = 0xffffffff;
  else
    repeats++;

  volume &= 0xff;

  switch(effect) {
  case 1:
    glk_sound_load_hint(number, 1);
    break;
  case 2:
    glk_schannel_play_ext(channel, number, repeats, routine);
    set_volume(volume);
    break;
  case 3:
    glk_schannel_stop(channel);
    break;
  case 4:
    glk_sound_load_hint(number, 0);
  }

}

#endif
