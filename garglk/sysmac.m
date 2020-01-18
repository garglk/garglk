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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <mach/mach_time.h>

#include "glk.h"
#include "garglk.h"

#import "Cocoa/Cocoa.h"
#import "sysmac.h"

static volatile int gli_event_waiting = FALSE;
static volatile int gli_mach_allowed = FALSE;
static volatile int gli_window_alive = TRUE;

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

        gli_mach_allowed = TRUE;
        [[NSRunLoop currentRunLoop] acceptInputForMode: NSDefaultRunLoopMode beforeDate: [NSDate distantFuture]];
        gli_mach_allowed = FALSE;
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
static NSBitmapImageRep * framebuffer = NULL;
static NSString * cliptext = NULL;
static pid_t processID = 0;

static int gli_refresh_needed = TRUE;
static int gli_window_hidden = FALSE;

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

void winabort(const char *fmt, ...)
{
    va_list ap;
    char buf[256];
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    [gargoyle abortWindowDialog: processID
                         prompt: [NSString stringWithCString: buf encoding: NSUTF8StringEncoding]];
    [pool drain];

    exit(1);
}

void winexit(void)
{
    [gargoyle closeWindow: processID];
    exit(0);
}

void winopenfile(char *prompt, char *buf, int len, int filter)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];    

    NSString * fileref = [gargoyle openWindowDialog: processID
                                             prompt: [NSString stringWithCString: prompt encoding: NSUTF8StringEncoding]
                                             filter: filter];

    strcpy(buf, "");

    if (fileref)
    {
        [fileref getCString: buf maxLength: len encoding: NSUTF8StringEncoding];
    }

    [pool drain];
}

void winsavefile(char *prompt, char *buf, int len, int filter)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSString * fileref = [gargoyle saveWindowDialog: processID
                                             prompt: [NSString stringWithCString: prompt encoding: NSUTF8StringEncoding]
                                             filter: filter];

    strcpy(buf, "");

    if (fileref)
    {
        [fileref getCString: buf maxLength: len encoding: NSUTF8StringEncoding];
    }

    [pool drain];
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

    if (gli_image_w == (unsigned int) viewRect.size.width
            && gli_image_h == (unsigned int) viewRect.size.height)
        return;

    gli_image_w = (unsigned int) viewRect.size.width;
    gli_image_h = (unsigned int) viewRect.size.height;
    gli_image_s = ((gli_image_w * 4 + 3) / 4) * 4;

    /* initialize offline bitmap store */
    if (gli_image_rgb)
        free(gli_image_rgb);

     gli_image_rgb = malloc(gli_image_s * gli_image_h);

    /* redraw window content */
    gli_resize_mask(gli_image_w, gli_image_h);
    gli_force_redraw = TRUE;
    gli_refresh_needed = TRUE;
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
        gli_window_alive = FALSE;
}

void wininit(int *argc, char **argv)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

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

    [gargoyle initWindow: processID
                   width: defw
                  height: defh];

    wintitle();
    winresize();

    [pool drain];
}

void winrepaint(int x0, int y0, int x1, int y1)
{
    gli_refresh_needed = TRUE;
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
    /* queue a screen refresh */
    gli_refresh_needed = TRUE;

    /* check for arrow keys */
    if ([evt modifierFlags] & NSFunctionKeyMask)
    {
        /* alt/option modified key */
        if ([evt modifierFlags] & NSAlternateKeyMask)
        {
            switch ([evt keyCode])
            {
                case NSKEY_LEFT  : gli_input_handle_key(keycode_SkipWordLeft);  return;
                case NSKEY_RIGHT : gli_input_handle_key(keycode_SkipWordRight); return;
                case NSKEY_DOWN  : gli_input_handle_key(keycode_PageDown);      return;
                case NSKEY_UP    : gli_input_handle_key(keycode_PageUp);        return;
                default: break;
            }
        }

        /* command modified key */
        if ([evt modifierFlags] & NSCommandKeyMask)
        {
            switch ([evt keyCode])
            {
                case NSKEY_LEFT  : gli_input_handle_key(keycode_Home);     return;
                case NSKEY_RIGHT : gli_input_handle_key(keycode_End);      return;
                case NSKEY_DOWN  : gli_input_handle_key(keycode_PageDown); return;
                case NSKEY_UP    : gli_input_handle_key(keycode_PageUp);   return;
                default: break;
            }
        }

        /* unmodified key for line editing */
        switch ([evt keyCode])
        {
            case NSKEY_LEFT  : gli_input_handle_key(keycode_Left);  return;
            case NSKEY_RIGHT : gli_input_handle_key(keycode_Right); return;
            case NSKEY_DOWN  : gli_input_handle_key(keycode_Down);  return;
            case NSKEY_UP    : gli_input_handle_key(keycode_Up);    return;
            default: break;
        }
    }

    /* check for menu commands */
    if ([evt modifierFlags] & NSCommandKeyMask)
    {
        switch ([evt keyCode])
        {
            case NSKEY_X:
            case NSKEY_C:
            {
                winclipsend();
                return;
            }

            case NSKEY_V:
            {
                winclipreceive();
                return;
            }

            default: break;
        }
    }

    /* check for command keys */
    switch ([evt keyCode])
    {
        case NSKEY_PGUP : gli_input_handle_key(keycode_PageUp);   return;
        case NSKEY_PGDN : gli_input_handle_key(keycode_PageDown); return;
        case NSKEY_HOME : gli_input_handle_key(keycode_Home);     return;
        case NSKEY_END  : gli_input_handle_key(keycode_End);      return;
        case NSKEY_DEL  : gli_input_handle_key(keycode_Erase);    return;
        case NSKEY_BACK : gli_input_handle_key(keycode_Delete);   return;
        case NSKEY_ESC  : gli_input_handle_key(keycode_Escape);   return;
        case NSKEY_F1   : gli_input_handle_key(keycode_Func1);    return;
        case NSKEY_F2   : gli_input_handle_key(keycode_Func2);    return;
        case NSKEY_F3   : gli_input_handle_key(keycode_Func3);    return;
        case NSKEY_F4   : gli_input_handle_key(keycode_Func4);    return;
        case NSKEY_F5   : gli_input_handle_key(keycode_Func5);    return;
        case NSKEY_F6   : gli_input_handle_key(keycode_Func6);    return;
        case NSKEY_F7   : gli_input_handle_key(keycode_Func7);    return;
        case NSKEY_F8   : gli_input_handle_key(keycode_Func8);    return;
        case NSKEY_F9   : gli_input_handle_key(keycode_Func9);    return;
        case NSKEY_F10  : gli_input_handle_key(keycode_Func10);   return;
        case NSKEY_F11  : gli_input_handle_key(keycode_Func11);   return;
        case NSKEY_F12  : gli_input_handle_key(keycode_Func12);   return;
        default: break;
    }

    /* send combined keystrokes to text buffer */
    [gargoyle setWindow: processID charString: evt];

    /* retrieve character from buffer as string */
    NSString * evt_char = [gargoyle getWindowCharString: processID];

    /* convert character to UTF-32 value */
    glui32 ch;
    if ([evt_char getBytes: &ch maxLength: sizeof ch usedLength: NULL
                  encoding: UTF32StringEncoding
                   options: 0
                     range: NSMakeRange(0, [evt_char length])
            remainingRange: NULL])
    {
        switch (ch)
        {
            case '\n': gli_input_handle_key(keycode_Return); break;
            case '\t': gli_input_handle_key(keycode_Tab);    break;
            default: gli_input_handle_key(ch); break;
        }
    }

    /* discard contents of text buffer */
    [gargoyle clearWindowCharString: processID];
}

void winmouse(NSEvent *evt)
{
    NSPoint coords = [evt locationInWindow];

    int x = coords.x;
    int y = gli_image_h - coords.y;

    /* disregard most events outside of content window */
    if ((coords.y < 0 || y < 0 || x < 0 || x > gli_image_w)
        && !([evt type] == NSLeftMouseUp))
        return;

    switch ([evt type])
    {
        case NSLeftMouseDown:
        {
            gli_input_handle_click(x, y);
            [gargoyle setCursor: kArrowCursor];
            break;
        }

        case NSLeftMouseDragged:
        {
            if (gli_copyselect)
            {
                [gargoyle setCursor: kIBeamCursor];
                gli_move_selection(x, y);
            }
            break;
        }

        case NSMouseMoved:
        {
            if (gli_get_hyperlink(x, y))
                [gargoyle setCursor: kPointingHandCursor];
            else
                [gargoyle setCursor: kArrowCursor]; 
            break;
        }

        case NSLeftMouseUp:
        {
            gli_copyselect = FALSE;
            [gargoyle setCursor: kArrowCursor];
            break;
        }

        case NSScrollWheel:
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
        gli_event_waiting = FALSE;
        return;
    }

    switch ([evt type])
    {
        case NSKeyDown:
        {
            winkey(evt);
            return;
        }

        case NSLeftMouseDown:
        case NSLeftMouseDragged:
        case NSLeftMouseUp:
        case NSMouseMoved:
        case NSScrollWheel:
        {
            winmouse(evt);
            return;
        }

        case NSApplicationDefined:
        {
            gli_refresh_needed = TRUE;
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

void gli_select(event_t *event, int polled)
{ 
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    gli_curevent = event;
    gli_event_clearevent(event);

    winpoll();
    gli_dispatch_event(gli_curevent, polled);

    if (gli_curevent->type == evtype_None && !polled)
    {
        while (![monitor timeout])
        {
            winloop();
            gli_dispatch_event(gli_curevent, polled);

            if (gli_curevent->type == evtype_None)
                [monitor sleep];
            else
                break;
        }
    }

    if (gli_curevent->type == evtype_None && [monitor timeout])
    {
        gli_event_store(evtype_Timer, NULL, 0, 0);
        gli_dispatch_event(gli_curevent, polled);
        [monitor reset];
    }

    gli_curevent = NULL;

    [pool drain];
}

/* monotonic clock time for profiling */
void wincounter(glktimeval_t *time)
{
    static mach_timebase_info_data_t info = {0,0};
    if (!info.denom)
        mach_timebase_info(&info);

    uint64_t tick = mach_absolute_time();
    tick *= info.numer;
    tick /= info.denom;
    uint64_t micro = tick / 1000;

    time->high_sec = 0;
    time->low_sec  = (unsigned int) ((micro / 1000000) & 0xFFFFFFFF);
    time->microsec = (unsigned int) ((micro % 1000000) & 0xFFFFFFFF);
}
