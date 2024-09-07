/* Copyright (c) 1992, 2004 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  h_ppc.h - TADS hardware abstraction definitions for PowerPC processor
Function
  This file provides some machine-dependent routines that allow the TadsVM
  to run on big-endian CPUs (PowerPC on a Mac running OS X, for example).

  These definitions are for 32-bit machines only.  These probably will
  NOT work on 64-bit hardware.
Notes
  Created by Nikos Chantziaras.
  
  The arithmetics used in this code are stolen from Iain Merrick's
  HyperTADS interpreter.

  The definitions provided here are actually quite generic; they are
  perfectly suitable for little-endian CPUs, like Intel x86 compatibles.
  Anyway, they shouldn't be used for Intel machines, as this generic version
  of the routines is somewhat slower than the ones written specifically for
  x86 CPUs, as Intel compatibles provide hardware support for these routines;
  on Intel compatibles, h_ix86.h should be used instead.
  
  Note that Iain Merrick's original file assigns the copyright to Mike
  Roberts, so that's what I'm going to do here too.
Modified
  05/19/04 MJRoberts - added to main TADS source tree
  05/17/04 Nikos Chantziaras - created
*/

#ifndef H_PPC_H
#define H_PPC_H

/* 
 *   Round a size up to worst-case alignment boundary.  
 */
#define osrndsz(s) (((s) + 3) & ~3)

/* 
 *   Round a pointer up to worst-case alignment boundary.  
 */
#define osrndpt(p) ((unsigned char*)((((unsigned long)(p)) + 3) & ~3))

/* 
 *   Service macros for osrp2 etc.  
 */
#define osc2u(p,i) ((unsigned short)(((unsigned char*)(p))[i]))
#define osc2l(p,i) ((unsigned long)(((unsigned char*)(p))[i]))
#define osc2s(p,i) ((short)(((signed char*)(p))[i]))
#define osc2sl(p,i) ((long)(((signed char*)(p))[i]))

/* 
 *   Read an unaligned portable unsigned 2-byte value, returning an int
 *   value.  
 */
#define osrp2(p) (osc2u(p,0) + (osc2u(p,1) << 8))

/* 
 *   Read an unaligned portable signed 2-byte value, returning int.  
 */
#define osrp2s(p) (((short)osc2u(p,0)) + (osc2s(p,1) << 8))

/* 
 *   Write int to unaligned portable 2-byte value.  
 */
#define oswp2(p,i) \
    ((((unsigned char*)(p))[1] = (i) >> 8), \
     (((unsigned char*)(p))[0] = (i) & 255))

#define oswp2s(p,i) oswp2(p,i)

/* 
 *   Read an unaligned portable 4-byte value, returning long.  
 */
#define osrp4s(p) \
    (((long)osc2l(p,0)) \
     + ((long)(osc2l(p,1)) << 8) \
     + ((long)(osc2l(p,2) << 16)) \
     + (osc2sl(p,3) << 24))

/* 
 *   Read an unaligned portable 4-byte value, returning unsigned long.  
 */
#define osrp4(p) \
    ((osc2l(p,0)) \
     + ((osc2l(p,1)) << 8) \
     + ((osc2l(p,2) << 16)) \
     + (osc2l(p,3) << 24))

/* 
 *   Write a long to an unaligned portable 4-byte value.  
 */
#define oswp4(p,i) \
    ((((unsigned char*)(p))[0] = (i)), \
     (((unsigned char*)(p))[1] = (i) >> 8), \
     (((unsigned char*)(p))[2] = (i) >> 16, \
     (((unsigned char*)(p))[3] = (i) >> 24)))

#define oswp4s(p,i) oswp4(p,i)

#endif /* H_PPC_H */
