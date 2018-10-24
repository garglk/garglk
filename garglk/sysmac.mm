/******************************************************************************
 *                                                                            *
 * Copyright (C) 2010 by Ben Cressey.                                         *
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

#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach_time.h>

#include "glk.h"
#include "garglk.h"

#import "Cocoa/Cocoa.h"
#import "sysmac.h"

static volatile sig_atomic_t gli_event_waiting = false;
static volatile sig_atomic_t gli_mach_allowed = false;
static volatile sig_atomic_t gli_window_alive = true;

#define kArrowCursor 1
#define kIBeamCursor 2
#define kPointingHandCursor 3

void wintick(CFRunLoopTimerRef timer, void *info);
void winhandler(int signal);


@interface GargoyleMonitor : NSObject
{
    NSRect size;
    CFRunLoopTimerRef timerid;
    int timeouts;
}
- (id) init;
- (void) sleep;
- (void) track: (CFTimeInterval) seconds;
- (void) tick;
- (BOOL) timeout;
- (void) reset;
- (void) connectionDied: (NSNotification *) notice;
@end

@implementation GargoyleMonitor

- (id) init
{
    self = [super init];

    timerid = 0;
    timeouts = 0;

    return self;
}

- (void) sleep
{
    while (!timeouts && !gli_event_waiting)
    {
        if (!gli_window_alive)
            exit(1);

        gli_mach_allowed = true;
        [[NSRunLoop currentRunLoop] acceptInputForMode: NSDefaultRunLoopMode beforeDate: [NSDate distantFuture]];
        gli_mach_allowed = false;
    }
}

- (void) track: (CFTimeInterval) seconds
{
    if (timerid)
    {
        if (CFRunLoopTimerIsValid(timerid))
            CFRunLoopTimerInvalidate(timerid);
        CFRelease(timerid);
        timerid = 0;
        timeouts = 0;
    }

    if (seconds)
    {
        timerid = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + seconds, seconds,
                                       0, 0, &wintick, NULL);
        if (timerid)
            CFRunLoopAddTimer([[NSRunLoop currentRunLoop] getCFRunLoop], timerid, kCFRunLoopDefaultMode);
    }
}

- (void) tick
{
    timeouts++;
    CFRunLoopStop([[NSRunLoop currentRunLoop] getCFRunLoop]);
}

- (BOOL) timeout
{
    return (timeouts != 0);
}

- (void) reset
{
    timeouts = 0;
}

- (void) connectionDied: (NSNotification *) notice
{
    exit(1);
}

@end

static NSObject<GargoyleApp> * gargoyle = NULL;
static GargoyleMonitor * monitor = NULL;
static NSString * cliptext = NULL;
static pid_t processID = 0;

static bool gli_refresh_needed = true;

void glk_request_timer_events(glui32 millisecs)
{
    [monitor track: ((double) millisecs) / 1000];
}

void wintick(CFRunLoopTimerRef timer, void *info)
{
    [monitor tick];
}

void gli_notification_waiting()
{
    winhandler(SIGUSR1);
}

void garglk::winabort(const std::string &msg)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    [gargoyle abortWindowDialog: processID
                         prompt: [NSString stringWithCString: msg.c_str() encoding: NSUTF8StringEncoding]];
    [pool drain];

    exit(1);
}

void winexit(void)
{
    [gargoyle closeWindow: processID];
    exit(0);
}

std::string garglk::winopenfile(const char *prompt, enum FILEFILTERS filter)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSString * fileref = [gargoyle openWindowDialog: processID
                                             prompt: [NSString stringWithCString: prompt encoding: NSUTF8StringEncoding]
                                             filter: filter];

    char buf[256];
    strcpy(buf, "");

    if (fileref)
    {
        [fileref getCString: buf maxLength: sizeof(buf) encoding: NSUTF8StringEncoding];
    }

    [pool drain];

    return buf;
}

std::string garglk::winsavefile(const char *prompt, enum FILEFILTERS filter)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSString * fileref = [gargoyle saveWindowDialog: processID
                                             prompt: [NSString stringWithCString: prompt encoding: NSUTF8StringEncoding]
                                             filter: filter];

    char buf[256];
    strcpy(buf, "");

    if (fileref)
    {
        [fileref getCString: buf maxLength: sizeof(buf) encoding: NSUTF8StringEncoding];
    }

    [pool drain];

    return buf;
}

void winclipstore(glui32 *text, int len)
{
    if (!len)
        return;

    if (cliptext) {
        [cliptext release];
        cliptext = nil;
    }

    cliptext = [[NSString alloc] initWithBytes: text
                                        length: (len * sizeof(glui32))
                                      encoding: UTF32StringEncoding];
}

void winclipsend(void)
{
    if (cliptext)
    {
        NSPasteboard * clipboard = [NSPasteboard generalPasteboard];
        [clipboard declareTypes: [NSArray arrayWithObject: NSStringPboardType] owner:nil];
        [clipboard setString: cliptext forType: NSStringPboardType];
    }
}

void winclipreceive(void)
{
    int i, len;
    glui32 ch;

    NSPasteboard * clipboard = [NSPasteboard generalPasteboard];

    if ([clipboard availableTypeFromArray: [NSArray arrayWithObject: NSStringPboardType]])
    {
        NSString * input = [clipboard stringForType: NSStringPboardType];
        if (input)
        {
            len = [input length];
            for (i=0; i < len; i++)
            {
                if ([input getBytes: &ch maxLength: sizeof ch usedLength: NULL
                           encoding: UTF32StringEncoding
                            options: 0
                              range: NSMakeRange(i, 1)
                     remainingRange: NULL])
                {
                    switch (ch)
                    {
                        case '\0':
                            return;

                        case '\r':
                        case '\n':
                        case '\b':
                        case '\t':
                            break;

                        default:
                            gli_input_handle_key(ch);
                    }
                }
            }
        }
    }
}

void wintitle(void)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSString * story_title = [NSString stringWithCString: gli_story_title encoding: NSUTF8StringEncoding];
    NSString * story_name = [NSString stringWithCString: gli_story_name encoding: NSUTF8StringEncoding];
    NSString * program_name = [NSString stringWithCString: gli_program_name encoding: NSUTF8StringEncoding];

    NSString * title = nil;
    if ([story_title length])
        title = story_title;
    else if ([story_name length])
        title = [NSString stringWithFormat: @"%@ - %@", story_name, program_name];
    else
        title = program_name;

    [gargoyle setWindow: processID
                  title: title];

    [pool drain];
}

void winresize(void)
{
    NSRect viewRect = [gargoyle getWindowSize: processID];

    unsigned int vw = (unsigned int) (NSWidth(viewRect) * gli_backingscalefactor);
    unsigned int vh = (unsigned int) (NSHeight(viewRect) * gli_backingscalefactor);

    if (gli_image_w == vw && gli_image_h == vh)
        return;

    gli_image_w = vw;
    gli_image_h = vh;
    gli_image_s = ((gli_image_w * 4 + 3) / 4) * 4;

    /* initialize offline bitmap store */
    delete [] gli_image_rgb;

    gli_image_rgb = new unsigned char[gli_image_s * gli_image_h];

    /* redraw window content */
    gli_resize_mask(gli_image_w, gli_image_h);
    gli_force_redraw = true;
    gli_refresh_needed = true;
    gli_windows_size_change();
}

static mach_port_t gli_signal_port = 0;

void winmach(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    mach_msg_header_t* hdr = (mach_msg_header_t*)msg;
    switch (hdr->msgh_id)
    {
        case SIGUSR1:
            gli_event_waiting = true;
            break;

        default:
            break;
    }
}

void winhandler(int signal)
{
    if (signal == SIGUSR1 && gli_mach_allowed)
    {
        mach_msg_header_t header;
        header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND, 0);
        header.msgh_size        = sizeof(header);
        header.msgh_remote_port = gli_signal_port;
        header.msgh_local_port  = MACH_PORT_NULL;
        header.msgh_reserved    = 0;
        header.msgh_id          = signal;

        mach_msg_send(&header);
    }

    if (signal == SIGUSR2)
        gli_window_alive = false;
}

void wininit(int *argc, char **argv)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    /* get the backing scale factor */
    NSRect rect = NSMakeRect(0, 0, 1000, 1);
    NSView * tmpview = [[NSView alloc] initWithFrame: rect];
    rect = [tmpview convertRectToBacking: rect];
    [tmpview release];
    gli_backingscalefactor = NSWidth(rect)/1000.;

    /* establish link to launcher */
    NSString * linkName = [NSString stringWithUTF8String: getenv("GargoyleApp")];
    NSConnection * link = [NSConnection connectionWithRegisteredName: linkName host: NULL];
    [link retain];

    /* monitor link for failure */
    monitor = [[GargoyleMonitor alloc] init];
    [[NSNotificationCenter defaultCenter] addObserver: monitor
                                             selector: @selector(connectionDied:)
                                                 name: NSConnectionDidDieNotification
                                               object: link];

    /* attach to app controller */
    gargoyle = (NSObject<GargoyleApp> *)[link rootProxy];
    [gargoyle retain];

    /* listen for mach messages */
    CFMachPortRef sigPort = CFMachPortCreate(NULL, winmach, NULL, NULL);
    gli_signal_port = CFMachPortGetPort(sigPort);
    [[NSRunLoop currentRunLoop] addPort: [NSMachPort portWithMachPort: gli_signal_port] forMode: NSDefaultRunLoopMode];

    /* load signal handler */
    signal(SIGUSR1, winhandler);
    signal(SIGUSR2, winhandler);

    processID = getpid();

    [pool drain];
}

void winopen(void)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    unsigned int defw = gli_wmarginx * 2 + gli_cellw * gli_cols;
    unsigned int defh = gli_wmarginy * 2 + gli_cellh * gli_rows;
    NSColor * windowColor = [NSColor colorWithCalibratedRed: (float) gli_window_color[0] / 255.0f
                                                      green: (float) gli_window_color[1] / 255.0f
                                                       blue: (float) gli_window_color[2] / 255.0f
                                                      alpha: 1.0f];

    [gargoyle initWindow: processID
                   width: defw
                  height: defh
                  retina: gli_hires
              fullscreen: gli_conf_fullscreen
         backgroundColor: windowColor];

    wintitle();
    winresize();

    [pool drain];
}

void winrepaint(int x0, int y0, int x1, int y1)
{
    gli_refresh_needed = true;
}

void winrefresh(void)
{
    gli_windows_redraw();

    NSData * frame = [NSData dataWithBytesNoCopy: gli_image_rgb
                                          length: gli_image_s * gli_image_h
                                    freeWhenDone: NO];

    int refreshed = [gargoyle setWindow: processID
                               contents: frame
                                  width: gli_image_w
                                 height: gli_image_h];

    gli_refresh_needed = !refreshed;
}

void winkey(NSEvent *evt)
{
    NSEventModifierFlags modifiers = [evt modifierFlags];
    NSEventModifierFlags modmasked = modifiers & (NSEventModifierFlagOption | NSEventModifierFlagShift | NSEventModifierFlagCommand | NSEventModifierFlagControl);

    /* queue a screen refresh */
    gli_refresh_needed = true;

    /* special key combinations */
    static const std::map<std::pair<decltype(modmasked), decltype([evt keyCode])>, std::function<void()>> keys = {
        /* alt/option modified arrow key */
        {{NSEventModifierFlagOption, NSKEY_LEFT},  []{ gli_input_handle_key(keycode_SkipWordLeft); }},
        {{NSEventModifierFlagOption, NSKEY_RIGHT}, []{ gli_input_handle_key(keycode_SkipWordRight); }},
        {{NSEventModifierFlagOption, NSKEY_DOWN},  []{ gli_input_handle_key(keycode_PageDown); }},
        {{NSEventModifierFlagOption, NSKEY_UP},    []{ gli_input_handle_key(keycode_PageUp); }},

        /* command modified arrow key */
        {{NSEventModifierFlagCommand, NSKEY_LEFT},  []{ gli_input_handle_key(keycode_Home); }},
        {{NSEventModifierFlagCommand, NSKEY_RIGHT}, []{ gli_input_handle_key(keycode_End); }},
        {{NSEventModifierFlagCommand, NSKEY_DOWN},  []{ gli_input_handle_key(keycode_PageDown); }},
        {{NSEventModifierFlagCommand, NSKEY_UP},    []{ gli_input_handle_key(keycode_PageUp); }},

        /* menu commands */
        {{NSEventModifierFlagCommand, NSKEY_X}, []{ winclipsend(); }},
        {{NSEventModifierFlagCommand, NSKEY_C}, []{ winclipsend(); }},
        {{NSEventModifierFlagCommand, NSKEY_V}, []{ winclipreceive(); }},

        /* unmodified key for line editing */
        {{0, NSKEY_LEFT},  []{ gli_input_handle_key(keycode_Left); }},
        {{0, NSKEY_RIGHT}, []{ gli_input_handle_key(keycode_Right); }},
        {{0, NSKEY_DOWN},  []{ gli_input_handle_key(keycode_Down); }},
        {{0, NSKEY_UP},    []{ gli_input_handle_key(keycode_Up); }},

        /* command keys */
        {{0, NSKEY_PGUP}, []{ gli_input_handle_key(keycode_PageUp); }},
        {{0, NSKEY_PGDN}, []{ gli_input_handle_key(keycode_PageDown); }},
        {{0, NSKEY_HOME}, []{ gli_input_handle_key(keycode_Home); }},
        {{0, NSKEY_END},  []{ gli_input_handle_key(keycode_End); }},
        {{0, NSKEY_DEL},  []{ gli_input_handle_key(keycode_Erase); }},
        {{0, NSKEY_BACK}, []{ gli_input_handle_key(keycode_Delete); }},
        {{0, NSKEY_ESC},  []{ gli_input_handle_key(keycode_Escape); }},
        {{0, NSKEY_F1},   []{ gli_input_handle_key(keycode_Func1); }},
        {{0, NSKEY_F2},   []{ gli_input_handle_key(keycode_Func2); }},
        {{0, NSKEY_F3},   []{ gli_input_handle_key(keycode_Func3); }},
        {{0, NSKEY_F4},   []{ gli_input_handle_key(keycode_Func4); }},
        {{0, NSKEY_F5},   []{ gli_input_handle_key(keycode_Func5); }},
        {{0, NSKEY_F6},   []{ gli_input_handle_key(keycode_Func6); }},
        {{0, NSKEY_F7},   []{ gli_input_handle_key(keycode_Func7); }},
        {{0, NSKEY_F8},   []{ gli_input_handle_key(keycode_Func8); }},
        {{0, NSKEY_F9},   []{ gli_input_handle_key(keycode_Func9); }},
        {{0, NSKEY_F10},  []{ gli_input_handle_key(keycode_Func10); }},
        {{0, NSKEY_F11},  []{ gli_input_handle_key(keycode_Func11); }},
        {{0, NSKEY_F12},  []{ gli_input_handle_key(keycode_Func12); }},
    };

    try
    {
        keys.at(std::make_pair(modmasked, [evt keyCode]))();
        return;
    }
    catch(const std::out_of_range &)
    {
    }


    /* send combined keystrokes to text buffer */
    [gargoyle setWindow: processID charString: evt];

    /* retrieve character from buffer as string */
    NSString * evt_char = [gargoyle getWindowCharString: processID];

    /* convert character to UTF-32 value */
    glui32 ch;
    NSUInteger used;
    if ([evt_char getBytes: &ch maxLength: sizeof ch usedLength: &used
                  encoding: UTF32StringEncoding
                   options: 0
                     range: NSMakeRange(0, [evt_char length])
            remainingRange: NULL])
    {
        if (used != 0)
        {
            switch (ch)
            {
                case '\n': gli_input_handle_key(keycode_Return); break;
                case '\t': gli_input_handle_key(keycode_Tab);    break;
                default: gli_input_handle_key(ch); break;
            }
        }
    }

    /* discard contents of text buffer */
    [gargoyle clearWindowCharString: processID];
}

void winmouse(NSEvent *evt)
{
    NSPoint coords = [gargoyle getWindowPoint: processID forEvent: evt];

    int x = coords.x * gli_backingscalefactor;
    int y = gli_image_h - (coords.y * gli_backingscalefactor);

    /* disregard most events outside of content window */
    if ((coords.y < 0 || y < 0 || x < 0 || x > gli_image_w)
        && !([evt type] == NSEventTypeLeftMouseUp))
        return;

    switch ([evt type])
    {
        case NSEventTypeLeftMouseDown:
        {
            gli_input_handle_click(x, y);
            [gargoyle setCursor: kArrowCursor];
            break;
        }

        case NSEventTypeLeftMouseDragged:
        {
            if (gli_copyselect)
            {
                [gargoyle setCursor: kIBeamCursor];
                gli_move_selection(x, y);
            }
            break;
        }

        case NSEventTypeMouseMoved:
        {
            if (gli_get_hyperlink(x, y))
                [gargoyle setCursor: kPointingHandCursor];
            else
                [gargoyle setCursor: kArrowCursor];
            break;
        }

        case NSEventTypeLeftMouseUp:
        {
            gli_copyselect = false;
            [gargoyle setCursor: kArrowCursor];
            break;
        }

        case NSEventTypeScrollWheel:
        {
            if ([evt deltaY] > 0)
                gli_input_handle_key(keycode_MouseWheelUp);
            else if ([evt deltaY] < 0)
                gli_input_handle_key(keycode_MouseWheelDown);
            break;
        }

        default: break;
    }

}

void winevent(NSEvent *evt)
{
    if (!evt)
    {
        gli_event_waiting = false;
        return;
    }

    switch ([evt type])
    {
        case NSEventTypeKeyDown:
        {
            winkey(evt);
            return;
        }

        case NSEventTypeLeftMouseDown:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeLeftMouseUp:
        case NSEventTypeMouseMoved:
        case NSEventTypeScrollWheel:
        {
            winmouse(evt);
            return;
        }

        case NSEventTypeApplicationDefined:
        {
            gli_refresh_needed = true;
            winresize();
            return;
        }

        default: return;
    }
}

/* winloop handles at most one event */
void winloop(void)
{
    NSEvent * evt = NULL;

    if (gli_refresh_needed)
        winrefresh();

    if (gli_event_waiting)
        evt = [gargoyle getWindowEvent: processID];

    winevent(evt);
}

/* winpoll handles all queued events */
void winpoll(void)
{
    NSEvent * evt = NULL;

    do
    {
        if (gli_refresh_needed)
            winrefresh();

        if (gli_event_waiting)
            evt = [gargoyle getWindowEvent: processID];

        winevent(evt);
    }
    while (evt);
}

std::string garglk::winfontpath(const std::string &filename)
{
    // There's no need to look up fonts in any special way on macOS: the
    // bundle sets fonts up in such a way that they're found without
    // needing a fallback.
    return "";
}

void gli_select(event_t *event, int polled)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    gli_event_clearevent(event);

    winpoll();
    gli_dispatch_event(event, polled);

    if (event->type == evtype_None && !polled)
    {
        while (![monitor timeout])
        {
            winloop();
            gli_dispatch_event(event, polled);

            if (event->type == evtype_None)
                [monitor sleep];
            else
                break;
        }
    }

    if (event->type == evtype_None && [monitor timeout])
    {
        gli_event_store(evtype_Timer, NULL, 0, 0);
        gli_dispatch_event(event, polled);
        [monitor reset];
    }

    [pool drain];
}
