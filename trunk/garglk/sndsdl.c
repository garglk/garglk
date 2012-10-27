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
#include <SDL_sound.h>

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

#define SDL_CHANNELS 64

static channel_t *gli_channellist = NULL;
static channel_t *sound_channels[SDL_CHANNELS];
static channel_t *music_channel;

static Sound_AudioInfo *output = NULL;
static const int FREE = 1;
static const int BUSY = 2;

void gli_initialize_sound(void)
{
    if (gli_conf_sound == 1)
    {
        if (SDL_Init(SDL_INIT_AUDIO) == -1)
        {
            gli_strict_warning("SDL init failed\n");
            gli_strict_warning(SDL_GetError());
            gli_conf_sound = 0;
            return;
        }
        if (Sound_Init() == -1)
        {
            gli_strict_warning("SDL Sound init failed\n");
            gli_strict_warning(Sound_GetError());
            gli_conf_sound = 0;
            return;
        }
        Sound_AudioInfo *audio;
        audio = malloc(sizeof(Sound_AudioInfo));
        audio->format = MIX_DEFAULT_FORMAT;
        audio->channels = 2;
        audio->rate = 44100;
        output = audio;
        if (Mix_OpenAudio(output->rate, output->format, output->channels, 4096) == -1)
        {
            gli_strict_warning("SDL Mixer init failed\n");
            gli_strict_warning(Mix_GetError());
            gli_conf_sound = 0;
            return;
        }
        int channels = Mix_AllocateChannels(SDL_CHANNELS);
        Mix_GroupChannels(0, channels - 1 , FREE);
    }
}

schanid_t glk_schannel_create(glui32 rock)
{
    channel_t *chan;

    if (!gli_conf_sound)
        return NULL;
    chan = malloc(sizeof(channel_t));

    if (!chan)
        return NULL;

    chan->rock = rock;
    chan->status = CHANNEL_IDLE;
    chan->volume = MIX_MAX_VOLUME;
    chan->resid = 0;
    chan->loop = 0;
    chan->notify = 0;
    chan->sdl_memory = 0;
    chan->sdl_rwops = 0;
    chan->sample = 0;
    chan->decode = 0;
    chan->buffered = 0;
    chan->sdl_channel = -1;
    chan->music = 0;

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

schanid_t glk_schannel_create_ext(glui32 rock, glui32 volume)
{
    /* not implemented */
    return NULL;
}

static void cleanup_channel(schanid_t chan)
{
    if (chan->sdl_rwops)
    {
        if (!chan->decode)
            SDL_FreeRW(chan->sdl_rwops);
        else
            Sound_FreeSample(chan->decode);
        chan->sdl_rwops = 0;
        chan->decode = 0;
        chan->buffered = 0;
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
    /* not implemented */
    return 0;
}

void glk_sound_load_hint(glui32 snd, glui32 flag)
{
    /* nop */
}

void glk_schannel_set_volume(schanid_t chan, glui32 vol)
{
    if (!chan)
    {
        gli_strict_warning("schannel_set_volume: invalid id.");
        return;
    }
    chan->volume = vol > 0x10000 ? MIX_MAX_VOLUME : round(pow(((double) vol) / 0x10000, log(4)) * MIX_MAX_VOLUME);
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

void glk_schannel_set_volume_ext(schanid_t chan, glui32 vol,
        glui32 duration, glui32 notify)
{
    /* not implemented */
}

/* Notify the music channel completion */
static void music_completion_callback()
{
    if (!music_channel)
    {
        gli_strict_warning("music callback failed");
        return;
    }
    if (music_channel->notify)
    {
        gli_event_store(evtype_SoundNotify, 0,
                        music_channel->resid, music_channel->notify);
    }
    cleanup_channel(music_channel);
}

/* Notify the sound channel completion */
static void sound_completion_callback(int chan)
{
    channel_t *sound_channel = sound_channels[chan];
    if (!sound_channel || Mix_Playing(chan))
    {
        gli_strict_warning("sound callback failed");
        return;
    }
    if (!sound_channel->buffered || !sound_channel->decode)
    {
        if (sound_channel->notify)
        {
            gli_event_store(evtype_SoundNotify, 0,
                            sound_channel->resid, sound_channel->notify);
        }
        cleanup_channel(sound_channel);
        sound_channels[chan] = 0;
        return;
    }
    Uint32 soundbytes = Sound_Decode(sound_channel->decode);
    if (!soundbytes)
    {
        sound_channel->loop--;
        if (!sound_channel->loop)
        {
            if (sound_channel->notify)
            {
                gli_event_store(evtype_SoundNotify, 0,
                                sound_channel->resid, sound_channel->notify);
            }
            cleanup_channel(sound_channel);
            sound_channels[chan] = 0;
            return;
        }
        else
        {
            Sound_Rewind(sound_channel->decode);
            soundbytes = Sound_Decode(sound_channel->decode);
        }
    }
    Sound_Sample *sample = sound_channel->decode;
    sound_channel->sample = Mix_QuickLoad_RAW(sample->buffer, soundbytes);
    Mix_ChannelFinished(&sound_completion_callback);
    if (Mix_PlayChannel(sound_channel->sdl_channel,
                        sound_channel->sample,
                        FALSE) >= 0)
    {
        return;
    }
    gli_strict_warning("buffer sound failed");
    gli_strict_warning(Mix_GetError());
    cleanup_channel(sound_channel);
    return;
}

static glui32 load_sound_resource(glui32 snd, long *len, char **buf)
{
    if (!giblorb_is_resource_map())
    {
        FILE *file;
        char name[1024];

        sprintf(name, "%s/SND%ld", gli_workdir, (long) snd);

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
        if (fread(*buf, 1, *len, file) != *len && !feof(file)) return 0;
        fclose(file);

        /* AIFF */
        if (*len > 4 && !memcmp(*buf, "FORM", 4))
            return giblorb_ID_FORM;

        /* WAVE */
        if (*len > 4 && !memcmp(*buf, "WAVE", 4))
            return giblorb_ID_WAVE;

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
    SDL_LockAudio();
    chan->status = CHANNEL_SOUND;
    chan->buffered = 0;
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
        if (Mix_PlayChannel(chan->sdl_channel, chan->sample, chan->loop-1) >= 0)
            return 1;
    }
    gli_strict_warning("play sound failed");
    gli_strict_warning(Mix_GetError());
    SDL_LockAudio();
    cleanup_channel(chan);
    SDL_UnlockAudio();
    return 0;
}

/** Start a compressed sound channel */
static glui32 play_compressed(schanid_t chan, char *ext)
{
    SDL_LockAudio();
    chan->status = CHANNEL_SOUND;
    chan->buffered = 1;
    chan->sdl_channel = Mix_GroupAvailable(FREE);
    Mix_GroupChannel(chan->sdl_channel, BUSY);
    SDL_UnlockAudio();
    chan->decode = Sound_NewSample(chan->sdl_rwops, ext, output, 65536);
    Uint32 soundbytes = Sound_Decode(chan->decode);
    Sound_Sample *sample = chan->decode;
    chan->sample = Mix_QuickLoad_RAW(sample->buffer, soundbytes);
    if (chan->sdl_channel < 0)
        gli_strict_warning("No available sound channels");
    if (chan->sdl_channel >= 0 && chan->sample)
    {
        SDL_LockAudio();
        sound_channels[chan->sdl_channel] = chan;
        SDL_UnlockAudio();
        Mix_Volume(chan->sdl_channel, chan->volume);
        Mix_ChannelFinished(&sound_completion_callback);
        if (Mix_PlayChannel(chan->sdl_channel, chan->sample, 0) >= 0)
            return 1;
    }
    gli_strict_warning("play sound failed");
    gli_strict_warning(Mix_GetError());
    SDL_LockAudio();
    cleanup_channel(chan);
    SDL_UnlockAudio();
    return 0;
}

/** Start a mod music channel */
static glui32 play_mod(schanid_t chan, long len)
{
    FILE *file;
    char *tn;
    char *tempdir;
    int music_busy;

    chan->status = CHANNEL_MUSIC;
    /* The fscking mikmod lib want to read the mod only from disk! */
    tempdir = getenv("TEMP");
    if (tempdir == NULL) tempdir = ".";
    tn = tempnam(tempdir, "gargtmp");
    file = fopen(tn, "wb");
    fwrite(chan->sdl_memory, 1, len, file);
    fclose(file);
    chan->music = Mix_LoadMUS(tn);
    remove(tn);
    free(tn);
    music_busy = Mix_PlayingMusic();
    if (music_busy)
        gli_strict_warning("MOD player already in use");
    if (!music_busy && chan->music)
    {
        SDL_LockAudio();
        music_channel = chan;
        SDL_UnlockAudio();
        Mix_VolumeMusic(chan->volume);
        Mix_HookMusicFinished(&music_completion_callback);
        if (Mix_PlayMusic(chan->music, chan->loop-1) >= 0)
            return 1;
    }
    gli_strict_warning("play mod failed");
    gli_strict_warning(Mix_GetError());
    SDL_LockAudio();
    cleanup_channel(chan);
    SDL_UnlockAudio();
    return 0;
}

glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats, glui32 notify)
{
    long len;
    glui32 type;
    char *buf = 0;

    if (!chan)
    {
        gli_strict_warning("schannel_play_ext: invalid id.");
        return 0;
    }

    /* stop previous noise */
    glk_schannel_stop(chan);

    if (repeats == 0)
        return 1;

    /* load sound resource into memory */
    type = load_sound_resource(snd, &len, &buf);

    chan->sdl_memory = (unsigned char*)buf;
    chan->sdl_rwops = SDL_RWFromConstMem(buf, len);
    chan->notify = notify;
    chan->resid = snd;
    chan->loop = repeats;

    switch (type)
    {
        case giblorb_ID_FORM:
        case giblorb_ID_AIFF:
        case giblorb_ID_WAVE:
            return play_sound(chan);
            break;

        case giblorb_ID_OGG:
            return play_compressed(chan, "OGG");
            break;

        case giblorb_ID_MP3:
            return play_compressed(chan, "MP3");
            break;

        case giblorb_ID_MOD:
            return play_mod(chan, len);
            break;

        default:
            gli_strict_warning("schannel_play_ext: unknown resource type.");
    }

    return 0;
}

void glk_schannel_pause(schanid_t chan)
{
    /* not implemented */
}

void glk_schannel_unpause(schanid_t chan)
{
    /* not implemented */
}

void glk_schannel_stop(schanid_t chan)
{
    if (!chan)
    {
        gli_strict_warning("schannel_stop: invalid id.");
        return;
    }
    SDL_LockAudio();
    chan->buffered = 0;
    SDL_UnlockAudio();
    switch (chan->status)
    {
        case CHANNEL_SOUND:
            Mix_HaltChannel(chan->sdl_channel);
            break;
        case CHANNEL_MUSIC:
            Mix_HaltMusic();
            break;
    }
    SDL_LockAudio();
    cleanup_channel(chan);
    SDL_UnlockAudio();
}
