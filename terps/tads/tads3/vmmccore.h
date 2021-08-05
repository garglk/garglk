/* $Header: d:/cvsroot/tads/tads3/VMMCCORE.H,v 1.2 1999/05/17 02:52:28 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmmccore.h - Core Metaclass Registrations
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

/* ------------------------------------------------------------------------ */
/*
 *   Before we begin, if we're building the registration table, redefine
 *   the CENTRAL REGISTRATION BUILDER version of the metaclass
 *   registration macro.  We do this here rather than in vmmcreg.cpp to
 *   make it easier to create separate versions of vmmcreg.cpp for
 *   separate subsystems.  This core file is always included, even in
 *   special configurations, before any other metaclass headers.  
 */
#ifdef VMMCCORE_BUILD_TABLE

#ifdef VM_REGISTER_METACLASS
#undef VM_REGISTER_METACLASS
#endif
#define VM_REGISTER_METACLASS(meta_class) \
    { &meta_class::metaclass_reg_ },

#endif /* VMMCCORE_BUILD_TABLE */

/* ------------------------------------------------------------------------ */
/*
 *   Now include each header for file the core metaclasses 
 */
#include "vmobj.h"
#include "vmtobj.h"
#include "vmstr.h"
#include "vmlst.h"
#include "vmdict.h"
#include "vmgram.h"
#include "vmbignum.h"
#include "vmintcls.h"
#include "vmanonfn.h"
#include "vmcoll.h"
#include "vmiter.h"
#include "vmvec.h"
#include "vmlookup.h"
#include "vmbytarr.h"
#include "vmcset.h"
#include "vmfilobj.h"
#include "vmtmpfil.h"
#include "vmfilnam.h"
#include "vmpat.h"
#include "vmstrcmp.h"
#include "vmstrbuf.h"
#include "vmdynfunc.h"
#include "vmfref.h"
#include "vmdate.h"
#include "vmtzobj.h"

/* if networking is enabled, include the networking metaclasses */
#ifdef TADSNET
#include "vmmcnet.h"
#endif

