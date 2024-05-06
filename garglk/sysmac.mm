// Copyright (C) 2010 by Ben Cressey.
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

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <mach/port.h>
#include <mach/message.h>
#include <unistd.h>

#include "glk.h"
#include "garglk.h"
#include "garversion.h"

#include "format.h"
#include "optional.hpp"

#import "Cocoa/Cocoa.h"
#import "sysmac.h"

static volatile sig_atomic_t gli_event_waiting = false;
static volatile sig_atomic_t gli_mach_allowed = false;
static volatile sig_atomic_t gli_window_alive = true;

#define kArrowCursor        1
#define kIBeamCursor        2
#define kPointingHandCursor 3

static void wintick(CFRunLoopTimerRef timer, void *info);
static void winhandler(int signal);

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

    timerid = nullptr;
    timeouts = 0;

    return self;
}

- (void) sleep
{
    while (!timeouts && !gli_event_waiting) {
        if (!gli_window_alive) {
            std::exit(1);
        }

        gli_mach_allowed = true;
        [[NSRunLoop currentRunLoop] acceptInputForMode: NSDefaultRunLoopMode beforeDate: [NSDate distantFuture]];
        gli_mach_allowed = false;
    }
}

- (void) track: (CFTimeInterval) seconds
{
    if (timerid) {
        if (CFRunLoopTimerIsValid(timerid)) {
            CFRunLoopTimerInvalidate(timerid);
        }
        CFRelease(timerid);
        timerid = nullptr;
        timeouts = 0;
    }

    if (seconds) {
        timerid = CFRunLoopTimerCreate(nullptr, CFAbsoluteTimeGetCurrent() + seconds, seconds,
                                       0, 0, &wintick, nullptr);
        if (timerid) {
            CFRunLoopAddTimer([[NSRunLoop currentRunLoop] getCFRunLoop], timerid, kCFRunLoopDefaultMode);
        }
    }
}

- (void) tick
{
    timeouts++;
    CFRunLoopStop([[NSRunLoop currentRunLoop] getCFRunLoop]);
}

- (BOOL) timeout
{
    return timeouts != 0;
}

- (void) reset
{
    timeouts = 0;
}

- (void) connectionDied: (NSNotification *) notice
{
    std::exit(1);
}

@end

static NSObject<GargoyleApp> *gargoyle = nullptr;
static GargoyleMonitor *monitor = nullptr;
static NSString *cliptext = nullptr;
static pid_t processID = 0;

static bool gli_refresh_needed = true;

void glk_request_timer_events(glui32 millisecs)
{
    [monitor track: millisecs / 1000.0];
}

static void wintick(CFRunLoopTimerRef timer, void *info)
{
    [monitor tick];
}

void gli_notification_waiting()
{
    winhandler(SIGUSR1);
}

void garglk::winabort(const std::string &msg)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [gargoyle abortWindowDialog: processID
                         prompt: [NSString stringWithCString: msg.c_str() encoding: NSUTF8StringEncoding]];
    [pool drain];

    std::exit(1);
}

void garglk::winwarning(const std::string &title, const std::string &msg)
{
    auto *nsTitle = [NSString stringWithCString: title.c_str() encoding: NSUTF8StringEncoding];
    auto *nsMsg = [NSString stringWithCString: msg.c_str() encoding: NSUTF8StringEncoding];
    NSRunAlertPanel(nsTitle, @"%@", nil, nil, nil, nsMsg);
}

void winexit()
{
    [gargoyle closeWindow:processID];
    gli_exit(0);
}

std::string garglk::winopenfile(const char *prompt, FileFilter filter)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSString *fileref = [gargoyle openWindowDialog: processID
                                            prompt: [NSString stringWithCString: prompt encoding: NSUTF8StringEncoding]
                                            filter: filter];

    std::string buf;

    if (fileref) {
        buf = [fileref UTF8String];
    }

    [pool drain];

    return buf;
}

std::string garglk::winsavefile(const char *prompt, FileFilter filter)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSString *fileref = [gargoyle saveWindowDialog: processID
                                            prompt: [NSString stringWithCString: prompt encoding: NSUTF8StringEncoding]
                                            filter: filter];

    std::string buf;

    if (fileref) {
        buf = [fileref UTF8String];
    }

    [pool drain];

    return buf;
}

void winclipstore(const glui32 *text, int len)
{
    if (!len) {
        return;
    }

    if (cliptext) {
        [cliptext release];
        cliptext = nil;
    }

    cliptext = [[NSString alloc] initWithBytes: text
                                        length: (len * sizeof(glui32))
                                      encoding: UTF32StringEncoding];
}

void winclipsend()
{
    if (cliptext) {
        NSPasteboard *clipboard = [NSPasteboard generalPasteboard];
        [clipboard declareTypes: [NSArray arrayWithObject: NSStringPboardType] owner:nil];
        [clipboard setString: cliptext forType: NSStringPboardType];
    }
}

void winclipreceive()
{
    int i, len;
    glui32 ch;

    NSPasteboard *clipboard = [NSPasteboard generalPasteboard];

    if ([clipboard availableTypeFromArray: [NSArray arrayWithObject: NSStringPboardType]]) {
        NSString *input = [clipboard stringForType: NSStringPboardType];
        if (input) {
            len = [input length];
            for (i = 0; i < len; i++) {
                if ([input getBytes: &ch maxLength: sizeof ch usedLength: nullptr
                           encoding: UTF32StringEncoding
                            options: 0
                              range: NSMakeRange(i, 1)
                     remainingRange: nullptr]) {
                    switch (ch) {
                    case '\0':
                        return;

                    case '\b':
                    case '\t':
                        break;

                    case '\r':
                    case '\n':
                        gli_input_handle_key_paste(keycode_Return);
                        break;

                    default:
                        gli_input_handle_key_paste(ch);
                    }
                }
            }
        }
    }
}

void wintitle()
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSString *story_title = [NSString stringWithCString: gli_story_title.c_str() encoding: NSUTF8StringEncoding];
    NSString *story_name = [NSString stringWithCString: gli_story_name.c_str() encoding: NSUTF8StringEncoding];
    NSString *program_name = [NSString stringWithCString: gli_program_name.c_str() encoding: NSUTF8StringEncoding];

    NSString *title = nil;
    if ([story_title length]) {
        title = story_title;
    } else if ([story_name length]) {
        title = [NSString stringWithFormat:@"%@ - %@", story_name, program_name];
    } else {
        title = program_name;
    }

    [gargoyle setWindow: processID
                  title: title];

    [pool drain];
}

void winresize()
{
    NSRect viewRect = [gargoyle updateBackingSize: processID];
    float textureFactor = BACKING_SCALE_FACTOR / [gargoyle getBackingScaleFactor: processID];

    unsigned int vw = NSWidth(viewRect) * textureFactor;
    unsigned int vh = NSHeight(viewRect) * textureFactor;

    if (gli_image_rgb.width() == vw && gli_image_rgb.height() == vh) {
        return;
    }

    gli_windows_size_change(vw, vh, true);

    // redraw window content
    gli_refresh_needed = true;
}

static mach_port_t gli_signal_port = 0;

void winmach(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    mach_msg_header_t *hdr = static_cast<mach_msg_header_t *>(msg);
    switch (hdr->msgh_id) {
    case SIGUSR1:
        gli_event_waiting = true;
        break;

    default:
        break;
    }
}

static void winhandler(int signal)
{
    if (signal == SIGUSR1) {
        if (gli_mach_allowed) {
            mach_msg_header_t header;
            header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND, 0);
            header.msgh_size        = sizeof(header);
            header.msgh_remote_port = gli_signal_port;
            header.msgh_local_port  = MACH_PORT_NULL;
            header.msgh_reserved    = 0;
            header.msgh_id          = signal;

            mach_msg_send(&header);
        } else {
            gli_event_waiting = true;
        }
    }

    if (signal == SIGUSR2) {
        gli_window_alive = false;

        // Stop all sound channels
        for (channel_t *chan = glk_schannel_iterate(nullptr, nullptr); chan; chan = glk_schannel_iterate(chan, nullptr)) {
            glk_schannel_stop(chan);
        }
    }
}

void wininit(int *argc, char **argv)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    gli_backingscalefactor = BACKING_SCALE_FACTOR;

    // establish link to launcher
    NSString *linkName = [NSString stringWithUTF8String: std::getenv("GargoyleApp")];
    NSConnection *link = [NSConnection connectionWithRegisteredName: linkName host: nullptr];
    [link retain];

    // monitor link for failure
    monitor = [[GargoyleMonitor alloc] init];
    [[NSNotificationCenter defaultCenter] addObserver: monitor
                                             selector: @selector(connectionDied:)
                                                 name: NSConnectionDidDieNotification
                                               object: link];

    // attach to app controller
    gargoyle = (NSObject<GargoyleApp> *)[link rootProxy];
    [gargoyle retain];

    // listen for mach messages
    CFMachPortRef sigPort = CFMachPortCreate(nullptr, winmach, nullptr, nullptr);
    gli_signal_port = CFMachPortGetPort(sigPort);
    [[NSRunLoop currentRunLoop] addPort: [NSMachPort portWithMachPort: gli_signal_port] forMode: NSDefaultRunLoopMode];

    // load signal handler
    signal(SIGUSR1, winhandler);
    signal(SIGUSR2, winhandler);

    processID = getpid();

    [pool drain];
}

NSString *get_qt_plist_path()
{
    const char *home = std::getenv("HOME");
    if (home != nullptr) {
        // Optimistically use the path that Qt uses with the hope/plan
        // of moving macOS to Qt one of these days.
        auto path = Format("{}/Library/Preferences/com.io-github-garglk.Gargoyle.plist", home);
        return [NSString stringWithUTF8String: path.c_str()];
    }

    return nil;
}

void winopen()
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    auto do_fullscreen = gli_conf_fullscreen;

    bool move = false;
    int x = 0;
    int y = 0;
    unsigned int width = gli_wmarginx * 2 + gli_cellw * gli_cols / BACKING_SCALE_FACTOR;
    unsigned int height = gli_wmarginy * 2 + gli_cellh * gli_rows / BACKING_SCALE_FACTOR;
    NSColor *windowColor = [NSColor colorWithCalibratedRed: (float) gli_window_color[0] / 255.0f
                                                     green: (float) gli_window_color[1] / 255.0f
                                                      blue: (float) gli_window_color[2] / 255.0f
                                                     alpha: 1.0f];

    auto path = get_qt_plist_path();
    if (path != nil) {
        NSDictionary *config = [NSDictionary dictionaryWithContentsOfFile: path];

        if (gli_conf_save_window_location) {
            NSString *positionobj = config[@"window.position"];
            if (positionobj) {
                std::string position = [positionobj UTF8String];
                std::regex position_re(R"(^@Point\((\d+) (\d+)\)$)");
                std::smatch cm;
                if (std::regex_match(position, cm, position_re) && cm.size() == 3) {
                    try {
                        auto tmpx = std::stoi(cm[1]);
                        auto tmpy = std::stoi(cm[2]);

                        x = tmpx;
                        y = tmpy;
                        move = true;
                    } catch (...) {
                    }
                }
            }
        }

        if (gli_conf_save_window_size) {
            NSString *sizeobj = config[@"window.size"];
            if (sizeobj) {
                std::string size = [sizeobj UTF8String];
                std::regex size_re(R"(^@Size\((\d+) (\d+)\)$)");
                std::smatch cm;
                if (std::regex_match(size, cm, size_re) && cm.size() == 3) {
                    try {
                        auto tmpwidth = std::stoi(cm[1]);
                        auto tmpheight = std::stoi(cm[2]);

                        width = tmpwidth;
                        height = tmpheight;
                    } catch (...) {
                    }
                }
            }
        }

        if (gli_conf_save_window_location || gli_conf_save_window_size) {
            NSNumber *fsobj = config[@"window.fullscreen"];
            if (fsobj) {
                do_fullscreen = [fsobj boolValue];
            }
        }
    }

    [gargoyle initWindow: processID
                    move: move
                       x: x
                       y: y
                   width: width
                  height: height
              fullscreen: do_fullscreen
         backgroundColor: windowColor];

    wintitle();
    winresize();

    [pool drain];
}

void winrepaint(int x0, int y0, int x1, int y1)
{
    gli_refresh_needed = true;
}

bool windark()
{
    NSString *theme = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];

    return [theme isEqualToString:@"Dark"];
}

void winrefresh()
{
    gli_windows_redraw();

    NSData *frame = [NSData dataWithBytesNoCopy: gli_image_rgb.data()
                                         length: gli_image_rgb.size()
                                   freeWhenDone: NO];

    int refreshed = [gargoyle setWindow: processID
                               contents: frame
                                  width: gli_image_rgb.width()
                                 height: gli_image_rgb.height()];

    gli_refresh_needed = !refreshed;
}

static void show_alert(NSAlertStyle style, NSString *title, const std::string &text)
{
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = style;
    alert.messageText = title;
    alert.informativeText = [NSString stringWithUTF8String: text.c_str()];
    alert.window.level = NSModalPanelWindowLevel;
    [alert runModal];
    [alert release];
}

static void show_info(NSString *title, const std::string &text)
{
    show_alert(NSAlertStyleInformational, title, text);
}

static void show_warning(NSString *title, const std::string &text)
{
    show_alert(NSAlertStyleWarning, title, text);
}

static void show_error(NSString *title, const std::string &text)
{
    show_alert(NSAlertStyleCritical, title, text);
}

static void show_paths()
{
    std::string text = "Configuration file paths:\n\n";
    for (const auto &config : garglk::all_configs) {
        text += Format("• {} {}\n", config.path, config.format_type());
    }

    text += "\nTheme paths:\n\n";
    auto theme_paths = garglk::theme::paths();
    std::reverse(theme_paths.begin(), theme_paths.end());
    for (const auto &path : theme_paths) {
        text += Format("• {}\n", path);
    }

    show_info(@"Paths", text);
}

static void show_themes()
{
    std::string text = "The following themes are available:\n\n";
    auto theme_names = garglk::theme::names();

    std::sort(theme_names.begin(), theme_names.end());
    for (const auto &theme_name : theme_names) {
        text += Format("• {}\n", theme_name);
    }

    show_info(@"Themes", text);
}

void winkey(NSEvent *evt)
{
    NSEventModifierFlags modifiers = [evt modifierFlags];
    NSEventModifierFlags modmasked = modifiers & (NSEventModifierFlagOption | NSEventModifierFlagShift | NSEventModifierFlagCommand | NSEventModifierFlagControl);

    // queue a screen refresh
    gli_refresh_needed = true;

    // special key combinations
    static const std::map<std::pair<decltype(modmasked), decltype([evt keyCode])>, std::function<void()>> keys = {
        // alt/option modified arrow key
        {{NSEventModifierFlagOption, NSKEY_LEFT},  []{ gli_input_handle_key(keycode_SkipWordLeft); }},
        {{NSEventModifierFlagOption, NSKEY_RIGHT}, []{ gli_input_handle_key(keycode_SkipWordRight); }},
        {{NSEventModifierFlagOption, NSKEY_DOWN},  []{ gli_input_handle_key(keycode_PageDown); }},
        {{NSEventModifierFlagOption, NSKEY_UP},    []{ gli_input_handle_key(keycode_PageUp); }},

        // command modified arrow key
        {{NSEventModifierFlagCommand, NSKEY_LEFT},  []{ gli_input_handle_key(keycode_Home); }},
        {{NSEventModifierFlagCommand, NSKEY_RIGHT}, []{ gli_input_handle_key(keycode_End); }},
        {{NSEventModifierFlagCommand, NSKEY_DOWN},  []{ gli_input_handle_key(keycode_PageDown); }},
        {{NSEventModifierFlagCommand, NSKEY_UP},    []{ gli_input_handle_key(keycode_PageUp); }},

        // menu commands
        {{NSEventModifierFlagCommand, NSKEY_X}, []{ winclipsend(); }},
        {{NSEventModifierFlagCommand, NSKEY_C}, []{ winclipsend(); }},
        {{NSEventModifierFlagCommand, NSKEY_V}, []{ winclipreceive(); }},

        // info
        {{NSEventModifierFlagCommand, NSKEY_PERIOD}, show_paths},
        {{NSEventModifierFlagShift | NSEventModifierFlagCommand, NSKEY_T}, show_themes},

        // readline/emacs-style controls
        {{NSEventModifierFlagControl, NSKEY_A}, []{ gli_input_handle_key(keycode_Home); }},
        {{NSEventModifierFlagControl, NSKEY_B}, []{ gli_input_handle_key(keycode_Left); }},
        {{NSEventModifierFlagControl, NSKEY_D}, []{ gli_input_handle_key(keycode_Erase); }},
        {{NSEventModifierFlagControl, NSKEY_E}, []{ gli_input_handle_key(keycode_End); }},
        {{NSEventModifierFlagControl, NSKEY_F}, []{ gli_input_handle_key(keycode_Right); }},
        {{NSEventModifierFlagControl, NSKEY_H}, []{ gli_input_handle_key(keycode_Delete); }},
        {{NSEventModifierFlagControl, NSKEY_N}, []{ gli_input_handle_key(keycode_Down); }},
        {{NSEventModifierFlagControl, NSKEY_P}, []{ gli_input_handle_key(keycode_Up); }},
        {{NSEventModifierFlagControl, NSKEY_U}, []{ gli_input_handle_key(keycode_Escape); }},

        // unmodified key for line editing
        {{0, NSKEY_LEFT},  []{ gli_input_handle_key(keycode_Left); }},
        {{0, NSKEY_RIGHT}, []{ gli_input_handle_key(keycode_Right); }},
        {{0, NSKEY_DOWN},  []{ gli_input_handle_key(keycode_Down); }},
        {{0, NSKEY_UP},    []{ gli_input_handle_key(keycode_Up); }},

        // command keys
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

        // save transcript
        {{NSEventModifierFlagShift | NSEventModifierFlagCommand, NSKEY_S}, []{
            auto text = gli_get_scrollback();
            if (text.has_value()) {
                NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

                NSString *fileref = [gargoyle saveWindowDialog: processID
                                                        prompt: @"Save transcript"
                                                        filter: FileFilter::Text];

                if (fileref != nullptr) {
                    std::string filename = [fileref UTF8String];

                    std::ofstream file(filename, std::ios::binary);
                    if (file.is_open()) {
                        if (!file.write(text->data(), text->size())) {
                            show_error(@"Error", "Error writing entire transcript.");
                        }
                    } else {
                        show_error(@"Error", "Unable to open file for writing.");
                    }
                }

                [pool drain];
            } else {
                show_warning(@"Warning", "Could not find appropriate window for scrollback.");
            }
        }},

        // toggle fullscreen
        {{NSEventModifierFlagControl | NSEventModifierFlagCommand, NSKEY_F}, []{
            [gargoyle toggleFullScreen: processID];
        }},
    };

    try {
        keys.at(std::make_pair(modmasked, [evt keyCode]))();
        return;
    } catch (const std::out_of_range &) {
    }

    // send combined keystrokes to text buffer
    [gargoyle setWindow: processID charString: evt];

    // retrieve character from buffer as string
    NSString *evt_char = [gargoyle getWindowCharString: processID];

    // convert character to UTF-32 value
    glui32 ch;
    NSUInteger used;
    if ([evt_char getBytes: &ch maxLength: sizeof ch usedLength: &used
                  encoding: UTF32StringEncoding
                   options: 0
                     range: NSMakeRange(0, [evt_char length])
            remainingRange: nullptr]) {
        if (used != 0) {
            switch (ch) {
            case '\n':
                gli_input_handle_key(keycode_Return);
                break;
            case '\t':
                gli_input_handle_key(keycode_Tab);
                break;
            default:
                gli_input_handle_key(ch);
                break;
            }
        }
    }

    // discard contents of text buffer
    [gargoyle clearWindowCharString: processID];
}

void winmouse(NSEvent *evt)
{
    NSPoint coords = [gargoyle getWindowPoint: processID forEvent: evt];

    int x = coords.x * gli_backingscalefactor;
    int y = gli_image_rgb.height() - (coords.y * gli_backingscalefactor);

    // disregard most events outside of content window
    if ((coords.y < 0 || y < 0 || x < 0 || x > gli_image_rgb.width())
        && ([evt type] != NSEventTypeLeftMouseUp)) {
        return;
    }

    switch ([evt type]) {
    case NSEventTypeLeftMouseDown: {
        gli_input_handle_click(x, y);
        [gargoyle setCursor:kArrowCursor];
        break;
    }

    case NSEventTypeLeftMouseDragged: {
        if (gli_copyselect) {
            [gargoyle setCursor:kIBeamCursor];
            gli_move_selection(x, y);
        }
        break;
    }

    case NSEventTypeMouseMoved: {
        if (gli_get_hyperlink(x, y)) {
            [gargoyle setCursor:kPointingHandCursor];
        } else {
            [gargoyle setCursor:kArrowCursor];
        }
        break;
    }

    case NSEventTypeLeftMouseUp: {
        gli_copyselect = false;
        [gargoyle setCursor:kArrowCursor];
        break;
    }

    case NSEventTypeScrollWheel: {
        if ([evt deltaY] > 0) {
            gli_input_handle_key(keycode_MouseWheelUp);
        } else if ([evt deltaY] < 0) {
            gli_input_handle_key(keycode_MouseWheelDown);
        }
        break;
    }

    default:
        break;
    }
}

void winevent(NSEvent *evt)
{
    if (!evt) {
        gli_event_waiting = false;
        return;
    }

    switch ([evt type]) {
    case NSEventTypeKeyDown: {
        winkey(evt);
        return;
    }

    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeMouseMoved:
    case NSEventTypeScrollWheel: {
        winmouse(evt);
        return;
    }

    case NSEventTypeApplicationDefined: {
        gli_refresh_needed = true;
        winresize();
        return;
    }

    default:
        return;
    }
}

// winloop handles at most one event
void winloop()
{
    NSEvent *evt = nullptr;

    if (gli_refresh_needed) {
        winrefresh();
    }

    if (gli_event_waiting) {
        evt = [gargoyle getWindowEvent:processID];
    }

    winevent(evt);
}

// winpoll handles all queued events
void winpoll()
{
    NSEvent *evt = nullptr;

    do {
        if (gli_refresh_needed) {
            winrefresh();
        }

        if (gli_event_waiting) {
            evt = [gargoyle getWindowEvent: processID];
        }

        winevent(evt);
    } while (evt);
}

nonstd::optional<std::string> garglk::winfontpath(const std::string &filename)
{
    char *resources = std::getenv("GARGLK_RESOURCES");

    if (resources != nullptr) {
        return Format("{}/Fonts/{}", resources, filename);
    }

    return nonstd::nullopt;
}

std::vector<std::string> garglk::winappdata()
{
    NSArray *appdir_paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    char *resources = std::getenv("GARGLK_RESOURCES");
    std::vector<std::string> paths;

    if (resources != nullptr) {
        paths.emplace_back(resources);
    }

    // This is what Qt returns for AppDataLocation (though Qt adds a
    // few more directories that aren't particularly relevant).
    for (NSString *appdir_path in appdir_paths) {
        paths.push_back(Format("{}/{}/{}", [appdir_path UTF8String], GARGOYLE_ORGANIZATION, GARGOYLE_NAME));
    }

    return paths;
}

nonstd::optional<std::string> garglk::winappdir()
{
    // This is only used on Windows.
    return nonstd::nullopt;
}

bool garglk::winisfullscreen()
{
    return [gargoyle isFullScreen: processID];
}

void gli_select(event_t *event, bool polled)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    gli_event_clearevent(event);

    winpoll();
    gli_dispatch_event(event, polled);

    if (event->type == evtype_None && !polled) {
        while (![monitor timeout]) {
            winloop();
            gli_dispatch_event(event, polled);

            if (event->type == evtype_None) {
                [monitor sleep];
            } else {
                break;
            }
        }
    }

    if (event->type == evtype_None && [monitor timeout]) {
        gli_event_store(evtype_Timer, nullptr, 0, 0);
        gli_dispatch_event(event, polled);
        [monitor reset];
    }

    [pool drain];
}
