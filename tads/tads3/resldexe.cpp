/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  resldexe.cpp - resource loader - executable file loader
Function
  Loads resources from the executable file, if possible.
Notes
  This module is separated from the main resload.cpp module to allow
  the executable resource loader to be omitted from the link.  If the
  program doesn't want to be able to load from the executable, the link
  can substitute resnoexe.cpp for this module.
Modified
  11/03/01 MJRoberts  - Creation
*/

#include <stddef.h>
#include <string.h>

#include "t3std.h"
#include "resload.h"
#include "vmimage.h"

/* ------------------------------------------------------------------------ */
/*
 *   Resource loader interface implementation 
 */
class CVmImageLoaderMres_resload: public CVmImageLoaderMres
{
public:
    CVmImageLoaderMres_resload(const char *respath)
    {
        /* remember the path of the resource we're trying to find */
        respath_ = respath;
        respath_len_ = strlen(respath);

        /* we haven't found it yet */
        found_ = FALSE;
        link_ = 0;
    }

    ~CVmImageLoaderMres_resload()
    {
        lib_free_str(link_);
    }

    /* add a resource */
    void add_resource(uint32_t seek_ofs, uint32_t siz,
                      const char *res_name, size_t res_name_len)
    {
        /* 
         *   if we've already found a match, there's no need to consider
         *   anything else 
         */
        if (found_)
            return;

        /* check to see if this is the one we're looking for */
        if (res_name_len == respath_len_
            && memicmp(respath_, res_name, res_name_len) == 0)
        {
            /* we found it */
            found_ = TRUE;

            /* remember the seek location */
            res_seek_ = seek_ofs;
            res_size_ = siz;
        }
    }

    /* add a resource link */
    void add_resource(const char *fname, size_t fnamelen,
                      const char *res_name, size_t res_name_len)
    {
        /* 
         *   if we've already found a match, there's no need to consider
         *   anything else 
         */
        if (found_)
            return;

        /* check to see if this is the one we're looking for */
        if (res_name_len == respath_len_
            && memicmp(respath_, res_name, res_name_len) == 0)
        {
            /* we found it */
            found_ = TRUE;

            /* remember the link */
            link_ = lib_copy_str(fname, fnamelen);
        }
    }

    /* did we find the resource? */
    int found_resource() const { return found_; }

    /* get the seek location and size of the resource we found */
    uint32_t get_resource_seek() const { return res_seek_; }
    uint32_t get_resource_size() const { return res_size_; }

    /* get the local filename link, if it's given as a link */
    const char *get_link_fname() const { return link_; }

private:
    /* name of the resource we're looking for */
    const char *respath_;
    size_t respath_len_;

    /* flag: we found the resource we're looking for */
    int found_;

    /* seek location and size of the resource we found */
    uint32_t res_seek_;
    uint32_t res_size_;

    /* local filename link, if the resource is given as a link */
    char *link_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Try loading a resource from the executable file 
 */
osfildef *CResLoader::open_exe_res(const char *respath,
                                   const char *restype)
{
    osfildef *exe_fp;

    /* 
     *   if we don't have an executable filename stored, or we don't have an
     *   executable resource type ID, we can't load the resource 
     */
    if (exe_filename_ == 0 || restype == 0)
        return 0;
    
    /* find the executable file's resources */
    exe_fp = os_exeseek(exe_filename_, restype);

    /* if we found something, try loading from that file */
    if (exe_fp != 0)
    {
        CVmImageLoaderMres_resload res_ifc(respath);
        
        /* try loading the resources */
        CVmImageLoader::
            load_resources_from_fp(exe_fp, exe_filename_, &res_ifc);
        
        /* check to see if we found it */
        if (res_ifc.found_resource())
        {
            /* check the type */
            if (res_ifc.get_link_fname() != 0)
            {
                /* 
                 *   it's a linked local file - close the exe file and open
                 *   the local file instead 
                 */
                osfcls(exe_fp);
                exe_fp = osfoprb(res_ifc.get_link_fname(), OSFOPRB);
            }
            else
            {
                /* we got an exe resource - seek to the starting byte */
                osfseek(exe_fp, res_ifc.get_resource_seek(), OSFSK_SET);
            }
        }
        else
        {
            /* didn't find it - close and forget the executable file */
            osfcls(exe_fp);
            exe_fp = 0;
        }
    }

    /* return the executable file pointer, if we found the resource */
    return exe_fp;
}

/* ------------------------------------------------------------------------ */
/*
 *   Try loading a resource from a resource library
 */
osfildef *CResLoader::open_lib_res(const char *libfile,
                                   const char *respath)
{
    osfildef *fp;

    /* try opening the file */
    fp = osfoprb(libfile, OSFTT3IMG);
    
    /* if we couldn't open the file, we can't load the resource */
    if (fp == 0)
        return 0;

    /* set up a resource finder for our resource */
    CVmImageLoaderMres_resload res_ifc(respath);
    
    /* load the file, so that we can try finding the resource */
    CVmImageLoader::load_resources_from_fp(fp, libfile, &res_ifc);
    
    /* check to see if we found it */
    if (res_ifc.found_resource())
    {
        /* we got it - check the type */
        if (res_ifc.get_link_fname() != 0)
        {
            /* 
             *   linked local file - close the library file and open the
             *   local file instead 
             */
            osfcls(fp);
            fp = osfoprb(res_ifc.get_link_fname(), OSFOPRB);
        }
        else
        {
            /* embedded resource - seek to the first byte */
            osfseek(fp, res_ifc.get_resource_seek(), OSFSK_SET);
        }

        /* return the library file handle */
        return fp;
    }
    else
    {
        /* didn't find the resource - close the library */
        osfcls(fp);

        /* tell the caller we didn't find the resource */
        return 0;
    }
}
