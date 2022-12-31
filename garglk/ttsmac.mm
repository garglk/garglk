// Copyright (C) 2006-2009 by Tor Andersson.
// Copyright (C) 2017 by Chris Spiegel.
// Copyright (C) 2017 by Kerry Guerrero.
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

#import <Cocoa/Cocoa.h>

#include <vector>

#include "glk.h"
#include "garglk.h"

#import "sysmac.h"

// a queue of phrases to feed to the speech synthesizer
static NSMutableArray *phraseQueue = nil;
static NSRange purgeRange;

@interface SpeechDelegate : NSObject <NSSpeechSynthesizerDelegate>
{
}
- (void) speechSynthesizer: (NSSpeechSynthesizer *) sender
         didFinishSpeaking: (BOOL) finishSpeaking;
@end

@implementation SpeechDelegate
{
}
- (void) speechSynthesizer: (NSSpeechSynthesizer *) sender
         didFinishSpeaking: (BOOL) finishSpeaking
{
    if (finishSpeaking == NO) {
        // speaking stopped abruptly (see gli_tts_purge())
        [phraseQueue removeObjectsInRange: purgeRange];
    } else {
        // remove the head of the list which just completed speaking
        if (phraseQueue.count > 0) {
            [phraseQueue removeObjectAtIndex: 0];
        }
    }

    // speak the next phrase
    if (phraseQueue.count > 0) {
        [sender startSpeakingString: [phraseQueue objectAtIndex: 0]];
    }
}
@end

static std::vector<glui32> txtbuf;

static NSSpeechSynthesizer *synth = nil;

void gli_initialize_tts()
{
    if (gli_conf_speak) {
        // spawn speech synthesizer using the default voice
        synth = [NSSpeechSynthesizer new];
        if (!synth) {
            NSLog(@"Unable to initialize TTS support!");
            return;
        }

        phraseQueue = [NSMutableArray arrayWithCapacity:64];

        synth.delegate = [SpeechDelegate new];

        // configure optional non-default speaking voice/language
        if (!gli_conf_speak_language.empty()) {
            NSString *lang = [NSString stringWithCString: gli_conf_speak_language.c_str()
                                                encoding: NSUTF8StringEncoding];

            NSArray *voices = [NSSpeechSynthesizer availableVoices];
            for (NSString *voice in voices) {
                NSDictionary *attr = [NSSpeechSynthesizer attributesForVoice: voice];
                if ([lang isEqualToString: [attr objectForKey: NSVoiceLocaleIdentifier]]) {
                    [synth setVoice: voice];
                }
            }
        }

        NSLog(@"TTS configured with voice: %@", [synth voice]);
    }

    txtbuf.clear();
}

static void ttsmac_add_phrase(NSString * phrase)
{
    // append the phrase to the queue
    [phraseQueue addObject: phrase];

    // if the queue was empty we need to explicitly start speaking
    if (phraseQueue.count == 1) {
        [synth startSpeakingString: [phraseQueue objectAtIndex: 0]];
    }
}

static void ttsmac_flush()
{
    if (!txtbuf.empty()) {
        // convert codepoints in buffer into an NSString
        NSString *text = [[NSString alloc] initWithBytes: txtbuf.data()
                                                  length: (txtbuf.size() * sizeof(glui32))
                                                encoding: UTF32StringEncoding];

        if (text) {
            ttsmac_add_phrase(text);
            [text release];
        }

        txtbuf.clear();
    }
}

void gli_tts_flush()
{
    if (synth) {
        ttsmac_flush();
    }
}

void gli_tts_purge()
{
    if (synth) {
        purgeRange = NSMakeRange(0, phraseQueue.count);
        [synth stopSpeaking];
    }
}

void gli_tts_speak(const glui32 *buf, size_t len)
{
    if (!synth) {
        return;
    }

    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '>' || buf[i] == '*') {
            continue;
        }

        txtbuf.push_back(buf[i]);

        // Feed the synthesizer in paragraph-sized chunks. A compromise
        // as doing it in smaller pieces (e.g. sentences) doesn't always
        // sound right when dealing with quote-delimited text. The quoted
        // text should be sent to the synthesizer in a single call which
        // requires scanning for quote pairs (including typographic
        // quotes) and correct handling of single-quotes and possesive
        // forms (at least for English). More work than I want to deal
        // with at this time.
        if (/*buf[i] == '.' || buf[i] == '!' || buf[i] == '?' || */ buf[i] == '\n') {
            gli_tts_flush();
        }
    }
}

void gli_free_tts()
{
    gli_tts_purge();

    if (synth) {
        id delegate = synth.delegate;
        synth.delegate = nil;

        [synth release];
        synth = nil;

        if (delegate) {
            [delegate release];
            delegate = nil;
        }
    }
}
