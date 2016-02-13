/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmhosttx.cpp - text-only host interface implementation
Function
  Provides a base class for the T3 VM Host Interface for implementing
  text-only applications.
Notes
  
Modified
  06/16/02 MJRoberts  - Creation
*/

#include "os.h"
#include "t3std.h"
#include "vmhash.h"
#include "vmhost.h"
#include "vmhosttx.h"

/* ------------------------------------------------------------------------ */
/*
 *   Hash table entry for a resource descriptor 
 */
class CResEntry: public CVmHashEntryCI
{
public:
    CResEntry(const char *resname, size_t resnamelen, int copy,
              unsigned long ofs, unsigned long siz, int fileno)
        : CVmHashEntryCI(resname, resnamelen, copy)
    {
        /* remember the file locator information */
        fileno_ = fileno;
        ofs_ = ofs;
        siz_ = siz;
        link_ = 0;
    }

    CResEntry(const char *resname, size_t resnamelen, int copy,
              const char *fname, size_t fnamelen)
        : CVmHashEntryCI(resname, resnamelen, copy)
    {
        /* save the local filename */
        link_ = lib_copy_str(fname, fnamelen);

        /* it's not in a resource file */
        fileno_ = 0;
        ofs_ = 0;
        siz_ = 0;
    }

    ~CResEntry()
    {
        lib_free_str(link_);
    }

    /* file number (this is an index in the hostifc's ext_ array) */
    int fileno_;

    /* byte offset of the start of the resource data in the file */
    unsigned long ofs_;

    /* byte size of the resource data */
    unsigned long siz_;

    /* local filename, for a resource link (rather than a stored resource) */
    char *link_;
};

/* ------------------------------------------------------------------------ */
/*
 *   construction 
 */
CVmHostIfcText::CVmHostIfcText()
{
    /* create our hash table */
    restab_ = new CVmHashTable(128, new CVmHashFuncCI(), TRUE);

    /* allocate an initial set of external resource filename entries */
    ext_max_ = 10;
    ext_ = (char **)t3malloc(ext_max_ * sizeof(ext_[0]));

    /* we use slot zero for the image filename, which we don't know yet */
    ext_cnt_ = 1;
    ext_[0] = 0;

    /* no resource directory specified yet */
    res_dir_ = 0;
}

/*
 *   deletion 
 */
CVmHostIfcText::~CVmHostIfcText()
{
    /* delete our hash table */
    delete restab_;

    /* delete our external filenames */
    for (size_t i = 0 ; i < ext_cnt_ ; ++i)
        lib_free_str(ext_[i]);

    /* delete our array of external filename entries */
    t3free(ext_);

    /* delete our resource directory path */
    lib_free_str(res_dir_);
}

/* 
 *   set the image file name - we always use resource file slot zero to
 *   store the image file 
 */
void CVmHostIfcText::set_image_name(const char *fname)
{
    /* free any old name string */
    lib_free_str(ext_[0]);

    /* remember the new name */
    ext_[0] = lib_copy_str(fname);
}

/*
 *   set the resource directory 
 */
void CVmHostIfcText::set_res_dir(const char *dir)
{
    /* forget any previous setting, and remember the new path */
    lib_free_str(res_dir_);
    res_dir_ = lib_copy_str(dir);
}

/* 
 *   add a resource file 
 */
int CVmHostIfcText::add_resfile(const char *fname)
{
    /* expand the resource file list if necessary */
    if (ext_cnt_ == ext_max_)
    {
        /* bump up the maximum a bit */
        ext_max_ += 10;

        /* reallocate the entry pointer array */
        ext_ = (char **)t3realloc(ext_, ext_max_ * sizeof(ext_[0]));
    }

    /* store the new entry */
    ext_[ext_cnt_++] = lib_copy_str(fname);

    /* 
     *   return the new entry's file number (we've already bumped the index,
     *   so it's the current count minus one) 
     */
    return ext_cnt_ - 1;
}

/* 
 *   add a resource 
 */
void CVmHostIfcText::add_resource(unsigned long ofs, unsigned long siz,
                                  const char *resname, size_t resnamelen,
                                  int fileno)
{
    CResEntry *entry;
    
    /* create a new entry desribing the resource */
    entry = new CResEntry(resname, resnamelen, TRUE, ofs, siz, fileno);

    /* add it to the table */
    restab_->add(entry);
}

/* 
 *   add a resource 
 */
void CVmHostIfcText::add_resource(const char *fname, size_t fnamelen,
                                  const char *resname, size_t resnamelen)
{
    /* create a new entry desribing the resource */
    CResEntry *entry = new CResEntry(
        resname, resnamelen, TRUE, fname, fnamelen);

    /* add it to the table */
    restab_->add(entry);
}

/* 
 *   find a resource 
 */
osfildef *CVmHostIfcText::find_resource(const char *resname,
                                        size_t resnamelen,
                                        unsigned long *res_size)
{
    osfildef *fp;
    char buf[OSFNMAX];
    char *fname;
    char fname_buf[OSFNMAX];
    char res_dir[OSFNMAX] = "";

    /* 
     *   get the resource directory - if there's an explicit resource path
     *   specified, use that, otherwise use the image file folder 
     */
    if (res_dir_ != 0)
        lib_strcpy(res_dir, sizeof(res_dir), res_dir_);
    else if (ext_[0] != 0)
        os_get_path_name(res_dir, sizeof(res_dir), ext_[0]);

    /* try finding an entry in the resource map */
    CResEntry *entry = (CResEntry *)restab_->find(resname, resnamelen);
    if (entry != 0)
    {
        /* found it - check the type */
        if (entry->link_ == 0)
        {
            /* it's a stored binary resource - load it */
            fp = osfoprb(ext_[entry->fileno_], OSFTBIN);
            
            /* if that succeeded, seek to the start of the resource */
            if (fp != 0)
                osfseek(fp, entry->ofs_, OSFSK_SET);
            
            /* tell the caller the size of the resource */
            *res_size = entry->siz_;
            
            /* return the file handle */
            return fp;
        }
        else
        {
            /* it's a link to a local file */
            fname = entry->link_;
        }
    }
    else
    {
        /* 
         *   There's no entry in the resource map, so convert the resource
         *   name from the URL notation to local file system conventions, and
         *   look for a file with the given name.  This is allowed only if
         *   the file safety level setting is 3 (read only local directory
         *   access) or lower.  
         */
        if (get_io_safety_read() > 3)
            return 0;
        
        /*   
         *   Make a null-terminated copy of the resource name, limiting the
         *   copy to our buffer size.  
         */
        if (resnamelen > sizeof(buf) - 1)
            resnamelen = sizeof(buf) - 1;
        memcpy(buf, resname, resnamelen);
        buf[resnamelen] = '\0';

        /* convert the resource name to a URL */
        os_cvt_url_dir(fname_buf, sizeof(fname_buf), buf);
        fname = fname_buf;

        /* if that yields an absolute path, it's an error */
        if (os_is_file_absolute(fname))
            return 0;

        /* 
         *   If it's not in the resource file folder, it's also an error - we
         *   don't allow paths to parent folders via "..", for example.  If
         *   we don't have an resource file folder to compare it to, fail,
         *   since we can't properly sandbox it.  Resource files are always
         *   sandboxed to the resource directory, even if the file safety
         *   settings are less restrictive.
         */
        if (res_dir[0] == 0
            || !os_is_file_in_dir(fname, res_dir, TRUE, FALSE))
            return 0;
    }

    /* 
     *   If we get this far, it's because we have a local file name in
     *   'fname' that we need to look up.  
     */

    /* check the path for relativity */
    if (os_is_file_absolute(fname))
    {
        /* it's already an absolute, fully-qualified path - use it as-is */
        lib_strcpy(buf, sizeof(buf), fname);
    }
    else
    {
        /* 
         *   it's a relative path - make sure we have a resource directory
         *   for it to be relative to 
         */
        if (res_dir[0] == 0)
            return 0;

        /* 
         *   build the full path name by combining the image file path with
         *   the relative path we got from the resource name URL, as
         *   converted local file system conventions 
         */
        os_build_full_path(buf, sizeof(buf), res_dir, fname);
    }

    /* try opening the file */
    fp = osfoprb(buf, OSFTBIN);

    /* return failure if we couldn't find the file */
    if (fp == 0)
        return 0;

    /* 
     *   the entire file is the resource data, so figure out how big the file
     *   is, and tell the caller that the resource size is the file size 
     */
    osfseek(fp, 0, OSFSK_END);
    *res_size = osfpos(fp);

    /* 
     *   seek back to the start of the resource data (which is simply the
     *   start of the file, since the entire file is the resource data) 
     */
    osfseek(fp, 0, OSFSK_SET);
        
    /* return the file handle */
    return fp;
}

/* 
 *   determine if a resource exists 
 */
int CVmHostIfcText::resfile_exists(const char *resname, size_t resnamelen)
{
    /* try opening the resource file */
    unsigned long res_size;
    osfildef *fp = find_resource(resname, resnamelen, &res_size);

    /* check to see if we successfully opened the resource */
    if (fp != 0)
    {
        /* found it - close the file and return success */
        osfcls(fp);
        return TRUE;
    }
    else
    {
        /* couldn't find it - indicate failure */
        return FALSE;
    }
}

