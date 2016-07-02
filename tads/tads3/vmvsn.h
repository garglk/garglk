/* $Header$ */

/* 
 *   Copyright (c) 1999, 2006 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmvsn.h - VM Version Information
Function
  
Notes
  
Modified
  07/12/99 MJRoberts  - Creation
*/

#ifndef VMVSN_H
#define VMVSN_H

/*
 *   The VM version number.  A VM program can obtain this value through
 *   the get_vm_vsn() function in the T3VM intrinsic function set.
 *   
 *   The value is encoded as a 32-bit value with the major version number
 *   in the high-order 16 bits, the minor version number in the next 8
 *   bits, and the patch release number in the low-order 8 bits.  So, the
 *   release 1.2.3 would be encoded as 0x00010203.  
 */
#define MAKE_VERSION_NUMBER(major,minor,maint) \
    (((major) << 16) | ((minor) << 8) | (maint))
#define T3VM_VSN_NUMBER  MAKE_VERSION_NUMBER(3,1,3)

/*
 *   The VM identification string 
 */
#define T3VM_IDENTIFICATION "mjr-T3"

/*
 *   The VM short version string.  This contains merely the version number,
 *   in display format.  
 */
#define T3VM_VSN_STRING "3.1.3"

/*
 *   The VM banner string.  A VM program can obtain this value through the
 *   get_vm_banner() function in the T3VM intrinsic function set. 
 */
/* copyright-date-string */
#define T3VM_BANNER_STRING \
    "T3 VM " T3VM_VSN_STRING " - Copyright 1999, 2012 Michael J. Roberts"

#endif /* VMVSN_H */

