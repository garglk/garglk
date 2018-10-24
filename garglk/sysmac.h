/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson, Jesse McGrew.                    *
 * Copyright (C) 2010 by Ben Cressey, Chris Spiegel.                          *
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

#ifndef GARGLK_SYSMAC_M
#define GARGLK_SYSMAC_M

#ifdef __ppc__
#define UTF32StringEncoding NSUTF32BigEndianStringEncoding
#else
#define UTF32StringEncoding NSUTF32LittleEndianStringEncoding
#endif

#define NSKEY_LEFT      0x7b
#define NSKEY_RIGHT     0x7c
#define NSKEY_DOWN      0x7d
#define NSKEY_UP        0x7e

#define NSKEY_X         0x07
#define NSKEY_C         0x08
#define NSKEY_V         0x09

#define NSKEY_PGUP      0x74
#define NSKEY_PGDN      0x79
#define NSKEY_HOME      0x73
#define NSKEY_END       0x77
#define NSKEY_DEL       0x75
#define NSKEY_BACK      0x33
#define NSKEY_ESC       0x35

#define NSKEY_F1        0x7a
#define NSKEY_F2        0x78
#define NSKEY_F3        0x63
#define NSKEY_F4        0x76
#define NSKEY_F5        0x60
#define NSKEY_F6        0x61
#define NSKEY_F7        0x62
#define NSKEY_F8        0x64
#define NSKEY_F9        0x65
#define NSKEY_F10       0x6d
#define NSKEY_F11       0x67
#define NSKEY_F12       0x6f

@protocol GargoyleApp

- (BOOL) initWindow: (pid_t) processID
              width: (unsigned int) width
             height: (unsigned int) height
             retina: (int) retina
         fullscreen: (BOOL) fullscreen
    backgroundColor: (NSColor *) backgroundColor;

- (NSEvent *) getWindowEvent: (pid_t) processID;

- (NSRect) getWindowSize: (pid_t) processID;

- (NSPoint) getWindowPoint: (pid_t) processID
                  forEvent: (NSEvent *) event;

- (NSString *) getWindowCharString: (pid_t) processID;

- (BOOL) clearWindowCharString: (pid_t) processID;

- (BOOL) setWindow: (pid_t) processID
        charString: (NSEvent *) event;

- (BOOL) setWindow: (pid_t) processID
             title: (NSString *) title;

- (BOOL) setWindow: (pid_t) processID
          contents: (NSData *) frame
             width: (unsigned int) width
            height: (unsigned int) height;

- (void) closeWindow: (pid_t) processID;

- (NSString *) openWindowDialog: (pid_t) processID
                         prompt: (NSString *) prompt
                         filter: (enum FILEFILTERS) filter;

- (NSString *) saveWindowDialog: (pid_t) processID
                         prompt: (NSString *) prompt
                         filter: (enum FILEFILTERS) filter;

- (void) abortWindowDialog: (pid_t) processID
                    prompt: (NSString *) prompt;

- (void) setCursor: (unsigned int) cursor;

@end

#endif
