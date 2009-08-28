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
#include <string.h>
#include <fmod.h>

#include "glk.h"
#include "garglk.h"

#include "gi_blorb.h"

#define giblorb_ID_MOD          (giblorb_make_id('M', 'O', 'D', ' '))
#define giblorb_ID_FORM         (giblorb_make_id('F', 'O', 'R', 'M'))
#define giblorb_ID_OGG		(giblorb_make_id('O', 'G', 'G', 'V'))

/* non-standard types */
#define giblorb_ID_MIDI		(giblorb_make_id('M', 'I', 'D', 'I'))
#define giblorb_ID_MP3		(giblorb_make_id('M', 'P', '3', ' '))
#define giblorb_ID_WAVE	 	(giblorb_make_id('W', 'A', 'V', 'E'))

static channel_t *gli_channellist = NULL;

void gli_initialize_sound(void)
{
    if (gli_conf_sound == 1)
	if (!FSOUND_Init(44100, 32, FSOUND_INIT_USEDEFAULTMIDISYNTH))
	    gli_conf_sound = 0;
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
    chan->module = NULL;
    chan->sample = NULL;
    chan->volume = 0x10000;

    chan->chain_prev = NULL;
    chan->chain_next = gli_channellist;
    gli_channellist = chan;
    if (chan->chain_next) {
	chan->chain_next->chain_prev = chan;
    }

    if (gli_register_obj)
	chan->disprock = (*gli_register_obj)(chan, gidisp_Class_Schannel);
    else
	chan->disprock.ptr = NULL;

    return chan;	
}

void glk_schannel_destroy(schanid_t chan)
{
    channel_t *prev, *next;

    if (!chan) {
	gli_strict_warning("schannel_destroy: invalid id.");
	return;
    }

    glk_schannel_stop(chan);

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
    if (!chan) {
	chan = gli_channellist;
    } else {
	chan = chan->chain_next;
    }

    if (chan) {
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
    if (!chan) {
	gli_strict_warning("schannel_get_rock: invalid id.");
	return 0;
    }
    return chan->rock;
}

glui32 glk_schannel_play(schanid_t chan, glui32 snd)
{
    return glk_schannel_play_ext(chan, snd, 1, 0);
}

void glk_sound_load_hint(glui32 snd, glui32 flag)
{
    /* nop */
}

void glk_schannel_set_volume(schanid_t chan, glui32 vol)
{
    if (!chan) {
	gli_strict_warning("schannel_set_volume: invalid id.");
	return;
    }
    chan->volume = vol;
    if (chan->module)
	FMUSIC_SetMasterVolume(chan->module, chan->volume / 256);
    if (chan->sample)
	FSOUND_SetVolume(chan->channel, chan->volume / 256);
}

glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats, glui32 notify)
{
    FILE *file;
    long pos, len;
    glui32 type;
    char *buf;
    int offset = 0;
    int i;

    if (!chan) {
	gli_strict_warning("schannel_play_ext: invalid id.");
	return 0;
    }

    /* stop previous noise */
    glk_schannel_stop(chan);

    if (repeats == 0)
	return 1;

    /* load sound resource into memory */

    if (!giblorb_is_resource_map())
    {
	char name[1024];

	sprintf(name, "%s/SND%ld", gli_workdir, snd); 

	file = fopen(name, "rb");
	if (!file)
	    return 0;

	fseek(file, 0, SEEK_END);
	len = ftell(file);

	buf = malloc(len);
	if (!buf) {
	    fclose(file);
	    return 0;
	}

	fseek(file, 0, 0);
	fread(buf, 1, len, file);
	fclose(file);

	/* identify by file magic the two types that fmod can do... */

	type = 0;	/* unidentified */

	/* AIFF */
	if (len > 4 && !memcmp(buf, "FORM", 4))		
	    type = giblorb_ID_FORM;

	/* WAVE */
	if (len > 4 && !memcmp(buf, "WAVE", 4))
	    type = giblorb_ID_WAVE;

	/* MIDI */
	if (len > 4 && !memcmp(buf, "MThd", 4))
	    type = giblorb_ID_MIDI;

	/* Ogg Vorbis */
	if (len > 4 && !memcmp(buf, "OggS", 4))
	    type = giblorb_ID_OGG;

	/* s3m */
	if (len > 0x30 && !memcmp(buf + 0x2c, "SCRM", 4))
	    type = giblorb_ID_MOD;

	/* XM */
	if (len > 20 && !memcmp(buf, "Extended Module: ", 17))
	    type = giblorb_ID_MOD;

	/* MOD */
	if (len > 1084)
	{
	    char resname[4];
	    memcpy(resname, buf + 1080, 4);
	    if (!strcmp(resname+1, "CHN") ||	/* 4CHN, 6CHN, 8CHN */
		    !strcmp(resname+2, "CN") ||	 /* 16CN, 32CN */
		    !strcmp(resname, "M.K.") || !strcmp(resname, "M!K!") ||
		    !strcmp(resname, "FLT4") || !strcmp(resname, "CD81") ||
		    !strcmp(resname, "OKTA") || !strcmp(resname, "    "))
		type = giblorb_ID_MOD;
	}

	if (!memcmp(buf, "\377\372", 2))	/* mp3 */
	    type = giblorb_ID_MP3;

	/* look for RIFF (future boy has broken resources...?) */
	if (len > 128 && type == 0)
	    for (i = 0; i < 124; i++)
		if (!memcmp(buf+i, "RIFF", 4))
		{
		    offset = i;
		    type = giblorb_ID_WAVE;
		    break;
		}

	if (type == 0)
	    type = giblorb_ID_MP3;
    }

    else
    {
	giblorb_get_resource(giblorb_ID_Snd, snd, &file, &pos, &len, &type);
	if (!file)
	    return 0;

	buf = malloc(len);
	if (!buf)
	    return 0;

	fseek(file, pos, 0);
	fread(buf, 1, len, file);
    }

    switch (type)
    {
    case giblorb_ID_FORM:
    case giblorb_ID_WAVE:
    case giblorb_ID_MP3:
	chan->sample = FSOUND_Sample_Load(FSOUND_UNMANAGED, buf + offset,
		FSOUND_NORMAL|FSOUND_LOADMEMORY, 0, len);
	if (!chan->sample) { free(buf); return 0; }
	if (repeats != 1)
	    FSOUND_Sample_SetMode(chan->sample, FSOUND_LOOP_NORMAL);
	else
	    FSOUND_Sample_SetMode(chan->sample, FSOUND_LOOP_OFF);
	chan->channel = FSOUND_PlaySound(FSOUND_FREE, chan->sample);
	FSOUND_SetVolume(chan->channel, chan->volume / 256);
	FSOUND_SetPaused(chan->channel, 0);
	break;

    case giblorb_ID_MOD:
    case giblorb_ID_MIDI:
	chan->module = FMUSIC_LoadSongEx(buf, 0, len, FSOUND_LOADMEMORY, 0, 0);
	if (!chan->module) { free(buf); return 0; }
	if (repeats != 1)
	    FMUSIC_SetLooping(chan->module, 1);
	else
	    FMUSIC_SetLooping(chan->module, 0);
	FMUSIC_SetMasterVolume(chan->module, chan->volume / 256);
	FMUSIC_PlaySong(chan->module);
	break;

    default:
	gli_strict_warning("schannel_play_ext: unknown resource type.");
    }

    free(buf);

    return 1;
}

void glk_schannel_stop(schanid_t chan)
{
    if (!chan) {
	gli_strict_warning("schannel_stop: invalid id.");
	return;
    }

    if (chan->module)
    {
	FMUSIC_StopSong(chan->module);
	FMUSIC_FreeSong(chan->module);
	chan->module = NULL;
    }

    if (chan->sample)
    {
	FSOUND_StopSound(chan->channel);
	FSOUND_Sample_Free(chan->sample);
	chan->sample = NULL;
    }
}

