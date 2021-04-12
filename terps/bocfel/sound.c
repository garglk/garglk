// Copyright 2011-2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "sound.h"
#include "process.h"

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#ifdef GLK_MODULE_SOUND
static schanid_t sound_channel = NULL;
uint16_t sound_routine;
#endif

void init_sound(void)
{
#ifdef GLK_MODULE_SOUND
    if (sound_loaded()) {
        return;
    }

    if (glk_gestalt(gestalt_Sound, 0)) {
        sound_channel = glk_schannel_create(0);
    }
#endif
}

bool sound_loaded(void)
{
#ifdef GLK_MODULE_SOUND
    return sound_channel != NULL;
#else
    return false;
#endif
}

void zsound_effect(void)
{
#ifdef GLK_MODULE_SOUND
    uint8_t repeats, volume;
    static uint32_t vols[8] = {
        0x02000, 0x04000, 0x06000, 0x08000,
        0x0a000, 0x0c000, 0x0e000, 0x10000
    };

    sound_routine = 0;

    if (sound_channel == NULL || zargs[0] < 3) {
        return;
    }

    repeats = zargs[2] >> 8;
    volume = zargs[2] & 0xff;

    // Illegal, but expected to work by “The Spy Who Came In From The
    // Garden” and recommended by standard 1.1.
    if (repeats == 0) {
        repeats = 1;
    }

    if (volume == 0) {
        volume = 1;
    } else if (volume > 8) {
        volume = 8;
    }

    glk_schannel_set_volume(sound_channel, vols[volume - 1]);

    switch (zargs[1]) {
    case 1: // prepare
        glk_sound_load_hint(zargs[0], 1);
        break;
    case 2: // start
        sound_routine = znargs >= 4 ? zargs[3] : 0;
        if (!glk_schannel_play_ext(sound_channel, zargs[0], repeats == 255 ? -1 : repeats, sound_routine != 0)) {
            sound_routine = 0;
        }
        break;
    case 3: // stop
        glk_schannel_stop(sound_channel);
        sound_routine = 0;
        break;
    case 4: // finish with
        glk_sound_load_hint(zargs[0], 0);
        break;
    }
#endif
}
