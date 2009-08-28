/* $Header: d:/cvsroot/tads/TADS2/H_IX86.H,v 1.2 1999/05/17 02:52:12 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  h_ix86.h - hardware definitions for Intel x86.
Function
  These definitions are for 16-bit and 32-bit Intel CPUs.  Note that these
  probably will NOT work on 64-bit Intel hardware, because we assume that
  the largest type is 32 bits.
Notes

Modified
  10/17/98 MJRoberts  - Creation
*/

#ifndef H_IX86_H
#define H_IX86_H

/* round a size to worst-case alignment boundary */
#define osrndsz(s) (((s)+3) & ~3)

/* round a pointer to worst-case alignment boundary */
#define osrndpt(p) ((uchar *)((((ulong)(p)) + 3) & ~3))

/* read unaligned portable unsigned 2-byte value, returning int */
#define osrp2(p) ((int)*(unsigned short *)(p))

/* read unaligned portable signed 2-byte value, returning int */
#define osrp2s(p) ((int)*(short *)(p))

/* write int to unaligned portable 2-byte value */
#define oswp2(p, i) (*(unsigned short *)(p)=(unsigned short)(i))

/* read unaligned portable 4-byte value, returning long */
#define osrp4(p) (*(long *)(p))

/* write long to unaligned portable 4-byte value */
#define oswp4(p, l) (*(long *)(p)=(l))


#endif /* H_IX86_H */

