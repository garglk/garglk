/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
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

/*
$Header: d:/cvsroot/tads/TADS2/OS.H,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $
*/

/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  os.h - operating system definitions
Function
  Definitions that vary by operating system
Notes
  Do NOT put your system-specific definitions in this file, EXCEPT for
  your system-specific #include lines.

  We want to avoid piling up a huge tree of #ifdef's, since it's almost
  impossible to decipher such code.  Instead, we want to isolate all of
  the code for each platform in its own header file -- this way, it
  should be easy to figure out what code is compiled on each system.

  So, when porting this code to a new platform, you should first
  create an osxxx.h file for your platform (for example, on Windows,
  the file is oswin.h), then add to this file an ifdef'd include for
  your file, something like this:

    #ifdef _FROBNIX
    #include "osfrobnix.h"
    #endif

  These should generally be the ONLY lines in this file that pertain
  to your system.  Everything else for your system should be defined
  in your osxxx.h file.

  Also note that some definitions belong in your *hardware* file,
  rather than your operating system file.  Since many types of hardware
  have several operating systems, and many operating systems run on
  more than one type of hardware, definitions that pertain to a
  particular type of hardware should be isolated in a separate file.
  So, if you're adding a new hardware platform as well as (or instead
  of) a new operating system, you should create a new h_xxx.h file
  (in the "hardware" source subdirectory), and add an include line
  like this:

    #ifdef _M_BANANA_3000
    #include "h_b3000.h"
    #endif

  Note that you may have to adjust your makefile's CFLAGS so that
  the proper hardware and software configuration is selected via -D
  options (or your local equivalent).
Modified
  10/17/98 MJRoberts  - creation (from TADS 2 os.h, los.h, etc)
*/

#ifndef OS_INCLUDED
#define OS_INCLUDED

/*
 *   For C++ files, define externals with C linkage 
 */
#ifdef __cplusplus
extern "C" {
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Include the appropriate hardware-specific header. 
 */

#ifdef __ppc__
#define _M_PPC
#else
#ifdef __x86_64__
#define _M_IX86_64
#else
#define _M_IX86
#endif
#endif

/*
 *   Intel x86 processors - 32-bit
 */
#ifdef _M_IX86
#include "h_ix86.h"
#endif

/*
 *   Intel x86 processors - 64-bit 
 */
#ifdef _M_IX86_64
#include "h_ix86_64.h"
#endif

/*
 *   PowerPC CPU's 
 */
#ifdef _M_PPC
#include "h_ppc.h"
#endif

/*
 *   Qt.  This isn't *truly* a hardware platform, but it looks like one to
 *   us, because it provides its own hardware virtualization scheme.  Rather
 *   than trying to figure out what sort of physical hardware we're
 *   targeting, we can simply define our hardware virtualization macros and
 *   functions in terms of Qt's hardware virtualization APIs, and let Qt take
 *   care of providing the target-specific implementation of those APIs.  
 */
#ifdef _M_QT
#include "h_qt.h"
#endif

/* add others here */

/* ------------------------------------------------------------------------ */
/*
 *   Include the portable OS interface type definitions.  These types can
 *   be used within OS-specific headers, so this type definitions header
 *   must be included before any of the OS-specific headers.  
 */
#include "osifctyp.h"


/* ------------------------------------------------------------------------ */
/*
 *   Include the appropriate OS-specific header.  We switch on system type
 *   here to avoid a big pile of ifdef's for each system scattered among
 *   all of the headers, and instead just select one big file for each
 *   system-specific definitions.  
 */

#ifdef GARGOYLE
#include "osansi.h"
#else

#ifdef _WIN32
# include "oswin.h"
#endif
#ifdef __MSDOS__
# ifdef __WIN32__
/* Windows-specific definitions are in oswin.h */
#  include "oswin.h"
# else
#  ifdef MSOS2
/* OS/2-specific definnitions are in osos2.h */
#   include "osos2.h"
#  else
/* DOS-specific definitions are in osdos.h */
#   include "osdos.h"
#  endif
# endif
#endif

#ifdef MAC_OS
/* macintosh definitions (Mac OS <=9) are in osmac.h */
#include "osmac.h"
#endif

#ifdef MAC_OS_X
/* macintosh OS X definitions are in osmacosx.h */
#include "osmacosx.h"
#endif

#ifdef UNIX
/* unix definitions are in osunixt.h */
#include "osunixt.h"
#endif

#ifdef ATARI
/* Atari ST definitions are in osatari.h */
#include "osatari.h"
#endif

#ifdef GLK
/* glk definitions are in os_glk.h */
#include "os_glk.h"
#endif

#ifdef __BEOS__
#include "osbeos.h"
#endif

#ifdef TROLLTECH_QT
/* Qt-specific definitions are in osqt.h */
#include "osqt.h"
#endif

#ifdef FROBTADS
/* 
 *   FrobTADS definitions are in osfrobtads.h.  (FrobTADS isn't really an OS,
 *   but from our perspective it looks like one, since it implements the
 *   various osifc entrypoints.  FrobTADS further virtualizes access to
 *   several Curses-style terminal i/o APIs, but we don't care about that; we
 *   just care that it provides our osifc implementations.)  
 */
#include "osfrobtads.h"
#endif

#endif /* SPATTERLIGHT */

/* **************** add other systems here **************** */


/*
 *   Done with C linkage section (osifc.h has its own)
 */
#ifdef __cplusplus
}
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Include the generic interface definitions for routines that must be
 *   implemented separately on each platform.
 *   
 *   Note that we include this file *after* including the system-specific
 *   osxxx.h header -- this allows definitions in the osxxx.h header to
 *   override certain defaults in osifc.h by #defining symbols to indicate
 *   to osifc.h that it should not include the defaults.  Refer to osifc.h
 *   for details of such overridable definitions.  
 */
#include "osifc.h"


/* ------------------------------------------------------------------------ */
/*
 *   If the system "long description" (for the banner) isn't defined,
 *   make it the same as the platform ID string.  
 */
#ifndef OS_SYSTEM_LDESC
# define OS_SYSTEM_LDESC  OS_SYSTEM_NAME
#endif

#endif /* OS_INCLUDED */

