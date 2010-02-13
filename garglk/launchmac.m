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

@interface GargoyleApp : NSObject
{
    BOOL openedFirstGame;
    NSString * processID;
    NSMutableArray * activeGames;
}
- (void) listen;
- (void) promote;
@end

@implementation GargoyleApp

- (id) init
{
    /* set internal variables */
    openedFirstGame = NO;
    processID = [[NSString alloc] initWithFormat: @"%d", [[NSProcessInfo processInfo] processIdentifier]];
    activeGames = [[NSMutableArray alloc] initWithCapacity:5];
    [activeGames addObject: [NSNumber numberWithInt: [processID intValue]]];

    /* set environment variables */
    NSString * nsResources = [NSString stringWithFormat: @"%@/%@",
                              [[NSWorkspace sharedWorkspace] fullPathForApplication: @"Gargoyle"],
                              @"Contents/Resources"];

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

    /* listen for dispatched notifications */
    [self listen];

    return [super init];
}

- (void) listen
{
    [[NSDistributedNotificationCenter defaultCenter] addObserver: self
                                                        selector: @selector(receive:)
                                                            name: NULL
                                                          object: @"com.googlecode.garglk"];
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

    if ([[message name] rangeOfString: @"+"].location != NSNotFound)
    {
        /* generate PID object to add */
        NSNumber * activePID = [NSNumber numberWithInt: [[[message name]
                                                          stringByTrimmingCharactersInSet: [NSCharacterSet
                                                                                            characterSetWithCharactersInString: @"+"]]
                                                          intValue]];
        /* remove other instances of PID */
        [activeGames removeObject: activePID];

        /* store PID */
        [activeGames addObject: activePID];

        return;
    }

    if ([[message name] rangeOfString: @"-"].location != NSNotFound)
    {
        /* generate PID object to remove */
        NSNumber * activePID = [NSNumber numberWithInt: [[[message name]
                                                          stringByTrimmingCharactersInSet: [NSCharacterSet
                                                                                            characterSetWithCharactersInString: @"-"]]
                                                          intValue]];
        /* remove all instances of PID */
        [activeGames removeObject: activePID];

        /* promote next game in queue */
        [self promote];

        return;
    }

    if ([[message name] isEqualToString: processID])
    {
        /* clean up PID array */
        [activeGames removeAllObjects];

        /* store PID */
        [activeGames addObject: [NSNumber numberWithInt: [processID intValue]]];

        /* activate launcher */
        [NSApp activateIgnoringOtherApps: YES];

        return;
    }
}

- (void) promote
{
    NSEnumerator * proc = [activeGames reverseObjectEnumerator];
    NSNumber * storedPID;
    int pid;

    while (storedPID = [proc nextObject])
    {
        /* retrieve PID in order of activation */
        pid = [storedPID intValue];

        /* check if PID is inaccessible */
        if ((kill(pid,0) == -1 && errno == ESRCH))
            continue;

        /* send out PID for activation */
        [self send: [NSString stringWithFormat: @"%d", pid]];

        return;
    }
}

- (BOOL) launchFile: (NSString *) file
{
    if (![file length])
        return YES;

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
    return rungame(dir, buf);
}

- (BOOL) launchFileBrowser
{
    NSOpenPanel * openDlg = [NSOpenPanel openPanel];
    [openDlg setCanChooseFiles: YES];
    [openDlg setCanChooseDirectories: NO];
    [openDlg setAllowsMultipleSelection: NO];

    NSString * nsTitle = [NSString stringWithCString: AppName encoding: NSASCIIStringEncoding];
    [openDlg setTitle: nsTitle];

    if ([openDlg runModal] == NSFileHandlingPanelOKButton)
        return [self launchFile:[openDlg filename]];
    else
        return NO;
}

- (BOOL) applicationShouldOpenUntitledFile: (NSApplication *) sender
{
    return (!openedFirstGame);
}

- (BOOL) applicationOpenUntitledFile: (NSApplication *) theApplication
{
    openedFirstGame = YES;
    return [self launchFileBrowser];
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


- (void) applicationDidBecomeActive: (NSNotification *) aNotification
{
    /* promote first game in queue */
    [self promote];
}

- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *) sender
{
    [self send: @"QUIT"];
    return NSTerminateNow;
}

- (void) applicationWillHide: (NSNotification *) aNotification
{
    [self send: @"HIDE"];
}

- (void) applicationWillUnhide: (NSNotification *) aNotification
{
    [self send: @"SHOW"];
}

- (IBAction) orderFrontStandardAboutPanel: (id) sender
{
}

- (IBAction) openFileBrowser: (id) sender
{
    [self launchFileBrowser];
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
    NSString * nsPath = [[NSWorkspace sharedWorkspace] fullPathForApplication: @"Gargoyle"];
    NSString * nsCmd = [NSString stringWithFormat: @"%@/%@/%@%@/%@/%@", nsPath, @"Contents/Interpreters", nsTerp, @".app", @"Contents/MacOS", nsTerp];

    /* prepare interpreter arguments */
    NSMutableArray * nsArgs = [NSMutableArray arrayWithCapacity:2];

    if (args[1])
        [nsArgs addObject: [[NSString alloc] initWithCString: args[1] encoding: NSASCIIStringEncoding]];

    if (args[2])
        [nsArgs addObject: [[NSString alloc] initWithCString: args[2] encoding: NSASCIIStringEncoding]];

    if ([nsCmd length] && [nsArgs count])
    {
        [proc setLaunchPath: nsCmd];
        [proc setArguments: nsArgs];
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
