#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/oem.c,v 1.2 1999/05/17 02:52:14 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  oem.c - "original equipment manufacturer" identification
Function
  This file provides some information about this version of TADS.
  If you're customizing the TADS user interface, you'll probably
  want to modify this file to identify your version.

  You should feel free to customize any of the strings in this file
  for your version of TADS.
Notes
  
Modified
  10/12/98 MJRoberts  - Creation
*/

#include "std.h"
#include "oem.h"


/*
 *   The TADS run-time "banner" (the text that the run-time displays when
 *   it first starts up, to identify itself and show its version
 *   information) is generated using definitions from this file and from
 *   oem.h.  The format of the banner is:
 *   
 *   <<G_tads_oem_app_name>> - A <<G_tads_oem_display_mode>> TADS
 *.      <<TADS_RUNTIME_VERSION>> Interpreter\n
 *.  <<G_tads_oem_copyright_prefix>> Copyright (c) 1993, 1998 by
 *.      Michael J. Roberts.\n
 *.  <<G_tads_oem_author>>
 *   
 *   For example:
 *   
 *   FrobTADS 1.1.3 - A text-only TADS 2.2.7 Interpreter.
 *.  TADS Copyright (c) 1993, 1998 by Michael J. Roberts
 *.  FrobTADS Copyright 1998 by L. J. Teeterwaller <teeter@omegatron.com>
 *   
 *   The default format looks like this:
 *   
 *   tadsr - A text-only TADS 2.2.7 Interpreter.
 *.  Copyright (c) 1993, 1998 by Michael J. Roberts 
 */


/*
 *   OEM Application Name.  If you develop a new user interface for TADS,
 *   you'll probably want to give your system its own name, as WinTADS and
 *   MaxTADS do.  Define G_tads_oem_app_name to the name and version number
 *   of your system.  For example: "WinTADS 1.1.3".
 *   
 *   The default setting here is simply "tadsr", since this is the name of
 *   the TADS run-time executable that most people use for the standard
 *   character-mode run-time.
 */
char G_tads_oem_app_name[] = "tadsr";

/*
 *   OEM Debugger Application Name.  If you develop a customized version
 *   of the TADS debugger, you can use this to provide the name of the
 *   debugger application for display in the debugger's start-up banner. 
 */
char G_tads_oem_dbg_name[] = "tadsdbg";

/*
 *   OEM Display Mode.  This should be either "text-only" or "multimedia".
 *   "Text-only" interpreters are those that are based on the normal TADS
 *   display model and do not interpret HTML mark-ups (beyond what the
 *   normal TADS output layer translates to ordinary text output).
 *   "Multimedia" interpreters are those based on HTML TADS and hence can
 *   fully interpret HTML mark-ups, and can display images and play sounds
 *   and music.
 *   
 *   Please use "text-only" or "multimedia" rather than variations on
 *   these terms, since consistent usage will help users know what to
 *   expect from the different available interpreters.  If you develop a
 *   new user interface that creates a whole new category (such as a
 *   text-to-speech audio interface, for example), use your best judgment
 *   in choosing a name here.
 *   
 *   By default, we'll define this to "text-only", since most interpreters
 *   use the standard text output layer.  
 */
char G_tads_oem_display_mode[] = "text-only";

/*
 *   OEM Author Credit - This is a line of text that is displayed after
 *   the other banner lines.  You can put anything you want here -- if you
 *   want to display a separate copyright message for the part of the
 *   system you wrote, for example, you could include that here.
 *   
 *   If you plan to maintain your version of TADS for any length of time,
 *   you are encouraged to put your email address here, so that users can
 *   more easily contact you if they have a question or wish to shower you
 *   with accolades.
 *   
 *   You should include a newline at the end of the line, unless you're
 *   leaving the line entirely blank.  The newline is not assumed, so that
 *   this string can be left empty without generating an extra blank line.
 *   
 *   For example:
 *   
 *   "FrobTADS copyright 1998 by L.J.Teeterwaller <teeter@omegatron.com>\n"
 *   
 *   The default leaves this line entirely blank.  
 */
char G_tads_oem_author[] = "";

/*
 *   TADS copyright prefix - if this is TRUE, the run-time displays the
 *   word "TADS" before the word "copyright" on the copyright line of its
 *   banner.  If you are adding your own copyright message, you probably
 *   want to define this to TRUE.  If not, you should leave this as FALSE. 
 */
int G_tads_oem_copyright_prefix = FALSE;

