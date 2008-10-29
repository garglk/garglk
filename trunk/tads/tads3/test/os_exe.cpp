#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  os_exe.cpp - dummy implementation os os_exeseek() for test programs
Function
  On some systems, it is desirable to link the test programs without a
  real implementation of os_exeseek(), to save space and avoid dragging
  in other dependency code.  This module provides a dummy implementation
  for use in such situations.  This module can be ignored for systems
  where it is not undesirable to link the test programs with the real
  os_exeseek implementation.
Notes
  
Modified
  06/24/00 MJRoberts  - Creation
*/

#include "os.h"

/*
 *   dummy implementation 
 */
osfildef *os_exeseek(const char *, const char *)
{
    return 0;
}
