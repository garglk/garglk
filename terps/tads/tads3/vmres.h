/* $Header: d:/cvsroot/tads/tads3/vmres.h,v 1.2 1999/05/17 02:52:30 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmres.h - resource object implementation
Function
  A resource is a named binary byte stream stored within the image
  file.  To the VM, resources are opaque; the VM merely maintains the
  resource name table, and provides access to the byte stream to the
  user program.
Notes
  
Modified
  04/03/99 MJRoberts  - Creation
*/

#ifndef VMRES_H
#define VMRES_H

#include <stdlib.h>
#include "t3std.h"

class CVmResource
{
    friend class CVmImageLoader;
    
public:
    CVmResource(long seek_pos, uint32_t len, size_t name_len);
    ~CVmResource();

    /* get the seek position */
    long get_seek_pos() const { return seek_pos_; }

    /* get the length of the byte stream */
    uint32_t get_len() const { return len_; }

    /* get my name */
    const char *get_name() const { return name_; }

    /* get/set next resource in list */
    CVmResource *get_next() const { return nxt_; }
    void set_next(CVmResource *nxt) { nxt_ = nxt; }

private:
    /* get my name buffer */
    char *get_name_buf() const { return name_; }

    /* seek position in image file of my binary data */
    long seek_pos_;

    /* length in bytes of my binary data stream */
    uint32_t len_;

    /* name string */
    char *name_;

    /* next resource in list */
    CVmResource *nxt_;
};

#endif /* VMRES_H */
