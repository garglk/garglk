/* $Header$ */

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  rcmain.h - T3 resource compiler main
Function
  
Notes
  
Modified
  01/03/00 MJRoberts  - Creation
*/

#ifndef RCMAIN_H
#define RCMAIN_H

#include "os.h"
#include "t3std.h"

/*
 *   Resource compiler main class
 */
class CResCompMain
{
public:
    /* 
     *   Add the given resources to an image file.  Returns zero on
     *   success, nonzero on failure.  If anything goes wrong, we'll use
     *   the given host interface to display error messages.  
     */
    static int add_resources(const char *image_fname,
                             const class CRcResList *reslist,
                             class CRcHostIfc *hostifc,
                             int create_new, os_filetype_t file_type,
                             int link_mode);

private:
    /* format an error message and display it via the host interface */
    static void disp_error(class CRcHostIfc *hostifc, const char *msg, ...);
};

/*
 *   Resource list element 
 */
class CRcResEntry
{
public:
    CRcResEntry(const char *fname, const char *url)
    {
        /* make a copy of the filename and URL */
        fname_ = lib_copy_str(fname);
        url_ = lib_copy_str(url);

        /* not in a list yet */
        next_ = 0;
    }

    ~CRcResEntry()
    {
        /* delete the filename and url strings */
        lib_free_str(fname_);
        lib_free_str(url_);
    }
    
    /* get/set the next entry in the list */
    CRcResEntry *get_next() const { return next_; }
    void set_next(CRcResEntry *nxt) { next_ = nxt; }

    /* get the entry's filename */
    const char *get_fname() const { return fname_; }

    /* get the entry's URL */
    const char *get_url() const { return url_; }

private:
    /* next entry in list */
    CRcResEntry *next_;

    /* filename */
    char *fname_;

    /* URL as stored in resource file */
    char *url_;
};

/*
 *   Resource operation modes 
 */
enum rcmain_res_op_mode_t
{
    /* add resources */
    RCMAIN_RES_OP_MODE_ADD,

    /* replace resources */
    RCMAIN_RES_OP_MODE_REPLACE,

    /* delete resources */
    RCMAIN_RES_OP_MODE_DELETE
};

/*
 *   Resource list object 
 */
class CRcResList
{
public:
    CRcResList()
    {
        /* nothing in the list yet */
        head_ = tail_ = 0;
        count_ = 0;
    }

    ~CRcResList()
    {
        /* delete the entire list */
        while (head_ != 0)
        {
            CRcResEntry *nxt;

            /* remember the next entry */
            nxt = head_->get_next();

            /* delete this entry */
            delete head_;

            /* move on */
            head_ = nxt;
        }
    }

    /* get the head of the list */
    class CRcResEntry *get_head() const { return head_; }

    /* get the number of elements in the list */
    unsigned int get_count() const { return count_; }

    /* 
     *   Add a file or directory.  If 'alias' is non-null, we'll store the
     *   alias string as-is (with no URL conversions or any other changes)
     *   as the name of the resource.  If 'alias' is null, we'll convert
     *   the filename to a URL using the normal rules and store the URL.
     *   
     *   If 'fname' refers to a directory rather than a file, we'll add
     *   all of the files within the directory.  'alias' is ignored in
     *   this case, since we construct a URL from the directory path.  If
     *   'recurse' is true, we'll also add the contents of any
     *   subdirectories found within the given directory; otherwise, we'll
     *   only add the files contained directly within the given directory.
     */
    void add_file(const char *fname, const char *alias, int recurse);

private:
    /* add an item to the list */
    void add_element(class CRcResEntry *entry)
    {
        /* add it to the end of my list */
        if (tail_ == 0)
            head_ = entry;
        else
            tail_->set_next(entry);
        tail_ = entry;

        /* count the new element */
        ++count_;
    }

    /* head of my list */
    class CRcResEntry *head_;

    /* tail of my list */
    class CRcResEntry *tail_;

    /* number of elements in the list */
    unsigned int count_;
};

/*
 *   Resource compiler host interface.  This provides access to
 *   application operations during resource compilation. 
 */
class CRcHostIfc
{
public:
    /* display an error message */
    virtual void display_error(const char *msg) = 0;

    /* display a status message */
    virtual void display_status(const char *msg) = 0;
};

#endif /* RCMAIN_H */

