/* 
 *   Copyright (c) 1991, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  t3_os.h - miscellaneous system-specific definitions for T3
Function
  Various standard definitions
Notes
  None
Modified
  05/31/03 MJRoberts  - creation
*/

/* ------------------------------------------------------------------------ */
/*
 *   Generic definitions.  We'll start with a set of default macro
 *   definitions that should be usable on most platforms.  System-specific
 *   versions can override these by #undefing and re-#defining them later,
 *   in #ifdef-protected sections for those specific platforms.  
 */

/*
 *   The name of the default project makefile.  The t3make program will look
 *   for a file with this name if no makefile is specifically identified
 *   (with the t3make -f option) and no source files are specified on the
 *   command line.
 *   
 *   Note that if this is overridden, it should NOT specify a directory
 *   prefix; the default makefile should always be sought in the current
 *   working directory.  Also, note that gratuitous changes to the name are
 *   discouraged; ports should only change the name as needed to conform to
 *   local conventions, and then should only change it as much as needed,
 *   and ideally in such a way that people accustomed to working with the
 *   local system would typically map "makefile.t3m" to local conventions.
 *   
 *   For example, one good reason to change the name would be that the
 *   platform only allows six-character filenames; in these cases we'd want
 *   to choose a reasonable abbreviation, such as "mkfile".  Another good
 *   reason would be that periods are not valid filename characters.
 *   Period-delimited suffixes are such a widespread convention that it's
 *   likely that users of such a platform would have adopted a standard (or
 *   at least typical) mapping for these suffixes; so that mapping should be
 *   applied.  A third good reason would be local upper/lower-case
 *   conventions.  
 */
#define T3_DEFAULT_PROJ_FILE "makefile.t3m"



/* ------------------------------------------------------------------------ */
/*
 *   Unix-specific definitions 
 */
#ifdef UNIX

/*
 *   Redefine the default project makefile to conform to Unix conventions.
 *   Unix makefiles are conventionally called "Makefile" - the "M" is
 *   capitalized to take advantage of (1) ASCII sorting order and (2) the
 *   fact that source files conventionally use all-lower-case names, so that
 *   the makefile sorts ahead of its related source files in directory
 *   listings.  Cheesy but effective.  We'll follow the convention by
 *   looking for "Makefile.t3m" by default.  
 */
#undef  T3_DEFAULT_PROJ_FILE
#define T3_DEFAULT_PROJ_FILE "Makefile.t3m"

#endif /* UNIX */

