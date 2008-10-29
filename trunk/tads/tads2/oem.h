/* $Header: d:/cvsroot/tads/TADS2/OEMPROTO.H,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1996, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  oem.h - "original equipment manufacturer" identification
Function
  This file identifies the person who created this version of TADS.
  If you're porting TADS to a new machine, or you're customizing
  something, THIS MEANS YOU!
  
  The OEM name defined herein is displayed in the version banner of
  each executable to make it easy to identify the source of a particular
  version of the system.
  
  This file also specifies the OEM version number.  This version
  number identifies the revision level of a particular base version
  of the TADS source code as released by a particular OEM.
Notes

  EACH PERSON WHO PRODUCES A VERSION OF TADS SHOULD SET THE OEM
  IDENTIFIER IN THIS FILE TO A UNIQUE VALUE.  The OEM identifier is
  intended to reduce the confusion that could result from different
  people producing versions of the system.  If you produce a version
  of TADS, and someone has a question about it, it will be possible
  to tell from the OEM identifier on the person's executable whether
  it's your version or a version someone else produced.

  We recommend that you use your email address or full name as your
  OEM identifier, since this is the best way to ensure that you don't
  use an ID that someone else is using.

  Note that, if you release multiple versions of a particular release
  of the base TADS source code (for example, if you find a fix a
  port-specific bug in your version, and the fixed version is based
  on the same base TADS source code as the original release), you
  should update the OEM version number as well.  This should start
  at zero for the first release that you make of a particular version
  of the base TADS source code, and should be incremented each time
  you release a new revision based on the same portable code.  This
  version number shows up as the fourth part of the version number
  displayed in each executable's banner.

Modified
  10/05/96 MJRoberts  - Creation
*/

#ifndef OEM_H
#define OEM_H

#ifndef OS_H
#include "os.h"
#endif

/*
 *   OEM name - this should uniquely identify your version, to
 *   distinguish it from versions produced by other people; you should use
 *   your name or email address to ensure uniqueness.  If the name is
 *   already defined (some people like to define it in the makefile), we
 *   won't redefine it here.  
 */
#ifndef TADS_OEM_NAME
$$$ #define your own TADS_OEM_NAME here, similar to this:
/* #define  TADS_OEM_NAME   "Mike Roberts" */
/* Please keep the size under 256 bytes */
/* Don't use "Mike Roberts" unless your name happens to be Mike Roberts. */
#endif

/*
 *   NOTE - if you redistribute this source file, please leave the dollar
 *   signs intact above so that the next person who gets a copy gets a
 *   compiler error and thereby realizes that they need to update this. 
 */

/*
 *   OEM version level - increment for each new release based on the same
 *   original source code version.  Note that this is a string.  Some
 *   people prefer to define the OEM patch level in their makefiles, so we
 *   won't attempt to redefine it if it's already been defined.  
 */
#ifndef TADS_OEM_VERSION
# define TADS_OEM_VERSION  "0"
#endif

/*
 *   Some external strings that must be defined for the run-time banner.
 *   Please refer to oem.c for the definitions of these strings. 
 */
extern char G_tads_oem_app_name[];
extern char G_tads_oem_dbg_name[];
extern char G_tads_oem_display_mode[];
extern char G_tads_oem_author[];
extern int  G_tads_oem_copyright_prefix;

#endif /* OEM_H */

