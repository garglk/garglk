/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2017 by Chris Spiegel.                                       *
 * Copyright (C) 2017 by Kerry Guerrero.                                      *
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

#import <Cocoa/Cocoa.h>

#include "glk.h"
#include "garglk.h"

#import "sysmac.h"

#define TXTSIZE 4096
static glui32 txtbuf[TXTSIZE + 1];
static size_t txtlen;

static NSMutableString * ttstext = nil;
static NSSpeechSynthesizer * synth = nil;

void gli_initialize_tts(void)
{
    if (gli_conf_speak)
    {
        synth = [NSSpeechSynthesizer new];
        if (!synth)
        {
            NSLog(@"Unable to initialize TTS support!");
            return;
        }

        ttstext = [NSMutableString stringWithCapacity: TXTSIZE];

        if (gli_conf_speak_language != NULL)
        {
            NSString * lang = [NSString stringWithCString: gli_conf_speak_language
                                                 encoding: NSUTF8StringEncoding];

            NSArray<NSString *> * voices = [NSSpeechSynthesizer availableVoices];
            for (NSString * voice in voices)
            {
                NSDictionary<NSString *, id> * attr = [NSSpeechSynthesizer attributesForVoice: voice];
                if ([lang isEqualToString: [attr objectForKey: NSVoiceLocaleIdentifier]])
                {
                    [synth setVoice: voice];
                }
            }
        }

        NSLog(@"TTS configured with voice: %@", [synth voice]);
    }

    txtlen = 0;
}

void gli_tts_flush(void)
{
    if (!synth)
        return;

    if ([ttstext length] > 0)
    {
        BOOL result = [synth startSpeakingString: ttstext];
        if (result == YES)
        {
            [ttstext setString: @""];
        }
    }

    txtlen = 0;
}

void gli_tts_purge(void)
{
    if (synth)
    {
        [synth stopSpeaking];
    }
}

static void txtbuf_append(void)
{
    if (txtlen > 0)
    {
        NSString * text = [[NSString alloc] initWithBytes: txtbuf
                                                   length: (txtlen * sizeof(glui32))
                                                 encoding: UTF32StringEncoding];

        if (text)
        {
            [ttstext appendString: text];
            [text release];
        }

        txtlen = 0;
    }
}

void gli_tts_speak(const glui32 *buf, size_t len)
{
    if (!synth)
        return;

    for (size_t i = 0; i < len; i++)
    {
        if (txtlen >= TXTSIZE)
            txtbuf_append();

        if (buf[i] == '>' || buf[i] == '*')
        {
            txtbuf_append();
            continue;
        }

        txtbuf[txtlen++] = buf[i];

        if (buf[i] == '.' || buf[i] == '!' || buf[i] == '?' || buf[i] == '\n')
            txtbuf_append();
    }
}

void gli_free_tts(void)
{
    gli_tts_purge();

    if (ttstext)
    {
        [ttstext release];
        ttstext = nil;
    }

    if (synth)
    {
        [synth release];
        synth = nil;
    }
}
