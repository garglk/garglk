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

#define WIN32_LEAN_AND_MEAN

#include <stdlib.h>
#include <stdio.h>

#include <sapi.h>

#include "glk.h"
#include "garglk.h"

static ISpVoice *voice = NULL;
static WCHAR txtbuf[4096];
static int txtlen;

void gli_initialize_tts(void)
{
	if (gli_conf_speak)
	{
		CoInitialize(NULL);
		CoCreateInstance(
			&CLSID_SpVoice,		/* rclsid */
			NULL,				/* aggregate */
			CLSCTX_ALL,			/* dwClsContext */
			&IID_ISpVoice,		/* riid */
			(void**)&voice);
		txtlen = 0;
	}
	else
		voice = NULL;
}

static void flushtts(int purge)
{
	txtbuf[txtlen] = 0;
	if (purge)
		voice->lpVtbl->Speak(voice, txtbuf,
				SPF_ASYNC|SPF_PURGEBEFORESPEAK, NULL);
	else
		voice->lpVtbl->Speak(voice, txtbuf,
				SPF_ASYNC, NULL);
	txtlen = 0;
}

void gli_speak_tts(char *buf, int len, int interrupt)
{
	int i;

	if (!voice)
		return;

	if (interrupt)
		flushtts(1);

	for (i = 0; i < len; i++)
	{
		if (txtlen >= 4095)
			flushtts(0);

		if (buf[i] == '>' || buf[i] == '*')
			continue;

		txtbuf[txtlen++] = glk_char_to_lower(buf[i]);

		if (buf[i] == '.' || buf[i] == '!' || buf[i] == '?' || buf[i] == '\n')
			flushtts(0);
	}
}

void gli_free_tts(void)
{
	if (voice)
	{
		voice->lpVtbl->Release(voice);
		CoUninitialize();
	}
}

