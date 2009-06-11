/* $Header$ */

/* 
 *   Copyright (c) 1997, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  osifctyp.h - TADS OS interface datatype definitions
Function
  Defines certain datatypes used in the TADS operating system interface
Notes
  DO NOT MODIFY THIS FILE WITH PLATFORM-SPECIFIC DEFINITIONS.  This file
  defines types that are part of the portable interface; these types do
  not vary by platform.

  These types are defined separately from osifc.h so that they can be
  #include'd before the OS-specific header.  osifc.h is included after
  the OS-specific header.
*/


/* ------------------------------------------------------------------------ */
/* 
 *   File types.  These type codes are used when opening or creating a
 *   file, so that the OS routine can set appropriate file system metadata
 *   to describe or find the file type.
 *   
 *   The type os_filetype_t is defined for documentary purposes; it's
 *   always just an int.  
 */
typedef int os_filetype_t;
#define OSFTGAME 0                               /* a game data file (.gam) */
#define OSFTSAVE 1                                   /* a saved game (.sav) */
#define OSFTLOG  2                               /* a transcript (log) file */
#define OSFTSWAP 3                                             /* swap file */
#define OSFTDATA 4      /* user data file (used with the TADS fopen() call) */
#define OSFTCMD  5                                   /* QA command/log file */
#define OSFTERRS 6                                    /* error message file */
#define OSFTTEXT 7                     /* text file - used for source files */
#define OSFTBIN  8          /* binary file of unknown type - resources, etc */
#define OSFTCMAP 9                                /* character mapping file */
#define OSFTPREF 10                                     /* preferences file */
#define OSFTUNK  11         /* unknown - as a filter, matches any file type */
#define OSFTT3IMG 12                 /* T3 image file (.t3 - formerly .t3x) */
#define OSFTT3OBJ 13                               /* T3 object file (.t3o) */
#define OSFTT3SYM 14                        /* T3 symbol export file (.t3s) */
#define OSFTT3SAV 15                          /* T3 saved state file (.t3v) */


