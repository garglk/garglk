/* $Header: d:/cvsroot/tads/TADS2/RES.H,v 1.1 1999/07/11 00:46:30 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmstrres.h - string resource definitions for TADS 3
Function
  Defines the ID's for string resources.  These strings must be loaded
  when needed with os_get_str_rsc(), since the specific mechanism by which
  these strings are loaded varies by operating system.

  We assign the resources sequential numbers starting at 1, to make it
  easier to map the resource loader to an operating system mechanism
  where such a mechanism exists.
Notes
  
Modified
  06/25/99 MJRoberts  - Creation
*/

#ifndef VMSTRRES_H
#define VMSTRRES_H

/*
 *   Dialog buttons.  These provide the text for standard buttons in
 *   dialogs created with os_input_dialog().
 *   
 *   These labels can use "&" to indicate a shortcut letter, per the
 *   normal os_input_dialog() interface; for example, if the Yes button
 *   label is "&Yes", the button has the shortcut letter "Y".
 *   
 *   The text of these buttons may vary by system, since these should
 *   conform to local conventions where there are local conventions.  In
 *   addition, of course, these strings will vary by language.  
 */

/* OK and Cancel buttons */
#define VMRESID_BTN_OK       1
#define VMRESID_BTN_CANCEL   2

/* "Yes" and "No" buttons */
#define VMRESID_BTN_YES      3
#define VMRESID_BTN_NO       4


#endif /* VMSTRRES_H */

