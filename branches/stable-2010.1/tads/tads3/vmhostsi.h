/* $Header$ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmhostsi.h - simple stdio-based VM host application environment
Function
  Provides a simple implementation of the VM host application interface.
  This implementation is suitable for simple command-line tools with
  minimal user interface; more complete implementations should be used
  for most applications that embed the VM.
Notes
  
Modified
  07/29/99 MJRoberts  - Creation
*/

#ifndef VMHOSTSI_H
#define VMHOSTSI_H

#include "vmhost.h"
#include "vmhosttx.h"

class CVmHostIfcStdio: public CVmHostIfcText
{
public:
    /* create */
    CVmHostIfcStdio(const char *argv0);

    /* delete */
    virtual ~CVmHostIfcStdio();
    
    /* get the I/O safety level */
    virtual int get_io_safety() { return io_safety_; }

    /* set I/O safety level */
    virtual void set_io_safety(int level) { io_safety_ = level; }

    /* get the resource loader */
    virtual class CResLoader *get_cmap_res_loader() { return cmap_loader_; }

    /* get the resource path */
    virtual const char *get_res_path() { return 0; }

    /* get an image file name */
    virtual vmhost_gin_t get_image_name(char *, size_t)
        { return VMHOST_GIN_IGNORED; }

    /* get a special file system path */
    virtual void get_special_file_path(char *buf, size_t buflen, int id)
        { os_get_special_path(buf, buflen, argv0_, id); }

protected:
    /* 
     *   the original main program's argv[0] - we need to remember this
     *   because it's sometimes needed to resolve special file system paths
     *   on the local system 
     */
    char *argv0_;

    /* character mapping file resource loader */
    class CResLoader *cmap_loader_;

    /* current I/O safety level */
    int io_safety_;
};

#endif /* VMHOSTSI_H */

