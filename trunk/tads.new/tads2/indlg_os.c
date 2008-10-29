#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  indlg_tx.c - input dialog - formatted text-only version
Function
  Implements the input dialog function using formatted text
Notes
  Only one of indlg_tx or indlg_os should be included in a given
  executable.  The choice depends on whether a system-level dialog
  is available or not.  If no system-level dialog is available,
  include indlg_tx, which provides an implementation using formatted
  text.  If a system-level dialog is available, use indlg_os, which
  calls os_input_dialog().

  This file exists in the portable layer, rather than in the OS layer,
  so that we can provide an implementation using formatted text.  An
  OS-layer implementation could not provide formatted text.
Modified
  09/27/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <string.h>

#include "std.h"
#include "os.h"
#include "tio.h"

/* ------------------------------------------------------------------------ */
/*
 *   Text-mode os_input_dialog implementation 
 */
int tio_input_dialog(int icon_id, const char *prompt,
                     int standard_button_set,
                     const char **buttons, int button_count,
                     int default_index, int cancel_index)
{
    /* call the OS implementation */
    return os_input_dialog(icon_id, prompt, standard_button_set,
                           buttons, button_count,
                           default_index, cancel_index);
}
