/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
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

#include <stdio.h>
#include <stdlib.h>
#include "glk.h"
#include "garglk.h"

/* The whole sound-channel situation is very simple for us;
   we don't support it. */

void gli_initialize_sound(void)
{
}

#ifdef GLK_MODULE_SOUND

schanid_t glk_schannel_create(glui32 rock)
{
    return NULL;
}

schanid_t glk_schannel_create_ext(glui32 rock, glui32 volume)
{
    return NULL;
}

void glk_schannel_destroy(schanid_t chan)
{
}

schanid_t glk_schannel_iterate(schanid_t chan, glui32 *rockptr)
{
    if (rockptr)
        *rockptr = 0;
    return NULL;
}

glui32 glk_schannel_get_rock(schanid_t chan)
{
    gli_strict_warning("schannel_get_rock: invalid id.");
    return 0;
}

glui32 glk_schannel_play(schanid_t chan, glui32 snd)
{
    gli_strict_warning("schannel_play: invalid id.");
    return 0;
}

glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats,
        glui32 notify)
{
    gli_strict_warning("schannel_play_ext: invalid id.");
    return 0;
}

glui32 glk_schannel_play_multi(schanid_t *chanarray, glui32 chancount,
        glui32 *sndarray, glui32 soundcount, glui32 notify)
{
    gli_strict_warning("schannel_play_multi: invalid id.");
    return 0;
}

void glk_schannel_stop(schanid_t chan)
{
    gli_strict_warning("schannel_stop: invalid id.");
}

void glk_schannel_pause(schanid_t chan)
{
    gli_strict_warning("schannel_pause: invalid id.");
}

void glk_schannel_unpause(schanid_t chan)
{
    gli_strict_warning("schannel_unpause: invalid id.");
}

void glk_schannel_set_volume(schanid_t chan, glui32 vol)
{
    gli_strict_warning("schannel_set_volume: invalid id.");
}

void glk_schannel_set_volume_ext(schanid_t chan, glui32 vol,
        glui32 duration, glui32 notify)
{
    gli_strict_warning("schannel_set_volume_ext: invalid id.");
}

void glk_sound_load_hint(glui32 snd, glui32 flag)
{
    gli_strict_warning("schannel_sound_load_hint: invalid id.");
}

#endif /* GLK_MODULE_SOUND */
