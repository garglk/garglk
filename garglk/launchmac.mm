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

#include "format.h"

#include "garglk.h"
#include "garversion.h"
#include "launcher.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/glu.h>
#include <libproc.h>
#include <unistd.h>
#import "sysmac.h"

static const char *AppName = "Gargoyle " GARGOYLE_VERSION;

// Qt's origin is the top-left, but Mac's is bottom left, so translate,
// since the config format is determined by Qt.
static int coord_flip(NSWindow *window)
{
    auto screen_height = [NSScreen mainScreen].frame.size.height;
    auto content = [NSWindow contentRectForFrameRect: window.frame styleMask: window.styleMask];
    return screen_height - content.size.height;
}

static const std::unordered_map<FileFilter, std::string> winfilters = {
    {FileFilter::Save, "glksave"},
    {FileFilter::Text, "txt"},
    {FileFilter::Data, "glkdata"},
};

@interface GargoyleView : NSOpenGLView
{
    GLuint output;
    unsigned int textureWidth;
    unsigned int textureHeight;
}

@property (retain) NSColor *backgroundColor;

- (void) addFrame: (NSData *) frame
            width: (unsigned int) width
           height: (unsigned int) height;

- (void) drawRect: (NSRect) bounds;
- (void) reshape;
@end;

@implementation GargoyleView

- (void) addFrame: (NSData *) frame
            width: (unsigned int) width
           height: (unsigned int) height
{
    [[self openGLContext] makeCurrentContext];

    // allocate new texture
    glDeleteTextures(1, &output);
    glGenTextures(1, &output);

    // bind target to texture
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, output);

    // set target parameters
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_CACHED_APPLE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // create texture from data
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,
                 0, GL_RGB8, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE,
                 [frame bytes]);
    textureWidth = width;
    textureHeight = height;
}

- (id) initWithFrame:(NSRect)frameRect pixelFormat:(NSOpenGLPixelFormat *)format
{
    self = [super initWithFrame: frameRect pixelFormat: format];
    if (self) {
        self.backgroundColor = [NSColor colorWithCalibratedRed: 1.0f green: 1.0f blue: 1.0f alpha: 1.0f];
    }
    return self;
}

- (void) drawRect: (NSRect) bounds
{
    if (textureHeight == 0) {
        return;
    }

    [[self openGLContext] makeCurrentContext];

    float viewScalingFactor = [self.window backingScaleFactor] / BACKING_SCALE_FACTOR;
    glViewport(0.0, 0.0, textureWidth * viewScalingFactor, textureHeight * viewScalingFactor);

    NSColor *clearColor = self.backgroundColor;
    glClearColor([clearColor redComponent], [clearColor greenComponent], [clearColor blueComponent], [clearColor alphaComponent]);
    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_QUADS);
    {
        glTexCoord2f(0.0f, textureHeight);
        glVertex2f(-1.0f, -1.0f);

        glTexCoord2f(textureWidth, textureHeight);
        glVertex2f(1.0f, -1.0f);

        glTexCoord2f(textureWidth, 0);
        glVertex2f(1.0f, 1.0f);

        glTexCoord2f(0.0f, 0);
        glVertex2f(-1.0f, 1.0f);
    }
    glEnd();

    glFlush();
}

- (void)reshape
{
}

@end

@interface GargoyleWindow : NSWindow
{
    NSMutableArray *eventlog;
    NSTextView *textbuffer;
    pid_t processID;
    NSTimeInterval lastMouseMove;
}
- (id) initWithContentRect: (NSRect) contentRect
                 styleMask: (unsigned int) windowStyle
                   backing: (NSBackingStoreType) bufferingType
                     defer: (BOOL) deferCreation
                   process: (pid_t) pid
           backgroundColor: (NSColor *) backgroundColor;
- (void) sendEvent: (NSEvent *) event;
- (NSEvent *) retrieveEvent;
- (void) sendChars: (NSEvent *) event;
- (NSString *) retrieveChars;
- (void) clearChars;
- (IBAction) cut: (id) sender;
- (IBAction) copy: (id) sender;
- (IBAction) paste: (id) sender;
- (void) moveWordBackward: (id) sender;
- (void) moveWordForward: (id) sender;
- (IBAction) performZoom: (id) sender;
- (void) saveWindows;
- (void) performRefresh: (NSNotification *) notice;
- (void) performMove: (NSNotification *) notice;
- (NSString *) openFileDialog: (NSString *) prompt
                   fileFilter: (FileFilter) filter
                      savedir: (NSString *) savedir;
- (NSString *) saveFileDialog: (NSString *) prompt
                   fileFilter: (FileFilter) filter
                      savedir: (NSString *) savedir;
- (pid_t) retrieveID;
- (void) quit;
@end

@implementation GargoyleWindow

- (id) initWithContentRect: (NSRect) contentRect
                 styleMask: (unsigned int) windowStyle
                   backing: (NSBackingStoreType) bufferingType
                     defer: (BOOL) deferCreation
                   process: (pid_t) pid
           backgroundColor: (NSColor *) backgroundColor
{
    self = [super initWithContentRect: contentRect
                            styleMask: windowStyle
                              backing: bufferingType
                                defer: deferCreation];

    GargoyleView *view = [[GargoyleView alloc] initWithFrame: contentRect pixelFormat: [GargoyleView defaultPixelFormat]];
    [view setWantsBestResolutionOpenGLSurface:YES];
    view.backgroundColor = backgroundColor;
    [self setContentView: view];

    eventlog = [[NSMutableArray alloc] initWithCapacity: 100];
    textbuffer = [[NSTextView alloc] init];
    processID = pid;

    [self setAcceptsMouseMovedEvents: YES];
    lastMouseMove = 0;

    std::vector<NSString *> names = {
        NSWindowDidDeminiaturizeNotification,
        NSWindowDidChangeScreenNotification,
        NSWindowDidEndLiveResizeNotification,
    };

    for (const auto &name : names) {
        [[NSNotificationCenter defaultCenter] addObserver: self
                                                 selector: @selector(performRefresh:)
                                                     name: name
                                                   object: self];
    }

    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(performRefresh:)
                                                 name: NSWindowDidMoveNotification
                                               object: self];

    return self;
}

static BOOL isTextbufferEvent(NSEvent *evt)
{
    if ([evt type] != NSKeyDown) {
        return NO;
    }

    // The following mirrors the checks in sysmac.m:winkey().

    // check for arrow keys
    if ([evt modifierFlags] & NSFunctionKeyMask) {
        // alt/option modified key
        if ([evt modifierFlags] & NSAlternateKeyMask) {
            switch ([evt keyCode]) {
            case NSKEY_LEFT:
            case NSKEY_RIGHT:
            case NSKEY_DOWN:
            case NSKEY_UP:
                return NO;
            default:
                break;
            }
        }

        // command modified key
        if ([evt modifierFlags] & NSCommandKeyMask) {
            switch ([evt keyCode]) {
            case NSKEY_LEFT:
            case NSKEY_RIGHT:
            case NSKEY_DOWN:
            case NSKEY_UP:
                return NO;
            default:
                break;
            }
        }

        // unmodified key for line editing
        switch ([evt keyCode]) {
        case NSKEY_LEFT:
        case NSKEY_RIGHT:
        case NSKEY_DOWN:
        case NSKEY_UP:
            return NO;
        default:
            break;
        }
    }

    // check for menu commands
    if ([evt modifierFlags] & NSCommandKeyMask) {
        switch ([evt keyCode]) {
        case NSKEY_X:
        case NSKEY_C:
        case NSKEY_V:
            return NO;
        default:
            break;
        }
    }

    // check for command keys
    switch ([evt keyCode]) {
    case NSKEY_PGUP:
    case NSKEY_PGDN:
    case NSKEY_HOME:
    case NSKEY_END:
    case NSKEY_DEL:
    case NSKEY_BACK:
    case NSKEY_ESC:
    case NSKEY_F1:
    case NSKEY_F2:
    case NSKEY_F3:
    case NSKEY_F4:
    case NSKEY_F5:
    case NSKEY_F6:
    case NSKEY_F7:
    case NSKEY_F8:
    case NSKEY_F9:
    case NSKEY_F10:
    case NSKEY_F11:
    case NSKEY_F12:
        return NO;
    default:
        break;
    }

    return YES;
}

- (void) sendEvent: (NSEvent *) event
{
    int store = NO;
    int forward = YES;

    switch ([event type]) {
    case NSKeyDown:
    case NSApplicationDefined:
        store = YES;
        forward = NO;
        break;

    case NSLeftMouseDown:
    case NSLeftMouseUp:
        store = YES;
        break;

    case NSLeftMouseDragged:
    case NSMouseMoved:
        if (([event timestamp] - lastMouseMove) > 0.05) {
            lastMouseMove = [event timestamp];
            store = YES;
        }
        break;

    case NSScrollWheel:
        if ([event deltaY] > 0.05 || [event deltaY] < -0.05) {
            store = YES;
        }

        break;

    default:
        break;
    }

    if (store) {
        [eventlog insertObject: [event copy] atIndex: 0];
        if (isTextbufferEvent(event)) {
            [textbuffer interpretKeyEvents: [NSArray arrayWithObject: event]];
        }
        kill(processID, SIGUSR1);
    }

    if (forward) {
        [super sendEvent: event];
    }

    return;
}

- (NSEvent *) retrieveEvent
{
    NSEvent *event = nullptr;

    if ([eventlog count]) {
        event = [eventlog lastObject];
        [eventlog removeLastObject];
    }

    return event;
}

- (void) sendChars: (NSEvent *) event
{
    // Does not work on macOS 10.12 (Sierra)
    //[textbuffer interpretKeyEvents: [NSArray arrayWithObject:event]];
}

- (NSString *) retrieveChars
{
    return [[textbuffer textStorage] string];
}

- (void) clearChars
{
    [[[textbuffer textStorage] mutableString] setString: @""];
}

- (IBAction) cut: (id) sender
{
    [self sendEvent: [NSEvent keyEventWithType: NSKeyDown
                                      location: NSZeroPoint
                                 modifierFlags: NSCommandKeyMask
                                     timestamp: NSTimeIntervalSince1970
                                  windowNumber: [self windowNumber]
                                       context: [self graphicsContext]
                                    characters: @"X"
                   charactersIgnoringModifiers: @"X"
                                     isARepeat: NO
                                       keyCode: NSKEY_X]];
}

- (IBAction) copy: (id) sender
{
    [self sendEvent: [NSEvent keyEventWithType: NSKeyDown
                                      location: NSZeroPoint
                                 modifierFlags: NSCommandKeyMask
                                     timestamp: NSTimeIntervalSince1970
                                  windowNumber: [self windowNumber]
                                       context: [self graphicsContext]
                                    characters: @"C"
                   charactersIgnoringModifiers: @"C"
                                     isARepeat: NO
                                       keyCode: NSKEY_C]];
}

- (IBAction) paste: (id) sender
{
    [self sendEvent: [NSEvent keyEventWithType: NSKeyDown
                                      location: NSZeroPoint
                                 modifierFlags: NSCommandKeyMask
                                     timestamp: NSTimeIntervalSince1970
                                  windowNumber: [self windowNumber]
                                       context: [self graphicsContext]
                                    characters: @"V"
                   charactersIgnoringModifiers: @"V"
                                     isARepeat: NO
                                       keyCode: NSKEY_V]];
}

- (void) moveWordBackward: (id) sender
{
    [self sendEvent: [NSEvent keyEventWithType: NSKeyDown
                                      location: NSZeroPoint
                                 modifierFlags: NSAlternateKeyMask
                                     timestamp: NSTimeIntervalSince1970
                                  windowNumber: [self windowNumber]
                                       context: [self graphicsContext]
                                    characters: @""
                   charactersIgnoringModifiers: @""
                                     isARepeat: NO
                                       keyCode: NSKEY_LEFT]];
}

- (void) moveWordForward: (id) sender
{
    [self sendEvent: [NSEvent keyEventWithType: NSKeyDown
                                      location: NSZeroPoint
                                 modifierFlags: NSAlternateKeyMask
                                     timestamp: NSTimeIntervalSince1970
                                  windowNumber: [self windowNumber]
                                       context: [self graphicsContext]
                                    characters: @""
                   charactersIgnoringModifiers: @""
                                     isARepeat: NO
                                       keyCode: NSKEY_RIGHT]];
}

- (IBAction) performZoom: (id) sender
{
    [self sendEvent: [NSEvent otherEventWithType: NSApplicationDefined
                                        location: NSZeroPoint
                                   modifierFlags: 0
                                       timestamp: NSTimeIntervalSince1970
                                    windowNumber: [self windowNumber]
                                         context: [self graphicsContext]
                                         subtype: 0
                                           data1: 0
                                           data2: 0]];
    [super performZoom: sender];
}

- (void) saveWindows
{
    if (gli_conf_save_window_location || gli_conf_save_window_size) {
        auto path = get_qt_plist_path();
        if (path != nil) {
            NSMutableDictionary *config = [NSMutableDictionary dictionaryWithContentsOfFile: path];
            if (config == nil) {
                config = [NSMutableDictionary dictionary];
            }

            if (gli_conf_save_window_location) {
                auto location = Format("@Point({} {})", self.frame.origin.x, coord_flip(self) - self.frame.origin.y);
                [config setObject: [NSString stringWithUTF8String: location.c_str()] forKey: @"window.position"];
            }

            if (gli_conf_save_window_size) {
                auto size = Format("@Size({} {})", self.frame.size.width, self.frame.size.height);
                [config setObject: [NSString stringWithUTF8String: size.c_str()] forKey: @"window.size"];
            }

            if (gli_conf_save_window_location || gli_conf_save_window_size) {
                auto is_fullscreen = (self.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen;
                auto as_object = [NSNumber numberWithBool: is_fullscreen];
                [config setObject: as_object forKey: @"window.fullscreen"];
            }

            [config writeToFile: path atomically: YES];
        }
    }
}

- (void) performRefresh: (NSNotification *) notice
{
    [self saveWindows];

    [self sendEvent: [NSEvent otherEventWithType: NSApplicationDefined
                                        location: NSZeroPoint
                                   modifierFlags: 0
                                       timestamp: NSTimeIntervalSince1970
                                    windowNumber: [self windowNumber]
                                         context: [self graphicsContext]
                                         subtype: 0
                                           data1: 0
                                           data2: 0]];
}

- (void) performMove: (NSNotification *) notice
{
    [self saveWindows];
}

static NSURL *get_save_dir(NSString *filename)
{

    NSError *error = nil;
    NSURL *dir = [[NSFileManager defaultManager] URLForDirectory: NSApplicationSupportDirectory
                                                        inDomain: NSUserDomainMask
                                               appropriateForURL: nil
                                                          create: YES
                                                           error: &error];

    if (error != nil) {
        return nil;
    }

    dir = [dir URLByAppendingPathComponent: @"Gargoyle/gamedata"];
    dir = [dir URLByAppendingPathComponent: filename];
    NSFileManager *fm = [NSFileManager defaultManager];
    [fm createDirectoryAtURL: dir withIntermediateDirectories: YES attributes: nil error: &error];

    if (error == nil) {
        return dir;
    } else {
        return nil;
    }
}

static void maybe_set_save_dir(NSSavePanel *panel, NSString *savedir)
{
    if (savedir == nil) {
        return;
    }

    auto initial_directory = get_save_dir(savedir);

    if (initial_directory != nil) {
        NSURL *startingDirectoryURL = [NSURL fileURLWithPath: [initial_directory path]];
        [panel setDirectoryURL: startingDirectoryURL];
    }
}

- (NSString *) openFileDialog: (NSString *) prompt
                   fileFilter: (FileFilter) filter
                      savedir: (NSString *) savedir
{
    int result;

    NSOpenPanel *openDlg = [NSOpenPanel openPanel];

    [openDlg setCanChooseFiles: YES];
    [openDlg setCanChooseDirectories: NO];
    [openDlg setAllowsMultipleSelection: NO];
    [openDlg setTitle: prompt];

    maybe_set_save_dir(openDlg, savedir);

    if (filter != FileFilter::Data) {
        NSArray *filterTypes = [NSArray arrayWithObject: [NSString stringWithCString: winfilters.at(filter).c_str()
                                                                            encoding: NSUTF8StringEncoding]];
        [openDlg setAllowedFileTypes: filterTypes];
        [openDlg setAllowsOtherFileTypes: NO];
    }
    result = [openDlg runModal];

    if (result == NSFileHandlingPanelOKButton) {
        return [[openDlg URL] path];
    }

    return nil;
}

- (NSString *) saveFileDialog: (NSString *) prompt
                   fileFilter: (FileFilter) filter
                      savedir: (NSString *) savedir
{
    int result;

    NSSavePanel *saveDlg = [NSSavePanel savePanel];

    [saveDlg setCanCreateDirectories: YES];
    [saveDlg setTitle: prompt];

    maybe_set_save_dir(saveDlg, savedir);

    if (filter != FileFilter::Data) {
        NSArray *filterTypes = [NSArray arrayWithObject: [NSString stringWithCString: winfilters.at(filter).c_str()
                                                                            encoding: NSUTF8StringEncoding]];
        [saveDlg setAllowedFileTypes: filterTypes];
        [saveDlg setAllowsOtherFileTypes: NO];
    }

    result = [saveDlg runModal];

    if (result == NSFileHandlingPanelOKButton) {
        return [[saveDlg URL] path];
    }

    return nil;
}

- (pid_t) retrieveID
{
    return processID;
}

- (void) quit
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [eventlog release];
    [textbuffer release];

    // shut interpreter down
    kill(processID, SIGUSR2);
}

@end

@interface GargoyleApp : NSWindowController
                         <GargoyleApp, NSApplicationDelegate, NSWindowDelegate>
{
    BOOL openedFirstGame;
    NSMutableDictionary *windows;
    NSConnection *link;
}
- (BOOL) launchFile: (NSString *) file;
- (BOOL) launchFileDialog;
@end

@implementation GargoyleApp

- (id) init
{
    self = [super init];

    // set internal variables
    openedFirstGame = NO;
    windows = [[NSMutableDictionary alloc] init];

    // set up controller link
    NSPort *port = [NSMachPort port];
    link = [[NSConnection connectionWithReceivePort: port sendPort: port] retain];
    [link setRootObject: self];
    [link addRunLoop: [NSRunLoop currentRunLoop]];
    [link registerName: [NSString stringWithFormat: @"com.googlecode.garglk-%04x", getpid()]];
    [link retain];

    // set environment variable
    NSString *nsResources = [[NSBundle mainBundle] resourcePath];

    auto etc = [nsResources UTF8String];
    setenv("GARGLK_RESOURCES", etc, true);

    // set preference defaults
    [[NSUserDefaults standardUserDefaults] registerDefaults:
     [NSDictionary dictionaryWithObject: [NSNumber numberWithBool: YES]
                                 forKey: @"NSDisabledCharacterPaletteMenuItem"]];

    // register for URL events
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler: self
                                                       andSelector: @selector(openURL:withReplyEvent:)
                                                     forEventClass: kInternetEventClass
                                                        andEventID: kAEGetURL];

    return self;
}

- (BOOL) initWindow: (pid_t) processID
               move: (BOOL) move
                  x: (unsigned int) x
                  y: (unsigned int) y
              width: (unsigned int) width
             height: (unsigned int) height
         fullscreen: (BOOL) fullscreen
    backgroundColor: (NSColor *) backgroundColor
{
    if (!(processID > 0)) {
        return NO;
    }

    unsigned int style = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask;

    // set up the window
    NSRect rect = NSMakeRect(0, 0, width, height);
    GargoyleWindow *window = [[GargoyleWindow alloc] initWithContentRect: rect
                                                               styleMask: style
                                                                 backing: NSBackingStoreBuffered
                                                                   defer: NO
                                                                 process: processID
                                                         backgroundColor: backgroundColor];

    [window setMinSize:NSMakeSize(gli_wmarginx * 2, gli_wmarginy * 2)];

    [window makeKeyAndOrderFront: window];
    if (move) {
        rect = NSMakeRect(x, coord_flip(window) - y, width, height);
        [window setFrame: rect display: YES];
    } else {
        [window center];
    }
    [window setReleasedWhenClosed: YES];
    [window setDelegate: self];
    if (fullscreen) {
        [window toggleFullScreen: self];
    }

    [windows setObject: window forKey: [NSNumber numberWithInt: processID]];

    return ([window isVisible]);
}

- (NSEvent *) getWindowEvent: (pid_t) processID
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window) {
        return [window retrieveEvent];
    }

    return nil;
}

- (NSRect) updateBackingSize: (pid_t) processID
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window) {
        id view = [window contentView];
        NSRect viewRect = [view bounds];
        NSRect backingRect = [window convertRectToBacking: viewRect];
        return backingRect;
    }

    return NSZeroRect;
}

- (CGFloat) getBackingScaleFactor: (pid_t) processID
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window) {
        id view = [window contentView];
        return [view backingScaleFactor];
    }

    return 1.0;
}

- (NSPoint) getWindowPoint: (pid_t) processID
                  forEvent: (NSEvent *) event;
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window) {
        NSPoint point = [event locationInWindow];
        return point;
    }

    return NSZeroPoint;
}

- (NSString *) getWindowCharString: (pid_t) processID
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window) {
        return [window retrieveChars];
    }

    return nil;
}

- (BOOL) clearWindowCharString: (pid_t) processID
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window) {
        [window clearChars];
        return YES;
    }

    return NO;
}

- (BOOL) setWindow: (pid_t) processID
        charString: (NSEvent *) event;
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window) {
        [window sendChars: event];
        return YES;
    }

    return NO;
}

- (BOOL) setWindow: (pid_t) processID
             title: (NSString *) title;
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window) {
        [window setTitle: title];
        return YES;
    }

    return NO;
}

- (BOOL) setWindow: (pid_t) processID
          contents: (NSData *) frame
             width: (unsigned int) width
            height: (unsigned int) height;
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window) {
        [[window contentView] addFrame: frame
                                 width: width
                                height: height];

        NSRect rect = NSMakeRect(0, 0, width, height);
        if ([[window contentView] wantsBestResolutionOpenGLSurface]) {
            rect = [[window contentView] convertRectFromBacking: rect];
        }
        [[window contentView] drawRect: rect];
        return YES;
    }

    return NO;
}

- (void) closeWindow: (pid_t) processID
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window) {
        [window performClose: self];
    }
}

- (NSString *) openWindowDialog: (pid_t) processID
                         prompt: (NSString *) prompt
                         filter: (FileFilter) filter
                        savedir: (NSString *) savedir
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window) {
        return [window openFileDialog: prompt fileFilter: filter savedir: savedir];
    }

    return nil;
}

- (NSString *) saveWindowDialog: (pid_t) processID
                         prompt: (NSString *) prompt
                         filter: (FileFilter) filter
                        savedir: (NSString *) savedir
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window) {
        return [window saveFileDialog: prompt fileFilter: filter savedir: savedir];
    }

    return nil;
}

- (void) abortWindowDialog: (pid_t) processID
                    prompt: (NSString *) prompt
{
    NSRunAlertPanel(@"Fatal error", @"%@", nil, nil, nil, prompt);
}

- (BOOL) isFullScreen: (pid_t) processID
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];
    return (window.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen;
}

- (void) toggleFullScreen: (pid_t) processID
{
    GargoyleWindow *window = [windows objectForKey: [NSNumber numberWithInt: processID]];
    [window toggleFullScreen: self];
}

#define kArrowCursor        1
#define kIBeamCursor        2
#define kPointingHandCursor 3

- (void) setCursor: (unsigned int) cursor
{
    switch (cursor) {
    case kArrowCursor:
        [[NSCursor arrowCursor] set];
        break;

    case kIBeamCursor:
        [[NSCursor IBeamCursor] set];
        break;

    case kPointingHandCursor:
        [[NSCursor pointingHandCursor] set];
        break;

    default:
        break;
    }
}

- (BOOL) launchFileDialog
{
    int result;

    NSOpenPanel *openDlg = [NSOpenPanel openPanel];

    [openDlg setCanChooseFiles: YES];
    [openDlg setCanChooseDirectories: NO];
    [openDlg setAllowsMultipleSelection: NO];
    [openDlg setTitle: [NSString stringWithCString: AppName encoding: NSUTF8StringEncoding]];

    NSMutableArray *filterTypes = [NSMutableArray arrayWithCapacity:100];

    NSEnumerator *docTypeEnum = [[[[NSBundle mainBundle] infoDictionary]
                                  objectForKey:@"CFBundleDocumentTypes"] objectEnumerator];
    NSDictionary *docType;

    while (docType = [docTypeEnum nextObject]) {
        [filterTypes addObjectsFromArray:[docType objectForKey: @"CFBundleTypeExtensions"]];
    }

    // Some advsys games have a .dat extension, but since that's so
    // generic, it's not included in the .plist file, so it won't be
    // registered system-wide for Gargoyle. Similarly, some Level9 games
    // have a .sna (memory snapshot) extension, which is not exclusive
    // to Level9 games, so it's also not in the .plist file. Add both
    // of them here so the file picker can select them, at least.
    [filterTypes addObject: @"dat"];
    [filterTypes addObject: @"sna"];

    [openDlg setAllowedFileTypes: filterTypes];
    [openDlg setAllowsOtherFileTypes: NO];
    result = [openDlg runModal];

    if (result == NSFileHandlingPanelOKButton) {
        return [self launchFile: [[openDlg URL] path]];
    }

    return NO;
}

- (BOOL) launchFile: (NSString *) file
{
    if (![file length]) {
        return NO;
    }

    // get story file
    auto game = [file UTF8String];

    // run story file
    bool ran = garglk::rungame(game);

    if (ran) {
        [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL: [NSURL fileURLWithPath: file]];
    }

    return ran;
}

- (BOOL) applicationShouldOpenUntitledFile: (NSApplication *) sender
{
    return (!openedFirstGame);
}

- (BOOL) applicationOpenUntitledFile: (NSApplication *) theApplication
{
    openedFirstGame = YES;
    return [self launchFileDialog];
}

- (BOOL) application: (NSApplication *) theApplication openFile: (NSString *) file
{
    openedFirstGame = YES;
    return [self launchFile:file];
}

- (void) application: (NSApplication *) theApplication openFiles: (NSArray *) files
{
    openedFirstGame = YES;

    int i;

    for (i = 0; i < [files count]; i++) {
        [self launchFile: [files objectAtIndex: i]];
    }
}

- (void) openURL: (NSAppleEventDescriptor *) event withReplyEvent: (NSAppleEventDescriptor *) reply
{
    NSArray *urlParts = [[[event paramDescriptorForKeyword: keyDirectObject] stringValue] componentsSeparatedByString: @"garglk://"];

    if (urlParts && [urlParts count] == 2) {
        openedFirstGame = YES;
        NSString *game = [[urlParts objectAtIndex: 1] stringByReplacingPercentEscapesUsingEncoding: NSUTF8StringEncoding];

        if ([[NSFileManager defaultManager] fileExistsAtPath: game] == YES) {
            [self launchFile: game];
        } else {
            NSRunAlertPanel(@"Could not open URL path:", @"%@", nil, nil, nil, game);
        }
    }
}

- (BOOL) windowShouldClose: (id) sender
{
    GargoyleWindow *window = sender;
    [windows removeObjectForKey: [NSNumber numberWithInt: [window retrieveID]]];
    [window quit];
    return YES;
}

- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *) sender
{
    for (id key in windows) {
        GargoyleWindow *window = [windows objectForKey: key];
        [window quit];
    }

    return NSTerminateNow;
}


- (IBAction) orderFrontStandardAboutPanel: (id) sender
{
}

- (IBAction) open: (id) sender
{
    [self launchFileDialog];
}

- (IBAction) toggle: (id) sender
{
    try {
        auto config = [NSString stringWithUTF8String: garglk::user_config().c_str()];
        NSTask *task = [[NSTask alloc] init];
        task.launchPath = @"/usr/bin/open";
        task.arguments = @[@"-t", config];
        [task launch];
    } catch (const std::runtime_error &e) {
        garglk::winwarning("Warning", e.what());
    }
}

@end

void garglk::winmsg(const std::string &msg)
{
    NSString *nsMsg;
    nsMsg = [NSString stringWithCString: msg.c_str() encoding: NSUTF8StringEncoding];
    NSRunAlertPanel(@"Fatal error", @"%@", nil, nil, nil, nsMsg);
}

static std::string winpath()
{
    char exepath[PROC_PIDPATHINFO_MAXSIZE];

    if (proc_pidpath(getpid(), exepath, sizeof exepath) == -1) {
        garglk::winmsg("Unable to locate executable path");
    }

    std::string buffer = exepath;

    auto slash = buffer.find_last_of('/');
    if (slash != std::string::npos) {
        buffer.erase(slash);
    }

    return buffer;
}

static int winexec(const std::string &cmd, const std::vector<std::string> &args)
{
    NSTask *proc = [[NSTask alloc] init];

    // prepare interpreter path
    NSArray *nsArray = [[NSString stringWithCString: cmd.c_str() encoding: NSUTF8StringEncoding] componentsSeparatedByString: @"/"];
    NSString *nsTerp = [nsArray objectAtIndex: [nsArray count] - 1];
    NSString *nsCmd = [NSString stringWithFormat: @"%@/%@", [[NSBundle mainBundle] builtInPlugInsPath], nsTerp];

    // prepare interpreter arguments
    NSMutableArray *nsArgs = [NSMutableArray arrayWithCapacity:2];

    // prepare environment
    NSMutableDictionary *nsEnv = [[[NSProcessInfo processInfo] environment] mutableCopy];
    [nsEnv setObject: [NSString stringWithFormat: @"com.googlecode.garglk-%04x", getpid()] forKey: @"GargoyleApp"];

    for (const auto &arg : args) {
        [nsArgs addObject: [[NSString alloc] initWithCString: arg.c_str() encoding: NSUTF8StringEncoding]];
    }

    if ([nsCmd length] && [nsArgs count]) {
        [proc setLaunchPath: nsCmd];
        [proc setArguments: nsArgs];
        [proc setEnvironment: nsEnv];
        [proc launch];
    }

    return [proc isRunning];
}

bool garglk::winterp(const std::string &exe, const std::vector<std::string> &flags, const std::string &game)
{
    // get dir of executable
    auto interpreter_dir = winpath();

    auto cmd = Format("{}/{}", interpreter_dir, exe);

    auto args = flags;

    args.push_back(game);

    if (!winexec(cmd, args)) {
        garglk::winmsg("Could not start 'terp.\nSorry.");
        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    [NSApp setDelegate:[[GargoyleApp alloc] init]];
    [pool drain];

    // Read the config file here so that the "save_window" settings are available.
    gli_read_config(argc, argv);

    return NSApplicationMain(argc, const_cast<const char **>(argv));
}
