#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmhostsi.cpp - stdio-based VM host application environment
Function
  
Notes
  
Modified
  08/06/99 MJRoberts  - Creation
*/

#include "t3std.h"
#include "os.h"
#include "resload.h"
#include "vmhost.h"
#include "vmhostsi.h"

/*
 *   initialize 
 */
CVmHostIfcStdio::CVmHostIfcStdio(const char *argv0)
{
    char buf[OSFNMAX];

    /* remember the program's argv[0], in case we need it later */
    argv0_ = lib_copy_str(argv0);

    /* 
     *   Create the resource loader for system resources (character mapping
     *   files, etc) in the same directory as the executable. 
     */
    os_get_special_path(buf, sizeof(buf), argv0, OS_GSP_T3_RES);
    sys_res_loader_ = new CResLoader(buf);

    /* set the executable filename in the loader, if available */
    if (os_get_exe_filename(buf, sizeof(buf), argv0))
        sys_res_loader_->set_exe_filename(buf);

    /* 
     *   the default safety level allows reading and writing to the current
     *   directory only 
     */
    io_safety_read_ = VM_IO_SAFETY_READWRITE_CUR;
    io_safety_write_ = VM_IO_SAFETY_READWRITE_CUR;
    net_client_safety_ = VM_NET_SAFETY_LOCALHOST;
    net_server_safety_ = VM_NET_SAFETY_LOCALHOST;
}

/*
 *   delete 
 */
CVmHostIfcStdio::~CVmHostIfcStdio()
{
    /* delete our system resource loader */
    delete sys_res_loader_;

    /* delete our saved argv[0] */
    lib_free_str(argv0_);
}
