/* $Header: d:/cvsroot/tads/TADS2/RES.H,v 1.1 1999/07/11 00:46:30 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  res.h - string resource definitions for TADS
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

#ifndef _RES_H_
#define _RES_H_

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
#define RESID_BTN_OK       1
#define RESID_BTN_CANCEL   2

/* "Yes" and "No" buttons */
#define RESID_BTN_YES      3
#define RESID_BTN_NO       4

/*
 *   Reply strings for the yorn() built-in function.  These strings are
 *   regular expressions as matched by the regex.h functions.  For
 *   English, for example, the "yes" string would be "[Yy].*" and the "no"
 *   string would be "[Nn].*".  For German, it might be desirable to
 *   accept both "Ja" and "Yes", so the "Yes" string might be "[JjYy].*".
 *   
 *   It's not necessary in these patterns to consider leading spaces,
 *   since the yorn() function will skip any leading spaces before
 *   performing the pattern match.  
 */
#define RESID_YORN_YES     5
#define RESID_YORN_NO      6


#endif /* _RES_H_ */

