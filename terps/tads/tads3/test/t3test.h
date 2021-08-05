/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  t3test.h - common test definitions
Function
  Provides some common definitions for the tads 3 test programs
Notes
  
Modified
  11/11/01 MJRoberts  - Creation
*/

#ifndef T3TEST_H
#define T3TEST_H

/* ------------------------------------------------------------------------ */
/*
 *   Test initialization.  Each test program invokes the macro test_init()
 *   at the start of its processing.  This can be defined as needed per
 *   platform. 
 */

/*
 *   For Unix systems, at initialization time, turn off buffering on
 *   standard output.  This will ensure that output sent to stdout is
 *   intermingled with stderr output in the same order in which the output
 *   is generated.  If we leave stdout buffered, the order of stderr/stdout
 *   output will not be preserved, so the captured log file won't be
 *   predictable. 
 */
#ifdef UNIX
#define test_init() setbuf(stdout, NULL)
#endif

/*
 *   If we haven't defined a system-dependent test_init() by now, it must
 *   not be needed on the local system, so define it to nothing. 
 */
#ifndef test_init
#define test_init()
#endif


#endif /* T3TEST_H */

