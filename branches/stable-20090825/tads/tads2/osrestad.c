#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/OSRESTAD.C,v 1.1 1999/07/11 00:46:30 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osrestad.c - Resource String default implementation for TADS 2
Function
  Implements os_get_str_rsc() using a set of static strings, compiled
  in to the application, for English versions of the TADS 2 resources.

  This file is specific to three things:

    - TADS 2.  If the OS functions are used by another application,
      this implementation must be replaced with one that defines the
      strings for that application rather than for TADS 2.  The actual
      resource ID's and their meanings are specific to the host
      application.

    - English.  To translate an application built with this
      implementation, this file must be replaced with one that defines
      translated versions of the strings.

    - Generic operating system.  The definitions here could be
      inappropriate for operating systems where strong local conventions
      exist.  In particular, the dialog button labels might not conform
      to local conventions.  When porting TADS to an operating system
      whose conventions differ from the ones used here, this file must
      be replaced.  Note that, whenever possible, an OS-level resource
      mechanism should be used instead of this default implementation
      to begin with.

Notes
  
Modified
  06/25/99 MJRoberts  - Creation
*/

#include <string.h>

#include "os.h"
#include "res.h"

/*
 *   The strings, as a static array.
 *   
 *   To translate a version of TADS built using this file, make a new copy
 *   of this file, translate the strings, and modify the makefile to build
 *   a new version of the executable with your translated version of this
 *   file.  
 */
static const char *S_res_strings[] =
{
    /* 
     *   OK/Cancel/Yes/No button labels.  We provide a shortcut for every
     *   label, to make the buttons work well in character mode. 
     */
    "&OK",                                              /* RESID_BTN_OK = 1 */
    "&Cancel",                                      /* RESID_BTN_CANCEL = 2 */
    "&Yes",                                            /* RESID_BTN_YES = 3 */
    "&No",                                              /* RESID_BTN_NO = 4 */

    /* reply strings for yorn() */
    "[Yy].*",                                             /* RESID_YORN_YES */
    "[Nn].*"                                               /* RESID_YORN_NO */
};


/*
 *   Load a string resource
 */
int os_get_str_rsc(int id, char *buf, size_t buflen)
{
    /* 
     *   make sure the index is in range of our array - note that the
     *   index values start at 1, but our array starts at 0, so adjust
     *   accordingly 
     */
    if (id < 0 || id >= (int)sizeof(S_res_strings)/sizeof(S_res_strings[0]))
        return 1;

    /* make sure the buffer is large enough */
    if (buflen < strlen(S_res_strings[id-1]) + 1)
        return 2;

    /* copy the string */
    strcpy(buf, S_res_strings[id-1]);

    /* return success */
    return 0;
}

