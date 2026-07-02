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

// SDL3 sound backend. Unlike the SDL2 backend (sndsdl.cpp), this does not use
// SDL_mixer: it decodes through the shared garglk::Decoder layer (snddecode)
// and feeds one SDL_AudioStream per channel, letting base SDL3 mix the bound
// streams and resample each to the device. The decoder libraries (libsndfile,
// mpg123, libopenmpt, fluidsynth) are the same ones the Qt backend uses, so
// format support is identical and SDL3_mixer is not required.

#ifdef _WIN32
#define SDL_MAIN_HANDLED
#endif

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "glk.h"
#include "garglk.h"

#include "gi_blorb.h"

#include "snddecode.h"

#include "sndsdl-common.h"

#define giblorb_ID_MOD  (giblorb_make_id('M', 'O', 'D', ' '))
#define giblorb_ID_OGG  (giblorb_make_id('O', 'G', 'G', 'V'))
#define giblorb_ID_FORM (giblorb_make_id('F', 'O', 'R', 'M'))
#define giblorb_ID_AIFF (giblorb_make_id('A', 'I', 'F', 'F'))

// non-standard types
#define giblorb_ID_MP3  (giblorb_make_id('M', 'P', '3', ' '))
#define giblorb_ID_WAVE (giblorb_make_id('W', 'A', 'V', 'E'))
#define giblorb_ID_MIDI (giblorb_make_id('M', 'I', 'D', 'I'))

static float volume_to_gain(int vol)
{
    return static_cast<float>(vol) / GLK_SDL_MAX_VOLUME;
}

struct glk_schannel_struct : SdlSoundChannel {
    glui32 rock;

    SDL_AudioStream *stream;
    std::shared_ptr<garglk::Decoder> decoder;
    std::vector<unsigned char> buffer; // reused by the stream callback

    int resid; // for notifies
    int notify;

    bool paused;
    bool notified; // SoundNotify already fired for this play

    gidispatch_rock_t disprock;
    channel_t *chain_next, *chain_prev;

    void apply_volume() override {
        if (stream != nullptr) {
            SDL_SetAudioStreamGain(stream, volume_to_gain(volume));
        }
    }
};

static channel_t *gli_channellist = nullptr;

static SDL_AudioDeviceID device = 0;

static schanid_t gli_bleep_channel;

gidispatch_rock_t gli_sound_get_channel_disprock(const channel_t *chan)
{
    return chan->disprock;
}

// SDL3 mixes streams without a single global audio-device lock (each stream has
// its own lock, and the fade path never needs it), so the lock-order hooks the
// fade timer uses are no-ops here; see sndsdl-common.h.
void gli_sound_backend_lock()
{
}

void gli_sound_backend_unlock()
{
}

// Pull decoded PCM for one channel. SDL calls this from its audio thread while
// holding the stream's lock, and mixes the result with the other bound streams.
static void SDLCALL stream_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int /* total_amount */)
{
    auto *chan = static_cast<channel_t *>(userdata);

    if (chan == nullptr || chan->decoder == nullptr) {
        return;
    }

    // A paused channel simply supplies no data, so it mixes as silence while
    // its decode position holds.
    if (!chan->paused && additional_amount > 0 && !chan->decoder->finished()) {
        if (chan->buffer.size() < static_cast<std::size_t>(additional_amount)) {
            chan->buffer.resize(additional_amount);
        }

        std::size_t n = chan->decoder->read(chan->buffer.data(), additional_amount);
        if (n > 0) {
            SDL_PutAudioStreamData(stream, chan->buffer.data(), static_cast<int>(n));
        }
    }

    // Fire the notify once the decoder is exhausted and the buffered tail has
    // drained, i.e. the sound has actually finished playing (not merely
    // finished decoding).
    if (chan->decoder->finished() && SDL_GetAudioStreamAvailable(stream) == 0 && !chan->notified) {
        chan->notified = true;
        if (chan->notify != 0) {
            gli_event_store(evtype_SoundNotify, nullptr, chan->resid, chan->notify);
            gli_notification_waiting();
        }
    }
}

void gli_initialize_sound()
{
    if (gli_conf_sound) {
        SDL_SetMainReady();
        if (!SDL_Init(SDL_INIT_AUDIO)) {
            gli_strict_warning("SDL init failed\n");
            gli_strict_warning(SDL_GetError());
            gli_conf_sound = false;
            return;
        }

        device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (device == 0) {
            gli_strict_warning("SDL audio device creation failed\n");
            gli_strict_warning(SDL_GetError());
            gli_conf_sound = false;
            return;
        }

        // A device opened with SDL_OpenAudioDevice plays bound streams without
        // an explicit resume, but resuming is a no-op if already running and
        // guards against a silent device.
        SDL_ResumeAudioDevice(device);

        garglk::init_decoders();
    }
}

schanid_t glk_schannel_create(glui32 rock)
{
    return glk_schannel_create_ext(rock, GLK_MAXVOLUME);
}

schanid_t glk_schannel_create_ext(glui32 rock, glui32 volume)
{
    channel_t *chan;

    if (!gli_conf_sound) {
        return nullptr;
    }
    chan = new channel_t;

    chan->rock = rock;
    chan->volume = GLK_VOLUME_TO_SDL_VOLUME(volume);
    chan->resid = 0;
    chan->notify = 0;
    chan->paused = false;
    chan->notified = false;

    chan->stream = nullptr;

    chan->volume_notify = 0;
    chan->volume_timeout = 0;
    chan->target_volume = 0;
    chan->float_volume = 0;
    chan->volume_delta = 0;
    chan->timer = 0;

    chan->chain_prev = nullptr;
    chan->chain_next = gli_channellist;
    gli_channellist = chan;
    if (chan->chain_next != nullptr) {
        chan->chain_next->chain_prev = chan;
    }

    if (gli_register_obj != nullptr) {
        chan->disprock = (*gli_register_obj)(chan, gidisp_Class_Schannel);
    } else {
        chan->disprock.ptr = nullptr;
    }

    return chan;
}

static void cleanup_channel(schanid_t chan)
{
    // Stop any fade first: cancel_fade waits out an in-flight fade callback and
    // untracks the channel, so afterward no fade callback will call
    // apply_volume() (and thus touch chan->stream).
    cancel_fade(chan);

    // Destroying the stream unbinds it and, per SDL, guarantees the stream
    // get-callback is no longer running once this returns, so the decoder can
    // then be freed.
    if (chan->stream != nullptr) {
        SDL_DestroyAudioStream(chan->stream);
        chan->stream = nullptr;
    }

    chan->decoder.reset();
    chan->buffer.clear();

    chan->paused = false;
    chan->notified = false;
}

void glk_schannel_destroy(schanid_t chan)
{
    channel_t *prev, *next;

    if (chan == nullptr) {
        gli_strict_warning("schannel_destroy: invalid id.");
        return;
    }

    glk_schannel_stop(chan);
    cleanup_channel(chan);
    if (gli_unregister_obj != nullptr) {
        (*gli_unregister_obj)(chan, gidisp_Class_Schannel, chan->disprock);
    }

    prev = chan->chain_prev;
    next = chan->chain_next;
    chan->chain_prev = nullptr;
    chan->chain_next = nullptr;

    if (prev != nullptr) {
        prev->chain_next = next;
    } else {
        gli_channellist = next;
    }

    if (next != nullptr) {
        next->chain_prev = prev;
    }

    delete chan;
}

schanid_t glk_schannel_iterate(schanid_t chan, glui32 *rock)
{
    if (chan == nullptr) {
        chan = gli_channellist;
    } else {
        chan = chan->chain_next;
    }

    if (chan != nullptr) {
        if (rock != nullptr) {
            *rock = chan->rock;
        }
        return chan;
    }

    if (rock != nullptr) {
        *rock = 0;
    }
    return nullptr;
}

glui32 glk_schannel_get_rock(schanid_t chan)
{
    if (chan == nullptr) {
        gli_strict_warning("schannel_get_rock: invalid id.");
        return 0;
    }
    return chan->rock;
}

glui32 glk_schannel_play(schanid_t chan, glui32 snd)
{
    return glk_schannel_play_ext(chan, snd, 1, 0);
}

glui32 glk_schannel_play_multi(schanid_t *chanarray, glui32 chancount,
        glui32 *sndarray, glui32 soundcount, glui32 notify)
{
    int successes = 0;

    for (glui32 i = 0; i < chancount; i++) {
        successes += glk_schannel_play_ext(chanarray[i], sndarray[i], 1, notify);
    }

    return successes;
}

void glk_sound_load_hint(glui32 snd, glui32 flag)
{
    // nop
}

void glk_schannel_set_volume(schanid_t chan, glui32 vol)
{
    glk_schannel_set_volume_ext(chan, vol, 0, 0);
}

void glk_schannel_set_volume_ext(schanid_t chan, glui32 vol,
        glui32 duration, glui32 notify)
{
    if (chan == nullptr) {
        gli_strict_warning("schannel_set_volume: invalid id.");
        return;
    }

    if (duration == 0) {
        // An immediate volume set overrides any in-progress fade.
        cancel_fade(chan);

        chan->volume = GLK_VOLUME_TO_SDL_VOLUME(vol);
        chan->apply_volume();
    } else {
        init_fade(chan, vol, duration, notify);
    }
}

static glui32 play_sound(schanid_t chan, glui32 type, std::vector<unsigned char> &data, glui32 repeats)
{
    try {
        chan->decoder = garglk::create_decoder(type, std::move(data), repeats);
    } catch (const garglk::SoundError &) {
        gli_strict_warning("play sound failed");
        cleanup_channel(chan);
        return 0;
    }

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_F32;
    spec.channels = chan->decoder->channels();
    spec.freq = chan->decoder->samplerate();

    // dst is null: binding sets the stream's output format to the device's, and
    // the stream resamples each channel's native rate for us.
    chan->stream = SDL_CreateAudioStream(&spec, nullptr);
    if (chan->stream == nullptr) {
        gli_strict_warning("play sound failed");
        gli_strict_warning(SDL_GetError());
        cleanup_channel(chan);
        return 0;
    }

    chan->notified = false;

    SDL_SetAudioStreamGetCallback(chan->stream, stream_callback, chan);
    SDL_SetAudioStreamGain(chan->stream, volume_to_gain(chan->volume));

    if (!SDL_BindAudioStream(device, chan->stream)) {
        gli_strict_warning("play sound failed");
        gli_strict_warning(SDL_GetError());
        cleanup_channel(chan);
        return 0;
    }

    return 1;
}

static glui32 gli_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats, glui32 notify, const std::function<garglk::Expected<std::pair<int, std::vector<unsigned char>>>(glui32)> &load_resource)
{
    glui32 result = 0;
    bool paused = false;

    if (chan == nullptr) {
        gli_strict_warning("schannel_play_ext: invalid id.");
        return 0;
    }

    // store paused state of channel
    paused = chan->paused;

    // stop previous noise
    glk_schannel_stop(chan);

    if (repeats == 0) {
        return 1;
    }

    // load sound resource into memory
    auto resource = load_resource(snd);
    if (!resource.has_value()) {
        return 0;
    }
    auto [type, data] = std::move(*resource);

    chan->notify = notify;
    chan->resid = snd;

    // If the channel was paused, restore that before binding the stream so the
    // replayed sound starts silent rather than leaking a moment of audio before
    // a post-play re-pause. The flag is set before bind, so the callback (which
    // only runs once bound) sees it without a race.
    chan->paused = paused;

    switch (type) {
    case giblorb_ID_FORM:
    case giblorb_ID_AIFF:
    case giblorb_ID_WAVE:
    case giblorb_ID_OGG:
    case giblorb_ID_MP3:
    case giblorb_ID_MOD:
    case giblorb_ID_MIDI:
        result = play_sound(chan, type, data, repeats);
        break;

    default:
        gli_strict_warning("schannel_play_ext: unknown resource type.");
    }

    return result;
}

glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats, glui32 notify)
{
    return gli_schannel_play_ext(chan, snd, repeats, notify, garglk::load_sound_resource);
}

void glk_schannel_pause(schanid_t chan)
{
    if (chan == nullptr) {
        gli_strict_warning("schannel_pause: invalid id.");
        return;
    }

    // The flag is read by the stream callback, so set it under the stream lock.
    if (chan->stream != nullptr) {
        SDL_LockAudioStream(chan->stream);
        chan->paused = true;
        SDL_UnlockAudioStream(chan->stream);
    } else {
        chan->paused = true;
    }
}

void glk_schannel_unpause(schanid_t chan)
{
    if (chan == nullptr) {
        gli_strict_warning("schannel_unpause: invalid id.");
        return;
    }

    if (chan->stream != nullptr) {
        SDL_LockAudioStream(chan->stream);
        chan->paused = false;
        SDL_UnlockAudioStream(chan->stream);
    } else {
        chan->paused = false;
    }
}

void glk_schannel_stop(schanid_t chan)
{
    if (chan == nullptr) {
        gli_strict_warning("schannel_stop: invalid id.");
        return;
    }

    // Suppress the completion notify for an explicit stop, then tear down.
    // Clearing notify under the stream lock keeps an in-flight callback from
    // firing a spurious notify mid-stop; destroying the stream in
    // cleanup_channel then waits out any callback and stops future ones.
    if (chan->stream != nullptr) {
        SDL_LockAudioStream(chan->stream);
        chan->notify = 0;
        SDL_UnlockAudioStream(chan->stream);
    } else {
        chan->notify = 0;
    }

    cleanup_channel(chan);
}

void garglk_zbleep(glui32 number)
{
    if (gli_bleep_channel == nullptr) {
        gli_bleep_channel = glk_schannel_create(0);
        if (gli_bleep_channel != nullptr) {
            glk_schannel_set_volume(gli_bleep_channel, 0x8000);
        }
    }

    if (gli_bleep_channel != nullptr) {
        try {
            gli_schannel_play_ext(gli_bleep_channel, number, 1, 0, garglk::load_bleep_resource);
        } catch (const Bleeps::Empty &) {
        }
    }
}
