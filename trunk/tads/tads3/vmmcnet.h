/* $Header: d:/cvsroot/tads/tads3/VMMCCORE.H,v 1.2 1999/05/17 02:52:28 MJRoberts Exp $ */

/* 
 *   Copyright (c) 2010 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmmcnet.h - Networking Metaclass Registrations
Function

Notes

Modified
  12/01/98 MJRoberts  - Creation
*/

/* 
 *   NOTE - this file is INTENTIONALLY not protected against multiple
 *   inclusion.  Because of the funny business involved in buildling the
 *   registration tables, we must include this file more than once in
 *   different configurations.  Therefore, there's no #ifndef
 *   VMMCCORE_INCLUDED test at all in this file.  
 */

#include "vmhttpsrv.h"
#include "vmhttpreq.h"
