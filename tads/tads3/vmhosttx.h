/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmhosttx.h - host interface base class for text-only implementations
Function
  Provides a base class for text-only T3 VM Host Interface
  implementations.  In particular, this class provides a simple resource
  manager implementation, which is usually needed in text-only interpreter
  systems because there is no larger host application (such as HTML TADS)
  that provides its own resource manager.

  Our resource manager implementation simply stores the names of the
  embedded resources in a hash table.
Notes

Modified
  07/29/99 MJRoberts  - Creation
*/

#ifndef VMHOSTTX_H
#define VMHOSTTX_H

#include "vmhost.h"

class CVmHostIfcText: public CVmHostIfc
{
public:
    CVmHostIfcText();
    ~CVmHostIfcText();

    /* set the image file name */
    virtual void set_image_name(const char *fname);

    /* set the resource directory */
    virtual void set_res_dir(const char *dir);

    /* add a resource file */
    virtual int add_resfile(const char *fname);

    /* we do support external resource files */
    virtual int can_add_resfiles() { return TRUE; }

    /* add a resource */
    virtual void add_resource(unsigned long ofs, unsigned long siz,
                              const char *resname, size_t resnamelen,
                              int fileno);

    /* add a resource file link */
    virtual void add_resource(const char *fname, size_t fnamelen,
                              const char *resname, size_t resnamelen);

    /* find a resource */
    virtual osfildef *find_resource(const char *resname, size_t resname_len,
                                    unsigned long *res_size);

    /* determine if a resource exists */
    virtual int resfile_exists(const char *resname, size_t resnamelen);

protected:
    /* resource map hash table */
    class CVmHashTable *restab_;

    /* array of external resource bundle filenames */
    char **ext_;
    size_t ext_cnt_;
    size_t ext_max_;

    /* resource root directory */
    char *res_dir_;
};

#endif /* VMHOSTTX_H */
