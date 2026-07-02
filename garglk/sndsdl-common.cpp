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

#include <mutex>
#include <set>

#ifdef GARGLK_CONFIG_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include "glk.h"
#include "garglk.h"

#include "sndsdl-common.h"

// volume_timer_callback runs on SDL's timer thread, and SDL_RemoveTimer does
// not synchronize against it: a callback that has already passed SDL's internal
// "canceled" check still runs to completion even after its timer is removed. To
// keep such an in-flight callback from touching a channel that is being freed,
// the callback and the teardown path coordinate through this mutex, and the
// callback refuses to touch any channel that is no longer present in
// fade_timer_channels. A channel is added when its fade timer is armed and
// removed (under the lock) by cancel_fade.
static std::mutex fade_timer_mutex;
static std::set<SdlSoundChannel *> fade_timer_channels;

// Make an incremental volume change when the fade timer fires.
#ifdef GARGLK_CONFIG_SDL3
static Uint32 volume_timer_callback(void *param, SDL_TimerID id, Uint32 interval)
#else
static Uint32 volume_timer_callback(Uint32 interval, void *param)
#endif
{
    auto *chan = static_cast<SdlSoundChannel *>(param);

    if (chan == nullptr) {
        gli_strict_warning("volume_timer_callback: invalid channel.");
        return 0;
    }

    // Held for the whole body so the teardown path can wait out an in-flight
    // callback before freeing the channel. If the channel is gone from the
    // registry it has been (or is being) destroyed; bail without touching it.
    // Only the pointer value is compared here, never dereferenced, so this is
    // safe even if chan was already freed.
    std::lock_guard<std::mutex> guard(fade_timer_mutex);
    if (fade_timer_channels.find(chan) == fade_timer_channels.end()) {
        return 0;
    }

    chan->float_volume += chan->volume_delta;

    if (chan->float_volume < 0) {
        chan->float_volume = 0;
    }

    if (static_cast<int>(chan->float_volume) != chan->volume) {
        chan->volume = static_cast<int>(chan->float_volume);
        chan->apply_volume();
    }

    chan->volume_timeout--;

    // If the timer has fired FADE_GRANULARITY times, kill it.
    if (chan->volume_timeout <= 0) {
        if (chan->volume_notify != 0) {
            gli_event_store(evtype_VolumeNotify, nullptr,
                0, chan->volume_notify);
            gli_notification_waiting();
        }

        if (chan->timer == 0) {
            gli_strict_warning("volume_timer_callback: invalid timer.");
            return 0;
        }
        SDL_RemoveTimer(chan->timer);
        chan->timer = 0;

        if (chan->volume != chan->target_volume) {
            chan->volume = chan->target_volume;
            chan->apply_volume();
        }
        return 0;
    }

    return interval;
}

void cancel_fade(SdlSoundChannel *chan)
{
    // Held across the timer teardown too: the timer-thread callback reads and
    // writes chan->timer under this same lock, so touching it here without the
    // lock would be a data race.
    std::lock_guard<std::mutex> guard(fade_timer_mutex);

    if (chan->timer != 0) {
        SDL_RemoveTimer(chan->timer);
        chan->timer = 0;
    }

    fade_timer_channels.erase(chan);
}

void init_fade(SdlSoundChannel *chan, int vol, int duration, int notify)
{
    if (chan == nullptr) {
        gli_strict_warning("init_fade: invalid channel.");
        return;
    }

    // Cancel any in-progress fade (and wait out its in-flight callback) before
    // overwriting the fade state, so the old timer can't race these writes.
    cancel_fade(chan);

    chan->volume_notify = notify;
    chan->target_volume = GLK_VOLUME_TO_SDL_VOLUME(vol);
    chan->float_volume = static_cast<double>(chan->volume);
    chan->volume_delta = static_cast<double>(chan->target_volume - chan->volume) / FADE_GRANULARITY;
    chan->volume_timeout = FADE_GRANULARITY;

    // Register the channel before arming the timer: the callback can fire (and
    // check fade_timer_channels) before SDL_AddTimer even returns.
    {
        std::lock_guard<std::mutex> guard(fade_timer_mutex);
        fade_timer_channels.insert(chan);
    }

    chan->timer = SDL_AddTimer(static_cast<Uint32>(duration / FADE_GRANULARITY), volume_timer_callback, chan);

    if (chan->timer == 0) {
        std::lock_guard<std::mutex> guard(fade_timer_mutex);
        fade_timer_channels.erase(chan);
        gli_strict_warning("init_fade: failed to create volume change timer.");
        return;
    }
}
