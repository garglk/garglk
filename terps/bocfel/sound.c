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
static bool sound_playing;
uint16_t sound_routine;

static struct {
    uint16_t number;
    uint8_t volume;
} queued;
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

#ifdef GLK_MODULE_SOUND
static void start_sound(uint16_t number, uint8_t repeats, uint8_t volume)
{
    static uint32_t vols[8] = {
        0x02000, 0x04000, 0x06000, 0x08000,
        0x0a000, 0x0c000, 0x0e000, 0x10000
    };

    queued.number = 0;

    glk_schannel_set_volume(sound_channel, vols[volume - 1]);

    if (glk_schannel_play_ext(sound_channel, number, repeats == 255 ? -1 : repeats, 1)) {
        sound_playing = true;
    } else {
        sound_playing = false;
        sound_routine = 0;
    }
}

void sound_stopped(void)
{
    sound_playing = false;

    if (queued.number != 0) {
        start_sound(queued.number, 1, queued.volume);
        queued.number = 0;
    }
}
#endif

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

        // Implement the hack described in the remarks to §9 for The
        // Lurking Horror. Moreover, only impelement this for the two
        // samples where it is relevant, since this behavior is so
        // idiosyncratic that it should be ignored everywhere else. In
        // addition, this doesn’t bother checking whether player input
        // has occurred since the previous sound was started, because
        // for the two specific samples in The Lurking Horror for which
        // this hack applies, it’s known that there will not have been
        // any input.
        //
        // Don’t queue the number of repeats: always play a queued sound
        // only once. This is because one of the queued sounds
        // (S-CRETIN, number 16) has a built-in loop; the game knows
        // this so explicitly stops the sample in the same game round as
        // it was started to prevent it from looping indefinitely.
        // Modern systems are so fast that this stop event comes in
        // roughly simultaneously with the start event, so the sound
        // doesn’t play at all, if the stop event is honored. As such,
        // stop events are ignored below if a queued sound exists, so
        // the “stopping” must happen here by only playing the sample
        // once.
        //
        // This seems to be the only place S-CRETIN is used so it
        // probably doesn’t even need to be tagged as a repeating sound,
        // but there’s no harm in doing so.
        if (is_lurking_horror() && sound_playing && (number == 9 || number == 16)) {
            queued.number = number;
            queued.volume = volume;
            return;
        }

        sound_routine = znargs >= 4 ? zargs[3] : 0;
        start_sound(number, repeats, volume);

        break;
    }
    case SOUND_EFFECT_STOP:
        // If there is a queued sound, ignore stop requests. This is
        // because The Lurking Horror does the following in one single
        // game round:
        //
        // @sound_effect 11 2 8
        // @sound_effect 16 2 8
        // @sound_effect 16 3
        //
        // Per the remarks to §9, this is because Infocom knew the Amiga
        // interpreter (the only system where a sound-enabled game was
        // available) was slow, so sound effects would be able to play
        // while the interpreter was running between user inputs. Thus
        // there was presumably a fair amount of time between the second
        // and third @sound_effect calls, meaning that the stop event
        // came in after the sample had played, or at least partially
        // played. On fast modern systems, the stop call will be
        // processed before either sound effect has had any time to
        // play. A queued sound will only play once, so ignoring this
        // stop request is effectively just delaying it till the queued
        // sound finishes.
        if (queued.number == 0) {
            glk_schannel_stop(sound_channel);
            sound_routine = 0;
        }
        break;
    case SOUND_EFFECT_FINISH:
        glk_sound_load_hint(number, 0);
        break;
    }
#endif
}
