#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmnetfil.cpp - network file operations
Function
  This module contains the network storage server implementation of
  CVmNetFile.
Notes
  
Modified
  09/08/10 MJRoberts  - Creation
*/

#include "t3std.h"
#include "os.h"
#include "osifcnet.h"
#include "vmnetfil.h"
#include "vmnet.h"
#include "vmfile.h"
#include "vmdatasrc.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmrun.h"
#include "vmglob.h"
#include "vmpredef.h"
#include "vmimport.h"
#include "sha2.h"
#include "vmhash.h"
#include "vmbif.h"
#include "vmtype.h"
#include "vmobj.h"
#include "vmtmpfil.h"
#include "vmtobj.h"


/* ------------------------------------------------------------------------ */
/*
 *   Ticket generator.  Generates the storage server "ticket" for a given
 *   filename.  The buffer must be at least 65 characters long.
 *   
 *   The ticket authenticates the game server to the storage server by
 *   proving that the game server has a valid API key, which is the
 *   equivalent of a password.  
 */
class ServerTicket
{
public:
    ServerTicket(VMG_ const char *fname, int sfid)
    {
        /*
         *   The ticket formula is as follows:
         *   
         *.    hash1 = SHA256(fname : gameServerID)
         *.    ticket = SHA256(hash1 : apiKey)
         *   
         *   The filename must be the fully qualified filename with session
         *   ID prefix, of the form ~SID/path.
         *   
         *   The ticket's purpose is to authenticate the game server by
         *   proving that we know the API key, but without revealing the key.
         *   A secure hash accomplishes this by generating a hash value that
         *   can't be reversed (i.e., you can't figure the input given the
         *   output).  The double hash is for protection against chosen
         *   plaintext attacks - these aren't a known vulnerability in
         *   SHA256, but they've been used to attack older hash algorithms,
         *   so we've tried to minimize the impact should SHA256 eventually
         *   be found weaker in this area than presently thought.  The idea
         *   is that the filename is explicitly chosen by the user, and the
         *   session ID *could* be chosen by an attacker who only wants to
         *   get the server to generate hash codes for analysis (the storage
         *   server issues true session IDs, so an attacker can't choose SIDs
         *   that actually work against the server, but remember, the
         *   attacker's goal is to analyze the hash output to discover the
         *   secret API key).  The game server ID *can't* be chosen by the
         *   attacker, so we combine it with the two choosable plaintext
         *   elements to come up with the first hash.  The attacker *can't*
         *   choose the output of this hash, since SHA256 is considered
         *   secure against collision attacks (i.e., it's impossible to find
         *   SHA256 input that produces a chosen output).  The attacker will
         *   still *know* the plaintext that's hashed with the API key, but
         *   can't *choose* the plaintext.  
         */
        char hash1[65];
        sha256_ezf(hash1, "%s:%s",
                   fname, G_net_config->get("serverid"));
        sha256_ezf(ticket, "%s:%s",
                   hash1, G_net_config->get("storage.apikey"));
    }

    /* get the ticket string */
    const char *get() const { return ticket; }

private:
    /* the ticket is an SHA256 hash (64 characters) */
    char ticket[65];
};

/*
 *   Are we in network storage server mode? 
 */
int CVmNetFile::is_net_mode(VMG0_)
{
    return (G_net_config != 0
            && G_net_config->get("storage.domain") != 0
            && G_net_config->get("storage.sessionid") != 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Build the full server filename for a given path
 */
const char *CVmNetFile::build_server_filename(
    VMG_ char *dst, size_t dstsiz, const char *fname, int sfid)
{
    /* if it's a special file, generate the server special file path */
    if (sfid != 0)
    {
        t3sprintf(dst, dstsiz, "~%s/special/%d",
                  G_net_config->get("storage.sessionid"), sfid);
        return dst;
    }

    /* if the filename doesn't have the session ID prefix, add it */
    if (fname != 0 && fname[0] != '~')
    {
        /* build the full name - ~SID/filename */
        t3sprintf(dst, dstsiz, "~%s/%s",
                  G_net_config->get("storage.sessionid"), fname);
        return dst;
    }

    /* no changes are required */
    return fname;
}


/* ------------------------------------------------------------------------ */
/*
 *   Open a network file 
 */
CVmNetFile *CVmNetFile::open(VMG_ const char *fname, int sfid,
                             int mode, os_filetype_t typ,
                             const char *mime_type)
{
    /* check to see if we're in network mode */
    if (is_net_mode(vmg0_))
    {
        /* 
         *   We're in network mode, so the file is on the remote storage
         *   server.  All operations on the open file will actually be
         *   performed on a local temporary copy.  Generate a name for the
         *   temp file.  
         */
        char tmp[OSFNMAX];
        if (!os_gen_temp_filename(tmp, sizeof(tmp)))
            err_throw(VMERR_CREATE_FILE);

        /* build the full server filename */
        char srvname[400];
        fname = build_server_filename(
            vmg_ srvname, sizeof(srvname), fname, sfid);

        /*
         *   If we're not truncating the file, and we're going to either read
         *   or write the file (or both), download a copy of the file from
         *   the server and save it into the temp file we just selected.  The
         *   truncate flag means that we're replacing any existing contents,
         *   so there's no need to download the old file.
         *   
         *   If NEITHER read nor write modes are set, we don't have to fetch
         *   the file: we're not going to access the contents, and we're not
         *   going to re-upload the contents on close.  
         *   
         *   It might seem strange at first glance that we have to fetch a
         *   file in write-only mode.  But we do: we need the old contents in
         *   this case, even though we're not going to look at them locally,
         *   because the caller might be patching a file somewhere in the
         *   middle.  This means that the rest of the file has to be kept
         *   intact.  To do this we have to download the old copy, so that
         *   the updated copy we upload on close retains the rest of the
         *   contents that we didn't modify.
         *   
         *   If we do decide to download the file, and the file doesn't
         *   exist, this is an error unless the CREATE flag is set.  The
         *   CREATE flag means that we're meant to create a new file if
         *   there's no existing file, so it's explicitly not an error if the
         *   file doesn't exist.  
         */
        if ((mode & (NETF_READ | NETF_WRITE)) != 0
            && (mode & NETF_TRUNC) == 0)
        {
            /* 
             *   Not in TRUNCATE mode, so we need to download the file.  Open
             *   the temp file for writing.  
             */
            osfildef *fp = osfoprwtb(tmp, typ);
            if (fp == 0)
                err_throw(VMERR_CREATE_FILE);

            /* get the storage server "ticket" for the file */
            ServerTicket ticket(vmg_ fname, sfid);
            
            /* build the request URL */
            char *url = t3sprintf_alloc(
                "%sgetfile?file=%P&ticket=%P",
                G_net_config->get("storage.rootpath", "/"),
                fname, ticket.get());
            
            /* set up a file stream writer on the temp file */
            CVmFileSource reply(fp);
            
            /* download the file from the server into the temp file */
            char *headers = 0;
            int hstat = OS_HttpClient::request(
                0, G_net_config->get("storage.domain"), 80,
                "GET", url, 0, 0, 0, &reply, &headers, 0, 0);
            
            /* done with the URL */
            t3free(url);

            /* get the reply status */
            char *stat = vmnet_get_storagesrv_stat(
                vmg_ hstat, &reply, headers);

            /* done with the headers */
            delete headers;

            /*
             *   If the reply is "FileNotFound", and the CREATE flag was set
             *   in the request, this counts as success - the caller wishes
             *   to create the file if it doesn't already exist, so it's not
             *   an error if it doesn't exist.  In this case simply replace
             *   the status code with "OK".  
             */
            if (memcmp(stat, "FileNotFound ", 13) == 0)
                strcpy(stat, "OK ");

            /* on failure, delete the temp file */
            if (memcmp(stat, "OK ", 3) != 0)
                osfdel(tmp);

            /* check the status and throw an error on failure */
            vmnet_check_storagesrv_stat(vmg_ stat);
        }

        /* 
         *   Success.  Return a file info structure with the temporary file
         *   as the local file name, and the caller's filename as the server
         *   filename.  
         */
        return new CVmNetFile(tmp, sfid, fname, mode, typ, mime_type);
    }
    else
    {
        /* 
         *   Local mode - the given file is our local file.  Don't bother
         *   storing the MIME type for these, since we don't use this with
         *   the local file system APIs. 
         */
        return new CVmNetFile(fname, sfid, 0, mode, typ, 0);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Close a network file 
 */
void CVmNetFile::close(VMG0_)
{
    /* if there's no network file, it's a local file */
    if (srvfname == 0 || !is_net_mode(vmg0_))
    {
        int err = 0;
        
        /* if we opened the file in "create" mode, set the OS file type */
        if ((mode & NETF_CREATE) != 0)
            os_settype(lclfname, typ);

        /* if we opened the file in "delete" mode, delete the file */
        if ((mode & NETF_DELETE) != 0 && osfdel(lclfname))
            err = VMERR_DELETE_FILE;

        /* if the file spec was a TadsObject object, call its closeFile */
        if (filespec != VM_INVALID_OBJ
            && CVmObjTads::is_tadsobj_obj(vmg_ filespec)
            && G_predef->filespec_closeFile != VM_INVALID_PROP)
        {
            /* set up an object value for the file spec */
            vm_val_t fs;
            fs.set_obj(filespec);

            /* call closeFile */
            vm_rcdesc rc("System.closeFile");
            G_interpreter->get_prop(
                vmg_ 0, &fs, G_predef->filespec_closeFile, &fs, 0, &rc);
        }

        /* delete self */
        delete this;

        /* if an error occurred, throw the error */
        if (err != 0)
            err_throw(err);

        /* done */
        return;
    }

    /* 
     *   presume we won't perform a network operation, so there'll be no
     *   server status code generated 
     */
    char *stat = 0;

    /*
     *   We have a network file.
     *   
     *   If we opened the file in DELETE mode, delete the server file.  Note
     *   that we check this before checking WRITE mode, since there's no
     *   point in sending the updated contents to the server if we're just
     *   going to turn around and delete the file anyway.
     *   
     *   Otherwise, if we opened the file with WRITE access, send the updated
     *   version of the temp file back to the server.  
     */
    if ((mode & NETF_DELETE) != 0)
    {
        /* get the ticket for deleting the file */
        ServerTicket ticket(vmg_ srvfname, sfid);

        /* generate the URL */
        char *url = t3sprintf_alloc(
            "%sdelfile?file=%P&ticket=%P",
            G_net_config->get("storage.rootpath", "/"),
            srvfname, ticket.get());

        /* send the GET request */
        char *headers = 0;
        CVmMemorySource rstr(1024);
        int hstat = OS_HttpClient::request(
            0, G_net_config->get("storage.domain"), 80,
            "GET", url, 0, 0, 0, &rstr, &headers, 0, 0);

        /* done with the URL */
        t3free(url);

        /* get the reply code from the server */
        stat = vmnet_get_storagesrv_stat(vmg_ hstat, &rstr, headers);

        /* done with the headers */
        if (headers != 0)
            delete [] headers;
    }
    else if ((mode & NETF_WRITE) != 0)
    {
        /* 
         *   open the local temp file for reading - if we can't, throw a
         *   "close file" error, since this is equivalent to an error
         *   flushing the write buffer to disk on closing a regular file 
         */
        osfildef *fp = osfoprb(lclfname, typ);
        if (fp == 0)
            err_throw(VMERR_CLOSE_FILE);
        
        /* set up a stream reader on the temp file */
        CVmFileSource *st = new CVmFileSource(fp);

        /* get the ticket for writing this file */
        ServerTicket ticket(vmg_ srvfname, sfid);

        /* build the POST data */
        OS_HttpPayload *post = new OS_HttpPayload();
        post->add("file", srvfname);
        post->add("sid", G_net_config->get("storage.sessionid"));
        post->add("ticket", ticket.get());
        post->add("contents", "noname", mime_type, st);

        /* generate the URL */
        char *url = t3sprintf_alloc(
            "%sputfile", G_net_config->get("storage.rootpath", "/"));

        /* send the POST request */
        CVmMemorySource rstr(128);
        char *headers = 0;
        int hstat = OS_HttpClient::request(
            0, G_net_config->get("storage.domain"), 80,
            "POST", url, 0, 0, post, &rstr, &headers, 0, 0);

        /* done with the URL and the POST parameters */
        t3free(url);
        delete post;

        /* get the reply code from the server */
        stat = vmnet_get_storagesrv_stat(vmg_ hstat, &rstr, headers);

        /* done with the headers */
        if (headers != 0)
            delete [] headers;
    }

    /* delete the temporary file */
    osfdel(lclfname);
    lclfname = 0;

    /* done with the file structure */
    delete this;

    /* check the status code from the server, if any */
    if (stat != 0)
        vmnet_check_storagesrv_stat(vmg_ stat);
}

/* ------------------------------------------------------------------------ */
/*
 *   Do a Read or Write check on a server file.
 */
static int server_file_check(VMG_ const char *srvfname, const char *mode)
{
    /* get the server ticket */
    ServerTicket ticket(vmg_ srvfname, 0);
    
    /* build the request URL to test file existence */
    char *url = t3sprintf_alloc(
        "%stestfile?file=%P&ticket=%P&mode=%s",
        G_net_config->get("storage.rootpath", "/"),
        srvfname, ticket.get(), mode);

    /* send the request */
    CVmMemorySource rstr(128);
    int hstat = OS_HttpClient::request(
        0, G_net_config->get("storage.domain"), 80,
        "GET", url, 0, 0, 0, &rstr, 0, 0, 0);
    
    /* done with the URL */
    t3free(url);
    
    /* check the result */
    char reply[128];
    return (hstat == 200
            && rstr.readc(reply, sizeof(reply)) >= 1
            && reply[0] == 'Y');
}

/* ------------------------------------------------------------------------ */
/*
 *   Check to see if a file exists 
 */
int CVmNetFile::exists(VMG_ const char *fname, int sfid)
{
    /* create a no-op descriptor for the file */
    CVmNetFile *nf = open(vmg_ fname, sfid, 0, OSFTUNK, 0);

    /* 
     *   if it's a network file, check with the server; otherwise check the
     *   local file system 
     */
    int ret = (nf->is_net_file()
               ? server_file_check(vmg_ nf->srvfname, "R")
               : !osfacc(nf->lclfname));

    /* done with the descriptor */
    nf->abandon(vmg0_);

    /* return the result */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Check to see if a file is writable 
 */
int CVmNetFile::can_write(VMG_ const char *fname, int sfid)
{
    /* create a no-op descriptor for the file */
    CVmNetFile *nf = open(vmg_ fname, sfid, 0, OSFTUNK, 0);

    /* 
     *   if it's a network file, check with the server; otherwise check the
     *   local file system 
     */
    int ret = (nf->is_net_file()
               ? server_file_check(vmg_ nf->srvfname, "W")
               : can_write_local(nf->lclfname));

    /* done with the descriptor */
    nf->abandon(vmg0_);

    /* return the result */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the file mode
 */
int CVmNetFile::get_file_mode(
    VMG_ unsigned long *mode, unsigned long *attrs, int follow_links)
{
    /* 
     *   if it's a network file, check with the server to see if the file
     *   exists; otherwise check the local file system 
     */
    int ret;
    if (is_net_file())
    {
        /* 
         *   network file - if the file exists, it's simply a regular file,
         *   since that's all we support 
         */
        ret = server_file_check(vmg_ srvfname, "R");
        *mode = (ret ? OSFMODE_FILE : 0);
    }
    else
    {
        /* local file - get the local file mode */
        ret = osfmode(lclfname, follow_links, mode, attrs);
    }
    
    /* return the result */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get file status 
 */
int CVmNetFile::get_file_stat(VMG_ os_file_stat_t *stat, int follow_links)
{
    if (is_net_file())
    {
        /* not supported for network files */
        err_throw(VMERR_NET_FILE_NOIMPL);;
        AFTER_ERR_THROW(return FALSE);
    }
    else
    {
        /* local file - get the local file status */
        return os_file_stat(lclfname, follow_links, stat);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Resolve a symbolic link
 */
int CVmNetFile::resolve_symlink(VMG_ char *target, size_t target_size)
{
    if (is_net_file())
    {
        /* 
         *   the storage server doesn't support symbolic links, so there's
         *   never anything to resolve 
         */
        if (target_size != 0)
            target[0] = '\0';
        return FALSE;
    }
    else
    {
        /* local file - get the local file status */
        return os_resolve_symlink(lclfname, target, target_size);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Rename a file
 */
void CVmNetFile::rename_to(VMG_ CVmNetFile *newname)
{
    if (is_net_file())
    {
        /* the storage server doesn't support directory creation */
        err_throw(VMERR_NET_FILE_NOIMPL);;
    }
    else
    {
        /* create the file locally */
        rename_to_local(vmg_ newname);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Create a directory 
 */
void CVmNetFile::mkdir(VMG_ int create_parents)
{
    if (is_net_file())
    {
        /* the storage server doesn't support directory creation */
        err_throw(VMERR_NET_FILE_NOIMPL);;
    }
    else
    {
        /* create the file locally */
        mkdir_local(vmg_ create_parents);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Remove a directory 
 */
void CVmNetFile::rmdir(VMG_ int remove_contents)
{
    if (is_net_file())
    {
        /* the storage server doesn't support directory removal */
        err_throw(VMERR_NET_FILE_NOIMPL);;
    }
    else
    {
        /* create the file locally */
        rmdir_local(vmg_ remove_contents);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Read a directory 
 */
int CVmNetFile::readdir(VMG_ const char *nominal_path, vm_val_t *retval)
{
    if (is_net_file())
    {
        /* the storage server doesn't support directory listings */
        err_throw(VMERR_NET_FILE_NOIMPL);;
        AFTER_ERR_THROW(return FALSE);
    }
    else
    {
        /* read the local directory */
        return readdir_local(vmg_ nominal_path, retval, 0, 0, FALSE);
    }
}

/*
 *   Read a directory, enumerating contents through a callback
 */
int CVmNetFile::readdir_cb(VMG_ const char *nominal_path,
                           const struct vm_rcdesc *rc,
                           const vm_val_t *cb, int recursive)
{
    if (is_net_file())
    {
        /* the storage server doesn't support directory listings */
        err_throw(VMERR_NET_FILE_NOIMPL);;
        AFTER_ERR_THROW(return FALSE);
    }
    else
    {
        /* read the local directory */
        return readdir_local(vmg_ nominal_path, 0, rc, cb, recursive);
    }
}

