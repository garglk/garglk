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

char filterlist[] = "";

@interface GargoyleApp : NSObject
{
    BOOL selectedFile;
}
@end

@implementation GargoyleApp

- (id) init
{
    selectedFile = NO;  
    return [super init]; 
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

- (BOOL) application: (NSApplication *) theApplication openFile: (NSString *) file
{
    selectedFile = YES;

    return [self launchFile:file];
}

- (BOOL) application: (NSApplication *) theApplication openFiles: (NSArray *) files
{
    selectedFile = YES;

    BOOL result = YES;
    int i;

    for (i=0; i < [files count]; i++)
    {
        result = result && [self launchFile:[files objectAtIndex: i]];
    }

    return result;
}

- (BOOL) applicationShouldOpenUntitledFile: (NSApplication *) sender
{
    return (!selectedFile);
}

- (BOOL) applicationOpenUntitledFile: (NSApplication *) theApplication
{
    selectedFile = YES;

    NSOpenPanel * openDlg = [NSOpenPanel openPanel];
    [openDlg setCanChooseFiles: YES];
    [openDlg setCanChooseDirectories: NO];
    [openDlg setAllowsMultipleSelection: NO];

    NSString * nsTitle = [[NSString alloc] initWithCString: AppName encoding: NSASCIIStringEncoding];
    [openDlg setTitle: nsTitle];
    [nsTitle release];    

    if ([openDlg runModal] == NSFileHandlingPanelOKButton)
    {
        return [self launchFile:[openDlg filename]];
    }

    return NO;
}

- (void) showDockIcon
{
    ProcessSerialNumber psn = { 0, kCurrentProcess }; 
    OSStatus returnCode = TransformProcessType(& psn, kProcessTransformToForegroundApplication);
    ProcessSerialNumber psnx = { 0, kNoProcess };
    GetNextProcess(&psnx);
    SetFrontProcess(&psnx);
}

@end

void winmsg(const char *msg)
{
    NSString * nsMsg = [[NSString alloc] initWithCString: msg encoding: NSASCIIStringEncoding];
    NSRunAlertPanel(@"Fatal error", nsMsg, NULL, NULL, NULL);
    [nsMsg release];
}

void winpath(char *buffer)
{
    char exepath[MaxBuffer] = {0};
    unsigned int exelen;

    exelen = sizeof(tmp);
    _NSGetExecutablePath(tmp, &exelen);
    exelen = (realpath(tmp, exepath) != NULL);    

    if (exelen <= 0 || exelen >= MaxBuffer) {
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

    NSString * nsCmd = [[NSString alloc] initWithCString: cmd encoding: NSASCIIStringEncoding];
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

    GargoyleApp * gargoyle = [[GargoyleApp alloc] init];
    [gargoyle showDockIcon];

    [NSApplication sharedApplication];
    [NSApp activateIgnoringOtherApps:TRUE];
    [NSApp setDelegate: gargoyle];
    [NSApp finishLaunching];
    [NSApp run];

    [pool drain];
}
