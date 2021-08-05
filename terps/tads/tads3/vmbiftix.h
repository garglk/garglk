/* Copyright (c) 2005 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmbiftix.h - function set definition - TADS I/O Extensions
Function
  The I/O Extensions function set provides access to some UI functionality
  beyond the basic tads-io set.
Notes
  
Modified
  03/02/05 MJRoberts  - Creation
*/

#ifndef VMBIFTIX_H
#define VMBIFTIX_H

#include "os.h"
#include "vmbif.h"
#include "utf8.h"

class CVmBifTIOExt: public CVmBif
{
public:
    /* function vector */
    static vm_bif_desc bif_table[];

    /* show a popup menu */
    static void show_popup_menu(VMG_ uint argc);

    /* enable/disable system menu command */
    static void enable_sys_menu_cmd(VMG_ uint argc);
};


#endif /* VMBIFTIX_H */

/* ------------------------------------------------------------------------ */
/*
 *   Function set vector.  Define this only if VMBIF_DEFINE_VECTOR has been
 *   defined, so that this file can be included for the prototypes alone
 *   without defining the function vector.
 *   
 *   Note that this vector is specifically defined outside of the section of
 *   the file protected against multiple inclusion.  
 */
#ifdef VMBIF_DEFINE_VECTOR

/* TADS input/output extension functions */
vm_bif_desc CVmBifTIOExt::bif_table[] =
{
    { &CVmBifTIOExt::show_popup_menu, 3, 0, FALSE },
    { &CVmBifTIOExt::enable_sys_menu_cmd, 2, 0, FALSE }
};

#endif /* VMBIF_DEFINE_VECTOR */
