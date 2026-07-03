// Copyright (C) 2006-2009 by Tor Andersson, Lorenzo Marcantonio.
// Copyright (C) 2010 by Ben Cressey, Chris Spiegel.
//
// This file is part of Gargoyle.
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Gargoyle is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Gargoyle; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#ifndef GARGLK_SNDSDL_COMMON_H
#define GARGLK_SNDSDL_COMMON_H

// Volume-fade machinery shared by the SDL2 and SDL3 sound backends. Both arm an
// SDL timer that nudges a channel's volume toward a target; that timer logic is
// thread-involved (it runs on SDL's timer thread and has historically been a
// source of races), so it lives here in one place. Each backend derives its
// glk_schannel_struct from SdlSoundChannel and implements apply_volume() to
// push the current volume to its own audio sink.

#include <cmath>

#ifdef GARGLK_CONFIG_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include "glk.h"

#define GLK_MAXVOLUME 0x10000
#define GLK_SDL_MAX_VOLUME 128
#define FADE_GRANULARITY 100

// Map a Glk volume (0..GLK_MAXVOLUME) onto the internal 0..GLK_SDL_MAX_VOLUME
// scale with a perceptual power curve.
#define GLK_VOLUME_TO_SDL_VOLUME(x) ((x) < GLK_MAXVOLUME ? (std::round(std::pow(((double)x) / GLK_MAXVOLUME, std::log(4)) * GLK_SDL_MAX_VOLUME)) : (GLK_SDL_MAX_VOLUME))

struct SdlSoundChannel {
    // Current volume on the 0..GLK_SDL_MAX_VOLUME scale.
    int volume = 0;

    // Volume-fade state, driven by the shared fade timer.
    int volume_notify = 0;
    int volume_timeout = 0;
    int target_volume = 0;
    double float_volume = 0;
    double volume_delta = 0;
    SDL_TimerID timer = 0;

    virtual ~SdlSoundChannel() = default;

    // Push the current `volume` to the backend's audio sink. A no-op when
    // nothing is playing on the channel.
    virtual void apply_volume() = 0;
};

// The fade timer runs apply_volume() on SDL's timer thread. In the SDL2 backend
// apply_volume() calls into SDL_mixer, which takes the audio-device lock, and
// every teardown path already holds that device lock when it stops a fade. To
// keep a single, consistent lock order (device lock, then fade mutex) the timer
// callback acquires the backend's device lock before the fade mutex via these
// hooks; without that the timer thread (fade mutex -> device lock) and a
// teardown (device lock -> fade mutex) can deadlock. Each backend provides an
// implementation: SDL2 locks the mixer's device, SDL3 has no such global lock
// and implements them as no-ops.
void gli_sound_backend_lock();
void gli_sound_backend_unlock();

// Fade chan to Glk volume vol over duration ms, firing a VolumeNotify carrying
// notify when complete. Any fade already running on the channel is cancelled.
void init_fade(SdlSoundChannel *chan, int vol, int duration, int notify);

// Stop any in-progress fade on chan and stop tracking it. After this returns no
// fade callback will touch the channel until a new fade is armed, so it is also
// what the teardown path calls before a channel is freed.
void cancel_fade(SdlSoundChannel *chan);

#endif
