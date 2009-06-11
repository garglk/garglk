/* $Header: d:/cvsroot/tads/TADS2/H_IX86.H,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2008 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  h_ix86_64.h - hardware definitions for x86-64 (64-bit x86) platforms
Function
  These definitions are designed ONLY for 64-bit x86 CPUs.  Note that these
  probably will NOT work on 16- and 32-bit x86 hardware.
Notes
  IMPORTANT!  See the note below about the typedef for hix_int32.
Modified
  03/25/08 MJRoberts  - created (from h_ix86.h)
*/

#ifndef H_IX86_64_H
#define H_IX86_64_H

/* ------------------------------------------------------------------------ */
/* 
 *   Define local covers for the 32-bit and 16-bit integer types.
 *   
 *   For some of our operations, we need to translate between the local
 *   native types and the portable types.  The portable types are explicitly
 *   defined with exact byte sizes and byte orderings.  One way to do these
 *   translations is to read and write the portable values one byte at a
 *   time, and do some arithmetic to carry out the translation to the native
 *   types.  However, there's a more efficient approach: x86 CPUs happen to
 *   have native types that match our portable types in size and byte order,
 *   so the efficient way to do the translations is simply to use the correct
 *   native type that corresponds to each portable type.
 *   
 *   That's the purpose of these definitions - since the portable types are
 *   of specific sizes, we need to map the native type that exactly matches
 *   each portable type.
 *   
 *   This is complicated by a *second* level of mapping, which is from C
 *   types (short, int, long) to native types (WORD, DWORD, QWORD): this
 *   mapping can vary by compiler.
 *   
 *   So, our typedefs here are designed to give us the correct mapping from C
 *   types to portable types, by taking into account the way our compiler
 *   maps from C to native.
 */

#define SIZEOF_INT 4
#define SIZEOF_SHORT 2

#if SIZEOF_INT == 4
typedef int hix_int32;
#else
#error SIZEOF_INT must be defined in your makefile - \
 set SIZEOF_INT to the number of bytes in an 'int' for your compiler
#endif

#if SIZEOF_SHORT == 2
typedef short hix_int16;
typedef unsigned short hix_uint16;
#else
#error SIZEOF_SHORT must be defined in your makefile - \
 set SIZEOF_SHORT to the number of bytes in a 'short' for your compiler
#endif

/* ------------------------------------------------------------------------ */

/* round a size to worst-case alignment boundary */
#define osrndsz(s) (((s)+3) & ~3)

/* round a pointer to worst-case alignment boundary */
#define osrndpt(p) ((uchar *)((((ulong)(p)) + 3) & ~3))

/* read unaligned portable unsigned 2-byte value, returning int */
#define osrp2(p) ((int)*(hix_uint16 *)(p))

/* read unaligned portable signed 2-byte value, returning int */
#define osrp2s(p) ((int)*(hix_int16 *)(p))

/* write int to unaligned portable 2-byte value */
#define oswp2(p, i) (*(hix_uint16 *)(p)=(hix_uint16)(i))

/* read unaligned portable 4-byte value, returning long */
#define osrp4(p) (*(hix_int32 *)(p))

/* write long to unaligned portable 4-byte value */
#define oswp4(p, l) (*(hix_int32 *)(p)=(l))


#endif /* H_IX86_64_H */

