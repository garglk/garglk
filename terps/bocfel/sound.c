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
#include "zterp.h"

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
#define SOUND_EFFECT_PREPARE	1
#define SOUND_EFFECT_START	2
#define SOUND_EFFECT_STOP	3
#define SOUND_EFFECT_FINISH	4

    uint16_t number, effect;

    if (sound_channel == NULL || znargs == 0) {
        return;
    }

    number = zargs[0];
    effect = zargs[1];

    // Beeps and boops aren’t supported.
    if (number == 1 || number == 2) {
        return;
    }

    // Sound effect 0 is invalid, except that it can be used to mean
    // “stop all sounds”.
    if (number == 0 && effect != SOUND_EFFECT_STOP) {
        return;
    }

    switch (effect) {
    case SOUND_EFFECT_PREPARE:
        glk_sound_load_hint(number, 1);
        break;
    case SOUND_EFFECT_START: {
        uint8_t repeats = zargs[2] >> 8;
        uint8_t volume = zargs[2] & 0xff;
        static uint32_t vols[8] = {
            0x02000, 0x04000, 0x06000, 0x08000,
            0x0a000, 0x0c000, 0x0e000, 0x10000
        };

        // V3 doesn’t support repeats, but in The Lurking Horror,
        // repeating sounds are achieved by encoding repeats into the
        // sound files themselves. According to sounds.txt from The
        // Lurking Horror’s source code, the following sounds are
        // “looping sounds”, so force that here.
        if (is_lurking_horror()) {
            switch (number) {
            case 4: case 10: case 13: case 15: case 16: case 17: case 18:
                repeats = 255;
                break;
            }
        }

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

        sound_routine = znargs >= 4 ? zargs[3] : 0;
        if (!glk_schannel_play_ext(sound_channel, number, repeats == 255 ? -1 : repeats, sound_routine != 0)) {
            sound_routine = 0;
        }

        break;
    }
    case SOUND_EFFECT_STOP:
        glk_schannel_stop(sound_channel);
        sound_routine = 0;
        break;
    case SOUND_EFFECT_FINISH:
        glk_sound_load_hint(number, 0);
        break;
    }
#endif
}
