/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson, Lorenzo Marcantonio.             *
 * Copyright (C) 2010 by Ben Cressey, Chris Spiegel.                          *
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

/* SDL support donated by Lorenzo Marcantonio */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include <SDL.h>
#include <SDL_mixer.h>

#include "glk.h"
#include "garglk.h"

#include "gi_blorb.h"

#define giblorb_ID_MOD  (giblorb_make_id('M', 'O', 'D', ' '))
#define giblorb_ID_OGG  (giblorb_make_id('O', 'G', 'G', 'V'))
#define giblorb_ID_FORM (giblorb_make_id('F', 'O', 'R', 'M'))
#define giblorb_ID_AIFF (giblorb_make_id('A', 'I', 'F', 'F'))

/* non-standard types */
#define giblorb_ID_MP3  (giblorb_make_id('M', 'P', '3', ' '))
#define giblorb_ID_WAVE (giblorb_make_id('W', 'A', 'V', 'E'))
#define giblorb_ID_MIDI (giblorb_make_id('M', 'I', 'D', 'I'))

#define SDL_CHANNELS 64
#define GLK_MAXVOLUME 0x10000
#define FADE_GRANULARITY 100

#define GLK_VOLUME_TO_SDL_VOLUME(x) ((x) < GLK_MAXVOLUME ? (round(pow(((double)x) / GLK_MAXVOLUME, log(4)) * MIX_MAX_VOLUME)) : (MIX_MAX_VOLUME))

static channel_t *gli_channellist = NULL;
static channel_t *sound_channels[SDL_CHANNELS];
static channel_t *music_channel;

static const int FREE = 1;
static const int BUSY = 2;

void gli_initialize_sound(void)
{
    if (gli_conf_sound == 1)
    {
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER) == -1)
        {
            gli_strict_warning("SDL init failed\n");
            gli_strict_warning("%s", SDL_GetError());
            gli_conf_sound = 0;
            return;
        }
        // MixInit?

        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == -1)
        {
            gli_strict_warning("SDL Mixer init failed\n");
            gli_strict_warning("%s", Mix_GetError());
            gli_conf_sound = 0;
            return;
        }

        int channels = Mix_AllocateChannels(SDL_CHANNELS);
        Mix_GroupChannels(0, channels - 1 , FREE);
        Mix_ChannelFinished(NULL);
    }
}

schanid_t glk_schannel_create(glui32 rock)
{
    return  glk_schannel_create_ext(rock, GLK_MAXVOLUME);
}

schanid_t glk_schannel_create_ext(glui32 rock, glui32 volume)
{
    channel_t *chan;

    if (!gli_conf_sound)
        return NULL;
    chan = malloc(sizeof(channel_t));

    if (!chan)
        return NULL;

    chan->rock = rock;
    chan->status = CHANNEL_IDLE;
    chan->volume = GLK_VOLUME_TO_SDL_VOLUME(volume);
    chan->resid = 0;
    chan->loop = 0;
    chan->notify = 0;
    chan->sdl_memory = 0;
    chan->sdl_rwops = 0;
    chan->sample = 0;
    chan->paused = 0;
    chan->sdl_channel = -1;
    chan->music = 0;

    chan->volume_notify = 0;
    chan->volume_timeout = 0;
    chan->target_volume = 0;
    chan->float_volume = 0;
    chan->volume_delta = 0;
    chan->timer = 0;

    chan->chain_prev = NULL;
    chan->chain_next = gli_channellist;
    gli_channellist = chan;
    if (chan->chain_next)
        chan->chain_next->chain_prev = chan;

    if (gli_register_obj)
        chan->disprock = (*gli_register_obj)(chan, gidisp_Class_Schannel);
    else
        chan->disprock.ptr = NULL;

    return chan;
}

static void cleanup_channel(schanid_t chan)
{
    if (chan->sdl_rwops)
    {
        SDL_FreeRW(chan->sdl_rwops);
        chan->sdl_rwops = 0;
    }
    if (chan->sdl_memory)
    {
        free(chan->sdl_memory);
        chan->sdl_memory = 0;
    }
    switch (chan->status)
    {
        case CHANNEL_SOUND:
            if (chan->sample)
                Mix_FreeChunk(chan->sample);
            if (chan->sdl_channel >= 0)
            {
                Mix_GroupChannel(chan->sdl_channel, FREE);
                sound_channels[chan->sdl_channel] = 0;
            }
            break;
        case CHANNEL_MUSIC:
            if (chan->music)
            {
                Mix_FreeMusic(chan->music);
                music_channel = 0;
            }
            break;
    }
    chan->status = CHANNEL_IDLE;
    chan->sdl_channel = -1;
    chan->music = 0;

    if (chan->timer)
        SDL_RemoveTimer(chan->timer);

    chan->timer = 0;
}

void glk_schannel_destroy(schanid_t chan)
{
    channel_t *prev, *next;

    if (!chan)
    {
        gli_strict_warning("schannel_destroy: invalid id.");
        return;
    }

    glk_schannel_stop(chan);
    cleanup_channel(chan);
    if (gli_unregister_obj)
        (*gli_unregister_obj)(chan, gidisp_Class_Schannel, chan->disprock);

    prev = chan->chain_prev;
    next = chan->chain_next;
    chan->chain_prev = NULL;
    chan->chain_next = NULL;

    if (prev)
        prev->chain_next = next;
    else
        gli_channellist = next;

    if (next)
        next->chain_prev = prev;

    free(chan);
}

schanid_t glk_schannel_iterate(schanid_t chan, glui32 *rock)
{
    if (!chan)
        chan = gli_channellist;
    else
        chan = chan->chain_next;

    if (chan)
    {
        if (rock)
            *rock = chan->rock;
        return chan;
    }

    if (rock)
        *rock = 0;
    return NULL;
}

glui32 glk_schannel_get_rock(schanid_t chan)
{
    if (!chan)
    {
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
    int i;
    int successes = 0;

    for (i = 0; i < chancount; i++)
    {
        successes += glk_schannel_play_ext(chanarray[i], sndarray[i], 1, notify);
    }

    return successes;
}

void glk_sound_load_hint(glui32 snd, glui32 flag)
{
    /* nop */
}

/** Make an incremental volume change when the fade timer fires */
Uint32 volume_timer_callback(Uint32 interval, void *param)
{
    schanid_t chan = (schanid_t)param;

    if (!chan)
    {
        gli_strict_warning("volume_timer_callback: invalid channel.");
        return 0;
    }

    chan->float_volume += chan->volume_delta;

    if (chan->float_volume < 0)
        chan->float_volume = 0;

    if ((int)chan->float_volume != chan->volume)
    {
        chan->volume = (int)chan->float_volume;

        if (chan->status == CHANNEL_SOUND)
            Mix_Volume(chan->sdl_channel, chan->volume);
        else if (chan->status == CHANNEL_MUSIC)
            Mix_VolumeMusic(chan->volume);
    }

    chan->volume_timeout--;

    /* If the timer has fired FADE_GRANULARITY times, kill it */
    if (chan->volume_timeout <= 0)
    {
        if (chan->volume_notify)
        {
            gli_event_store(evtype_VolumeNotify, 0,
                0, chan->volume_notify);
            gli_notification_waiting();
        }

        if (!chan->timer)
        {
            gli_strict_warning("volume_timer_callback: invalid timer.");
            return 0;
        }
        SDL_RemoveTimer(chan->timer);
        chan->timer = 0;

        if (chan->volume != chan->target_volume)
        {
            chan->volume = chan->target_volume;
            if (chan->status == CHANNEL_SOUND)
                Mix_Volume(chan->sdl_channel, chan->volume);
            else if (chan->status == CHANNEL_MUSIC)
                Mix_VolumeMusic(chan->volume);
        }
        return 0;
    }

    return interval;
}

/** Start a fade timer */
void init_fade(schanid_t chan, int glk_volume, int duration, int notify)
{
    if (!chan)
    {
        gli_strict_warning("init_fade: invalid channel.");
        return;
    }

    chan->volume_notify = notify;

    chan->target_volume = GLK_VOLUME_TO_SDL_VOLUME(glk_volume);

    chan->float_volume = (double)chan->volume;
    chan->volume_delta = (double)(chan->target_volume - chan->volume) / FADE_GRANULARITY;

    chan->volume_timeout = FADE_GRANULARITY;

    if (chan->timer)
        SDL_RemoveTimer(chan->timer);

    chan->timer = SDL_AddTimer((Uint32)(duration / FADE_GRANULARITY), volume_timer_callback, (void *)chan);

    if (!chan->timer)
    {
        gli_strict_warning("init_fade: failed to create volume change timer.");
        return;
    }
}

void glk_schannel_set_volume(schanid_t chan, glui32 vol)
{
    glk_schannel_set_volume_ext(chan, vol, 0, 0);
}

void glk_schannel_set_volume_ext(schanid_t chan, glui32 glk_volume,
        glui32 duration, glui32 notify)
{
    if (!chan)
    {
        gli_strict_warning("schannel_set_volume: invalid id.");
        return;
    }

    if (!duration)
    {

        chan->volume = GLK_VOLUME_TO_SDL_VOLUME(glk_volume);

        switch (chan->status)
        {
            case CHANNEL_IDLE:
                break;
            case CHANNEL_SOUND:
                Mix_Volume(chan->sdl_channel, chan->volume);
                break;
            case CHANNEL_MUSIC:
                Mix_VolumeMusic(chan->volume);
                break;
        }
    }
    else
    {
        init_fade(chan, glk_volume, duration, notify);
    }
}

/* Notify the music channel completion */
static void music_completion_callback()
{
    if (!music_channel)
    {
        gli_strict_warning("music callback failed");
        return;
    }
    else
    {
        gli_event_store(evtype_SoundNotify, 0, music_channel->resid,
            music_channel->notify);
        gli_notification_waiting();
    }
    cleanup_channel(music_channel);
}

/* Notify the sound channel completion */
static void sound_completion_callback(int chan)
{
    channel_t *sound_channel = sound_channels[chan];

    if (!sound_channel)
    {
        gli_strict_warning("sound completion callback called with invalid channel");
        return;
    }

    if (sound_channel->notify)
    {
        gli_event_store(evtype_SoundNotify, 0,
            sound_channel->resid, sound_channel->notify);
        gli_notification_waiting();
    }
    cleanup_channel(sound_channel);
    sound_channels[chan] = 0;
    return;
}

static glui32 load_sound_resource(glui32 snd, long *len, char **buf)
{
    if (!giblorb_is_resource_map())
    {
        FILE *file;
        char name[1024];

        snprintf(name, sizeof name, "%s/SND%ld", gli_workdir, (long) snd);

        file = fopen(name, "rb");
        if (!file)
            return 0;

        fseek(file, 0, SEEK_END);
        *len = ftell(file);

        *buf = malloc(*len);
        if (!*buf)
        {
            fclose(file);
            return 0;
        }

        fseek(file, 0, 0);
        if (fread(*buf, 1, *len, file) != *len && !feof(file))
        {
            fclose(file);
            return 0;
        }
        fclose(file);

        /* AIFF */
        if (*len > 4 && !memcmp(*buf, "FORM", 4))
            return giblorb_ID_FORM;

        /* WAVE */
        if (*len > 4 && !memcmp(*buf, "WAVE", 4))
            return giblorb_ID_WAVE;

        if (*len > 4 && !memcmp(*buf, "RIFF", 4))
            return giblorb_ID_WAVE;

        /* midi */
        if (*len > 4 && !memcmp(*buf, "MThd", 4))
            return giblorb_ID_MIDI;

        /* s3m */
        if (*len > 0x30 && !memcmp(*buf + 0x2c, "SCRM", 4))
            return giblorb_ID_MOD;

        /* XM */
        if (*len > 20 && !memcmp(*buf, "Extended Module: ", 17))
            return giblorb_ID_MOD;

        /* MOD */
        if (*len > 1084)
        {
            char resname[4];
            memcpy(resname, (*buf) + 1080, 4);
            if (!strcmp(resname+1, "CHN") ||        /* 4CHN, 6CHN, 8CHN */
                    !strcmp(resname+2, "CN") ||         /* 16CN, 32CN */
                    !strcmp(resname, "M.K.") || !strcmp(resname, "M!K!") ||
                    !strcmp(resname, "FLT4") || !strcmp(resname, "CD81") ||
                    !strcmp(resname, "OKTA") || !strcmp(resname, "    "))
                return giblorb_ID_MOD;
        }

        /* ogg */
        if (*len > 4 && !memcmp(*buf, "OggS", 4))
            return giblorb_ID_OGG;

        /* mp3 */
        if (*len > 2 && !memcmp(*buf, "\377\372", 2))
            return giblorb_ID_MP3;

        return giblorb_ID_MP3;
    }
    else
    {
        FILE *file;
        glui32 type;
        long pos;

        giblorb_get_resource(giblorb_ID_Snd, snd, &file, &pos, len, &type);
        if (!file)
            return 0;

        *buf = malloc(*len);
        if (!*buf)
            return 0;

        fseek(file, pos, 0);
        if (fread(*buf, 1, *len, file) != *len && !feof(file)) return 0;
        return type;
    }
}

/** Start a sound channel */
static glui32 play_sound(schanid_t chan)
{
    int loop;
    SDL_LockAudio();
    chan->status = CHANNEL_SOUND;
    chan->sdl_channel = Mix_GroupAvailable(FREE);
    Mix_GroupChannel(chan->sdl_channel, BUSY);
    SDL_UnlockAudio();
    chan->sample = Mix_LoadWAV_RW(chan->sdl_rwops, FALSE);
    if (chan->sdl_channel < 0)
    {
        gli_strict_warning("No available sound channels");
    }
    if (chan->sdl_channel >= 0 && chan->sample)
    {
        SDL_LockAudio();
        sound_channels[chan->sdl_channel] = chan;
        SDL_UnlockAudio();
        Mix_Volume(chan->sdl_channel, chan->volume);
        Mix_ChannelFinished(&sound_completion_callback);
        loop = chan->loop - 1;
        if (loop < -1)
            loop = -1;
        if (Mix_PlayChannel(chan->sdl_channel, chan->sample, loop) >= 0)
            return 1;
    }
    gli_strict_warning("play sound failed");
    gli_strict_warning("%s", Mix_GetError());
    SDL_LockAudio();
    cleanup_channel(chan);
    SDL_UnlockAudio();
    return 0;
}

/** Start a mod music channel */
static glui32 play_mod(schanid_t chan, long len)
{
    FILE *file;
    char *tn, *tempdir;
    int music_busy, loop;

    if (chan == NULL)
    {
        gli_strict_warning("MOD player called with an invalid channel!");
        return 0;
    }

    music_busy = Mix_PlayingMusic();

    if (music_busy)
    {
        /* We already checked for music playing on *this* channel
        in glk_schannel_play_ext */

        gli_strict_warning("MOD player already in use on another channel!");
        return 0;
    }

    chan->status = CHANNEL_MUSIC;
    /* The fscking mikmod lib want to read the mod only from disk! */
    tempdir = getenv("TMPDIR");
    if (tempdir == NULL)
    {
        tempdir = getenv("TEMP");
        if (tempdir == NULL)
        {
            tempdir = getenv("TMP");
            if (tempdir == NULL)
            {
                tempdir = ".";
            }
        }
    }

    /* malloc size of string tempdir + "XXXXXX' + terminator */
    tn = malloc(strlen(tempdir + 7));
    sprintf(tn, "%sXXXXXX", tempdir);
    int fd;
    fd = mkstemp(tn);
    if (fd == -1)
    {
        free(tn);
        gli_strict_warning("play mod failed: could not create temp file");
        return 0;
    }
    file = fdopen(fd, "wb");
    fwrite(chan->sdl_memory, 1, len, file);
    fclose(file);
    chan->music = Mix_LoadMUS(tn);
    remove(tn);
    free(tn);
    if (chan->music)
    {
        SDL_LockAudio();
        music_channel = chan;
        SDL_UnlockAudio();
        Mix_VolumeMusic(chan->volume);
        Mix_HookMusicFinished(&music_completion_callback);
        loop = chan->loop;
        if (loop < -1)
            loop = -1;
        if (Mix_PlayMusic(chan->music, loop) >= 0)
            return 1;
    }
    gli_strict_warning("play mod failed");
    gli_strict_warning("%s", Mix_GetError());
    SDL_LockAudio();
    cleanup_channel(chan);
    SDL_UnlockAudio();
    return 0;
}

glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats, glui32 notify)
{
    long len = 0;
    glui32 type;
    glui32 result = 0;
    glui32 paused = 0;
    char *buf = 0;

    if (!chan)
    {
        gli_strict_warning("schannel_play_ext: invalid id.");
        return 0;
    }

    /* store paused state of channel */
    paused = chan->paused;

    /* stop previous noise */
    glk_schannel_stop(chan);

    if (repeats == 0)
        return 1;

    /* load sound resource into memory */
    type = load_sound_resource(snd, &len, &buf);

    chan->sdl_memory = (unsigned char*)buf;
    chan->sdl_rwops = SDL_RWFromConstMem(buf, (int)len);
    chan->notify = notify;
    chan->resid = snd;
    chan->loop = repeats;

    switch (type)
    {
        case giblorb_ID_FORM:
        case giblorb_ID_AIFF:
        case giblorb_ID_WAVE:
        case giblorb_ID_OGG:
        case giblorb_ID_MP3:
            result = play_sound(chan);
            break;

        case giblorb_ID_MOD:
        case giblorb_ID_MIDI:
            result = play_mod(chan, len);
            break;

        default:
            gli_strict_warning("schannel_play_ext: unknown resource type.");
    }

    /* if channel was paused it should be paused again */
    if (result && paused)
    {
        glk_schannel_pause(chan);
    }

    return result;
}

void glk_schannel_pause(schanid_t chan)
{
    if (!chan)
    {
        gli_strict_warning("schannel_pause: invalid id.");
        return;
    }

    switch (chan->status)
    {
        case CHANNEL_SOUND:
            Mix_Pause(chan->sdl_channel);
            break;
        case CHANNEL_MUSIC:
            Mix_PauseMusic();
            break;
    }

    chan->paused = 1;
}

void glk_schannel_unpause(schanid_t chan)
{
    if (!chan)
    {
        gli_strict_warning("schannel_unpause: invalid id.");
        return;
    }
    switch (chan->status)
    {
        case CHANNEL_SOUND:
            Mix_Resume(chan->sdl_channel);
            break;
        case CHANNEL_MUSIC:
            Mix_ResumeMusic();
            break;
    }

    chan->paused = 0;
}

void glk_schannel_stop(schanid_t chan)
{
    if (!chan)
    {
        gli_strict_warning("schannel_stop: invalid id.");
        return;
    }
    SDL_LockAudio();
    chan->paused = 0;
    glk_schannel_unpause(chan);
    SDL_UnlockAudio();
    switch (chan->status)
    {
        case CHANNEL_SOUND:
            chan->notify = 0;
            Mix_HaltChannel(chan->sdl_channel);
            break;
        case CHANNEL_MUSIC:
            if (music_channel == chan)
            {
                Mix_HookMusicFinished(NULL);
            }
            Mix_HaltMusic();
            break;
    }
    SDL_LockAudio();
    cleanup_channel(chan);
    SDL_UnlockAudio();
}
