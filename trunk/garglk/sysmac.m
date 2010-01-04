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

#import <Cocoa/Cocoa.h>

static NSWindow *window;

static int titlecount = 0;


void glk_request_timer_events(glui32 millisecs)
{
}

void winabort(const char *fmt, ...)
{
    abort();
}

void winopenfile(char *prompt, char *buf, int len, char *filter)
{
}

void winsavefile(char *prompt, char *buf, int len, char *filter)
{
}

void winclipstore(glui32 *text, int len)
{
}

void winclipsend(int source)
{
}

void winclipreceive(int source)
{
}

void wininit(int *argc, char **argv)
{
    [NSApplication sharedApplication];
}

void winopen(void)
{
    NSUInteger style = NSTitledWindowMask
	| NSClosableWindowMask
	| NSMiniaturizableWindowMask
	| NSResizableWindowMask;
    window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,800,600)
										 styleMask:style
										   backing:NSBackingStoreBuffered
											 defer:NO];
    [window makeKeyAndOrderFront:window];	
    wintitle();
}

void wintitle(void)
{	
    char buf[256];
    if (strlen(gli_story_name))
        sprintf(buf, "%s - %s", gli_program_name, gli_story_name);
    else
        sprintf(buf, "%s", gli_program_name);
	
	NSString *title = [[NSString alloc] initWithCString:buf encoding:NSASCIIStringEncoding];
	[window setTitle:title];
    [title release];
}

void winrepaint(int x0, int y0, int x1, int y1)
{
}

void gli_select(event_t *event, int polled)
{
}
