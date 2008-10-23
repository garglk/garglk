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

