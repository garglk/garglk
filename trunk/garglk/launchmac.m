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
#include "garversion.h"
#include "launcher.h"

#import <Cocoa/Cocoa.h>

static const char * AppName = "Gargoyle " VERSION;
static const char * LaunchingTemplate = "%s/%s";
static const char * DirSeparator = "/";

#define MaxBuffer 1024
char dir[MaxBuffer];
char buf[MaxBuffer];
char tmp[MaxBuffer];
char etc[MaxBuffer];
char fnt[MaxBuffer];

char filterlist[] = "";

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
                   process: (pid_t) pid;
- (void) sendEvent: (NSEvent *) event;
- (NSEvent *) retrieveEvent;
- (void) sendChars: (NSEvent *) event;
- (NSString *) retrieveChars;
- (void) clearChars;
- (IBAction) cut: (id) sender;
- (IBAction) copy: (id) sender;
- (IBAction) paste: (id) sender;
- (IBAction) performZoom: (id) sender;
- (void) performRefresh: (NSNotification *) notice;
- (NSString *) openFileDialog: (NSString *) prompt;
- (NSString *) saveFileDialog: (NSString *) prompt;
- (pid_t) retrieveID;
- (void) quit;
@end

@implementation GargoyleWindow

- (id) initWithContentRect: (NSRect) contentRect
                 styleMask: (unsigned int) windowStyle
                   backing: (NSBackingStoreType) bufferingType
                     defer: (BOOL) deferCreation
                   process: (pid_t) pid
{
    self = [super initWithContentRect: contentRect
                            styleMask: windowStyle
                              backing: bufferingType
                                defer: deferCreation];

    eventlog = [[NSMutableArray alloc] initWithCapacity: 100];
    textbuffer = [[NSTextView alloc] init];
    processID = pid;

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
        {
            if (([event timestamp] - lastMouseMove) > 0.05)
            {
                lastMouseMove = [event timestamp];
                store = YES;
            }
            break;
        }

        default: break;
    }

    if (store)
    {
        [eventlog insertObject: [event copy] atIndex:0];
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
    [textbuffer interpretKeyEvents: [NSArray arrayWithObject:event]];
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
                                       keyCode: 0x07]];
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
                                       keyCode: 0x08]];
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
                                       keyCode: 0x09]];
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
{
    NSOpenPanel * openDlg = [NSOpenPanel openPanel];

    [openDlg setCanChooseFiles: YES];
    [openDlg setCanChooseDirectories: NO];
    [openDlg setAllowsMultipleSelection: NO];
    [openDlg setTitle: prompt];

    if ([openDlg runModal] == NSFileHandlingPanelOKButton)
        return [openDlg filename];

    return NULL;
}

- (NSString *) saveFileDialog: (NSString *) prompt
{
    NSSavePanel * saveDlg = [NSSavePanel savePanel];

    [saveDlg setCanCreateDirectories: YES];
    [saveDlg setTitle: prompt];

    if ([saveDlg runModal] == NSFileHandlingPanelOKButton)
        return [saveDlg filename];

    return NULL;
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

@interface GargoyleApp : NSWindowController //<NSWindowDelegate>
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

    int size = [nsResources length];
    CFStringGetBytes((CFStringRef) nsResources, CFRangeMake(0, size),
                     kCFStringEncodingASCII, 0, FALSE,
                     etc, MaxBuffer, NULL);
    int bounds = size < MaxBuffer ? size : MaxBuffer;
    etc[bounds] = '\0';

    setenv("FONTCONFIG_PATH", etc, TRUE);
    setenv("GARGLK_INI", etc, TRUE);

    /* create symlink to fonts */
    if (getenv("HOME"))
    {
        strcpy(fnt, getenv("HOME"));
        strcat(fnt, "/Library/Fonts/Gargoyle");
        unlink(fnt);
        strcat(etc, "/Fonts");
        symlink(etc, fnt);
    }

    /* set preference defaults */
    [[NSUserDefaults standardUserDefaults] registerDefaults:
     [NSDictionary dictionaryWithObject: [NSNumber numberWithBool: YES]
                                 forKey: @"NSDisabledCharacterPaletteMenuItem"]];

    return self;
}

- (BOOL) initWindow: (pid_t) processID
              width: (unsigned int) width
             height: (unsigned int) height
{
    if (!(processID > 0))
        return NO;

    unsigned int style = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask;

    /* set up the window */
    GargoyleWindow * window = [[GargoyleWindow alloc] initWithContentRect: NSMakeRect(0,0, width, height)
                                                                styleMask: style
                                                                  backing: NSBackingStoreBuffered
                                                                    defer: NO
                                                                  process: processID];

    [window makeKeyAndOrderFront: window];
    [window center];
    [window setReleasedWhenClosed: YES];
    [window setDelegate: self];
    [windows setObject: window forKey: [NSString stringWithFormat: @"%04x", processID]];

    return ([window isVisible]);
}

- (NSEvent *) getWindowEvent: (pid_t) processID
{
    id storedWindow = [windows objectForKey: [NSString stringWithFormat: @"%04x", processID]];

    if (storedWindow)
    {
        GargoyleWindow * window = (GargoyleWindow *) storedWindow;
        return [window retrieveEvent];
    }

    return NULL;
}

- (NSRect) getWindowSize: (pid_t) processID
{
    id storedWindow = [windows objectForKey: [NSString stringWithFormat: @"%04x", processID]];

    if (storedWindow)
    {
        GargoyleWindow * window = (GargoyleWindow *) storedWindow;
        return [[window contentView] bounds];
    }

    return NSZeroRect;
}

- (NSString *) getWindowCharString: (pid_t) processID
{
    id storedWindow = [windows objectForKey: [NSString stringWithFormat: @"%04x", processID]];

    if (storedWindow)
    {
        GargoyleWindow * window = (GargoyleWindow *) storedWindow;
        return [window retrieveChars];
    }

    return NULL;
}

- (BOOL) clearWindowCharString: (pid_t) processID
{
    id storedWindow = [windows objectForKey: [NSString stringWithFormat: @"%04x", processID]];

    if (storedWindow)
    {
        GargoyleWindow * window = (GargoyleWindow *) storedWindow;
        [window clearChars];
        return YES;
    }

    return NO;
}

- (BOOL) setWindow: (pid_t) processID
        charString: (NSEvent *) event;
{
    id storedWindow = [windows objectForKey: [NSString stringWithFormat: @"%04x", processID]];

    if (storedWindow)
    {
        GargoyleWindow * window = (GargoyleWindow *) storedWindow;
        [window sendChars: event];
        return YES;
    }

    return NO;
}

- (BOOL) setWindow: (pid_t) processID
             title: (NSString *) title;
{
    id storedWindow = [windows objectForKey: [NSString stringWithFormat: @"%04x", processID]];

    if (storedWindow)
    {
        GargoyleWindow * window = (GargoyleWindow *) storedWindow;
        [window setTitle: title];
        return YES;
    }

    return NO;
}

- (BOOL) setWindow: (pid_t) processID
          contents: (NSData *) frame
             width: (unsigned int) width
            height: (unsigned int) height
        background: (NSColor *) color;
{
    id storedWindow = [windows objectForKey: [NSString stringWithFormat: @"%04x", processID]];

    if (storedWindow)
    {
        GargoyleWindow * window = (GargoyleWindow *) storedWindow;
        [NSGraphicsContext setCurrentContext: [window graphicsContext]];

        if ([[window contentView] lockFocusIfCanDraw])
        {
            /* repaint the backing window */
            [window setBackgroundColor: color];
            [[window contentView] displayIfNeeded];

            /* refresh the screen */
            NSImage * output = [[NSImage alloc] initWithData: frame];

            [output drawAtPoint: NSMakePoint(0, 0)
                      fromRect: NSZeroRect
                     operation: NSCompositeCopy
                      fraction: 1.0];

            [output release];

            /* repaint the resize control */
            int xsize = width > 12 ? width - 12 : 0;
            [[window contentView] setNeedsDisplayInRect: NSMakeRect(xsize, 0, 12, 12)];
            [[window contentView] displayIfNeededInRect: NSMakeRect(xsize, 0, 12, 12)];

            /* flush window buffer to screen */
            [window flushWindow];
            [[window contentView] unlockFocus];

            return YES;
        }
    }

    return NO;
}

- (void) closeWindow: (pid_t) processID
{
    id storedWindow = [windows objectForKey: [NSString stringWithFormat: @"%04x", processID]];

    if (storedWindow)
    {
        GargoyleWindow * window = (GargoyleWindow *) storedWindow;
        [window performClose:self];
    }

}

- (NSString *) openWindowDialog: (pid_t) processID
                         prompt: (NSString *) prompt
{
    id storedWindow = [windows objectForKey: [NSString stringWithFormat: @"%04x", processID]];

    if (storedWindow)
    {
        GargoyleWindow * window = (GargoyleWindow *) storedWindow;
        return [window openFileDialog: prompt];
    }

    return NULL;
}

- (NSString *) saveWindowDialog: (pid_t) processID
                         prompt: (NSString *) prompt
{
    id storedWindow = [windows objectForKey: [NSString stringWithFormat: @"%04x", processID]];

    if (storedWindow)
    {
        GargoyleWindow * window = (GargoyleWindow *) storedWindow;
        return [window saveFileDialog: prompt];
    }

    return NULL;
}

- (void) abortWindowDialog: (pid_t) processID
                    prompt: (NSString *) prompt
{
    NSRunAlertPanel(@"Fatal error", prompt, NULL, NULL, NULL);
}

- (BOOL) launchFileDialog
{
    NSOpenPanel * openDlg = [NSOpenPanel openPanel];

    [openDlg setCanChooseFiles: YES];
    [openDlg setCanChooseDirectories: NO];
    [openDlg setAllowsMultipleSelection: NO];
    [openDlg setTitle: [NSString stringWithCString: AppName encoding: NSASCIIStringEncoding]];

    if ([openDlg runModal] == NSFileHandlingPanelOKButton)
        return [self launchFile:[openDlg filename]];

    return NO;
}

- (BOOL) launchFile: (NSString *) file
{
    if (![file length])
        return NO;

    /* get dir of executable */
    winpath(dir);

    /* get story file */
    int size = [file length];
    CFStringGetBytes((CFStringRef) file, CFRangeMake(0, size),
                     kCFStringEncodingASCII, 0, FALSE,
                     buf, MaxBuffer, NULL);
    int bounds = size < MaxBuffer ? size : MaxBuffer;
    buf[bounds] = '\0';

    /* run story file */
    int ran = rungame(dir, buf);

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

- (BOOL) application: (NSApplication *) theApplication openFiles: (NSArray *) files
{
    openedFirstGame = YES;

    BOOL result = YES;
    int i;

    for (i=0; i < [files count]; i++)
    {
        result = result && [self launchFile:[files objectAtIndex: i]];
    }

    return result;
}

- (BOOL) windowShouldClose: (id) sender
{
    GargoyleWindow * window = sender;
    [windows removeObjectForKey: [NSString stringWithFormat: @"%04x", [window retrieveID]]];
    [window quit];
    return YES;
}

- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *) sender
{
    NSEnumerator * windowList = [windows objectEnumerator];

    id storedWindow;

    while (storedWindow = [windowList nextObject])
    {
        GargoyleWindow * window = (GargoyleWindow *) storedWindow;
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

void winmsg(const char *msg)
{
    NSString * nsMsg = [NSString stringWithCString: msg encoding: NSASCIIStringEncoding];
    NSRunAlertPanel(@"Fatal error", nsMsg, NULL, NULL, NULL);
}

void winpath(char *buffer)
{
    char exepath[MaxBuffer] = {0};
    unsigned int exelen;

    exelen = sizeof(tmp);
    _NSGetExecutablePath(tmp, &exelen);
    exelen = (realpath(tmp, exepath) != NULL);

    if (exelen <= 0 || exelen >= MaxBuffer)
    {
        winmsg("Unable to locate executable path");
    }

    strcpy(buffer, exepath);

    char *dirpos = strrchr(buffer, *DirSeparator);
    if ( dirpos != NULL )
        *dirpos = '\0';
}

int winexec(const char *cmd, char **args)
{
    NSTask * proc = [[NSTask alloc] init];

    /* prepare interpreter path */
    NSArray * nsArray = [[NSString stringWithCString: cmd encoding: NSASCIIStringEncoding] componentsSeparatedByString: @"/"];
    NSString * nsTerp = [nsArray objectAtIndex: [nsArray count] - 1];
    NSString * nsCmd = [NSString stringWithFormat: @"%@/%@", [[NSBundle mainBundle] builtInPlugInsPath], nsTerp];

    /* prepare interpreter arguments */
    NSMutableArray * nsArgs = [NSMutableArray arrayWithCapacity:2];

    /* prepare environment */
    NSMutableDictionary * nsEnv = [[[NSProcessInfo processInfo] environment] mutableCopy];
    [nsEnv setObject: [NSString stringWithFormat: @"com.googlecode.garglk-%04x", getpid()] forKey: @"GargoyleApp"];

    if (args[1])
        [nsArgs addObject: [[NSString alloc] initWithCString: args[1] encoding: NSASCIIStringEncoding]];

    if (args[2])
        [nsArgs addObject: [[NSString alloc] initWithCString: args[2] encoding: NSASCIIStringEncoding]];

    if ([nsCmd length] && [nsArgs count])
    {
        [proc setLaunchPath: nsCmd];
        [proc setArguments: nsArgs];
        [proc setEnvironment: nsEnv];
        [proc launch];
    }

    return [proc isRunning];
}

int winterp(char *path, char *exe, char *flags, char *game)
{
    sprintf(tmp, LaunchingTemplate, dir, exe);

    char *args[] = {NULL, NULL, NULL};

    if (strstr(flags, "-"))
    {
        args[0] = exe;
        args[1] = flags;
        args[2] = buf;
    }
    else
    {
        args[0] = exe;
        args[1] = buf;
    }

    if (!winexec(tmp, args)) {
        winmsg("Could not start 'terp.\nSorry.");
        return FALSE;
    }

    return TRUE;
}

int main (int argc, char **argv)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    [NSApp setDelegate: [[GargoyleApp alloc] init]];
    [pool drain];
    return NSApplicationMain(argc, (const char **) argv);
}
