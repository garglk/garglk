/******************************************************************************
 *                                                                            *
 * Copyright (C) 2009 by Baltasar GarcÌa Perez-Schofield.                     *
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
#include <stdbool.h>
#include <ctype.h>

#include "glk.h"
#include "glkstart.h"
#include "gi_blorb.h"
#include "garversion.h"

/* Detect OS */
#ifdef __WIN32__
	#define OS_WINDOWS
#else
#ifdef _Windows	
	#define OS_WINDOWS
#else
#ifdef __Windows
	#define OS_WINDOWS
#endif
#endif
#endif

#ifdef __linux__
	#define OS_UNIX
#else
#ifdef __unix__
	#define OS_UNIX
#endif
#endif

#ifdef __APPLE__
	#define OS_MAC
#endif

/* Check OS detected */
#ifndef OS_WINDOWS
#ifndef OS_UNIX
#ifndef OS_MAC
	#error "Compilation stopped. No OS found."
#endif
#endif
#endif

/* Include required headers for GUI */
#ifdef OS_WINDOWS
	#include <windows.h>
	#include <commdlg.h>
#else
#ifdef OS_UNIX
	#include <gtk/gtk.h>
	#include <unistd.h>
#else
#ifdef OS_MAC
	#import <Cocoa/Cocoa.h>
#endif
#endif
#endif

#ifdef OS_MAC

@interface nsDelegate : NSObject
{
	NSString * fileref ;
}
@end

@implementation nsDelegate

- (id) init
{
	fileref = [[NSString alloc] initWithString: @""];
	return [super init];
}

- (BOOL) application: (NSApplication *) theApplication openFile: (NSString *) file
{
	if (fileref)
		[fileref release];
	
	if ([file length])
		fileref = [[NSString alloc] initWithString: file];
}

- (BOOL) application: (NSApplication *) theApplication openFiles: (NSArray *) files
{
	if (fileref)
		[fileref release];
	
	if ([files count])
		fileref = [[NSString alloc] initWithString: [files objectAtIndex: 0]];
	
	return YES;
}

- (NSString *) argv
{
	if (fileref)
		return fileref;
}

@end

#endif /* OS_MAC */