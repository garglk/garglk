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

#include "glk.h"
#include "garglk.h"

#import "Cocoa/Cocoa.h"

static NSWindow * window = NULL;
static NSBitmapImageRep * framebuf = NULL;
static NSTextView * textbuf = NULL;
static NSString * cliptext = NULL;
static NSPasteboard * clipboard = NULL;

static int statusbar = 20;
static int launchPID = 0;

static int timeouts = 0;
static int timerid = -1;

static int gli_refresh_needed = TRUE;
static int gli_window_hidden = FALSE;

@interface GargoyleTerp : NSObject
{
}
@end

@implementation GargoyleTerp

- (id) init
{
    [[NSDistributedNotificationCenter defaultCenter] addObserver: self
                                                        selector: @selector(receive:)
                                                            name: NULL
                                                          object: @"com.googlecode.garglk"];
    return [super init];
}

- (void) send: (NSString *) message
{
    if ([message length])
    {
        [[NSDistributedNotificationCenter defaultCenter] postNotificationName: message
                                                                       object: @"com.googlecode.garglk"
                                                                     userInfo: NULL
                                                           deliverImmediately: YES];
    }

}

- (void) receive: (NSNotification *) message
{
    if ([[message name] isEqualToString: @"QUIT"])
    {
        exit(0);
    }

    if ([[message name] isEqualToString: @"HIDE"])
    {
        [window orderOut:self];
        gli_window_hidden = TRUE;
    }

    if ([[message name] isEqualToString: @"SHOW"])
    {
        ProcessSerialNumber psnt = { 0, kCurrentProcess };
        SetFrontProcess(&psnt);
        [window makeKeyAndOrderFront: window];
        gli_window_hidden = FALSE;
    }
}

@end

GargoyleTerp * gargoyle;

void glk_request_timer_events(glui32 millisecs)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    if (timerid != -1) {
        [NSEvent stopPeriodicEvents];
        timerid = -1;
        timeouts = 0;
    }

    if (millisecs)
    {
        double interval = ((double) millisecs) / 1000;
        [NSEvent startPeriodicEventsAfterDelay: interval
                                    withPeriod: interval];
        timerid = 1;
    }

    [pool drain];
}

void winabort(const char *fmt, ...)
{
    va_list ap;
    char buf[256];
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSString * msg = [[NSString alloc] initWithCString: buf encoding: NSASCIIStringEncoding];
    NSRunAlertPanel(@"Fatal error", msg, NULL, NULL, NULL);
    [msg release];
    [pool drain];

    abort();
}

void winopenfile(char *prompt, char *buf, int len, char *filter)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSOpenPanel * openDlg = [NSOpenPanel openPanel];

    [openDlg setCanChooseFiles: YES];
    [openDlg setCanChooseDirectories: NO];
    [openDlg setAllowsMultipleSelection: NO];

    NSString * msg = [[NSString alloc] initWithCString: prompt encoding: NSASCIIStringEncoding];
    [openDlg setTitle: msg];
    [msg release];

    strcpy(buf, "");

    if ([openDlg runModal] == NSFileHandlingPanelOKButton)
    {
        NSString * fileref = [openDlg filename];
        int size = [fileref length];
        
        CFStringGetBytes((CFStringRef) fileref, CFRangeMake(0, size),
                         kCFStringEncodingASCII, 0, FALSE,
                         buf, len, NULL);
        
        int bounds = size < len ? size : len;
        buf[bounds] = '\0';
    }

    [pool drain];
}

void winsavefile(char *prompt, char *buf, int len, char *filter)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSSavePanel * saveDlg = [NSSavePanel savePanel];

    [saveDlg setCanCreateDirectories: YES];

    NSString * msg = [[NSString alloc] initWithCString: prompt encoding: NSASCIIStringEncoding];
    [saveDlg setTitle: msg];
    [msg release];

    strcpy(buf, "");

    if ([saveDlg runModal] == NSFileHandlingPanelOKButton)
    {
        NSString * fileref = [saveDlg filename];
        int size = [fileref length];

        CFStringGetBytes((CFStringRef) fileref, CFRangeMake(0, size),
                         kCFStringEncodingASCII, 0, FALSE,
                         buf, len, NULL);

        int bounds = size < len ? size : len;
        buf[bounds] = '\0';
    }

    [pool drain];
}

void winclipstore(glui32 *text, int len)
{
    if (!len)
        return;

    if (cliptext) {
        [cliptext release];
        cliptext = NULL;
    }

    cliptext = (NSString *) CFStringCreateWithBytes(kCFAllocatorDefault,
                                                    (char *) text, (len * 4),
                                                    kCFStringEncodingUTF32LE, FALSE);
}

void winclipsend(void)
{
    if (cliptext)
    {
        [clipboard declareTypes: [NSArray arrayWithObject: NSStringPboardType] owner:nil];
        [clipboard setString: cliptext forType: NSStringPboardType];
    }
}

void winclipreceive(void)
{
    int i, len;
    glui32 ch;

    if ([clipboard availableTypeFromArray: [NSArray arrayWithObject: NSStringPboardType]])
    {
        NSString * input = [clipboard stringForType: NSStringPboardType];
        if (input)
        {
            len = [input length];
            for (i=0; i < len; i++)
            {
                if (CFStringGetBytes((CFStringRef) input, CFRangeMake(i, 1),
                    kCFStringEncodingUTF32, 0, FALSE,
                    (char *) &ch, 4, NULL))
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
    char buf[256];

    if (strlen(gli_story_name))
        sprintf(buf, "%s - %s", gli_program_name, gli_story_name);
    else
        sprintf(buf, "%s", gli_program_name);

    NSString * title = [[NSString alloc] initWithCString: buf encoding: NSASCIIStringEncoding];
    [window setTitle:title];
    [title release];

    [pool drain];
}

void winresize(NSRect viewRect)
{
    gli_image_w = viewRect.size.width;
    gli_image_h = viewRect.size.height > statusbar ? viewRect.size.height - statusbar : 0;
    gli_image_s = ((gli_image_w * 3 + 3) / 4) * 4;

    /* initialize offline bitmap store */
    if (gli_image_rgb)
        free(gli_image_rgb);

    gli_image_rgb = malloc(gli_image_s * gli_image_h);	

    /* initialize buffer for screen refreshes */
    if (framebuf)
        [framebuf release];

    framebuf = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes: NULL
                                                       pixelsWide: gli_image_w 
                                                       pixelsHigh: gli_image_h
                                                    bitsPerSample: 8
                                                  samplesPerPixel: 3
                                                         hasAlpha: NO
                                                         isPlanar: NO
                                                   colorSpaceName: NSCalibratedRGBColorSpace
                                                      bytesPerRow: gli_image_s
                                                     bitsPerPixel: 24];

    gli_resize_mask(gli_image_w, gli_image_h);
    gli_force_redraw = TRUE;
    gli_refresh_needed = TRUE;
    gli_windows_size_change();
}

void wininit(int *argc, char **argv)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    gargoyle = [[GargoyleTerp alloc] init];
    [NSApplication sharedApplication];
    [NSApp activateIgnoringOtherApps: YES];
    [NSApp setDelegate: gargoyle];
    [pool drain];
}

void winopen(void)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSUInteger defw = gli_wmarginx * 2 + gli_cellw * gli_cols;
    NSUInteger defh = gli_wmarginy * 2 + gli_cellh * gli_rows + statusbar;
    NSSize limit = {0, statusbar};

    NSUInteger style = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask;

    /* set up the window */
    window = [[NSWindow alloc] initWithContentRect: NSMakeRect(0,0, defw, defh)
                                         styleMask: style
                                           backing: NSBackingStoreBuffered
                                             defer: NO];
    [window setContentMinSize: limit];	
    [window makeKeyAndOrderFront: window];
    [window center];
    [window setReleasedWhenClosed: NO];
    wintitle();
    winresize([[window contentView] bounds]);

    /* set up the text buffer to receive composed keys */
    textbuf = [[NSTextView alloc] init];

    /* set up the clipboard object */
    clipboard = [NSPasteboard generalPasteboard];
    
    /* find launcher PID */
    if (getenv("GARGOYLEPID"))
    {
        NSString * nsPID = [[NSString alloc] initWithCString: getenv("GARGOYLEPID") encoding: NSASCIIStringEncoding];
        launchPID = [nsPID intValue];
        [nsPID release];
    }

    [pool drain];
}

void winrepaint(int x0, int y0, int x1, int y1)
{
    gli_refresh_needed = TRUE;
}

void winrefresh(void)
{
    if (![window isVisible])
        return;

    gli_windows_redraw();

    /* convert bitmap store */
    memcpy([framebuf bitmapData], gli_image_rgb, gli_image_s * gli_image_h);
    CIImage * output = [[CIImage alloc] initWithBitmapImageRep: framebuf];

    /* refresh the screen */
    [output drawAtPoint: NSMakePoint(0, statusbar)
               fromRect: NSMakeRect(0, 0, gli_image_w, gli_image_h)
              operation: NSCompositeSourceOver
               fraction: 1.0];
    [output release];

    [window setContentBorderThickness: (CGFloat) statusbar forEdge: NSMinYEdge];
    [window flushWindow];

    gli_refresh_needed = FALSE;
}

#define NSKEY_LEFT	0x7b
#define NSKEY_RIGHT	0x7c
#define NSKEY_DOWN	0x7d
#define NSKEY_UP	0x7e

#define NSKEY_X		0x07
#define NSKEY_C		0x08
#define NSKEY_V		0x09

#define NSKEY_PGUP	0x74
#define NSKEY_PGDN	0x79
#define NSKEY_HOME	0x73
#define NSKEY_END	0x77
#define NSKEY_DEL	0x75
#define NSKEY_BACK	0x33
#define NSKEY_ESC	0x35

#define NSKEY_F1	0x7a
#define NSKEY_F2	0x78
#define NSKEY_F3	0x63
#define NSKEY_F4	0x76
#define NSKEY_F5	0x60
#define NSKEY_F6	0x61
#define NSKEY_F7	0x62
#define NSKEY_F8	0x64
#define NSKEY_F9	0x65
#define NSKEY_F10	0x6d
#define NSKEY_F11	0x67
#define NSKEY_F12	0x6f

void winkey(NSEvent *evt)
{
    /* queue a screen refresh */
    gli_refresh_needed = TRUE;

    /* check for arrow keys */
    if ([evt modifierFlags] & NSNumericPadKeyMask)
    {
        /* modified keys for scrolling */
        if ([evt modifierFlags] & NSCommandKeyMask
                    || [evt modifierFlags] & NSAlternateKeyMask
                    || [evt modifierFlags] & NSControlKeyMask)
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

        /* unmodified keys for line editing */
        else
        {
            switch ([evt keyCode])
            {
                case NSKEY_LEFT  : gli_input_handle_key(keycode_Left);  return;
                case NSKEY_RIGHT : gli_input_handle_key(keycode_Right); return;
                case NSKEY_DOWN  : gli_input_handle_key(keycode_Down);  return;
                case NSKEY_UP    : gli_input_handle_key(keycode_Up);    return;
                default: break;
            }
        }
    }

    /* check for copy and paste */
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
    [textbuf interpretKeyEvents: [NSArray arrayWithObject:evt]];
    
    /* retrieve character from buffer as string */
    NSString * evt_char = [[textbuf textStorage] string];

    /* convert character to UTF-32 value */
    glui32 ch;
    if (CFStringGetBytes((CFStringRef) evt_char,
                         CFRangeMake(0, [evt_char length]),
                         kCFStringEncodingUTF32, 0, FALSE,
                         (char *)&ch, 4, NULL)) {
        switch (ch)
        {
            case '\n': gli_input_handle_key(keycode_Return); break;
            case '\t': gli_input_handle_key(keycode_Tab);    break;
            default: gli_input_handle_key(ch); break;
        }
    }

    /* discard contents of text buffer */
    [[[textbuf textStorage] mutableString] setString: @""];
}

void winfocus(void)
{
    /* move launcher to front */
    ProcessSerialNumber psnl;
    GetProcessForPID(launchPID, &psnl);
    SetFrontProcess(&psnl);

    /* move terp to front */
    ProcessSerialNumber psnt = { 0, kCurrentProcess };
    SetFrontProcess(&psnt);

    /* make this window key */
    [window makeKeyAndOrderFront: window];
}

void winmouse(NSEvent *evt)
{
    NSPoint coords = [[window contentView] convertPoint: [evt locationInWindow]
                                               fromView: NULL];
    int x = coords.x;
    int y = (gli_image_h + statusbar) - coords.y;

    /* disregard events outside of content window */
    if (coords.y < statusbar || y < 0 || x < 0 || x > gli_image_w)
    {
        winfocus();
        [NSApp sendEvent: evt];
        return;
    }

    switch ([evt type])
    {
        case NSLeftMouseDown:
        {
            winfocus();
            gli_input_handle_click(x, y);
            break;
        }

        case NSLeftMouseUp:
        {
            gli_copyselect = FALSE;
            [[NSCursor arrowCursor] set];
            break;
        }

        case NSLeftMouseDragged:
        {
            if (gli_copyselect)
            {
                [[NSCursor IBeamCursor] set];
                gli_move_selection(x, y);
            }

            else
            {
                if (gli_get_hyperlink(x, y))
                    [[NSCursor pointingHandCursor] set];
                
                else
                    [[NSCursor arrowCursor] set];
            }

            break;
        }

        case NSRightMouseDown:
        case NSOtherMouseDown:
        {
            winfocus();
            break;
        }

        default: break;
    }

    /* let the window manager process the event as well */
    [NSApp sendEvent: evt];
}

void winevent(NSEvent *evt)
{
    /* window was closed */
    if (![window isVisible] && ![window isMiniaturized] && !gli_window_hidden)
        exit(0);

    /* window was minimized or hidden */
    if (![window isVisible] && ([window isMiniaturized] || gli_window_hidden))
        gli_refresh_needed = TRUE;

    /* window was moved between screens */
    if ([evt type] == NSAppKitDefined && [evt subtype] == NSWindowMovedEventType)
        gli_refresh_needed = TRUE;

    /* window was resized */
    NSRect viewRect = [[window contentView] bounds];
    if (viewRect.size.width != gli_image_w || viewRect.size.height != gli_image_h + statusbar)
        winresize(viewRect);

    switch ([evt type])
    {
        case NSKeyDown :
        {
            winkey(evt);
            return;
        }

        case NSLeftMouseDown :
        case NSRightMouseDown :
        case NSOtherMouseDown :
        case NSLeftMouseDragged :
        case NSLeftMouseUp :
        {
            winmouse(evt);
            return;
        }

        case NSPeriodic :
        {
            timeouts++;
            return;
        }

        default: [NSApp sendEvent: evt]; return;
    }

}

/* winloop handles at most one event */
void winloop(void)
{
    NSEvent * evt;

    if (gli_refresh_needed)
        winrefresh();

    /* return the next window event that occurs */
    evt = [NSApp nextEventMatchingMask: NSAnyEventMask 
                             untilDate: [NSDate distantFuture] 
                                inMode: NSDefaultRunLoopMode 
                               dequeue: YES];
    if (evt)
        winevent(evt);
}

/* winpoll handles all queued events */
void winpoll(void)
{
    NSEvent * evt;

    do {

        if (gli_refresh_needed)
            winrefresh();

        /* return the first window event from the queue */
        evt = [NSApp nextEventMatchingMask: NSAnyEventMask 
                                 untilDate: [NSDate distantPast] 
                                    inMode: NSDefaultRunLoopMode 
                                   dequeue: YES];
        if (evt)
            winevent(evt);

    } while (evt);
}

void gli_select(event_t *event, int polled)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    gli_curevent = event;
    gli_event_clearevent(event);

    gli_input_guess_focus();

    if (!polled)
    {
        while (gli_curevent->type == evtype_None && !timeouts)
        {
            winloop();
            gli_dispatch_event(gli_curevent, polled);
        }
    }

    else {
        if (!timeouts)
            winpoll();
        gli_dispatch_event(gli_curevent, polled);
    }

    if (gli_curevent->type == evtype_None && timeouts)
    {
        gli_event_store(evtype_Timer, NULL, 0, 0);
        gli_dispatch_event(gli_curevent, polled);
        timeouts = 0;
    }

    gli_curevent = NULL;

    [pool drain];
}
