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
  vmsave.cpp - T3 save/restore state
Function
  
Notes
  
Modified
  08/02/99 MJRoberts  - Creation
*/

#include "t3std.h"
#include "os.h"
#include "vmglob.h"
#include "vmsave.h"
#include "vmfile.h"
#include "vmimage.h"
#include "vmobj.h"
#include "vmrun.h"
#include "vmstack.h"
#include "vmundo.h"
#include "vmmeta.h"
#include "vmcrc.h"


/* ------------------------------------------------------------------------ */
/*
 *   Saved state signature.  This signature is tied to a specific format
 *   version; it should be changed whenever the format version is modified
 *   so that it is incompatible with older versions.
 *   
 *   Note that incompatible changes to intrinsic class serialization formats
 *   will require updating the version number.
 *   
 *   Incompatible changes to the format are not a particularly big deal.
 *   Saved states tend to be local to a particular machine, since they're
 *   mostly used to suspend sessions for later resumption and to "branch"
 *   the state evolution (i.e., to allow playing a game from a particular
 *   point, then returning later to that same point to play again, but doing
 *   different things this time; this is used particularly to save "good"
 *   positions as a precaution against later getting into unwinnable
 *   states).  
 */
#define VMSAVEFILE_SIG "T3-state-v0009\015\012\032"


/* ------------------------------------------------------------------------ */
/*
 *   Compute the checksum of the contents of a file stream.  We'll compute
 *   the checksum of the given file, starting at the current seek position
 *   and running for the requested number of bytes.  
 */
static unsigned long compute_checksum(CVmFile *fp, unsigned long len)
{
    CVmCRC32 crc;

    /* read the file and compute the CRC value for its contents */
    while (len != 0)
    {
        char buf[256];
        size_t cur_len;
        
        /* figure out how much we can load from the file */
        cur_len = sizeof(buf);
        if (cur_len > len)
            cur_len = (size_t)len;

        /* load the data from the file */
        fp->read_bytes(buf, cur_len);

        /* deduct the amount we read from the overall file length remaining */
        len -= cur_len;

        /* scan this block into the checksum */
        crc.scan_bytes(buf, cur_len);
    }

    /* return the computed value */
    return crc.get_crc_val();
}


/* ------------------------------------------------------------------------ */
/*
 *   Save VM state to a file 
 */
void CVmSaveFile::save(VMG_ CVmFile *fp)
{
    const char *fname;
    size_t fname_len;
    long startpos;
    long endpos;
    unsigned long crcval;
    unsigned long datasize;
    
    /* write the signature */
    fp->write_bytes(VMSAVEFILE_SIG, sizeof(VMSAVEFILE_SIG)-1);

    /* note the seek position of the start of the file header */
    startpos = fp->get_pos();

    /* write a placeholder for the stream size and checksum */
    fp->write_int4(0);
    fp->write_int4(0);

    /* write the image file's timestamp */
    fp->write_bytes(G_image_loader->get_timestamp(), 24);

    /* get the image filename */
    fname = G_image_loader->get_filename();
    fname_len = strlen(fname);

    /* 
     *   write the image filename, so we can figure out what image file to
     *   load if we start the interpreter specifying only the saved state
     *   to be restored 
     */
    fp->write_int2(strlen(fname));
    fp->write_bytes(fname, fname_len);

    /* save all modified object state */
    G_obj_table->save(vmg_ fp);

    /* save the synthesized exports */
    G_image_loader->save_synth_exports(vmg_ fp);

    /* remember where the file ends */
    endpos = fp->get_pos();

    /* 
     *   compute the size of the data stream - this includes everything
     *   after the size/checksum fields 
     */
    datasize = endpos - startpos - 8;

    /* 
     *   seek back to just after the size/checksum header - this is the
     *   start of the section of the file for which we must compute the
     *   checksum 
     */
    fp->set_pos(startpos + 8);

    /* compute the checksum */
    crcval = compute_checksum(fp, datasize);

    /* 
     *   seek back to the size/checksum header, and fill in those fields now
     *   that we know their values 
     */
    fp->set_pos(startpos);
    fp->write_int4(datasize);
    fp->write_int4(crcval);

    /* seek back to the end of the file */
    fp->set_pos(endpos);
}

/* ------------------------------------------------------------------------ */
/*
 *   Given a saved state file, get the name of the image file that was
 *   loaded when the state file was created. 
 */
int CVmSaveFile::restore_get_image(osfildef *fp,
                                   char *fname_buf, size_t fname_buf_len)
{
    char buf[128];
    size_t len;

    /* read the signature, size/checksum, and timestamp fields */
    if (osfrb(fp, buf, sizeof(VMSAVEFILE_SIG)-1 + 8 + 24))
        return VMERR_READ_FILE;

    /* check the signature */
    if (memcmp(buf, VMSAVEFILE_SIG, sizeof(VMSAVEFILE_SIG)-1) != 0)
        return VMERR_NOT_SAVED_STATE;

    /* read the length of the image file name */
    if (osfrb(fp, buf, 2))
        return VMERR_READ_FILE;

    /* get the length from the buffer */
    len = osrp2(buf);

    /* if it won't fit in the buffer, return an error */
    if (len + 1 > fname_buf_len)
        return VMERR_READ_FILE;

    /* read the name into the caller's buffer */
    if (osfrb(fp, fname_buf, len))
        return VMERR_READ_FILE;

    /* null-terminate the name */
    fname_buf[len] = '\0';

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Restore VM state from a file.  Returns zero on success, non-zero on
 *   error.  
 */
int CVmSaveFile::restore(VMG_ CVmFile *fp)
{
    char buf[128];
    int err;
    unsigned long datasize;
    unsigned long old_crcval;
    unsigned long new_crcval;
    long startpos;
    int old_gc_enabled;
    CVmObjFixup *fixups;

    /* we don't have a fixup table yet (the object loader will create one) */
    fixups = 0;

    /* read the file's signature */
    fp->read_bytes(buf, sizeof(VMSAVEFILE_SIG)-1);

    /* check the signature */
    if (memcmp(buf, VMSAVEFILE_SIG, sizeof(VMSAVEFILE_SIG)-1) != 0)
        return VMERR_NOT_SAVED_STATE;

    /* read the size/checksum fields */
    datasize = fp->read_uint4();
    old_crcval = fp->read_uint4();

    /* note the starting position of the datastream */
    startpos = fp->get_pos();

    /* compute the checksum of the file data */
    new_crcval = compute_checksum(fp, datasize);

    /* 
     *   if the checksum we computed doesn't match the one stored in the
     *   file, the file is corrupted 
     */
    if (new_crcval != old_crcval)
        return VMERR_BAD_SAVED_STATE;

    /* seek back to the starting position */
    fp->set_pos(startpos);

    /* check the timestamp */
    fp->read_bytes(buf, 24);
    if (memcmp(buf, G_image_loader->get_timestamp(), 24) != 0)
        return VMERR_WRONG_SAVED_STATE;

    /* 
     *   skip the image filename - since we already have an image file
     *   loaded, this information is of no use to us here (it's only
     *   useful when we want to restore a saved state before we know what
     *   the image file is) 
     */
    fp->set_pos_from_cur(fp->read_int2());

    /* 
     *   discard all undo information - any undo information we currently
     *   have obviously can't be applied to the restored state 
     */
    G_undo->drop_undo(vmg0_);

    /* 
     *   Disable garbage collection while restoring.  This is necessary
     *   because there are possible intermediate states where we have
     *   restored some of the objects but not all of them, so objects that
     *   are reachable from the fully restored state won't necessarily appear
     *   to be reachable from all possible intermediate states. 
     */
    old_gc_enabled = G_obj_table->enable_gc(vmg_ FALSE);

    err_try
    {
        /* forget any IntrinsicClass instances we created at startup */
        G_meta_table->forget_intrinsic_class_instances(vmg0_);

        /* load the object data from the file */
        if ((err = G_obj_table->restore(vmg_ fp, &fixups)) != 0)
            goto read_done;
        
        /* load the synthesized exports from the file */
        err = G_image_loader->restore_synth_exports(vmg_ fp, fixups);
        if (err != 0)
            goto read_done;

        /* 
         *   re-link to the exports and synthesized exports loaded from the
         *   saved session 
         */
        G_image_loader->do_dynamic_link(vmg0_);

        /* create any missing IntrinsicClass instances */
        G_meta_table->create_intrinsic_class_instances(vmg0_);

        /* perform any requested post-load object initializations */
        G_obj_table->do_all_post_load_init(vmg0_);

    read_done: ;
    }
    err_catch(exc)
    {
        /* remember the error code */
        err = exc->get_error_code();
    }
    err_end;

    /* we're done with the fixup table, so delete it if we created one */
    if (fixups != 0)
        delete fixups;

    /* restore the garbage collector's enabled state */
    G_obj_table->enable_gc(vmg_ old_gc_enabled);

    /* 
     *   explicitly run garbage collection, since any dynamic objects that
     *   were reachable before the restore only through non-transient
     *   references will no longer be reachable, all of the non-transient
     *   references having been replaced now 
     */
    G_obj_table->gc_full(vmg0_);

    /* if any error occurred, throw the error */
    if (err != 0)
        err_throw(err);

    /* success */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Reset to initial image file state 
 */
void CVmSaveFile::reset(VMG0_)
{
    /* 
     *   discard undo information, since it applies only to the current VM
     *   state and obviously is no longer relevant after we reset to the
     *   initial state 
     */
    G_undo->drop_undo(vmg0_);

    /* 
     *   discard all synthesized exports, since we want to dynamically link
     *   to the base image file state 
     */
    G_image_loader->discard_synth_exports();

    /* forget any IntrinsicClass instances we created at startup */
    G_meta_table->forget_intrinsic_class_instances(vmg0_);

    /* reset all objects to initial image file load state */
    G_obj_table->reset_to_image(vmg0_);

    /* 
     *   forget the previous dynamic linking information and relink to the
     *   image file again - this will ensure that any objects created after
     *   load are properly re-created now 
     */
    G_image_loader->do_dynamic_link(vmg0_);

    /* create any missing IntrinsicClass instances */
    G_meta_table->create_intrinsic_class_instances(vmg0_);

    /* perform any requested post-load object initializations */
    G_obj_table->do_all_post_load_init(vmg0_);

    /* 
     *   explicitly run garbage collection to clean up dynamic objects that
     *   are no longer reachable from the initial state
     */
    G_obj_table->gc_full(vmg0_);

    /* run the static initializers */
    G_image_loader->run_static_init(vmg0_);
}

