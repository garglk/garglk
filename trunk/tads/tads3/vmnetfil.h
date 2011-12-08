/* $Header$ */

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmnetfil.h - network file interface
Function
  This module was added in 3.1 to manage access to files stored on a
  network storage server when the game is running in web server mode.  In
  this mode, files are not stored on the same machine as the game server, nor
  on the user's client PC, but on a third machine known as the storage
  server.  The purpose of separating the game (execution) server and the
  storage server is that it allows execution servers to be interchangeable,
  as they don't store any state.  This lets us launch a game on any available
  server to distribute execution load.  The execution location is transparent
  to the user since all execution servers access the same storage pool.

  Access to remote files is handled by local caching.  When we open a file
  for reading, we download a temporary copy from the storage server via HTTP,
  and perform the local file operations on the temp copy.  If the file is
  opened for writing, we upload our temporary copy to the storage server on
  close, again via HTTP.
  
  The interface is implemented in two versions.  The Network version senses
  at run-time whether or not we're in web server mode; if so, it does the
  HTTP GET/PUT operations to download and upload the temp files, and if not
  it just opens files in the local file system.  The Local version can be
  used in builds that don't include the networking functionality at all; this
  version unconditionally maps to local files, and is a very thin layer over
  the regular file handling.

  Use the TADSNET macro to control the build configuration.  If the macro
  is defined, the network run-time checking is included in the build.  If
  not, the local-only version is selected, in which case the CVmNetFile
  object is basically a no-op that just passes through the local filename.
Notes
  
Modified
  09/08/10 MJRoberts  - Creation
*/

#ifndef VMNETFIL_H
#define VMNETFIL_H

#include "t3std.h"
#include "vmtype.h"
#include "os.h"


/* ------------------------------------------------------------------------ */
/*
 *   File modes for CVmNetFile::open() 
 */
#define NETF_READ      0x0001                      /* open with read access */
#define NETF_WRITE     0x0002                     /* open with write access */
#define NETF_CREATE    0x0004        /* create the file if it doesn't exist */
#define NETF_TRUNC     0x0008   /* discard existing contents if file exists */
#define NETF_DELETE    0x0010         /* no content access; delete the file */

/* composite flags for generic new file creation (create or replace) */
#define NETF_NEW       (NETF_WRITE | NETF_CREATE | NETF_TRUNC)



/* ------------------------------------------------------------------------ */
/*
 *   Network file object.
 *   
 *   The protocol for operating on a user file is relatively simple to use:
 *   
 *   1. Prepare the network file by calling CVmNetFile::open().  This returns
 *   a CVmNetFile object that you hang onto for the duration of the file
 *   manipulation - call this 'nfile'.
 *   
 *   2. Use any local file system API (e.g., any of the osfopxxx() functions)
 *   to open 'nfile->lclfname'.
 *   
 *   3. Use the local file system APIs (osfread(), osfwrite(), etc) to
 *   read/write the local file.
 *   
 *   4. Close the local file (with osfcls(), say).
 *   
 *   5. Close the network file by calling 'nfile->close()'.  This has the
 *   side effect of destroying 'nfile', so you don't have to manually
 *   'delete' it or otherwise dispose it.
 *   
 *   To retrofit this into existing osifc file management code, all that's
 *   required is to bracket the existing code with the CVmNetFile::open() and
 *   close() calls.  Note that open() and close() can throw errors, so
 *   protect these calls with err_try if you have to do any resource cleanup
 *   if an error is thrown.  
 */
class CVmNetFile
{
public:
    /*
     *   Are we in network storage server mode?  This returns true if we have
     *   a storage server, false if we're storing files in the local file
     *   system.
     */
    static int is_net_mode(VMG0_)
#ifdef TADSNET
        ;
#else
        /* local-only implementation - we definitely use local storage only */
        { return FALSE; }
#endif

    /*
     *   Open a file.  In local mode, this simply returns a CVmNetFile object
     *   with the given name in the lclfname field, so the caller will
     *   directly access the local file.  For a network file, if the existing
     *   contents are needed, this downloads a copy of the network file to a
     *   local temporary file, otherwise it simply generates a name for a new
     *   local temporary file; in any case, it returns the temp file name in
     *   the lclfname field.
     *   
     *   If 'sfid' is non-zero, it's a CVmObjFile::SFID_xxx value specifying
     *   the special file ID.  Use zero for an ordinary file.
     *   
     *   This throws an error on any download failure.  
     */
    static CVmNetFile *open(VMG_ const char *fname, int sfid, int mode,
                            os_filetype_t typ, const char *mime_type)
#ifdef TADSNET
        ;
#else
    {
        /* 
         *   Local implementation - access the local file directly.  We don't
         *   care about MIME types for the local implementation, since our
         *   local file system interfaces don't use them.  
         */
        return new CVmNetFile(fname, sfid, 0, mode, typ, 0);
    }
#endif

    /*
     *   Open a file, based on an argument value from the byte-code program.
     *   This accepts string filenames, TemporaryFile (CVmObjTemporaryFile)
     *   objects, or TadsObject objects providing the file spec interface.
     */
    static CVmNetFile *open(
        VMG_ const vm_val_t *filespec, const struct vm_rcdesc *rc,
        int mode, os_filetype_t typ, const char *mime_type);

    /*
     *   Open a file in local mode.  This is essentially a no-op that allows
     *   a caller to explicitly manipulate a local file through code that's
     *   written to support network files, without adding a special case to
     *   that code.  This routine creates a local file descriptor regardless
     *   of the interpreter mode.  
     */
    static CVmNetFile *open_local(VMG_ const char *fname, int sfid,
                                  int mode, os_filetype_t typ)
    {
        /* return an explicitly local file descriptor */
        return new CVmNetFile(fname, sfid, 0, mode, typ, 0);
    }

    /* 
     *   Is this a network file?  This returns true if the file is stored on
     *   the network, false if it's in the local file system. 
     */
    int is_net_file() const
    {
        /* it's a network file if it has a server-side filename */
        return srvfname != 0;
    }

    /*
     *   Does the given file exist?  Returns true if so, nil if not.
     */
    static int exists(VMG_ const char *fname, int sfid)
#ifdef TADSNET
        ;
#else
    {
        /* 
         *   local-only mode: check the local file system; it exists if
         *   osfacc() returns success (zero) 
         */
        return !osfacc(fname);
    }
#endif

    /*
     *   Test to see if we can write to the given file.  We attempt to open
     *   the file for writing (retaining any existing data in the file, or
     *   creating a new file if it doesn't exist).  If we succeed, we close
     *   the file (and delete it if it didn't exist), and return TRUE.  If we
     *   fail, return FALSE.  
     */
    static int can_write(VMG_ const char *fname, int sfid)
#ifdef TADSNET
        ;
#else
    {
        return can_write_local(fname);
    }
#endif

    /*
     *   Close the file.  Call this after closing the local file handle.  For
     *   a network file opened with write access, this will copy the local
     *   temp copy back to the storage server, then delete the local temp
     *   copy.  For any network file, deletes the local temp file. For all
     *   files, deletes the CVmNetFile object.
     *   
     *   Throws an error on upload failure.  The CVmNetFile object is always
     *   reliably deleted, whether or not an error is thrown.  
     */
    void close(VMG0_)
#ifdef TADSNET
        ;
#else
    {
        /* 
         *   Local file:
         *   
         *   - if we opened it in "delete" mode, delete it
         *.  - if we opened it in "create" mode, set the file's OS type code 
         */
        int err = 0;
        if ((mode & NETF_DELETE) != 0)
        {
            if (osfdel(lclfname))
                err = VMERR_DELETE_FILE;
        }
        else if ((mode & NETF_CREATE) != 0)
            os_settype(lclfname, typ);

        /* delete self */
        delete this;

        /* if an error occurred, throw the error */
        if (err != 0)
            err_throw(err);
    }
#endif

    /*
     *   Abandon the network file.  Call this if the local file manipulation
     *   fails, and the caller decides it doesn't want to save any changes
     *   back to the network after all.  This deletes any local temp file,
     *   and frees the CVmNetFile object.  Call this only after closing the
     *   local file handle.
     */
    void abandon(VMG0_)
    {
#ifdef TADSNET
        /* delete the temp file */
        if (srvfname != 0)
            osfdel(lclfname);
#endif

        /* free self */
        delete this;
    }

    /* 
     *   Mark references for garbage collection.  This marks our reference to
     *   the filespec object, if we have one. 
     */
    void mark_refs(VMG_ uint state);

    /* 
     *   The local filename.  After obtaining this object from vmnet_fopen(),
     *   the caller can use any osfopxxx() function to open the file.  If
     *   we're using the network storage server, this will name a temp file
     *   in the system temp directory; if the file isn't on the storage
     *   server, this is simply the local file name.  
     */
    char *lclfname;

    /* special file ID */
    int sfid;

    /* 
     *   The server filename.  When we're operating on a storage server file,
     *   this is the name of the file on the server; otherwise it's null.
     */
    char *srvfname;

    /*
     *   The file spec object.  This can be a TemporaryFile object, or a
     *   TadsObject object with a file spec interface.  If the file was
     *   opened based on a string filename, this is nil.  
     */
    vm_obj_id_t filespec;

    /* the file mode */
    int mode;

    /* is this a temporary file? */
    int is_temp;

    /* the OS file type */
    os_filetype_t typ;

    /* MIME type of the file */
    char *mime_type;

protected:
    CVmNetFile(const char *lclfname, int sfid, const char *srvfname,
               int mode, os_filetype_t typ, const char *mime_type)
    {
        /* save the filenames, mode, and type */
        this->lclfname = lib_copy_str(lclfname);
        this->srvfname = lib_copy_str(srvfname);
        this->sfid = sfid;
        this->mode = mode;
        this->typ = typ;
        this->mime_type = lib_copy_str(mime_type);
        this->filespec = VM_INVALID_OBJ;
        this->is_temp = FALSE;
    }

    ~CVmNetFile()
    {
        lib_free_str(lclfname);
        lib_free_str(srvfname);
        lib_free_str(mime_type);
    }

    /* build the full server-side filename for a given file */
    static const char *build_server_filename(
        VMG_ char *dst, size_t dstsiz, const char *fname, int sfid);

    /* check to see if we can create/write to a local file */
    static int can_write_local(const char *fname)
    {
        /* note whether or not the file exists already */
        int existed = !osfacc(fname);

        /* 
         *   try opening for read/write, keeping the existing contents or
         *   creating a new file if it doesn't exist 
         */
        osfildef *fp = osfoprwb(fname, OSFTBIN);
        if (fp != 0)
        {
            /* 
             *   Successfully opened the file, so we evidently can write it.
             *   We don't actually need to do anything with the file other
             *   than check that we could open it, so close it.  
             */
            osfcls(fp);

            /* if the file didn't exist before, restore the status quo ante */
            if (!existed)
                osfdel(fname);

            /* indicate success */
            return TRUE;
        }
        else
        {
            /* couldn't open the file - return failure */
            return FALSE;
        }
    }
};

#endif /* VMNETFIL_H */
