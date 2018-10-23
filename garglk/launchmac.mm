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

#include "glk.h"
#include "garglk.h"
#include "garversion.h"
#include "launcher.h"

#include <string>

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/glu.h>
#include <mach-o/dyld.h>
#import "sysmac.h"

#ifdef __ppc__
#define ByteOrderOGL GL_UNSIGNED_INT_8_8_8_8
#else
#define ByteOrderOGL GL_UNSIGNED_INT_8_8_8_8_REV
#endif

#define MaxBuffer 1024

static const char * AppName = "Gargoyle " VERSION;
static const char * DirSeparator = "/";

static char dir[MaxBuffer];
static char buf[MaxBuffer];
static char tmp[MaxBuffer];
static char etc[MaxBuffer];

static void winpath(char *buffer);

static const char *winfilters[] =
{
    "glksave",
    "txt",
    "glkdata",
};

@interface GargoyleView : NSOpenGLView
{
    GLuint output;
}
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

    /* allocate new texture */
    glDeleteTextures(1, &output);
    glGenTextures(1, &output);

    /* bind target to texture */
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, output);

    /* set target parameters */
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_CACHED_APPLE);
    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);

    /* create texture from data */
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,
                 0, GL_RGBA, width, height, 0, GL_BGRA,
                 ByteOrderOGL,
                 [frame bytes]);
}

- (void) drawRect: (NSRect) bounds
{
    [[self openGLContext] makeCurrentContext];

    float width = bounds.size.width;
    float height = bounds.size.height;

    glBegin(GL_QUADS);
    {
        glTexCoord2f(0.0f, height);
        glVertex2f(-1.0f, -1.0f);

        glTexCoord2f(width, height);
        glVertex2f(1.0f, -1.0f);

        glTexCoord2f(width, 0.0f);
        glVertex2f(1.0f, 1.0f);

        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-1.0f, 1.0f);
    }
    glEnd();

    glFlush();
}

- (void)reshape
{
    [[self openGLContext] makeCurrentContext];
    NSRect rect = [self bounds];
    if ([self wantsBestResolutionOpenGLSurface])
        rect = [self convertRectToBacking: rect];
    glViewport(0.0, 0.0, NSWidth(rect), NSHeight(rect));
}

@end

@interface GargoyleWindow : NSWindow
{
    NSMutableArray * eventlog;
    NSTextView * textbuffer;
    pid_t processID;
    NSTimeInterval lastMouseMove;
}
- (id) initWithContentRect: (NSRect) contentRect
                 styleMask: (unsigned int) windowStyle
                   backing: (NSBackingStoreType) bufferingType
                     defer: (BOOL) deferCreation
                   process: (pid_t) pid
                    retina: (int) retina;
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
- (void) performRefresh: (NSNotification *) notice;
- (NSString *) openFileDialog: (NSString *) prompt
                   fileFilter: (enum FILEFILTERS) filter;
- (NSString *) saveFileDialog: (NSString *) prompt
                   fileFilter: (enum FILEFILTERS) filter;
- (pid_t) retrieveID;
- (void) quit;
@end

@implementation GargoyleWindow

- (id) initWithContentRect: (NSRect) contentRect
                 styleMask: (unsigned int) windowStyle
                   backing: (NSBackingStoreType) bufferingType
                     defer: (BOOL) deferCreation
                   process: (pid_t) pid
                    retina: (int) retina
{
    self = [super initWithContentRect: contentRect
                            styleMask: windowStyle
                              backing: bufferingType
                                defer: deferCreation];

    GargoyleView * view = [[GargoyleView alloc] initWithFrame: contentRect pixelFormat: [GargoyleView defaultPixelFormat]];
    if (retina)
        [view setWantsBestResolutionOpenGLSurface:YES];
    [self setContentView: view];

    eventlog = [[NSMutableArray alloc] initWithCapacity: 100];
    textbuffer = [[NSTextView alloc] init];
    processID = pid;

    [self setAcceptsMouseMovedEvents: YES];
    lastMouseMove = 0;

    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(performRefresh:)
                                                 name: NSWindowDidDeminiaturizeNotification
                                               object: self];

    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(performRefresh:)
                                                 name: NSWindowDidChangeScreenNotification
                                               object: self];

    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(performRefresh:)
                                                 name: NSWindowDidResizeNotification
                                               object: self];

    return self;
}

static BOOL isTextbufferEvent(NSEvent * evt)
{
    if ([evt type] != NSKeyDown)
    {
        return NO;
    }

    /*
     * The following mirrors the checks in sysmac.m:winkey().
     */

    /* check for arrow keys */
    if ([evt modifierFlags] & NSFunctionKeyMask)
    {
        /* alt/option modified key */
        if ([evt modifierFlags] & NSAlternateKeyMask)
        {
            switch ([evt keyCode])
            {
                case NSKEY_LEFT  : return NO;
                case NSKEY_RIGHT : return NO;
                case NSKEY_DOWN  : return NO;
                case NSKEY_UP    : return NO;
                default: break;
            }
        }

        /* command modified key */
        if ([evt modifierFlags] & NSCommandKeyMask)
        {
            switch ([evt keyCode])
            {
                case NSKEY_LEFT  : return NO;
                case NSKEY_RIGHT : return NO;
                case NSKEY_DOWN  : return NO;
                case NSKEY_UP    : return NO;
                default: break;
            }
        }

        /* unmodified key for line editing */
        switch ([evt keyCode])
        {
            case NSKEY_LEFT  : return NO;
            case NSKEY_RIGHT : return NO;
            case NSKEY_DOWN  : return NO;
            case NSKEY_UP    : return NO;
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
                return NO;

            case NSKEY_V:
                return NO;

            default: break;
        }
    }

    /* check for command keys */
    switch ([evt keyCode])
    {
        case NSKEY_PGUP : return NO;
        case NSKEY_PGDN : return NO;
        case NSKEY_HOME : return NO;
        case NSKEY_END  : return NO;
        case NSKEY_DEL  : return NO;
        case NSKEY_BACK : return NO;
        case NSKEY_ESC  : return NO;
        case NSKEY_F1   : return NO;
        case NSKEY_F2   : return NO;
        case NSKEY_F3   : return NO;
        case NSKEY_F4   : return NO;
        case NSKEY_F5   : return NO;
        case NSKEY_F6   : return NO;
        case NSKEY_F7   : return NO;
        case NSKEY_F8   : return NO;
        case NSKEY_F9   : return NO;
        case NSKEY_F10  : return NO;
        case NSKEY_F11  : return NO;
        case NSKEY_F12  : return NO;
        default: break;
    }

    return YES;
}

- (void) sendEvent: (NSEvent *) event
{
    int store = NO;
    int forward = YES;

    switch ([event type])
    {
        case NSKeyDown:
        case NSApplicationDefined:
        {
            store = YES;
            forward = NO;
            break;
        }

        case NSLeftMouseDown:
        case NSLeftMouseUp:
        {
            store = YES;
            break;
        }

        case NSLeftMouseDragged:
        case NSMouseMoved:
        {
            if (([event timestamp] - lastMouseMove) > 0.05)
            {
                lastMouseMove = [event timestamp];
                store = YES;
            }
            break;
        }

        case NSScrollWheel:
        {
            if ([event deltaY] > 0.05 || [event deltaY] < -0.05)
                store = YES;

            break;
        }

        default: break;
    }

    if (store)
    {
        [eventlog insertObject: [event copy] atIndex:0];
        if (isTextbufferEvent(event))
        {
            [textbuffer interpretKeyEvents: [NSArray arrayWithObject: event]];
        }
        kill(processID, SIGUSR1);
    }

    if (forward)
    {
        [super sendEvent: event];
    }

    return;
}

- (NSEvent *) retrieveEvent
{
    NSEvent * event = NULL;

    if ([eventlog count])
    {
        event = [eventlog lastObject];
        [eventlog removeLastObject];
    }

    return event;
}

- (void) sendChars: (NSEvent *) event
{
    /* Does not work on macOS 10.12 (Sierra) */
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

- (void) performRefresh: (NSNotification *) notice
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
}

- (NSString *) openFileDialog: (NSString *) prompt
                   fileFilter: (enum FILEFILTERS) filter
{
    int result;

    NSOpenPanel * openDlg = [NSOpenPanel openPanel];

    [openDlg setCanChooseFiles: YES];
    [openDlg setCanChooseDirectories: NO];
    [openDlg setAllowsMultipleSelection: NO];
    [openDlg setTitle: prompt];

    if (filter != FILTER_DATA)
    {
        NSArray * filterTypes = [NSArray arrayWithObject: [NSString stringWithCString: winfilters[filter]
                                                                             encoding: NSUTF8StringEncoding]];
        [openDlg setAllowedFileTypes: filterTypes];
        [openDlg setAllowsOtherFileTypes: NO];
    }
    result = [openDlg runModal];

    if (result == NSFileHandlingPanelOKButton)
        return [[openDlg URL] path];

    return nil;
}

- (NSString *) saveFileDialog: (NSString *) prompt
                   fileFilter: (enum FILEFILTERS) filter
{
    int result;

    NSSavePanel * saveDlg = [NSSavePanel savePanel];

    [saveDlg setCanCreateDirectories: YES];
    [saveDlg setTitle: prompt];

    if (filter != FILTER_DATA)
    {
        NSArray * filterTypes = [NSArray arrayWithObject: [NSString stringWithCString: winfilters[filter]
                                                                             encoding: NSUTF8StringEncoding]];
        [saveDlg setAllowedFileTypes: filterTypes];
        [saveDlg setAllowsOtherFileTypes: NO];
    }

    result = [saveDlg runModal];

    if (result == NSFileHandlingPanelOKButton)
        return [[saveDlg URL] path];

    return nil;
}

- (pid_t) retrieveID
{
    return processID;
}

- (void) quit
{
    [eventlog release];
    [textbuffer release];

    /* shut interpreter down */
    kill(processID, SIGUSR2);
}

@end

@interface GargoyleApp : NSWindowController
                         <GargoyleApp, NSApplicationDelegate, NSWindowDelegate>
{
    BOOL openedFirstGame;
    NSMutableDictionary * windows;
    NSConnection * link;
}
- (BOOL) launchFile: (NSString *) file;
- (BOOL) launchFileDialog;
@end

@implementation GargoyleApp

- (id) init
{
    self = [super init];

    /* set internal variables */
    openedFirstGame = NO;
    windows = [[NSMutableDictionary alloc] init];

    /* set up controller link */
    NSPort * port = [NSMachPort port];
    link = [[NSConnection connectionWithReceivePort: port sendPort: port] retain];
    [link setRootObject: self];
    [link addRunLoop: [NSRunLoop currentRunLoop]];
    [link registerName: [NSString stringWithFormat: @"com.googlecode.garglk-%04x", getpid()]];
    [link retain];

    /* set environment variable */
    NSString * nsResources = [[NSBundle mainBundle] resourcePath];

    [nsResources getCString: etc maxLength: sizeof etc encoding: NSUTF8StringEncoding];

    setenv("GARGLK_INI", etc, true);

    /* set preference defaults */
    [[NSUserDefaults standardUserDefaults] registerDefaults:
     [NSDictionary dictionaryWithObject: [NSNumber numberWithBool: YES]
                                 forKey: @"NSDisabledCharacterPaletteMenuItem"]];

    /* register for URL events */
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler: self
                                                       andSelector: @selector(openURL:withReplyEvent:)
                                                     forEventClass: kInternetEventClass
                                                        andEventID: kAEGetURL];

    return self;
}

- (BOOL) initWindow: (pid_t) processID
              width: (unsigned int) width
             height: (unsigned int) height
             retina: (int) retina
         fullscreen: (BOOL) fullscreen
{
    if (!(processID > 0))
        return NO;

    unsigned int style = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask;

    /* set up the window */
    NSRect rect = NSMakeRect(0, 0, width, height);
    if (retina)
    {
        NSView * tmpview = [[NSView alloc] initWithFrame: rect];
        rect = [tmpview convertRectFromBacking: rect];
        [tmpview release];
    }
    GargoyleWindow * window = [[GargoyleWindow alloc] initWithContentRect: rect
                                                                styleMask: style
                                                                  backing: NSBackingStoreBuffered
                                                                    defer: NO
                                                                  process: processID
                                                                   retina: retina];

    [window makeKeyAndOrderFront: window];
    [window center];
    [window setReleasedWhenClosed: YES];
    [window setDelegate: self];
    if (fullscreen)
        [window toggleFullScreen: self];

    [windows setObject: window forKey: [NSNumber numberWithInt: processID]];

    return ([window isVisible]);
}

- (NSEvent *) getWindowEvent: (pid_t) processID
{
    GargoyleWindow * window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window)
    {
        return [window retrieveEvent];
    }

    return nil;
}

- (NSRect) getWindowSize: (pid_t) processID
{
    GargoyleWindow * window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window)
    {
        id view = [window contentView];
        NSRect rect = [view bounds];
        if ([view wantsBestResolutionOpenGLSurface])
            rect = [view convertRectToBacking: rect];
        return rect;
    }

    return NSZeroRect;
}

- (NSPoint) getWindowPoint: (pid_t) processID
                  forEvent: (NSEvent *) event;
{
    GargoyleWindow * window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window)
    {
        id view = [window contentView];
        NSPoint point = [event locationInWindow];
        if ([view wantsBestResolutionOpenGLSurface])
            point = [view convertPointToBacking: point];
        return point;
    }

    return NSZeroPoint;
}

- (NSString *) getWindowCharString: (pid_t) processID
{
    GargoyleWindow * window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window)
    {
        return [window retrieveChars];
    }

    return nil;
}

- (BOOL) clearWindowCharString: (pid_t) processID
{
    GargoyleWindow * window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window)
    {
        [window clearChars];
        return YES;
    }

    return NO;
}

- (BOOL) setWindow: (pid_t) processID
        charString: (NSEvent *) event;
{
    GargoyleWindow * window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window)
    {
        [window sendChars: event];
        return YES;
    }

    return NO;
}

- (BOOL) setWindow: (pid_t) processID
             title: (NSString *) title;
{
    GargoyleWindow * window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window)
    {
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
    GargoyleWindow * window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window)
    {
        [[window contentView] addFrame: frame
                                 width: width
                                height: height];

        [[window contentView] drawRect: NSMakeRect(0, 0, width, height)];
        return YES;
    }

    return NO;
}

- (void) closeWindow: (pid_t) processID
{
    GargoyleWindow * window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window)
    {
        [window performClose:self];
    }

}

- (NSString *) openWindowDialog: (pid_t) processID
                         prompt: (NSString *) prompt
                         filter: (enum FILEFILTERS) filter
{
    GargoyleWindow * window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window)
    {
        return [window openFileDialog: prompt fileFilter: filter];
    }

    return nil;
}

- (NSString *) saveWindowDialog: (pid_t) processID
                         prompt: (NSString *) prompt
                         filter: (enum FILEFILTERS) filter
{
    GargoyleWindow * window = [windows objectForKey: [NSNumber numberWithInt: processID]];

    if (window)
    {
        return [window saveFileDialog: prompt fileFilter: filter];
    }

    return nil;
}

- (void) abortWindowDialog: (pid_t) processID
                    prompt: (NSString *) prompt
{
    NSRunAlertPanel(@"Fatal error", @"%@", nil, nil, nil, prompt);
}

#define kArrowCursor 1
#define kIBeamCursor 2
#define kPointingHandCursor 3

- (void) setCursor: (unsigned int) cursor
{
    switch (cursor)
    {
        case (kArrowCursor):
            [[NSCursor arrowCursor] set];
            break;

        case (kIBeamCursor):
            [[NSCursor IBeamCursor] set];
            break;

        case (kPointingHandCursor):
            [[NSCursor pointingHandCursor] set];
            break;

        default:
            break;
    }
}

- (BOOL) launchFileDialog
{
    int result;

    NSOpenPanel * openDlg = [NSOpenPanel openPanel];

    [openDlg setCanChooseFiles: YES];
    [openDlg setCanChooseDirectories: NO];
    [openDlg setAllowsMultipleSelection: NO];
    [openDlg setTitle: [NSString stringWithCString: AppName encoding: NSUTF8StringEncoding]];

    NSMutableArray *filterTypes = [NSMutableArray arrayWithCapacity:100];

    NSEnumerator *docTypeEnum = [[[[NSBundle mainBundle] infoDictionary]
                                  objectForKey:@"CFBundleDocumentTypes"] objectEnumerator];
    NSDictionary *docType;

    while (docType = [docTypeEnum nextObject])
    {
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

    if (result == NSFileHandlingPanelOKButton)
        return [self launchFile: [[openDlg URL] path]];

    return NO;
}

- (BOOL) launchFile: (NSString *) file
{
    if (![file length])
        return NO;

    /* get dir of executable */
    winpath(dir);

    /* get story file */
    if (![file getCString: buf maxLength: sizeof buf encoding: NSUTF8StringEncoding])
        return NO;

    /* run story file */
    int ran = garglk::rungame(dir, buf);

    if (ran)
        [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL: [NSURL fileURLWithPath: file]];

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

    BOOL result = YES;
    int i;

    for (i=0; i < [files count]; i++)
    {
        result = result && [self launchFile:[files objectAtIndex: i]];
    }
}

- (void) openURL: (NSAppleEventDescriptor *) event withReplyEvent: (NSAppleEventDescriptor *) reply
{
    NSArray * urlParts = [[[event paramDescriptorForKeyword: keyDirectObject] stringValue] componentsSeparatedByString: @"garglk://"];

    if (urlParts && [urlParts count] == 2)
    {
        openedFirstGame = YES;
        NSString * game = [[urlParts objectAtIndex: 1] stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];

        if ([[NSFileManager defaultManager] fileExistsAtPath: game] == YES)
            [self launchFile: game];
        else
            NSRunAlertPanel(@"Could not open URL path:", @"%@", nil, nil, nil, game);
    }

}

- (BOOL) windowShouldClose: (id) sender
{
    GargoyleWindow * window = sender;
    [windows removeObjectForKey: [NSNumber numberWithInt: [window retrieveID]]];
    [window quit];
    return YES;
}

- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *) sender
{
    for (id key in windows)
    {
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
    NSFileManager * fm = [NSFileManager defaultManager];
    NSString * home = [NSString stringWithFormat: @"%@/%@", [[[NSProcessInfo processInfo] environment] objectForKey: @"HOME"], @"garglk.ini"];
    NSString * main = [NSString stringWithFormat: @"%@/%@", [[NSBundle mainBundle] resourcePath], @"garglk.ini"];

    if (![fm isWritableFileAtPath: home] && [fm isReadableFileAtPath: main])
        [fm createFileAtPath: home contents: [NSData dataWithContentsOfFile: main] attributes: NULL];

    [[NSWorkspace sharedWorkspace] openFile: home
                            withApplication: @"TextEdit"];
}

@end

void garglk::winmsg(const std::string &msg)
{
    NSString * nsMsg;
    nsMsg = [NSString stringWithCString: msg.c_str() encoding: NSUTF8StringEncoding];
    NSRunAlertPanel(@"Fatal error", @"%@", nil, nil, nil, nsMsg);
}

static void winpath(char *buffer)
{
    char exepath[MaxBuffer] = {0};
    unsigned int exelen;

    exelen = sizeof(tmp);
    _NSGetExecutablePath(tmp, &exelen);
    exelen = (realpath(tmp, exepath) != NULL);

    if (exelen <= 0 || exelen >= MaxBuffer)
    {
        garglk::winmsg("Unable to locate executable path");
    }

    strcpy(buffer, exepath);

    char *dirpos = strrchr(buffer, *DirSeparator);
    if ( dirpos != NULL )
        *dirpos = '\0';
}

static int winexec(const char *cmd, const char **args)
{
    NSTask * proc = [[NSTask alloc] init];

    /* prepare interpreter path */
    NSArray * nsArray = [[NSString stringWithCString: cmd encoding: NSUTF8StringEncoding] componentsSeparatedByString: @"/"];
    NSString * nsTerp = [nsArray objectAtIndex: [nsArray count] - 1];
    NSString * nsCmd = [NSString stringWithFormat: @"%@/%@", [[NSBundle mainBundle] builtInPlugInsPath], nsTerp];

    /* prepare interpreter arguments */
    NSMutableArray * nsArgs = [NSMutableArray arrayWithCapacity:2];

    /* prepare environment */
    NSMutableDictionary * nsEnv = [[[NSProcessInfo processInfo] environment] mutableCopy];
    [nsEnv setObject: [NSString stringWithFormat: @"com.googlecode.garglk-%04x", getpid()] forKey: @"GargoyleApp"];

    if (args[1])
        [nsArgs addObject: [[NSString alloc] initWithCString: args[1] encoding: NSUTF8StringEncoding]];

    if (args[2])
        [nsArgs addObject: [[NSString alloc] initWithCString: args[2] encoding: NSUTF8StringEncoding]];

    if ([nsCmd length] && [nsArgs count])
    {
        [proc setLaunchPath: nsCmd];
        [proc setArguments: nsArgs];
        [proc setEnvironment: nsEnv];
        [proc launch];
    }

    return [proc isRunning];
}

int garglk::winterp(const std::string &path, const std::string &exe, const std::string &flags, const std::string &game)
{
    sprintf(tmp, "%s/%s", dir, exe.c_str());

    const char *args[] = {NULL, NULL, NULL};

    if (flags.find('-') != std::string::npos)
    {
        args[0] = (char *)exe.c_str();
        args[1] = (char *)flags.c_str();
        args[2] = buf;
    }
    else
    {
        args[0] = (char *)exe.c_str();
        args[1] = buf;
    }

    if (!winexec(tmp, args)) {
        garglk::winmsg("Could not start 'terp.\nSorry.");
        return false;
    }

    return true;
}

int main (int argc, char **argv)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    [NSApp setDelegate: [[GargoyleApp alloc] init]];
    [pool drain];
    return NSApplicationMain(argc, (const char **) argv);
}
