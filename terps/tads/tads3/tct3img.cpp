#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/TCT3IMG.CPP,v 1.1 1999/07/11 00:46:57 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tct3.cpp - TADS 3 Compiler - T3 VM Code Generator - image writing functions
Function
  Image writing routines for the T3-specific code generator
Notes
  
Modified
  05/08/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdlib.h>

#include "t3std.h"
#include "os.h"
#include "tcprs.h"
#include "tct3.h"
#include "tcgen.h"
#include "vmtype.h"
#include "vmwrtimg.h"
#include "vmgram.h"
#include "vmfile.h"
#include "tcmain.h"
#include "tcerr.h"
#include "tcmake.h"
#include "tctok.h"


/* ------------------------------------------------------------------------ */
/*
 *   Object file signature.  The numerical suffix in the first part is the
 *   format version number: whenever we make an incompatible change to the
 *   format, we'll increment this number so that the linker will recognize an
 *   incompatible file format and require a full rebuild.  
 */
static const char obj_file_sig[] = "TADS3.Object.0011\n\r\032";


/* ------------------------------------------------------------------------ */
/*
 *   Write an object file.  The object file contains the raw byte streams
 *   with the generated code; the fixup lists for the streams; the global
 *   symbol table; and the function and metaclass dependency lists.  
 */
void CTcGenTarg::write_to_object_file(CVmFile *fp, CTcMake *)
{
    ulong flags;
    
    /* write the signature */
    fp->write_bytes(obj_file_sig, sizeof(obj_file_sig) - 1);

    /* compute the object file flags */
    flags = 0;
    if (G_debug)
        flags |= TCT3_OBJHDR_DEBUG;

    /* write the flags */
    fp->write_uint4(flags);

    /* write the constant and code pool indivisible object maxima */
    fp->write_uint4(max_str_len_);
    fp->write_uint4(max_list_cnt_);
    fp->write_uint4(max_bytecode_len_);

    /* 
     *   Write the maximum object and property ID's.  When we load this
     *   object file, we'll need to generate a translated ID number for
     *   each object ID and for each property ID, to translate from the
     *   numbering system in the object file to the final image file
     *   numbering system.  It is helpful if we know early on how many of
     *   each there are, so that we can allocate table space accordingly.  
     */
    fp->write_uint4(next_obj_);
    fp->write_uint4(next_prop_);
    fp->write_uint4(G_prs->get_enum_count());
    
    /* write the function set dependency table */
    write_funcdep_to_object_file(fp);

    /* 
     *   write the metaclass dependency table - note that we must do this
     *   before writing the global symbol table, because upon loading the
     *   object file, we must have the dependency table loaded before we
     *   can load the symbols (so that any metaclass symbols can be
     *   resolved to their dependency table indices) 
     */
    write_metadep_to_object_file(fp);

    /* write the global symbol table */
    G_prs->write_to_object_file(fp);

    /* write the main code stream and its fixup list */
    G_cs_main->write_to_object_file(fp);

    /* write the static code stream and its fixup list */
    G_cs_static->write_to_object_file(fp);

    /* write the data stream and its fixup list */
    G_ds->write_to_object_file(fp);

    /* write the object stream and its fixup list */
    G_os->write_to_object_file(fp);

    /* write the intrinsic class modifier stream */
    G_icmod_stream->write_to_object_file(fp);

    /* write the BigNumber stream and its fixup list */
    G_bignum_stream->write_to_object_file(fp);

    /* write the RexPattern stream and its fixup list */
    G_rexpat_stream->write_to_object_file(fp);

    /* write the static initializer ID stream */
    G_static_init_id_stream->write_to_object_file(fp);

    /* write the local variable symbol stream */
    G_lcl_stream->write_to_object_file(fp);

    /* write the object ID fixup list */
    CTcIdFixup::write_to_object_file(fp, G_objfixup);

    /* write the property ID fixup list */
    CTcIdFixup::write_to_object_file(fp, G_propfixup);

    /* write the enumerator ID fixup list */
    CTcIdFixup::write_to_object_file(fp, G_enumfixup);

    /* write debugging information if we're compiling for the debugger */
    if (G_debug)
    {
        tct3_debug_line_page *pg;
        
        /* write the source file list */
        write_sources_to_object_file(fp);

        /* 
         *   Write the pointers to the debug line records in the code
         *   stream, so that we can fix up the line records on re-loading
         *   the object file (they'll need to be adjusted for the new
         *   numbering system for the source file descriptors).  First,
         *   write the total number of pointers.  
         */
        fp->write_uint4(debug_line_cnt_);

        /* now write the pointers, one page at a time */
        for (pg = debug_line_head_ ; pg != 0 ; pg = pg->nxt)
        {
            size_t pgcnt;
            
            /* 
             *   if this is the last entry, it might only be partially
             *   full; otherwise, it's completely full, because we always
             *   fill a page before allocating a new one 
             */
            if (pg->nxt == 0)
                pgcnt = (size_t)(debug_line_cnt_ % TCT3_DEBUG_LINE_PAGE_SIZE);
            else
                pgcnt = TCT3_DEBUG_LINE_PAGE_SIZE;

            /* 
             *   Write the data - we prepared it in portable format, so we
             *   can just copy it directly to the file.  Note that each
             *   entry is four bytes.  
             */
            fp->write_bytes((char *)pg->line_ofs,
                            pgcnt * TCT3_DEBUG_LINE_REC_SIZE);
        }
    }

    /* write the #define symbols */
    G_tok->write_macros_to_file_for_debug(fp);
}

/* ------------------------------------------------------------------------ */
/*
 *   Write the function-set dependency table to an object file 
 */
void CTcGenTarg::write_funcdep_to_object_file(CVmFile *fp)
{
    tc_fnset_entry *cur;

    /* write the count */
    fp->write_uint2(fnset_cnt_);

    /* write the entries */
    for (cur = fnset_head_ ; cur != 0 ; cur = cur->nxt)
    {
        size_t len;

        len = strlen(cur->nm);
        fp->write_uint2(len);
        fp->write_bytes(cur->nm, len);
    }
}

/*
 *   Write the metaclass dependency table to an object file 
 */
void CTcGenTarg::write_metadep_to_object_file(CVmFile *fp)
{
    tc_meta_entry *cur;

    /* write the count */
    fp->write_uint2(meta_cnt_);

    /* write the entries */
    for (cur = meta_head_ ; cur != 0 ; cur = cur->nxt)
    {
        size_t len;

        len = strlen(cur->nm);
        fp->write_uint2(len);
        fp->write_bytes(cur->nm, len);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Error handler for CTcTokenizer::load_macros_from_file() 
 */
class MyLoadMacErr: public CTcTokLoadMacErr
{
public:
    MyLoadMacErr(const char *fname) { fname_ = fname; }

    /* log an error */
    virtual void log_error(int err)
    {
        /* check the error code */
        switch(err)
        {
        case 1:
        case 2:
            G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                TCERR_OBJFILE_MACRO_SYM_TOO_LONG, fname_);
            break;
        }
    }

private:
    /* the name of the object file we're loading */
    const char *fname_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Load an object file.  We'll read the file, load its data into memory
 *   (creating global symbol table entries and writing to the code and
 *   data streams), fix up the fixups to the new base offsets in the
 *   streams, and translate object and property ID values from the object
 *   file numbering system to our in-memory numbering system (which will
 *   usually differ after more than one object file is loaded, because the
 *   numbering systems in the different files must be reconciled).
 *   
 *   Returns zero on success; logs errors and returns non-zero on error.  
 */
int CTcGenTarg::load_object_file(CVmFile *fp, const textchar_t *fname)
{
    char buf[128];
    ulong obj_cnt;
    ulong prop_cnt;
    ulong enum_cnt;
    vm_obj_id_t *obj_xlat = 0;
    vm_prop_id_t *prop_xlat = 0;
    ulong *enum_xlat = 0;
    int err;
    ulong hdr_flags;
    ulong siz;
    ulong main_cs_start_ofs;
    ulong static_cs_start_ofs;
    
    /*
     *   Before loading anything from the file, go through all of the
     *   streams and set their object file base offset.  All stream
     *   offsets that we read from the object file will be relative to the
     *   these values, since the object file stream data will be loaded in
     *   after any data currently in the streams.  
     */
    G_cs_main->set_object_file_start_ofs();
    G_cs_static->set_object_file_start_ofs();
    G_ds->set_object_file_start_ofs();
    G_os->set_object_file_start_ofs();
    G_icmod_stream->set_object_file_start_ofs();
    G_bignum_stream->set_object_file_start_ofs();
    G_rexpat_stream->set_object_file_start_ofs();
    G_static_init_id_stream->set_object_file_start_ofs();
    G_lcl_stream->set_object_file_start_ofs();

    /* 
     *   note the main code stream's start offset, since we'll need this
     *   later in order to process the debug line records; likewise, note
     *   the static stream's start offset 
     */
    main_cs_start_ofs = G_cs_main->get_ofs();
    static_cs_start_ofs = G_cs_static->get_ofs();
    
    /* read the signature, and make sure it matches */
    fp->read_bytes(buf, sizeof(obj_file_sig) - 1);
    if (memcmp(buf, obj_file_sig, sizeof(obj_file_sig) - 1) != 0)
    {
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_OBJFILE_INV_SIG);
        return 1;
    }

    /* read the file header flags */
    hdr_flags = fp->read_uint4();

    /*
     *   If we're linking with debugging information, but this object file
     *   wasn't compiled with debugging information, we won't be able to
     *   produce a complete debuggable image - log an error to that
     *   effect. 
     */
    if (G_debug && (hdr_flags & TCT3_OBJHDR_DEBUG) == 0)
        G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                            TCERR_OBJFILE_NO_DBG, fname);

    /*
     *   Read the constant and code pool indivisible object maxima.  Note
     *   each maximum that exceeds the current maximum, since we must keep
     *   track of the largest indivisible object of each type in the
     *   entire program. 
     */

    /* read and note the maximum string constant length */
    siz = fp->read_uint4();
    if (siz > max_str_len_)
        max_str_len_ = siz;

    /* read and note the maximum list size */
    siz = fp->read_uint4();
    if (siz > max_list_cnt_)
        max_list_cnt_ = siz;
    
    /* read and note the maximum code pool object size */
    siz = fp->read_uint4();
    if (siz > max_bytecode_len_)
        max_bytecode_len_ = siz;

    /*
     *   read the object, property, and enumerator ID counts from the file
     *   - these give the highest ID values that are assigned anywhere in
     *   the object file's numbering system 
     */
    obj_cnt = fp->read_uint4();
    prop_cnt = fp->read_uint4();
    enum_cnt = fp->read_uint4();

    /*
     *   Allocate object and property ID translation tables.  These are
     *   simply arrays of ID's.  Each element of an array gives the global
     *   ID number assigned to the object whose local ID is the array
     *   index.  So, obj_xlat[local_id] = global_id.  We need one element
     *   in the object ID translation array for each local ID in the
     *   object file, which is obj_cnt; likewise for properties and
     *   prop_cnt.
     *   
     *   We're being a bit lazy here by using flat arrays.  This could be
     *   a problem for very large object files on 16-bit machines: if a
     *   single object file has more than 16k object ID's (which means
     *   that it defines and imports more than 16k unique objects), or
     *   more than 32k property ID's, we'll go over the 64k allocation
     *   limit on these machines.  This seems unlikely to become a problem
     *   in practice, but to ensure a graceful failure in such cases,
     *   check the allocation requirement to make sure we don't go over
     *   the present platform's architectural limits.  
     */
    if (obj_cnt * sizeof(obj_xlat[0]) > OSMALMAX
        || prop_cnt * sizeof(prop_xlat[0]) > OSMALMAX
        || enum_cnt * sizeof(enum_xlat[0]) > OSMALMAX)
    {
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_OBJFILE_TOO_MANY_IDS);
        return 2;
    }

    /* allocate the translation arrays */
    obj_xlat = (vm_obj_id_t *)
               t3malloc((size_t)(obj_cnt * sizeof(obj_xlat[0])));
    prop_xlat = (vm_prop_id_t *)
                t3malloc((size_t)(prop_cnt * sizeof(prop_xlat[0])));
    enum_xlat = (ulong *)
                t3malloc((size_t)(enum_cnt * sizeof(enum_xlat[0])));

    err_try
    {
        /* check to make sure we got the memory */
        if (obj_xlat == 0 || prop_xlat == 0 || enum_xlat == 0)
        {
            /* log an error and return failure */
            G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_OBJFILE_OUT_OF_MEM);
            err = 3;
            goto done;
        }

        /* 
         *   Clear out the translation arrays initially.  We should, in the
         *   course of loading the symbol table, assign a translation value
         *   for every entry.  If anything is left at zero (which is invalid
         *   as an object or property ID), something must be wrong.  
         */
        memset(obj_xlat, 0, (size_t)(obj_cnt * sizeof(obj_xlat[0])));
        memset(prop_xlat, 0, (size_t)(prop_cnt * sizeof(prop_xlat[0])));
        memset(enum_xlat, 0, (size_t)(enum_cnt * sizeof(enum_xlat[0])));
        
        /* read the function set dependency table */
        load_funcdep_from_object_file(fp, fname);
        
        /* read the metaclass dependency table */
        load_metadep_from_object_file(fp, fname);
        
        /* 
         *   Read the symbol table.  This will create translation entries for
         *   the object and property names found in the symbol table.  
         */
        if ((err = G_prs->load_object_file(fp, fname, obj_xlat, prop_xlat,
                                           enum_xlat)) != 0)
        {
            /* that failed - abort the load */
            goto done;
        }
        
        /* read the main code stream and its fixup list */
        G_cs_main->load_object_file(fp, fname);
        
        /* read the static code stream and its fixup list */
        G_cs_static->load_object_file(fp, fname);
        
        /* read the data stream and its fixup list */
        G_ds->load_object_file(fp, fname);
        
        /* read the object data stream and its fixup list */
        G_os->load_object_file(fp, fname);
        
        /* read the intrinsic class modifier stream */
        G_icmod_stream->load_object_file(fp, fname);
        
        /* read the BigNumber stream and its fixup list */
        G_bignum_stream->load_object_file(fp, fname);

        /* read the RexPattern stream and its fixup list */
        G_rexpat_stream->load_object_file(fp, fname);
        
        /* read the static initializer ID stream */
        G_static_init_id_stream->load_object_file(fp, fname);
        
        /* read the local variable symbol stream */
        G_lcl_stream->load_object_file(fp, fname);
        
        /* read the object ID fixup list */
        CTcIdFixup::load_object_file(
            fp, obj_xlat, obj_cnt, TCGEN_XLAT_OBJ, 4,
            fname, G_keep_objfixups ? &G_objfixup : 0);

        /* read the property ID fixup list */
        CTcIdFixup::load_object_file(
            fp, prop_xlat, prop_cnt, TCGEN_XLAT_PROP, 2,
            fname, G_keep_propfixups ? &G_propfixup : 0);

        /* read the enum ID fixup list */
        CTcIdFixup::load_object_file(
            fp, enum_xlat, enum_cnt, TCGEN_XLAT_ENUM, 2,
            fname, G_keep_enumfixups ? &G_enumfixup : 0);

        /* if the object file contains debugging information, read that */
        if ((hdr_flags & TCT3_OBJHDR_DEBUG) != 0)
        {
            /* load the debug records */
            load_debug_records_from_object_file(
                fp, fname, main_cs_start_ofs, static_cs_start_ofs);
        }
        
        /* read the macro definitions */
        {
            CVmFileStream str(fp);
            MyLoadMacErr err_handler(fname);
            G_tok->load_macros_from_file(&str, &err_handler);
        }

done: ;
    }
    err_finally
    {
        /* 
         *   free the ID translation arrays - we no longer need them after
         *   loading the object file, because we translate everything in the
         *   course of loading, so what's left in memory when we're done uses
         *   the new global numbering system 
         */
        if (obj_xlat != 0)
            t3free(obj_xlat);
        if (prop_xlat != 0)
            t3free(prop_xlat);
        if (enum_xlat != 0)
            t3free(enum_xlat);
    }
    err_end;

    /* return the result */
    return err;
}


/* ------------------------------------------------------------------------ */
/*
 *   Add a debug line table to our list 
 */
void CTcGenTarg::add_debug_line_table(ulong ofs)
{
    size_t idx;
    uchar *p;

    /* calculate the index of the next free entry on its page */
    idx = (size_t)(debug_line_cnt_ % TCT3_DEBUG_LINE_PAGE_SIZE);

    /* 
     *   if we've completely filled the last page, allocate a new one - we
     *   know we've exhausted the page if we're at the start of a new page
     *   (i.e., the index is zero) 
     */
    if (idx == 0)
    {
        tct3_debug_line_page *pg;

        /* allocate the new page */
        pg = (tct3_debug_line_page *)t3malloc(sizeof(*pg));

        /* link it in at the end of the list */
        pg->nxt = 0;
        if (debug_line_tail_ == 0)
            debug_line_head_ = pg;
        else
            debug_line_tail_->nxt = pg;
        debug_line_tail_ = pg;
    }

    /* get a pointer to the entry */
    p = debug_line_tail_->line_ofs + (idx * TCT3_DEBUG_LINE_REC_SIZE);

    /* 
     *   set this entry - one byte for the code stream ID, then a UINT4 with
     *   the offset in the stream 
     */
    *p = G_cs->get_stream_id();
    oswp4(p + 1, ofs);

    /* count it */
    ++debug_line_cnt_;
}

/*
 *   Load the debug records from an object file 
 */
void CTcGenTarg::load_debug_records_from_object_file(
    CVmFile *fp, const textchar_t *fname,
    ulong main_cs_start_ofs, ulong static_cs_start_ofs)
{
    int first_filedesc;
    ulong line_table_cnt;

    /* 
     *   Note the starting number of our file descriptors - in the file,
     *   we started numbering them at zero, but if we have already loaded
     *   other object files before this one, we'll be numbering ours after
     *   the ones previously loaded.  So, we'll need to fix up the
     *   references to the file descriptor indices accordingly.  
     */
    first_filedesc = G_tok->get_next_filedesc_index();
        
    /* read the source file list */
    read_sources_from_object_file(fp);

    /*
     *   Read the line record pointers.  For each line record table, we
     *   must fix up the line records to reflect the file descriptor
     *   numbering system.  
     */
    for (line_table_cnt = fp->read_uint4() ; line_table_cnt != 0 ;
         --line_table_cnt)
    {
        uchar stream_id;
        ulong ofs;
        CTcCodeStream *cs;
        ulong start_ofs;

        /* read the stream ID */
        stream_id = fp->read_byte();

        /* find the appropriate code stream */
        cs = (CTcCodeStream *)
             CTcDataStream::get_stream_from_id(stream_id, fname);

        /* get the appropriate starting offset */
        start_ofs = (cs == G_cs_main ? main_cs_start_ofs
                                     : static_cs_start_ofs);
        
        /* 
         *   Read the next line table offset - this is the offset in the
         *   code stream of the next debug line table.  Add our starting
         *   offset to get the true offset.  
         */
        ofs = fp->read_uint4() + start_ofs;
        
        /* update this table */
        fix_up_debug_line_table(cs, ofs, first_filedesc);
    }
}

/*
 *   Fix up a debug line record table for the current object file
 */
void CTcGenTarg::fix_up_debug_line_table(CTcCodeStream *cs,
                                         ulong line_table_ofs,
                                         int first_filedesc)
{
    uint cnt;
    ulong ofs;
    
    /* read the number of line records in the table */
    cnt = cs->readu2_at(line_table_ofs);

    /* adjust each entry */
    for (ofs = line_table_ofs + 2 ; cnt != 0 ; --cnt, ofs += G_sizes.dbg_line)
    {
        uint filedesc;
        
        /* read the old file descriptor ID */
        filedesc = cs->readu2_at(ofs + 2);

        /* adjust it to the new numbering system */
        filedesc += first_filedesc;

        /* write it back */
        cs->write2_at(ofs + 2, filedesc);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Load a function set dependency table from the object file.  We can
 *   add to the existing set of functions, but if we have N function sets
 *   defined already, the first N in the file must match the ones we have
 *   loaded exactly. 
 */
void CTcGenTarg::load_funcdep_from_object_file(class CVmFile *fp,
                                               const textchar_t *fname)
{
    int cnt;
    tc_fnset_entry *cur;

    /* read the count */
    cnt = fp->read_int2();

    /* read the entries */
    for (cur = fnset_head_ ; cnt != 0 ; --cnt)
    {
        char buf[128];
        size_t len;
        
        /* read this entry */
        len = fp->read_uint2();
        if (len + 1 > sizeof(buf))
        {
            G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                TCERR_OBJFILE_INV_FN_OR_META, fname);
            return;
        }

        /* read the name and null-terminate it */
        fp->read_bytes(buf, len);
        buf[len] = '\0';

        /* 
         *   if we are still scanning existing entries, make sure it
         *   matches; otherwise, add it 
         */
        if (cur != 0)
        {
            const char *vsn;
            char *ent_vsn;
            size_t name_len;
            size_t ent_name_len;

            /* get the version suffixes, if any */
            vsn = lib_find_vsn_suffix(buf, '/', 0, &name_len);
            ent_vsn = (char *)
                      lib_find_vsn_suffix(cur->nm, '/', 0, &ent_name_len);
            
            /* if it doesn't match, it's an error */
            if (name_len != ent_name_len
                || memcmp(cur->nm, buf, name_len) != 0)
                G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                    TCERR_OBJFILE_FNSET_CONFLICT,
                                    buf, fname);

            /* 
             *   if the new version string is higher than the old version
             *   string, keep the new version string 
             */
            if (vsn != 0 && ent_vsn != 0 && strcmp(vsn, ent_vsn) > 0
                && strlen(vsn) <= strlen(ent_vsn))
            {
                /* 
                 *   the new version is newer than the version in the
                 *   table - overwrite the table version with the new
                 *   version, so that the table keeps the newest version
                 *   mentioned anywhere (newer versions are upwardly
                 *   compatible with older versions, so the code that uses
                 *   the older version will be equally happy with the
                 *   newer version) 
                 */
                strcpy(ent_vsn, vsn);
            }

            /* move on to the next one */
            cur = cur->nxt;
        }
        else
        {
            /* we're past the existing list - add the new function set */
            add_fnset(buf, len);
        }
    }
}

/*
 *   Load a metaclass dependency table from the object file.  We can add
 *   to the existing set of metaclasses, but if we have N metaclasses
 *   defined already, the first N in the file must match the ones we have
 *   loaded exactly.  
 */
void CTcGenTarg::load_metadep_from_object_file(class CVmFile *fp,
                                               const textchar_t *fname)
{
    int cnt;
    tc_meta_entry *cur;

    /* read the count */
    cnt = fp->read_int2();

    /* read the entries */
    for (cur = meta_head_ ; cnt != 0 ; --cnt)
    {
        char buf[128];
        size_t len;

        /* read this entry */
        len = fp->read_uint2();
        if (len + 1 > sizeof(buf))
        {
            G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                TCERR_OBJFILE_INV_FN_OR_META, fname);
            return;
        }

        /* read the name and null-terminate it */
        fp->read_bytes(buf, len);
        buf[len] = '\0';

        /* 
         *   if we are still scanning existing entries, make sure it
         *   matches; otherwise, add it 
         */
        if (cur != 0)
        {
            const char *vsn;
            char *ent_vsn;
            size_t name_len;
            size_t ent_name_len;

            /* find the version suffix, if any */
            vsn = lib_find_vsn_suffix(buf, '/', 0, &name_len);

            /* find the version suffix in this entry's name */
            ent_vsn = (char *)
                      lib_find_vsn_suffix(cur->nm, '/', 0, &ent_name_len);

            /* if it doesn't match the entry name, it's an error */
            if (name_len != ent_name_len
                || memcmp(cur->nm, buf, name_len) != 0)
            {
                /* log a mis-matched metaclass error */
                G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                    TCERR_OBJFILE_META_CONFLICT, buf, fname);
            }

            /* 
             *   if the new version string is higher than the old version
             *   string, keep the new version string 
             */
            if (vsn != 0 && ent_vsn != 0 && strcmp(vsn, ent_vsn) > 0
                && strlen(vsn) <= strlen(ent_vsn))
            {
                /* 
                 *   the new version is newer than the version in the
                 *   table - overwrite the table version with the new
                 *   version, so that the table keeps the newest version
                 *   mentioned anywhere (newer versions are upwardly
                 *   compatible with older versions, so the code that uses
                 *   the older version will be equally happy with the
                 *   newer version) 
                 */
                strcpy(ent_vsn, vsn);
            }
                
            /* move on to the next one */
            cur = cur->nxt;
        }
        else
        {
            /* we're past the existing list - add the new metaclass */
            add_meta(buf, len, 0);
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Write the source file list to an object file 
 */
void CTcGenTarg::write_sources_to_object_file(CVmFile *fp)
{
    CTcTokFileDesc *desc;

    /* write the number of entries */
    fp->write_uint2(G_tok->get_filedesc_count());

    /* write the entries */
    for (desc = G_tok->get_first_filedesc() ; desc != 0 ;
         desc = desc->get_next())
    {
        size_t len;
        const char *fname;

        /* get the filename - use the resolved local filename */
        fname = desc->get_fname();

        /* write the length of the filename */
        len = strlen(fname);
        fp->write_uint2(len);

        /* write the filename */
        fp->write_bytes(fname, len);
    }
}

/*
 *   Read a source file list from an object file 
 */
void CTcGenTarg::read_sources_from_object_file(CVmFile *fp)
{
    uint cnt;
    uint i;

    /* read the number of entries */
    cnt = fp->read_uint2();

    /* read the entries */
    for (i = 0 ; i < cnt ; ++i)
    {
        size_t len;
        char fname[OSFNMAX];

        /* read the length of the entry */
        len = fp->read_uint2();

        /* see if it fits in our buffer */
        if (len <= sizeof(fname))
        {
            /* read it */
            fp->read_bytes(fname, len);
        }
        else
        {
            /* it's too long - truncate to the buffer size */
            fp->read_bytes(fname, sizeof(fname));

            /* skip the rest */
            fp->set_pos(fp->get_pos() + len - sizeof(fname));

            /* note the truncated length */
            len = sizeof(fname);
        }

        /* 
         *   Add it to the tokenizer list.  Always create a new entry,
         *   rather than re-using an existing entry.  When loading
         *   multiple object files, this might result in the same file
         *   appearing as multiple different descriptors, but it's a small
         *   price to pay (it doesn't add too much redundant space to the
         *   image file, and in any case the information is only retained
         *   when we're compiling for debugging) for a big gain in
         *   simplicity (the source references in the object file can be
         *   fixed up simply by adding the object file's base index to all
         *   of the reference indices).  
         */
        G_tok->create_file_desc(fname, len);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Calculate pool layouts.  This is called at the start of the link
 *   phase: at this point, we know the sizes of the largest constant pool
 *   and code pool objects, so we can figure the layouts of the pools.  
 */
void CTcGenTarg::calc_pool_layouts(size_t *first_static_page)
{
    size_t max_str;
    size_t max_list;
    size_t max_item;

    /*
     *   We've parsed the entire program, so we now know the lengths of
     *   the longest string constant and the longest list constant.  From
     *   this, we can figure the size of our constant pool pages: since
     *   each list or string must be contained entirely in a single page,
     *   the minimum page size is the length of the longest string or list.
     *   
     *   We must pick a power of two for our page size.  We don't want to
     *   make the page size too small; each page requires a small amount
     *   of overhead, hence the more pages for a given total constant pool
     *   size, the more overhead.  We also don't want to make the page
     *   size too large, because smaller page sizes will give us better
     *   performance on small machines that will have to swap pages in and
     *   out (the smaller a page, the less time it will take to load a
     *   page).
     *   
     *   Start at 2k, which is big enough that the data part will
     *   overwhelm the per-page overhead, but small enough that it can be
     *   loaded quickly on a small machine.  If that's at least twice the
     *   length of the longest string or list, use it; otherwise, double
     *   it and try again.  
     */

    /* 
     *   find the length in bytes of the longest string - we require the
     *   length prefix in addition to the bytes of the string 
     */
    max_str = max_str_len_ + VMB_LEN;

    /* 
     *   find the length in bytes of the longest list - we require one
     *   data holder per element, plus the length prefix 
     */
    max_list = (max_list_cnt_ * VMB_DATAHOLDER) + VMB_LEN;

    /* get the larger of the two - this will be our minimum size */
    max_item = max_str;
    if (max_list > max_item)
        max_item = max_list;

    /* 
     *   if the maximum item size is under 16k, look for a size that will
     *   hold twice the maximum item size; otherwise, relax this
     *   requirement, since the pages are getting big, and look for
     *   something that merely fits the largest element 
     */
    if (max_item < 16*1024)
        max_item <<= 1;

    /* calculate the constant pool layout */
    const_layout_.calc_layout(G_ds, max_item, TRUE);

    /* calculate the main code pool layout */
    code_layout_.calc_layout(G_cs_main, max_bytecode_len_, TRUE);

    /* note the number of pages of regular code */
    *first_static_page = code_layout_.page_cnt_;

    /* 
     *   add the static pool into the code pool layout, since we'll
     *   ultimately write the static code as part of the plain code pages 
     */
    code_layout_.calc_layout(G_cs_static, max_bytecode_len_, FALSE);
}


/* ------------------------------------------------------------------------ */
/*
 *   Write the image file
 */
void CTcGenTarg::write_to_image(CVmFile *fp, uchar data_xor_mask,
                                const char tool_data[4])
{
    tc_meta_entry *meta;
    CTcSymbol *sym;
    unsigned long main_ofs;
    vm_prop_id_t construct_prop = VM_INVALID_PROP;
    vm_prop_id_t finalize_prop = VM_INVALID_PROP;
    vm_prop_id_t graminfo_prop = VM_INVALID_PROP;
    vm_prop_id_t gramtag_prop = VM_INVALID_PROP;
    vm_prop_id_t miscvocab_prop = VM_INVALID_PROP;
    tc_fnset_entry *fnset;
    CVmImageWriter *image_writer;
    int bignum_idx;
    int rexpat_idx;
    int int_class_idx;
    CTcPrsExport *exp;
    CTcDataStream *cs_list[2];
    size_t first_static_code_page;

    /* 
     *   if we have any BigNumber data, get the BigNumber metaclass index
     *   (or define it, if the program didn't do so itself) 
     */
    if (G_bignum_stream->get_ofs() != 0)
        bignum_idx = find_or_add_meta("bignumber", 9, 0);

    /* if we have any RexPattern data, get the RexPattern metaclass index */
    if (G_rexpat_stream->get_ofs() != 0)
        rexpat_idx = find_or_add_meta("regex-pattern", 13, 0);

    /* apply internal object/property ID fixups in the symbol table */
    G_prs->apply_internal_fixups();

    /* build the grammar productions */
    G_prs->build_grammar_productions();

    /* 
     *   Build the dictionaries.  We must wait until after applying the
     *   internal fixups to build the dictionaries, so that we have the
     *   final, fully-resolved form of each object's vocabulary list before
     *   we build the dictionaries.  We must also wait until after we build
     *   the grammar productions, because the grammar productions can add
     *   dictionary entries for their literal token matchers.  
     */
    G_prs->build_dictionaries();

    /* 
     *   Build the multi-method static initializers.  Note: this must be done
     *   before we generate the intrinsic class objects, because we might add
     *   intrinsic class modifiers in the course of building the mm
     *   initializers. 
     */
    build_multimethod_initializers();

    /* make sure the the IntrinsicClass intrinsic class is itself defined */
    int_class_idx = find_or_add_meta("intrinsic-class", 15, 0);

    /* build the IntrinsicClass objects */
    build_intrinsic_class_objs(G_int_class_stream);

    /* 
     *   Apply fixups for the local variable stream.  This stream isn't
     *   written to the image file, but we need to do the layout in order to
     *   apply its fixups. 
     */
    CTcStreamLayout vpl;
    vpl.calc_layout(G_lcl_stream, 65535, TRUE);

    /* 
     *   Build the local symbol records.  We need to do this before
     *   calculating the code pool layout, because this writes more data to
     *   the constant pool.  We need to build the symbol records separately
     *   for the regular and static code pools, since they haven't been
     *   merged yet.
     */
    CVmHashTable *lcltab = new CVmHashTable(1024, new CVmHashFuncCS(), TRUE);
    build_local_symbol_records(G_cs_main, lcltab);
    build_local_symbol_records(G_cs_static, lcltab);

    /* we're done with the local symbol hash table */
    delete lcltab;

    /* calculate the final pool layouts */
    calc_pool_layouts(&first_static_code_page);

    /* build the source line location maps, if debugging */
    if (G_debug)
        build_source_line_maps();

    /* look up the "_main" symbol in the global symbol table */
    sym = G_prs->get_global_symtab()->find("_main");

    /* 
     *   if there's no "_main" symbol, or it's not a function, it's an
     *   error 
     */
    if (sym == 0)
    {
        /* "_main" isn't defined - log an error and abort */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_MAIN_NOT_DEFINED);
        return;
    }
    else if (sym->get_type() != TC_SYM_FUNC)
    {
        /* "_main" isn't a function - log an error and abort */
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_MAIN_NOT_FUNC);
        return;
    }
    else
    {
        /* 
         *   Get the "_main" symbol's code pool address - this is the
         *   program's entrypoint.  We can ask for this information at
         *   this point because we don't start writing the image file
         *   until after the final fixup pass, which is where this address
         *   is finally calculated.  
         */
        main_ofs = ((CTcSymFunc *)sym)->get_code_pool_addr();
    }

    /* get the constructor and finalizer property ID's */
    construct_prop = (tctarg_prop_id_t)G_prs->get_constructor_prop();
    finalize_prop = (tctarg_prop_id_t)G_prs->get_finalize_prop();

    /* get the special generated GrammarProd property IDs */
    graminfo_prop = (tctarg_prop_id_t)G_prs->get_grammarInfo_prop();
    gramtag_prop = (tctarg_prop_id_t)G_prs->get_grammarTag_prop();
    miscvocab_prop = (tctarg_prop_id_t)G_prs->get_miscvocab_prop();

    /* create our image writer */
    image_writer = new CVmImageWriter(fp);

    /* prepare the image file - use file format version 1 */
    image_writer->prepare(1, tool_data);

    /* write the entrypoint offset and data structure parameters */
    image_writer->write_entrypt(main_ofs, G_sizes.mhdr,
                                G_sizes.exc_entry, G_sizes.dbg_line,
                                G_sizes.dbg_hdr, G_sizes.lcl_hdr,
                                G_sizes.dbg_frame,
                                G_sizes.dbg_fmt_vsn);

    /* begin writing the symbolic items */
    image_writer->begin_sym_block();

    /* run through the list of exports in the parser */
    for (exp = G_prs->get_exp_head() ; exp != 0 ; exp = exp->get_next())
    {
        CTcPrsExport *exp2;
        int dup_err_cnt;
        
        /* 
         *   if this one's external name is null, it means that we've
         *   previously encountered it as a duplicate and marked it as such
         *   - in this case, simply skip it 
         */
        if (exp->get_ext_name() == 0)
            continue;

        /* make sure it's not one of our special ones */
        if (exp->ext_name_matches("LastProp")
            || exp->ext_name_matches("Constructor")
            || exp->ext_name_matches("Destructor")
            || exp->ext_name_starts_with("operator "))
        {
            /* it's a reserved export - flag an error */
            G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                TCERR_RESERVED_EXPORT,
                                (int)exp->get_ext_len(),
                                exp->get_ext_name());
        }
            

        /* look up the symbol, defining as a property if undefined */
        sym = G_prs->get_global_symtab()
              ->find_or_def_prop(exp->get_sym(), exp->get_sym_len(), FALSE);

        /*
         *   Scan the rest of the export list for duplicates.  If we find
         *   the symbol external name exported with a different value, it's
         *   an error. 
         */
        for (dup_err_cnt = 0, exp2 = exp->get_next() ; exp2 != 0 ;
             exp2 = exp2->get_next())
        {
            /* if this one has already been marked as a duplicate, skip it */
            if (exp2->get_ext_name() == 0)
                continue;
            
            /* check for a match of the external name */
            if (exp->ext_name_matches(exp2))
            {
                /* 
                 *   This one matches, so it's a redundant export for the
                 *   same name.  If it's being exported as the same internal
                 *   symbol as the other one, this is fine; otherwise it's
                 *   an error, since the same external name can't be given
                 *   two different meanings.
                 */
                if (!exp->sym_matches(exp2))
                {
                    /* 
                     *   It doesn't match - log an error.  If we've already
                     *   logged an error, show a continuation error;
                     *   otherwise show the first error for the symbol.
                     */
                    ++dup_err_cnt;
                    if (dup_err_cnt == 1)
                    {
                        /* it's the first error - show the long form */
                        G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                            TCERR_DUP_EXPORT,
                                            (int)exp->get_ext_len(),
                                            exp->get_ext_name(),
                                            (int)exp->get_sym_len(),
                                            exp->get_sym(),
                                            (int)exp2->get_sym_len(),
                                            exp2->get_sym());
                    }
                    else
                    {
                        /* it's a follow-up error */
                        G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                            TCERR_DUP_EXPORT_AGAIN,
                                            (int)exp->get_ext_len(),
                                            exp->get_ext_name(),
                                            (int)exp2->get_sym_len(),
                                            exp2->get_sym());
                    }
                }

                /* 
                 *   Regardless of whether this one matches or not, remove
                 *   it from the list by setting its external name to null -
                 *   we only want to include each symbol in the export list
                 *   once. 
                 */
                exp2->set_extern_name(0, 0);
            }
        }

        /* write it out according to its type */
        switch(sym->get_type())
        {
        case TC_SYM_OBJ:
            /* write the object symbol */
            image_writer->write_sym_item_objid(
                exp->get_ext_name(), exp->get_ext_len(),
                ((CTcSymObj *)sym)->get_obj_id());
            break;

        case TC_SYM_PROP:
            /* write the property symbol */
            image_writer->write_sym_item_propid(
                exp->get_ext_name(), exp->get_ext_len(),
                ((CTcSymProp *)sym)->get_prop());
            break;

        case TC_SYM_FUNC:
            /* write the function symbol */
            image_writer->write_sym_item_func(
                exp->get_ext_name(), exp->get_ext_len(),
                ((CTcSymFunc *)sym)->get_code_pool_addr());
            break;

        default:
            /* can't export other types */
            G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                                TCERR_INVALID_TYPE_FOR_EXPORT,
                                (int)exp->get_sym_len(), exp->get_sym());
            break;
        }
    }

    /* 
     *   write the last property ID - this is a special synthetic export
     *   that we provide automatically 
     */
    image_writer->write_sym_item_propid("LastProp", next_prop_);

    /* write our Constructor and Destructor property ID's */
    if (construct_prop != VM_INVALID_PROP)
        image_writer->write_sym_item_propid("Constructor", construct_prop);
    if (finalize_prop != VM_INVALID_PROP)
        image_writer->write_sym_item_propid("Destructor", finalize_prop);

    /* the compiler generates grammarTag and grammarInfo properties */
    if (graminfo_prop != VM_INVALID_PROP)
        image_writer->write_sym_item_propid(
            "GrammarProd.grammarInfo", graminfo_prop);
    if (gramtag_prop != VM_INVALID_PROP)
        image_writer->write_sym_item_propid(
            "GrammarProd.grammarTag", gramtag_prop);

    /* likewise miscVocab */
    if (miscvocab_prop != VM_INVALID_PROP)
        image_writer->write_sym_item_propid(
            "GrammarProd.miscVocab", miscvocab_prop);

    /* write the operator properties */
    write_op_export(image_writer, G_prs->ov_op_add_);
    write_op_export(image_writer, G_prs->ov_op_sub_);
    write_op_export(image_writer, G_prs->ov_op_mul_);
    write_op_export(image_writer, G_prs->ov_op_div_);
    write_op_export(image_writer, G_prs->ov_op_mod_);
    write_op_export(image_writer, G_prs->ov_op_xor_);
    write_op_export(image_writer, G_prs->ov_op_shl_);
    write_op_export(image_writer, G_prs->ov_op_ashr_);
    write_op_export(image_writer, G_prs->ov_op_lshr_);
    write_op_export(image_writer, G_prs->ov_op_bnot_);
    write_op_export(image_writer, G_prs->ov_op_bor_);
    write_op_export(image_writer, G_prs->ov_op_band_);
    write_op_export(image_writer, G_prs->ov_op_neg_);
    write_op_export(image_writer, G_prs->ov_op_idx_);
    write_op_export(image_writer, G_prs->ov_op_setidx_);

    /* done with the symbolic names */
    image_writer->end_sym_block();

    /* write the function-set dependency table */
    image_writer->begin_func_dep(fnset_cnt_);
    for (fnset = fnset_head_ ; fnset != 0 ; fnset = fnset->nxt)
        image_writer->write_func_dep_item(fnset->nm);
    image_writer->end_func_dep();

    /* start the metaclass dependency table */
    image_writer->begin_meta_dep(meta_cnt_);

    /* write the metaclass dependency items */
    for (meta = meta_head_ ; meta != 0 ; meta = meta->nxt)
    {
        /* write the dependency item */
        image_writer->write_meta_dep_item(meta->nm);

        /* if there's an associated symbol, write the property list */
        if (meta->sym != 0)
        {
            CTcSymMetaProp *prop;

            /* scan the list of properties and write each one */
            for (prop = meta->sym->get_prop_head() ; prop != 0 ;
                 prop = prop->nxt_)
            {
                /* write this item's property */
                image_writer->write_meta_item_prop(prop->prop_->get_prop());
            }
        }
    }

    /* end the metaclass dependency table */
    image_writer->end_meta_dep();

    /* write the code pool streams (don't bother masking the code bytes) */
    cs_list[0] = G_cs_main;
    cs_list[1] = G_cs_static;
    code_layout_.write_to_image(cs_list, 2, image_writer, 1, 0);

    /* 
     *   write the constant pool (applying the constant pool data mask to
     *   obscure any text strings in the data) 
     */
    const_layout_.write_to_image(&G_ds, 1, image_writer, 2, data_xor_mask);

    /* write the "TADS object" data */
    write_tads_objects_to_image(G_os, image_writer, TCT3_METAID_TADSOBJ);

    /* write the intrinsic class modifier object data */
    write_tads_objects_to_image(G_icmod_stream, image_writer,
                                TCT3_METAID_ICMOD);

    /* write the dictionary data - this is a stream of dictionary objects */
    write_nontads_objs_to_image(G_dict_stream, image_writer,
                                TCT3_METAID_DICT, TRUE);

    /* write the grammar data - this is a stream of production objects */
    write_nontads_objs_to_image(G_gramprod_stream, image_writer,
                                TCT3_METAID_GRAMPROD, TRUE);

    /* if we have any BigNumber data, write it out */
    if (G_bignum_stream->get_ofs() != 0)
        write_nontads_objs_to_image(G_bignum_stream,
                                    image_writer, bignum_idx, FALSE);

    /* if we have any RexPattern data, write it out */
    if (G_rexpat_stream->get_ofs() != 0)
        write_nontads_objs_to_image(G_rexpat_stream,
                                    image_writer, rexpat_idx, FALSE);

    /* if we have any IntrinsicClass data, write it out */
    if (G_int_class_stream->get_ofs() != 0)
        write_nontads_objs_to_image(G_int_class_stream, image_writer,
                                    int_class_idx, FALSE);

    /* write the static initializer list */
    write_static_init_list(image_writer,
                           first_static_code_page * code_layout_.page_size_);

    /* write debug information if desired */
    if (G_debug)
    {
        /* write the source file table */
        write_sources_to_image(image_writer);

        /* write the global symbol table to the image file */
        write_global_symbols_to_image(image_writer);

        /* write the method header list */
        write_method_list_to_image(image_writer);

        /* write the macro records */
        write_macros_to_image(image_writer);
    }

    /* finish the image file */
    image_writer->finish();

    /* delete our image writer */
    delete image_writer;
    image_writer = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Write an export record for an operator overload property 
 */
void CTcGenTarg::write_op_export(CVmImageWriter *image_writer,
                                 CTcSymProp *prop)
{
    if (prop != 0 && prop->get_prop() != VM_INVALID_PROP)
    {
        image_writer->write_sym_item_propid(
            prop->get_sym(), prop->get_sym_len(), prop->get_prop());
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Write the static initializer ID list 
 */
void CTcGenTarg::write_static_init_list(CVmImageWriter *image_writer,
                                        ulong main_cs_size)
{
    ulong rem;
    ulong ofs;
    ulong init_cnt;

    /* 
     *   calculate the number of initializers - this is simply the size of
     *   the stream divided by the size of each record (4 bytes for object
     *   ID, 2 bytes for property ID) 
     */
    init_cnt = G_static_init_id_stream->get_ofs() / 6;

    /* add the multi-method initializer object, if there is one */
    if (mminit_obj_ != VM_INVALID_OBJ)
        init_cnt += 1;
    
    /* start the block */
    image_writer->begin_sini_block(main_cs_size, init_cnt);

    /* write the multi-method initializer object, if applicable */
    if (mminit_obj_ != VM_INVALID_OBJ)
    {
        /* write the object data */
        char buf[6];
        oswp4(buf, mminit_obj_);                           /* the object ID */
        oswp2(buf+4, 1);           /* our arbitrary initializer property ID */
        image_writer->write_bytes(buf, 6);
    }

    /* write the bytes */
    for (ofs = 0, rem = G_static_init_id_stream->get_ofs() ; rem != 0 ; )
    {
        const char *ptr;
        ulong cur;
        
        /* get the next chunk */
        ptr = G_static_init_id_stream->get_block_ptr(ofs, rem, &cur);

        /* write this chunk */
        image_writer->write_bytes(ptr, cur);

        /* advance past this chunk */
        ofs += cur;
        rem -= cur;
    }

    /* end the block */
    image_writer->end_sini_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   Build synthesized code.  This is called after all of the object files
 *   are loaded and before we generate the final image file, to give the
 *   linker a chance to generate any automatically generated code.  We use
 *   this to generate the stub base functions for multi-methods.  
 */
struct mmstub_ctx
{
    mmstub_ctx()
    {
        mmc = 0;
        cnt = 0;
    }
    
    /* _multiMethodCall function symbol */
    CTcSymFunc *mmc;

    /* number of multi-method stubs we generated */
    int cnt;
};

void CTcGenTarg::build_synthesized_code()
{
    mmstub_ctx ctx;
    
    /* look up the _multiMethodCall function */
    ctx.mmc = (CTcSymFunc *)G_prs->get_global_symtab()->find(
        "_multiMethodCall", 16);

    /* 
     *   our generated code isn't part of any object file - flag a new object
     *   file so that we don't get confused into thinking this came from the
     *   last object file loaded 
     */
    G_cs_static->set_object_file_start_ofs();
    G_os->set_object_file_start_ofs();

    /* build out the stubs for the multi-method base functions */
    G_prs->get_global_symtab()->enum_entries(&multimethod_stub_cb, &ctx);

    /* 
     *   if we generated any stubs, we definitely need _multiMethodCall to be
     *   defined - if it's not, it's an error 
     */
    if (ctx.cnt != 0 && (ctx.mmc == 0 || ctx.mmc->get_type() != TC_SYM_FUNC))
    {
        G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_MISSING_MMREG,
                            "_multiMethodCall");
    }
}

/* callback context - build multi-method base function stubs */
void CTcGenTarg::multimethod_stub_cb(void *ctx0, CTcSymbol *sym)
{
    mmstub_ctx *ctx = (mmstub_ctx *)ctx0;

    /* if this is a function, check to see if it's a multi-method stub */
    if (sym->get_type() == TC_SYM_FUNC)
    {
        CTcSymFunc *fsym = (CTcSymFunc *)sym;

        /* 
         *   It's a base function if it's marked as a multi-method and it
         *   doesn't have a '*' in its name.  (If there's a '*', it's a
         *   concrete multi-method rather than a base function.)  
         */
        if (fsym->is_multimethod())
        {
            /* it's marked as a multi-method - check for a decorated name */
            const char *p = sym->getstr();
            size_t rem = sym->getlen();
            for ( ; rem != 0 && *p != '*' ; ++p, --rem) ;
            if (rem == 0)
            {
                tct3_method_gen_ctx gen_ctx;

                /* 
                 *   It's a multi-method base function - build out its stub.
                 *   The stub function is a varargs function with no fixed
                 *   parameters - i.e., funcName(...).  Its body simply calls
                 *   _multiMethodCall with a pointer to itself as the base
                 *   function.  
                 */
                G_cg->open_method(G_cs_main,
                                  fsym, fsym->get_fixup_list_anchor(),
                                  0, 0, 0, 0, TRUE, FALSE, FALSE, FALSE,
                                  &gen_ctx);

                /* set the anchor in the function symbol */
                fsym->set_anchor(gen_ctx.anchor);

                /* 
                 *   turn the arguments into a list, leaving this on the
                 *   stack as the second argument for _multiMethodCall 
                 */
                G_cg->write_op(OPC_PUSHPARLST);
                G_cs->write(0);
                G_cg->note_push();

                /* push the function address argument */
                CTcConstVal funcval;
                funcval.set_funcptr(fsym);
                CTPNConst cfunc(&funcval);
                cfunc.gen_code(FALSE, FALSE);
                G_cg->note_push();

                /* 
                 *   call _multiMethodCall, if it's defined (if not, the
                 *   caller will flag it as an error, so we don't need to
                 *   worry about that here - just skip generating the call) 
                 */
                if (ctx->mmc != 0)
                    ctx->mmc->gen_code_call(FALSE, 2, FALSE, FALSE);

                /* return the result */
                G_cg->write_op(OPC_RETVAL);
                G_cg->note_pop();

                /* finish the method */
                G_cg->close_method(0, 0, 0, 0, &gen_ctx, 0);
                G_cg->close_method_cleanup(&gen_ctx);

                /* the stub symbol now has a definition */
                fsym->set_extern(FALSE);

                /* count it */
                ctx->cnt += 1;
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Start a OBJS header for a TadsObject to a given stream.  This only
 *   writes the fixed part; the caller must then write the superclass list
 *   and the property table.  After the contents have been written, call
 *   close_tadsobj() to finalize the header data.  
 */
void CTcGenTarg::open_tadsobj(tct3_tadsobj_ctx *ctx,
                              CTcDataStream *stream,
                              vm_obj_id_t obj_id,
                              int sc_cnt, int prop_cnt,
                              unsigned int internal_flags,
                              unsigned int vm_flags)
{
    /* remember the stream in the context */
    ctx->stream = stream;

    /* write the internal header */
    stream->write2(internal_flags);
    
    /* note the start of the VM object data */
    ctx->obj_ofs = stream->get_ofs();

    /* write the fixed header data */
    stream->write_obj_id(obj_id);                              /* object ID */
    stream->write2(0);   /* byte size placeholder - we'll fix up at "close" */
    stream->write2(sc_cnt);                             /* superclass count */
    stream->write2(prop_cnt);                             /* property count */
    stream->write2(vm_flags);                               /* object flags */
}

/*
 *   Close a TadsObject header.  This must be called after the object's
 *   contents have been written so that we can fix up the header with the
 *   actual data size. 
 */
void CTcGenTarg::close_tadsobj(tct3_tadsobj_ctx *ctx)
{
    /* fix up the object size data */
    ctx->stream->write2_at(ctx->obj_ofs + 4,
                           ctx->stream->get_ofs() - ctx->obj_ofs - 6);
}


/* ------------------------------------------------------------------------ */
/*
 *   Linker support: ensure that the given intrinsic class has a modifier
 *   object.  If there's no modifier, we'll create one and add the code for
 *   it to the object stream.
 *   
 *   This should only be called during the linking phase, after code
 *   generation is completed.  If you want to create a modifier during
 *   compilation, you should instead use CTcParser::find_or_def_obj(), since
 *   that creates the necessary structures for object file generation and
 *   later linking.  
 */
void CTcGenTarg::linker_ensure_mod_obj(CTcSymMetaclass *mc)
{
    /* if there's no modifier object, create one */
    if (mc->get_mod_obj() == 0)
    {
        /* create a modifier object */
        CTcSymObj *mod_sym = CTcSymObj::synthesize_modified_obj_sym(FALSE);

        /* set it to be an IntrinsicClassModifier object */
        mod_sym->set_metaclass(TC_META_ICMOD);

        /* link the modifier to the metaclass */
        mc->set_mod_obj(mod_sym);
        
        /* 
         *   generate the object data - this is simply an empty object with
         *   no superclasses, and it goes in the intrinsic class modifier
         *   stream 
         */
        tct3_tadsobj_ctx obj_ctx;
        G_cg->open_tadsobj(
            &obj_ctx, G_icmod_stream,
            mod_sym->get_obj_id(), 0, 0, 0, 0);
        G_cg->close_tadsobj(&obj_ctx);
    }
}

/*
 *   Ensure that the given intrinsic class has a modifier object, by name. 
 */
void CTcGenTarg::linker_ensure_mod_obj(const char *name, size_t len)
{
    /* look up the symbol */
    CTcSymMetaclass *mc = (CTcSymMetaclass *)G_prs->get_global_symtab()
                          ->find(name, len);

    /* if we found the metaclass symbol, add a modifier if needed */
    if (mc != 0 && mc->get_type() == TC_SYM_METACLASS)
        linker_ensure_mod_obj(mc);
}


/* ------------------------------------------------------------------------ */
/*
 *   Build the multi-method initializers 
 */

/* enumerator callback context */
struct mminit_ctx
{
    mminit_ctx()
    {
        mmr = 0;
        cnt = 0;
    }
    
    /* _multiMethodRegister function symbol */
    CTcSymFunc *mmr;

    /* number of multi-method registrations we generated */
    int cnt;
};

/* main initializer builder */
void CTcGenTarg::build_multimethod_initializers()
{
    tct3_method_gen_ctx genctx;
    mminit_ctx ctx;

    /* look up the _multiMethodRegister function */
    ctx.mmr = (CTcSymFunc *)G_prs->get_global_symtab()->find(
        "_multiMethodRegister", 20);

    /* 
     *   open the method - it's a static initializer, so write it to the
     *   static stream 
     */
    G_cg->open_method(G_cs_static, 0, 0, 0, 0, 0, 0, FALSE,
                      FALSE, FALSE, FALSE, &genctx);

    /* scan the symbol table for multimethods and generate initializers */
    G_prs->get_global_symtab()->enum_entries(&multimethod_init_cb, &ctx);

    /* 
     *   if we found any multi-methods, generate a call to
     *   _multiMethodBuildBindings 
     */
    if (ctx.cnt != 0)
    {
        /* look up the function - it's an error if it's not defined */
        CTcSymFunc *mmb = (CTcSymFunc *)G_prs->get_global_symtab()->find(
            "_multiMethodBuildBindings", 25);
        if (mmb == 0 || mmb->get_type() != TC_SYM_FUNC)
        {
            G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_MISSING_MMREG,
                                "_multiMethodBuildBindings");
            return;
        }

        /* write the call instruction */
        G_cg->write_op(OPC_CALL);
        G_cs->write(0);                                   /* argument count */
        mmb->add_abs_fixup(G_cs);                 /* function address fixup */
        G_cs->write4(0);                               /* fixup placeholder */
    }

    /* close the method and clean up */
    G_cg->close_method(0, 0, 0, 0, &genctx, 0);
    G_cg->close_method_cleanup(&genctx);

    /* 
     *   if we generated any registrations, create the initializer object -
     *   this will go in the static initializer block to trigger invocation
     *   of the registration routine at load time 
     */
    if (ctx.cnt != 0)
    {
        /* we have multi-methods, so we definitely need _multiMethodRegister */
        if (ctx.mmr == 0 || ctx.mmr->get_type() != TC_SYM_FUNC)
        {
            G_tcmain->log_error(0, 0, TC_SEV_ERROR, TCERR_MISSING_MMREG,
                                "_multiMethodRegister");
            return;
        }

        /* create an anonymous object to hold the initializer code */
        mminit_obj_ = G_cg->new_obj_id();

        /* add a debugging symbol for it */
        G_prs->get_global_symtab()->add_entry(new CTcSymObj(
            "<multiMethodInit>", 17, FALSE, mminit_obj_, TRUE,
            TC_META_TADSOBJ, 0));

        /* write the object header: no superclasses, 1 property */
        tct3_tadsobj_ctx obj_ctx;
        open_tadsobj(&obj_ctx, G_os, mminit_obj_, 0, 1, 0, 0);
        
        /* write the static initializer property */
        G_os->write2(1);                           /* arbitrary property ID */
        G_os->write(VM_CODEOFS);    /* offset of the code we just generated */
        CTcAbsFixup::add_abs_fixup(
            &genctx.anchor->fixup_info_.internal_fixup_head_,
            G_os, G_os->get_ofs());
        G_os->write4(0);                                     /* placeholder */
        
        /* fix up the object size data */
        close_tadsobj(&obj_ctx);
    }

    /* switch back to the main stream */
    G_cs = G_cs_main;
}

/* callback context - build multi-method registration calls */
void CTcGenTarg::multimethod_init_cb(void *ctx0, CTcSymbol *sym)
{
    mminit_ctx *ctx = (mminit_ctx *)ctx0;
    
    /* if this is a function, check to see if it's a multi-method instance */
    if (sym->get_type() == TC_SYM_FUNC)
    {
        CTcSymFunc *fsym = (CTcSymFunc *)sym;

        /* 
         *   multi-method instances have names of the form
         *   "name*type1,type2", so check the name to see if it fits the
         *   pattern 
         */
        const char *p = sym->getstr();
        size_t rem = sym->getlen();
        int is_mm = FALSE;
        for ( ; rem != 0 ; ++p, --rem)
        {
            /* 
             *   if we found a '*', it's a multimethod; otherwise, if it's
             *   anything other than a symbol character, it's not a
             *   multimethod 
             */
            if (*p == '*')
            {
                is_mm = TRUE;
                break;
            }
            else if (!is_sym(*p))
                break;
        }

        /* 
         *   If it's a multi-method symbol, build the initializer.  If it's
         *   the base function for a multi-method, build out the stub
         *   function. 
         */
        if (is_mm)
        {
            int argc;
            
            /* note the function base name - it's the part up to the '*' */
            const char *funcname = sym->getstr();
            size_t funclen = (size_t)(p - funcname);

            /* look up the base function symbol */
            CTcSymFunc *base_sym = (CTcSymFunc *)G_prs->get_global_symtab()
                                   ->find(funcname, funclen);

            /* if it's not defined as a function, ignore it */
            if (base_sym == 0 || base_sym->get_type() != TC_SYM_FUNC)
                return;

            /* 
             *   skip to the end of the string, and remove the '*' from the
             *   length count 
             */
            p += rem;
            --rem;

            /* 
             *   Run through the decorated name and look up each mentioned
             *   class.  We need to push the parameters onto the stack in
             *   reverse order to match the VM calling conventions.  
             */
            for (argc = 0 ; rem != 0 ; ++argc)
            {
                /* remember where the current name starts */
                size_t plen;

                /* skip the terminator for this item */
                --p, --rem;

                /* scan backwards to the previous delimiter */
                for (plen = 0 ; rem != 0 && *(p-1) != ';' ;
                     --p, --rem, ++plen) ;

                /* look up this name */
                if (plen == 0)
                {
                    /* 
                     *   empty name - this slot accepts any type; represent
                     *   this in the run-time formal list with 'nil' 
                     */
                    G_cg->write_op(OPC_PUSHNIL);

                    /* 
                     *   An untyped slot is implicitly an Object slot, so we
                     *   need to make sure that Object can participate in the
                     *   binding property system by ensuring that it has a
                     *   modifier object. 
                     */
                    G_cg->linker_ensure_mod_obj("Object", 6);
                }
                else if (plen == 3 && memcmp(p, "...", 3) == 0)
                {
                    /* 
                     *   varargs indicator - represent this in the list with
                     *   the literal string '...' 
                     */
                    CTcConstVal val;
                    val.set_sstr("...", 3);
                    CTPNConst cval(&val);
                    cval.gen_code(FALSE, FALSE);

                    /* 
                     *   a varargs slot is implicitly an Object slot, so make
                     *   sure Object has a modifier object
                     */
                    G_cg->linker_ensure_mod_obj("Object", 6);
                }
                else
                {
                    /* class name - look it up */
                    CTcSymbol *cl = G_prs->get_global_symtab()->find(p, plen);
                    CTcConstVal val;

                    /* 
                     *   if it's missing, unresolved, or not an object, flag
                     *   an error 
                     */
                    if (cl == 0
                        || (cl->get_type() == TC_SYM_OBJ
                            && ((CTcSymObj *)cl)->is_extern()))
                    {
                        G_tcmain->log_error(
                            0, 0, TC_SEV_ERROR, TCERR_UNDEF_SYM,
                            (int)plen, p);
                        return;
                    }
                    else if (cl->get_type() == TC_SYM_OBJ)
                    {
                        /* get the object information */
                        CTcSymObj *co = (CTcSymObj *)cl;
                        val.set_obj(co->get_obj_id(), co->get_metaclass());
                    }
                    else if (cl->get_type() == TC_SYM_METACLASS)
                    {
                        /* get the metaclass information */
                        CTcSymMetaclass *cm = (CTcSymMetaclass *)cl;
                        val.set_obj(cm->get_class_obj(), TC_META_UNKNOWN);

                        /*
                         *   If this metaclass doesn't have a modifier
                         *   object, create one for it.  This is needed
                         *   because the run-time library's multi-method
                         *   implementation stores the method binding
                         *   information in properties of the parameter
                         *   objects.  Since we're using this metaclass as a
                         *   parameter type, we'll need to write at least one
                         *   property to it.  We can only write properties to
                         *   intrinsic class objects when they're equipped
                         *   with modifier objects.
                         *   
                         *   The presence of a modifier object has no effect
                         *   at all on performance for ordinary method call
                         *   operations on the intrinsic class, and the
                         *   modifier itself is just a bare object, so the
                         *   cost of creating this extra object is trivial.  
                         */
                        G_cg->linker_ensure_mod_obj(cm);
                    }
                    else
                    {
                        /* it's not a valid object type */
                        G_tcmain->log_error(
                            0, 0, TC_SEV_ERROR, TCERR_MMPARAM_NOT_OBJECT,
                            (int)plen, p, (int)funclen, funcname);
                        return;
                    }

                    /* 
                     *   represent the object or class in the parameter list
                     *   with the object reference
                     */
                    CTPNConst cval(&val);
                    cval.gen_code(FALSE, FALSE);
                }

                /* note the value we pushed */
                G_cg->note_push();
            }

            /* build and push the list from the parameters */
            if (argc <= 255)
            {
                G_cg->write_op(OPC_NEW1);
                G_cs->write((char)argc);
            }
            else
            {
                G_cg->write_op(OPC_NEW2);
                G_cs->write2(argc);
            }
            G_cs->write((char)G_cg->get_predef_meta_idx(TCT3_METAID_LIST));
            G_cg->write_op(OPC_GETR0);
            G_cg->note_pop(argc);
            G_cg->note_push();

            /* push the function pointer argument */
            fsym->gen_code(FALSE);

            /* push the base function pointer argument */
            base_sym->gen_code(FALSE);

            /* 
             *   call _multiMethodRegister, if it's available (if it's not,
             *   our caller will flag this as an error, so just skip the code
             *   generation here) 
             */
            if (ctx->mmr != 0)
            {
                G_cg->write_op(OPC_CALL);
                G_cs->write(3);                           /* argument count */
                ctx->mmr->add_abs_fixup(G_cs);    /* function address fixup */
                G_cs->write4(0);                       /* fixup placeholder */
            }

            /* the 3 arguments will be gone on return */
            G_cg->note_pop(3);

            /* count the registration */
            ctx->cnt += 1;
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Build the IntrinsicClass objects 
 */
void CTcGenTarg::build_intrinsic_class_objs(CTcDataStream *str)
{
    tc_meta_entry *meta;
    uint idx;
    
    /* 
     *   run through the dependency table, and create an IntrinsicClass
     *   object for each entry 
     */
    for (idx = 0, meta = meta_head_ ; meta != 0 ; meta = meta->nxt, ++idx)
    {
        /* 
         *   if we have a symbol for this class, add the object to the
         *   intrinsic class stream 
         */
        if (meta->sym != 0)
        {
            /* write the OBJS header */
            str->write4(meta->sym->get_class_obj());
            str->write2(8);

            /* 
             *   write the data - the data length (8), followed by the
             *   intrinsic class index that this object is associated
             *   with, followed by the modifier object
             */
            str->write2(8);
            str->write2(idx);
            str->write4(meta->sym->get_mod_obj() == 0
                        ? VM_INVALID_OBJ
                        : meta->sym->get_mod_obj()->get_obj_id());

            /* 
             *   fix up the inheritance chain in the modifier objects, if
             *   necessary 
             */
            meta->sym->fix_mod_obj_sc_list();
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   symbol table entry for a local variable debug entry 
 */
class dbglcl: public CVmHashEntryCS
{
public:
    dbglcl(const char *str, size_t len, CTcStreamAnchor *anchor, ulong ofs)
        : CVmHashEntryCS(str, len, TRUE)
    {
        this->anchor = anchor;
        this->ofs = ofs;
    }

    CTcStreamAnchor *anchor;
    ulong ofs;
};

/*
 *   Build the local symbol records.
 */
void CTcGenTarg::build_local_symbol_records(CTcCodeStream *cs,
                                            CVmHashTable *tab)
{
    CTcStreamAnchor *anchor;

    /* this only applies for debug format 2+ */
    if (G_sizes.dbg_fmt_vsn < 2)
        return;

    /* go through the list of anchors in the code stream */
    for (anchor = cs->get_first_anchor() ; anchor != 0 ;
         anchor = anchor->nxt_)
    {
        ulong start_ofs;
        ulong dbg_ofs;
        uint cnt;
        ulong ofs;

        /* get the anchor's stream offset */
        start_ofs = anchor->get_ofs();

        /* read the debug table offset from the method header */
        dbg_ofs = start_ofs + cs->readu2_at(start_ofs + 8);

        /* if there's no debug table for this method, go on to the next */
        if (dbg_ofs == start_ofs)
            continue;

        /* read the number of line entries */
        ofs = dbg_ofs + G_sizes.dbg_hdr;
        cnt = cs->readu2_at(ofs);
        ofs += 2;

        /* skip past the line entries */
        ofs += cnt * G_sizes.dbg_line;

        /* skip the end offset */
        ofs += 2;

        /* read the frame count */
        uint frame_cnt = cs->readu2_at(ofs);
        ofs += 2;

        /* skip the frame index */
        ofs += 2 * frame_cnt;

        /* go through the individual frames */
        for (uint fi = 0 ; fi < frame_cnt ; ++fi)
        {
            /* get the number of entries */
            cnt = cs->readu2_at(ofs + 2);

            /* skip to the first entry */
            ofs += G_sizes.dbg_frame;

            /* parse each entry */
            for (uint i = 0 ; i < cnt ; ++i)
            {
                /* read the flags */
                uint flags = cs->readu2_at(ofs + 2);
                ofs += G_sizes.lcl_hdr;

                /* 
                 *   If this is an out-of-line entry, we currently have a
                 *   pointer to the local variable stream, and we need to
                 *   move it to the constant pool instead.  If it's in-line,
                 *   there's nothing to do - we just skip the record. 
                 */
                if (flags & 0x0004)
                {
                    /* 
                     *   It's a constant pool entry.  Read the name from the
                     *   local variable pool.  
                     */
                    ulong lofs = cs->readu4_at(ofs);
                    size_t nlen = G_lcl_stream->readu2_at(lofs);
                    if (nlen <= TOK_SYM_MAX_LEN)
                    {
                        /* read the name */
                        char nbuf[TOK_SYM_MAX_LEN + 1];
                        G_lcl_stream->copy_to_buf(nbuf, lofs + 2, nlen);

                        /* 
                         *   Look up the name in our symbol table, to see if
                         *   we've already defined it.  If so, re-use the
                         *   same name.  A few local variable names tend to
                         *   be used over and over, so it saves a lot of
                         *   space to share one copy for each instance of a
                         *   reused name. 
                         */
                        dbglcl *l = (dbglcl *)tab->find(nbuf, nlen);
                        if (l == 0)
                        {
                            /* add an anchor for the constant pool entry */
                            CTcStreamAnchor *anchor = G_ds->add_anchor(0, 0);

                            /* 
                             *   It's not already defined.  Add a new symbol
                             *   table entry for it.  The entry will go at
                             *   the current constant pool stream offset.  
                             */
                            l = new dbglcl(nbuf, nlen, anchor,
                                           G_ds->get_ofs());
                            tab->add(l);

                            /* copy the name to the constant pool stream */
                            G_ds->write2(nlen);
                            G_ds->write(nbuf, nlen);
                        }

                        /* add a fixup for this pointer in the code stream */
                        CTcAbsFixup::add_abs_fixup(
                            l->anchor->fixup_list_head_, cs, ofs);

                        /* 
                         *   Overwrite the local stream offset with the
                         *   constant pool offset of the symbol. 
                         */
                        cs->write4_at(ofs, l->ofs);
                    }

                    /* skip the record */
                    ofs += 4;
                }
                else
                {
                    /* it's in-line - just skip to the next record */
                    ofs += cs->readu2_at(ofs) + 2;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Build the source file line maps.  These maps provide listings from
 *   the source location to the executable location, so the debugger can
 *   do things such as set a breakpoint at a given source file location.  
 */
void CTcGenTarg::build_source_line_maps()
{
    CTcStreamAnchor *anchor;

    /* go through the list of anchors in the code stream */
    for (anchor = G_cs->get_first_anchor() ; anchor != 0 ;
         anchor = anchor->nxt_)
    {
        ulong start_ofs;
        ulong start_addr;
        ulong dbg_ofs;
        uint cnt;
        ulong ofs;

        /* get the anchor's stream offset */
        start_ofs = anchor->get_ofs();

        /* get the anchor's absolute address in the image file */
        start_addr = anchor->get_addr();

        /* read the debug table offset from the method header */
        dbg_ofs = start_ofs + G_cs->readu2_at(start_ofs + 8);

        /* if there's no debug table for this method, go on to the next */
        if (dbg_ofs == start_ofs)
            continue;

        /* read the number of line entries */
        cnt = G_cs->readu2_at(dbg_ofs + G_sizes.dbg_hdr);

        /* go through the individual line entries */
        for (ofs = dbg_ofs + G_sizes.dbg_hdr + 2 ; cnt != 0 ;
             --cnt, ofs += G_sizes.dbg_line)
        {
            uint file_id;
            ulong linenum;
            uint method_ofs;
            ulong line_addr;
            CTcTokFileDesc *file_desc;
            
            /* 
             *   get the file position, and the byte-code offset from the
             *   start of the method of the executable code for the line 
             */
            method_ofs = G_cs->readu2_at(ofs);
            file_id = G_cs->readu2_at(ofs + 2);
            linenum = G_cs->readu4_at(ofs + 4);

            /* calculate the absolute address of the line in the image file */
            line_addr = start_addr + method_ofs;

            /* find the given file descriptor */
            file_desc = G_tok->get_filedesc(file_id);

            /* 
             *   get the original file descriptor, since we always want to
             *   add to the original, not to the duplicates, if the file
             *   appears more than once (because this is a one-way mapping
             *   from file to byte-code location - we thus require a
             *   single index)
             */
            if (file_desc->get_orig() != 0)
                file_desc = file_desc->get_orig();

            /* add this line to the file descriptor */
            file_desc->add_source_line(linenum, line_addr);
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Callback to write enumerated source lines to an image file 
 */
static void write_source_lines_cb(void *ctx, ulong linenum, ulong code_addr)
{
    CVmImageWriter *image_writer;

    /* get the image writer */
    image_writer = (CVmImageWriter *)ctx;

    /* write the data */
    image_writer->write_srcf_line_entry(linenum, code_addr);
}

/*
 *   Write the list of source file descriptors to an image file 
 */
void CTcGenTarg::write_sources_to_image(CVmImageWriter *image_writer)
{
    CTcTokFileDesc *desc;

    /* write the block prefix */
    image_writer->begin_srcf_block(G_tok->get_filedesc_count());

    /* write the entries */
    for (desc = G_tok->get_first_filedesc() ; desc != 0 ;
         desc = desc->get_next())
    {
        const char *fname;

        /* 
         *   Get the filename.  Use the fully resolved local filename, so
         *   that the debugger can correlate the resolved file back to the
         *   project configuration.  This ties the debug records to the local
         *   directory structure, but the only drawback of this is that the
         *   program must be recompiled wherever it's to be used with the
         *   debugger.  
         */
        fname = desc->get_fname();

        /* 
         *   if we're in test reporting mode, write only the root name, not
         *   the full name - this insulates test logs from the details of
         *   local pathname conventions and the local directory structure,
         *   allowing for more portable test logs 
         */
        if (G_tcmain->get_test_report_mode())
            fname = os_get_root_name((char *)fname);
        
        /* begin this entry */
        image_writer->begin_srcf_entry(desc->get_orig_index(), fname);

        /* write the source lines */
        desc->enum_source_lines(write_source_lines_cb, image_writer);

        /* end this entry */
        image_writer->end_srcf_entry();
    }

    /* end the block */
    image_writer->end_srcf_block();
}

/*
 *   Write the method header list to the image file 
 */
void CTcGenTarg::write_method_list_to_image(CVmImageWriter *image_writer)
{
    CTcStreamAnchor *anchor;

    /* begin the method header list block in the image file */
    image_writer->begin_mhls_block();

    /* go through the list of anchors in the code stream */
    for (anchor = G_cs->get_first_anchor() ; anchor != 0 ;
         anchor = anchor->nxt_)
    {
        /* write this entry's code pool address */
        image_writer->write_mhls_entry(anchor->get_addr());
    }

    /* end the block */
    image_writer->end_mhls_block();
}

/*
 *   Write the preprocessor macros to the image file, for debugger use 
 */
void CTcGenTarg::write_macros_to_image(CVmImageWriter *image_writer)
{
    /* begin the macro block */
    image_writer->begin_macr_block();

    /* 
     *   ask the tokenizer to dump the data directly to the file underlying
     *   the image writer 
     */
    G_tok->write_macros_to_file_for_debug(image_writer->get_fp());

    /* end the macro block */
    image_writer->end_macr_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   Callback context for global symbol table writer 
 */
struct write_sym_to_image_cb
{
    /* number of symbols written */
    ulong count;
    
    /* the image writer */
    CVmImageWriter *image_writer;
};

/*
 *   Callback for writing the global symbol table to an object file 
 */
static void write_sym_to_image(void *ctx0, CTcSymbol *sym)
{
    write_sym_to_image_cb *ctx;

    /* cast the context */
    ctx = (write_sym_to_image_cb *)ctx0;

    /* 
     *   If the symbol's name starts with a period, don't write it - the
     *   compiler constructs certain private symbol names for its own
     *   internal use, and marks them as such by starting the name with a
     *   period.  These symbols cannot be used to evaluate expressions, so
     *   they're of no use in teh global symbol table in the image file. 
     */
    if (sym->get_sym()[0] == '.')
        return;

    /* ask the symbol to do the work */
    if (sym->write_to_image_file_global(ctx->image_writer))
    {
        /* we wrote the symbol - count it */
        ++(ctx->count);
    }
}

/*
 *   Write the global symbol table to an object file 
 */
void CTcGenTarg::write_global_symbols_to_image(CVmImageWriter *image_writer)
{
    write_sym_to_image_cb ctx;

    /* set up the callback context */
    ctx.count = 0;
    ctx.image_writer = image_writer;

    /* start the block */
    image_writer->begin_gsym_block();
    
    /* ask the symbol table to enumerate itself through our symbol writer */
    G_prs->get_global_symtab()->enum_entries(&write_sym_to_image, &ctx);

    /* end the block */
    image_writer->end_gsym_block(ctx.count);
}

/* ------------------------------------------------------------------------ */
/*
 *   Look up a property 
 */
vm_prop_id_t CTcGenTarg::look_up_prop(const char *propname, int required,
                                      int err_if_undef, int err_if_not_prop)
{
    /* look up the symbol */
    CTcSymbol *sym = G_prs->get_global_symtab()->find(propname);

    /* check to see if it's defined and of the proper type */
    if (sym == 0)
    {
        /* log the 'undefined' error */
        G_tcmain->log_error(0, 0, required ? TC_SEV_ERROR : TC_SEV_PEDANTIC,
                            err_if_undef);
    }
    else if (sym->get_type() != TC_SYM_PROP)
    {
        /* log the 'not a property' error */
        G_tcmain->log_error(0, 0, required ? TC_SEV_ERROR : TC_SEV_PEDANTIC,
                            err_if_not_prop);
    }
    else
    {
        /* return the property ID */
        return ((CTcSymProp *)sym)->get_prop();
    }

    /* if we got here, we didn't find a valid property */
    return VM_INVALID_PROP;
}


/* ------------------------------------------------------------------------ */
/*
 *   Write a TADS object stream to the image file.  We'll write blocks of
 *   size up to somewhat less than 64k, to ensure that the file is usable on
 *   16-bit machines.  
 */
void CTcGenTarg::write_tads_objects_to_image(CTcDataStream *os,
                                             CVmImageWriter *image_writer,
                                             int meta_idx)
{
    /* write the persistent (non-transient) objects */
    write_tads_objects_to_image(os, image_writer, meta_idx, FALSE);

    /* write the transient objects */
    write_tads_objects_to_image(os, image_writer, meta_idx, TRUE);
}

/*
 *   Write the TADS object stream to the image file, writing only persistent
 *   or transient objects. 
 */
void CTcGenTarg::write_tads_objects_to_image(CTcDataStream *os,
                                             CVmImageWriter *image_writer,
                                             int meta_idx, int trans)
{    
    ulong start_ofs;
    
    /* keep going until we've written the whole file */
    for (start_ofs = 0 ; start_ofs < os->get_ofs() ; )
    {
        ulong ofs;
        uint siz;
        uint cnt;
        uint block_size;

        /* 
         *   Scan the stream.  Each entry in the stream is a standard
         *   object record, which means that it starts with the object ID
         *   (UINT4) and the length (UINT2) of the metaclass-specific
         *   data, which is then followed by the metaclass data.  Skip as
         *   many objects as we can while staying within our approximately
         *   64k limit.  
         */
        for (block_size = 0, ofs = start_ofs, cnt = 0 ; ; )
        {
            uint flags;
            ulong rem_len;
            size_t orig_prop_cnt;
            size_t write_prop_cnt;
            size_t write_size;
            ulong next_ofs;
            ulong orig_ofs;

            /* if we've reached the end of the stream, we're done */
            if (ofs >= os->get_ofs())
                break;

            /* remember the starting offset */
            orig_ofs = ofs;

            /* read our internal flags */
            flags = os->readu2_at(ofs + TCT3_OBJ_INTERNHDR_FLAGS_OFS);

            /* 
             *   get the size of this block - this is the
             *   metaclass-specific data size at offset 4 in the T3
             *   metaclass header, plus the size of the T3 metaclass
             *   header, plus the size of our internal header 
             */
            siz = TCT3_OBJ_INTERNHDR_SIZE
                  + TCT3_META_HEADER_SIZE
                  + os->readu2_at(ofs + TCT3_META_HEADER_OFS + 4);

            /* 
             *   Calculate the offset of the next block.  Note that this is
             *   the current offset plus the original block size; the amount
             *   of data we end up writing might be less than the original
             *   block size because we might have deleted property slots
             *   when we sorted and compressed the property table.  
             */
            next_ofs = ofs + siz;

            /* if this object was deleted, skip it */
            if ((flags & TCT3_OBJ_REPLACED) != 0)
            {
                ofs = next_ofs;
                continue;
            }

            /* 
             *   if this object is of the wrong persistent/transient type,
             *   skip it 
             */
            if (((flags & TCT3_OBJ_TRANSIENT) != 0) != (trans != 0))
            {
                ofs = next_ofs;
                continue;
            }
            
            /* 
             *   if this would push us over the limit, stop here and start a
             *   new block 
             */
            if (block_size + siz > 64000L)
                break;
                
            /*
             *   We must sort the property table, in order of ascending
             *   property ID, before we write the image file.  We had to
             *   wait until now to do this, because the final property ID
             *   assignments aren't made until link time.
             */
            write_prop_cnt = sort_object_prop_table(os, ofs);
            
            /* note the original property count */
            orig_prop_cnt = CTPNStmObject::get_stream_prop_cnt(os, ofs);
            
            /* 
             *   Then temporarily pdate the property count in the stream, in
             *   case we changed it in the sorting process.
             *   
             *   Calculate the new size of the data to write.  Note that we
             *   must add in the size of the T3 metaclass header, since this
             *   isn't reflected in the data size.  
             */
            write_size =
                CTPNStmObject::set_stream_prop_cnt(os, ofs, write_prop_cnt)
                + TCT3_META_HEADER_SIZE;

            /* 
             *   if this is the first object in this block, write the
             *   block header 
             */
            if (cnt == 0)
                image_writer->begin_objs_block(meta_idx, FALSE, trans);

            /* 
             *   skip past our internal header - we don't want to write
             *   our internal header to the image file, since this was
             *   purely for our own use in the compiler and linker 
             */
            ofs += TCT3_OBJ_INTERNHDR_SIZE;

            /* 
             *   write the object data; write the size returned from
             *   sorting the property table, which might be different than
             *   the original block data size in the stream, because we
             *   might have compressed the property table 
             */
            for (rem_len = write_size ; rem_len != 0 ; )
            {
                const char *p;
                ulong avail_len;
                
                /* get the next block */
                p = os->get_block_ptr(ofs, rem_len, &avail_len);
                
                /* write it out */
                image_writer->write_objs_bytes(p, avail_len);
                
                /* move past this block */
                ofs += avail_len;
                rem_len -= avail_len;
            }
                
            /* count the object */
            ++cnt;

            /* restore the original stream property count */
            CTPNStmObject::set_stream_prop_cnt(os, orig_ofs, orig_prop_cnt);

            /* move on to the next block */
            ofs = next_ofs;
        }

        /* if we wrote any objects, end the block */
        if (cnt != 0)
            image_writer->end_objs_block(cnt);

        /* move on to the next block */
        start_ofs = ofs;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Write an object stream of non-TADS objects to the image file 
 */
void CTcGenTarg::write_nontads_objs_to_image(CTcDataStream *os,
                                             CVmImageWriter *image_writer,
                                             int meta_idx, int large_objs)
{
    /* keep going until we've written the whole file */
    for (ulong start_ofs = 0 ; start_ofs < os->get_ofs() ; )
    {
        ulong ofs;
        uint siz;
        uint cnt;
        uint block_size;

        /* 
         *   Scan the stream.  Each entry in the stream is either a small or
         *   large object record, which means that it starts with the object
         *   ID (UINT4) and the length (UINT2 for small, UINT4 for large) of
         *   the metaclass-specific data, which is then followed by the
         *   metaclass data.
         *   
         *   Include as many objects as we can while staying within our
         *   approximately 64k limit, if this is a small-format block; fill
         *   the block without limit if this is a large-format block.  
         */
        for (block_size = 0, ofs = start_ofs, cnt = 0 ; ; )
        {
            ulong rem_len;
            ulong next_ofs;

            /* if we've reached the end of the stream, we're done */
            if (ofs >= os->get_ofs())
                break;

            /* 
             *   get the size of this block - this is the
             *   metaclass-specific data size at offset 4 in the T3
             *   metaclass header, plus the size of the T3 metaclass
             *   header 
             */
            if (large_objs)
            {
                /* 
                 *   Get the 32-bit size value.  Note that we don't worry
                 *   about limiting the overall block size to 64k when we're
                 *   writing a "large" object block.  
                 */
                siz = (ulong)os->readu4_at(ofs + 4)
                      + TCT3_LARGE_META_HEADER_SIZE;
            }
            else
            {
                /* get the 16-bit size value */
                siz = (ulong)os->read2_at(ofs + 4)
                      + TCT3_META_HEADER_SIZE;

                /* 
                 *   Since this is a small-object block, limit the aggregate
                 *   size of the entire block to 64k.  So, if this block
                 *   would push us over the 64k aggregate for the block,
                 *   start a new OBJS block with this object.  
                 */
                if (cnt != 0 && block_size + siz > 64000L)
                    break;
            }

            /* 
             *   if this is the first object in this block, write the
             *   block header - the dictionary uses large object headers,
             *   so note that 
             */
            if (cnt == 0)
                image_writer->begin_objs_block(meta_idx, large_objs, FALSE);

            /* calculate the offset of the next block */
            next_ofs = ofs + siz;

            /* write the object data */
            for (rem_len = siz ; rem_len != 0 ; )
            {
                const char *p;
                ulong avail_len;

                /* get the next block */
                p = os->get_block_ptr(ofs, rem_len, &avail_len);

                /* write it out */
                image_writer->write_objs_bytes(p, avail_len);
                
                /* move past this block */
                ofs += avail_len;
                rem_len -= avail_len;
            }
                
            /* count the object */
            ++cnt;

            /* move on to the next block */
            ofs = next_ofs;
        }

        /* if we wrote any objects, end the block */
        if (cnt != 0)
            image_writer->end_objs_block(cnt);

        /* move on to the next block */
        start_ofs = ofs;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Property comparison callback function for qsort() when invoked from
 *   sort_object_prop_table() 
 */
//extern "C" int prop_compare(const void *p1, const void *p2);
extern "C" {
    static int prop_compare(const void *p1, const void *p2)
    {
        uint id1, id2;

        /* get the ID's */
        id1 = osrp2(p1);
        id2 = osrp2(p2);

        /* compare them and return the result */
        return (id1 < id2 ? -1 : id1 == id2 ? 0 : 1);
    }
}

/*
 *   Sort an object's property table.  This puts the property table into
 *   order of ascending property ID, and deletes any unused properties from
 *   the table.
 *   
 *   Note that we do NOT update the stream to indicate the reduced number of
 *   properties if we delete any properties.  Instead, we simply return the
 *   new number of properties.  
 */
size_t CTcGenTarg::sort_object_prop_table(CTcDataStream *os, ulong start_ofs)
{
    /* read the number of properties from the header */
    uint prop_cnt = CTPNStmObject::get_stream_prop_cnt(os, start_ofs);

    /* calculate the property table size */
    uint prop_table_size = prop_cnt * TCT3_TADSOBJ_PROP_SIZE;

    /* get the offset of the first property */
    ulong prop_ofs = CTPNStmObject::get_stream_first_prop_ofs(os, start_ofs);

    /* reallocate the sort buffer if necessary */
    if (prop_table_size > sort_buf_size_)
    {
        /* increase the sort buffer size to the next 4k increment */
        sort_buf_size_ = (prop_table_size + 4095) & ~4096;

        /* reallocate the buffer */
        sort_buf_ = (char *)t3realloc(sort_buf_, sort_buf_size_);
        if (sort_buf_ == 0 || sort_buf_size_ < prop_table_size)
            G_tok->throw_internal_error(TCERR_CODEGEN_NO_MEM);
    }

    /* extract the table into our buffer */
    os->copy_to_buf(sort_buf_, prop_ofs, prop_table_size);

    /* 
     *   Compress the table by removing any properties that have been
     *   marked as deleted -- if we had any 'modify + replace' properties
     *   that we resolved at link time, we will have marked those
     *   properties for deletion by setting their property ID's to zero in
     *   the table.  Scan the table for any such properties and remove
     *   them now.  
     */
    size_t src, dst;
    for (src = dst = 0, prop_cnt = 0 ; src < prop_table_size ;
         src += TCT3_TADSOBJ_PROP_SIZE)
    {
        /* if this property isn't marked for deletion, keep it */
        if (osrp2(sort_buf_ + src) != VM_INVALID_PROP)
        {
            /* 
             *   we're keeping it - if we can move it to a lower table
             *   position, copy the data to the new position, otherwise
             *   leave it alone 
             */
            if (src != dst)
                memcpy(sort_buf_ + dst, sort_buf_ + src,
                       TCT3_TADSOBJ_PROP_SIZE);

            /* 
             *   advance the destination pointer past this slot, since
             *   we're going to keep the data in the slot 
             */
            dst += TCT3_TADSOBJ_PROP_SIZE;

            /* count this property, since we're keeping it */
            ++prop_cnt;
        }
    }

    /* sort the table */
    qsort(sort_buf_, prop_cnt, TCT3_TADSOBJ_PROP_SIZE, &prop_compare);

    /* add back any unused slots after all of the sorted slots */
    for ( ; dst < prop_table_size ; dst += TCT3_TADSOBJ_PROP_SIZE)
        oswp2(sort_buf_ + dst, VM_INVALID_PROP);

    /* put the sorted table back in the buffer */
    os->write_at(prop_ofs, sort_buf_, prop_table_size);

    /* return the (possibly reduced) number of properties */
    return prop_cnt;
}


/*
 *   callback context for enumerating a dictionary 
 */
struct enum_dict_ctx
{
    /* number of entries written so far */
    uint cnt;
};

/*
 *   Generate code for a dictionary object
 */
void CTcGenTarg::gen_code_for_dict(CTcDictEntry *dict)
{
    long size_ofs;
    long entry_cnt_ofs;
    long end_ofs;
    enum_dict_ctx ctx;

    /* 
     *   Write the OBJS header - object ID plus byte count for
     *   metaclass-specific data (use a placeholder length for now) 
     */
    G_dict_stream->write4(dict->get_sym()->get_obj_id());
    size_ofs = G_dict_stream->get_ofs();
    G_dict_stream->write4(0);

    /*
     *   Write the metaclass-specific data for the 'dictionary' metaclass 
     */

    /* write a nil comparator object initially */
    G_dict_stream->write4(0);

    /* write a placeholder for the entry count */
    entry_cnt_ofs = G_dict_stream->get_ofs();
    G_dict_stream->write2(0);

    /* write the dictionary entries */
    ctx.cnt = 0;
    dict->get_hash_table()->enum_entries(&enum_dict_gen_cb, &ctx);

    /* remember the ending offset of the table */
    end_ofs = G_dict_stream->get_ofs();

    /* go back and fix up the total size of the object data */
    G_dict_stream->write4_at(size_ofs, end_ofs - size_ofs - 4);

    /* fix up the dictionary entry count */
    G_dict_stream->write2_at(entry_cnt_ofs, ctx.cnt);
}

/*
 *   Callback - enumerate dictionary entries for code generation 
 */
void CTcGenTarg::enum_dict_gen_cb(void *ctx0, CVmHashEntry *entry0)
{
    enum_dict_ctx *ctx = (enum_dict_ctx *)ctx0;
    CVmHashEntryPrsDict *entry = (CVmHashEntryPrsDict *)entry0;
    char buf[255];
    size_t len;
    char *p;
    size_t rem;
    uint cnt;
    CTcPrsDictItem *item;

    /* count this entry */
    ++(ctx->cnt);

    /* limit the key length to 255 bytes */
    len = entry->getlen();
    if (len > 255)
        len = 255;

    /* copy the entry to our buffer */
    memcpy(buf, entry->getstr(), len);

    /* apply the XOR obfuscation to the key text */
    for (p = buf, rem = len ; rem != 0 ; ++p, --rem)
        *p ^= 0xBD;

    /* write the length of the key followed by the key string */
    G_dict_stream->write((uchar)len);
    G_dict_stream->write(buf, len);

    /* count the items in this entry */
    for (cnt = 0, item = entry->get_list() ; item != 0 ;
         ++cnt, item = item->nxt_) ;

    /* write the number of entries */
    G_dict_stream->write2(cnt);

    /* write the entries */
    for (item = entry->get_list() ; item != 0 ; item = item->nxt_)
    {
        /* write the object ID and property ID of this entry */
        G_dict_stream->write4(item->obj_);
        G_dict_stream->write2(item->prop_);
    }
}

/*
 *   Generate code for a grammar production 
 */
void CTcGenTarg::gen_code_for_gramprod(CTcGramProdEntry *prod)
{
    long size_ofs;
    long end_ofs;
    uint cnt;
    CTcGramProdAlt *alt;
    CTcDataStream *str = G_gramprod_stream;
    
    /* 
     *   write the OBJS header - object ID plus byte count for
     *   metaclass-specific data (use a placeholder length for now) 
     */
    str->write4(prod->get_prod_sym()->get_obj_id());
    size_ofs = str->get_ofs();
    str->write4(0);

    /*
     *   Write the metaclass-specific data for the 'grammar-production'
     *   metaclass 
     */

    /* count the alternatives */
    for (cnt = 0, alt = prod->get_alt_head() ; alt != 0 ;
         ++cnt, alt = alt->get_next()) ;

    /* 
     *   If this production has no alternatives and was not explicitly
     *   declared, flag an error indicating that the production is
     *   undeclared.  We treat this as an error because there's a good chance
     *   that the an alternative referring to the production misspelled the
     *   name.  If the production was explicitly declared, then we have
     *   sufficient confirmation that the name is correct, so no error is
     *   indicated.  
     */
    if (cnt == 0 && !prod->is_declared())
        G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                            TCERR_GRAMPROD_HAS_NO_ALTS,
                            (int)prod->get_prod_sym()->get_sym_len(),
                            prod->get_prod_sym()->get_sym());

    /* 
     *   The count has to fit in 16 bits; it's surprisingly easy to exceed
     *   this by using the power of permutation (with nested '|'
     *   alternatives), so check for overflow and flag an error.  Even though
     *   it's not hard to exceed the limit, it's not desirable to create so
     *   many permutations, so the limit isn't really in need of being
     *   raised; it's better to rewrite a rule with a huge number of
     *   permutations using sub-productions.  
     */
    if (cnt > 65535)
        G_tcmain->log_error(0, 0, TC_SEV_ERROR,
                            TCERR_GRAMPROD_TOO_MANY_ALTS,
                            (int)prod->get_prod_sym()->get_sym_len(),
                            prod->get_prod_sym()->get_sym());

    /* write the number of alternatives */
    str->write2(cnt);

    /* write the alternatives */
    for (alt = prod->get_alt_head() ; alt != 0 ; alt = alt->get_next())
    {
        CTcGramProdTok *tok;

        /* write the score and badness for the alternative */
        str->write2(alt->get_score());
        str->write2(alt->get_badness());
        
        /* write the processor object ID for this alternative */
        str->write4(alt->get_processor_obj()->get_obj_id());

        /* count the tokens in this alternative */
        for (cnt = 0, tok = alt->get_tok_head() ; tok != 0 ;
             ++cnt, tok = tok->get_next()) ;

        /* write the token count */
        str->write2(cnt);

        /* write the tokens */
        for (tok = alt->get_tok_head() ; tok != 0 ; tok = tok->get_next())
        {
            size_t idx;

            /* write the property association */
            str->write2((uint)tok->get_prop_assoc());
            
            /* write the token data */
            switch(tok->get_type())
            {
            case TCGRAM_PROD:
                /* write the type */
                str->write((uchar)VMGRAM_MATCH_PROD);

                /* write the sub-production object ID */
                str->write4((ulong)tok->getval_prod()->get_obj_id());
                break;

            case TCGRAM_PART_OF_SPEECH:
                /* write the type */
                str->write((uchar)VMGRAM_MATCH_SPEECH);

                /* write the part-of-speech property */
                str->write2((uint)tok->getval_part_of_speech());
                break;

            case TCGRAM_PART_OF_SPEECH_LIST:
                /* write the type */
                str->write((uchar)VMGRAM_MATCH_NSPEECH);

                /* write the number of elements in the property list */
                str->write2((uint)tok->getval_part_list_len());

                /* write each element */
                for (idx = 0 ; idx < tok->getval_part_list_len() ; ++idx)
                    str->write2((uint)tok->getval_part_list_ele(idx));

                /* done */
                break;

            case TCGRAM_LITERAL:
                /* write the type */
                str->write((uchar)VMGRAM_MATCH_LITERAL);

                /* write the string length prefix */
                str->write2(tok->getval_literal_len());

                /* write the string text */
                str->write(tok->getval_literal_txt(),
                           tok->getval_literal_len());

                /* 
                 *   add the word to the dictionary that was active when the
                 *   alternative was defined 
                 */
                if (alt->get_dict() != 0)
                {
                    /* 
                     *   there's a dictionary - add the word, associating it
                     *   with the production object and with the parser's
                     *   miscVocab property 
                     */
                    alt->get_dict()->add_word(
                        tok->getval_literal_txt(), tok->getval_literal_len(),
                        FALSE, prod->get_prod_sym()->get_obj_id(),
                        G_prs->get_miscvocab_prop());
                }
                break;

            case TCGRAM_TOKEN_TYPE:
                /* write the type */
                str->write((uchar)VMGRAM_MATCH_TOKTYPE);

                /* write the enum ID of the token */
                str->write4(tok->getval_token_type());
                break;

            case TCGRAM_STAR:
                /* write the type - there's no additional data */
                str->write((uchar)VMGRAM_MATCH_STAR);
                break;

            default:
                assert(FALSE);
                break;
            }
        }
    }

    /* remember the ending offset of the object data */
    end_ofs = str->get_ofs();

    /* go back and fix up the total size of the object data */
    str->write4_at(size_ofs, end_ofs - size_ofs - 4);
}


/* ------------------------------------------------------------------------ */
/*
 *   Data Stream Layout Manager 
 */

/*
 *   calculate the size of the pool pages, given the size of the largest
 *   single item 
 */
void CTcStreamLayout::calc_layout(CTcDataStream *ds, ulong max_len,
                                  int is_first)
{
    ulong rem;
    ulong free_ofs;
    CTcStreamAnchor *anchor;

    /* if this is the first page, handle some things specially */
    if (is_first)
    {
        ulong pgsiz;

        /* 
         *   Starting at 2k, look for a page size that will fit the
         *   desired minimum size.  
         */
        for (pgsiz = 2048 ; pgsiz < max_len ; pgsiz <<= 1) ;

        /* remember our selected page size */
        page_size_ = pgsiz;

        /* start at the bottom of the first page */
        rem = pgsiz;
        free_ofs = 0;
        page_cnt_ = 1;
    }
    else
    {
        /* 
         *   this isn't the first page - if there are no anchors, don't
         *   bother adding anything 
         */
        if (ds->get_first_anchor() == 0)
            return;

        /* 
         *   start at the end of the last existing page - this will ensure
         *   that everything added from the new stream will go onto a
         *   brand new page after everything from the previous stream 
         */
        rem = 0;
        free_ofs = page_size_ * page_cnt_;
    }
    
    /*
     *   Run through the list of stream anchors and calculate the layout.
     *   For each item, assign its final pool address and apply its
     *   fixups.  
     */
    for (anchor = ds->get_first_anchor() ; anchor != 0 ;
         anchor = anchor->nxt_)
    {
        ulong len;

        /* 
         *   if this anchor has been marked as replaced, don't include it
         *   in our calculations, because we don't want to include this
         *   block in the image file 
         */
        if (anchor->is_replaced())
            continue;
        
        /* 
         *   if this item fits on the current page, assign it the next
         *   sequential address; otherwise, go to the next page
         *   
         *   if this anchor is at the dividing point, put it on the next
         *   page, unless we just started a new page 
         */
        len = anchor->get_len(ds);
        if (len > rem)
        {
            /* 
             *   we must start the next page - skip to the next page by
             *   moving past the remaining free space on this page 
             */
            free_ofs += rem;

            /* count the new page */
            ++page_cnt_;

            /* the whole next page is available to us now */
            rem = page_size_;
        }

        /* 
         *   set the anchor's final address, which will apply fixups for
         *   the object's fixup list 
         */
        anchor->set_addr(free_ofs);

        /* advance past this block */
        free_ofs += len;
        rem -= len;
    }

    /* if there's no data at all, we have zero pages */
    if (free_ofs == 0)
        page_cnt_ = 0;
}


/*
 *   Write our stream to an image file 
 */
void CTcStreamLayout::write_to_image(CTcDataStream **ds_arr, size_t ds_cnt,
                                     CVmImageWriter *image_writer,
                                     int pool_id, uchar xor_mask)
{
    CTcStreamAnchor *anchor;
    ulong free_ofs;
    ulong next_page_start;
    int pgnum;
    
    /* write the constant pool definition block - the pool's ID is 2 */
    image_writer->write_pool_def(pool_id, page_cnt_, page_size_, TRUE);

    /* 
     *   start out before the first page - the next page starts with the
     *   item at offset zero 
     */
    pgnum = 0;
    next_page_start = 0;

    /* run through each stream */
    for ( ; ds_cnt != 0 ; ++ds_arr, --ds_cnt)
    {
        CTcDataStream *ds;

        /* get the current stream */
        ds = *ds_arr;

        /* run through the anchor list for this stream */
        for (anchor = ds->get_first_anchor() ; anchor != 0 ;
             anchor = anchor->nxt_)
        {
            ulong len;
            ulong stream_ofs;
            ulong addr;
            
            /* 
             *   if this anchor is marked as replaced, skip it entirely -
             *   we omit replaced blocks from the image file, because
             *   they're completely unreachable 
             */
            if (anchor->is_replaced())
                continue;
            
            /* 
             *   if this item's assigned address is on the next page, move
             *   to the next page 
             */
            len = anchor->get_len(ds);
            addr = anchor->get_addr();
            if (addr == next_page_start)
            {
                /* if this isn't the first page, close the previous page */
                if (pgnum != 0)
                    image_writer->end_pool_page();
                
                /* start the new page */
                image_writer->begin_pool_page(pool_id, pgnum, TRUE, xor_mask);
                
                /* this item is at the start of the new page */
                free_ofs = next_page_start;
                
                /* count the new page */
                ++pgnum;
                
                /* calculate the address of the start of the next page */
                next_page_start += page_size_;
            }
            
            /* advance past this block */
            free_ofs += len;
            
            /* 
             *   write the data from the stream to the image file - we
             *   must iterate over the chunks the code stream returns,
             *   since it might not be able to return the entire block in
             *   a single operation 
             */
            for (stream_ofs = anchor->get_ofs() ; len != 0 ; )
            {
                ulong cur;
                const char *ptr;
                
                /* get the pointer to this chunk */
                ptr = ds->get_block_ptr(stream_ofs, len, &cur);
                
                /* write this chunk */
                image_writer->write_pool_page_bytes(ptr, cur, xor_mask);
                
                /* advance our pointers past this chunk */
                len -= cur;
                stream_ofs += cur;
            }
        }
    }

    /* if we started a page, end it */
    if (pgnum != 0)
        image_writer->end_pool_page();
}

/* ------------------------------------------------------------------------ */
/*
 *   Object Symbol subclass - image-file functions 
 */

/* 
 *   mark the compiled data for the object as a 'class' object 
 */
void CTcSymObj::mark_compiled_as_class()
{
    uint flags;
    CTcDataStream *str;

    /* get the appropriate stream for generating the data */
    str = get_stream();
    
    /* get my original object flags */
    flags = CTPNStmObject::get_stream_obj_flags(str, stream_ofs_);

    /* add in the 'class' flag */
    flags |= TCT3_OBJFLG_CLASS;

    /* set the updated flags */
    CTPNStmObject::set_stream_obj_flags(str, stream_ofs_, flags);
}

/*
 *   Delete a property from our modified base classes 
 */
void CTcSymObj::delete_prop_from_mod_base(tctarg_prop_id_t prop_id)
{
    uint prop_cnt;
    uint i;
    CTcDataStream *str;

    /* get the correct data stream */
    str = get_stream();

    /* get the number of properties in the object */
    prop_cnt = CTPNStmObject::get_stream_prop_cnt(str, stream_ofs_);

    /* find the property in our property table */
    for (i = 0 ; i < prop_cnt ; ++i)
    {
        /* if this property ID matches, delete it */
        if (CTPNStmObject::get_stream_prop_id(str, stream_ofs_, i)
            == prop_id)
        {
            /* delete the object by setting its ID to 'invalid' */
            CTPNStmObject::set_stream_prop_id(str, stream_ofs_, i,
                                              VM_INVALID_PROP);

            /* 
             *   there's no need to look any further - a property can
             *   occur only once in an object 
             */
            break;
        }
    }
}

/*
 *   Build the dictionary
 */
void CTcSymObj::build_dictionary()
{
    uint prop_cnt;
    uint i;

    /* 
     *   Inherit the default handling - this will explicitly add all
     *   superclass dictionary data into my own internal dictionary list,
     *   so that we don't have to worry at all about superclasses here.
     *   This will also add our words to my associated dictionary object.  
     */
    CTcSymObjBase::build_dictionary();

    /* if I'm not a regular tads object, there's nothing to do here */
    if (metaclass_ != TC_META_TADSOBJ)
        return;

    /* 
     *   Examine my properties.  Each time we find a property whose value
     *   is set to vocab-list, replace it with an actual list of strings
     *   for my vocabulary words associated with the property.  
     */

    /* get the number of properties in the object */
    prop_cnt = CTPNStmObject::get_stream_prop_cnt(G_os, stream_ofs_);

    /* find the property in our property table */
    for (i = 0 ; i < prop_cnt ; ++i)
    {
        CTcConstVal val;
        vm_datatype_t prop_type;
        
        /* get this property value */
        prop_type = CTPNStmObject::get_stream_prop_type(G_os, stream_ofs_, i);

        /* 
         *   if it's a vocabulary list placeholder, replace it with the
         *   actual list of vocabulary strings 
         */
        if (prop_type == VM_VOCAB_LIST)
        {
            vm_prop_id_t prop_id;
            CTcVocabEntry *entry;
            CTPNList *lst;
            ulong prop_val_ofs;

            /* get the property ID */
            prop_id = CTPNStmObject::get_stream_prop_id(G_os, stream_ofs_, i);

            /* get the value offset of this property */
            prop_val_ofs = CTPNStmObject::
                           get_stream_prop_val_ofs(G_os, stream_ofs_, i);

            /* create a list */
            lst = new CTPNList();

            /* 
             *   scan my internal vocabulary list and add the entries
             *   associated with this property 
             */
            for (entry = vocab_ ; entry != 0 ; entry = entry->nxt_)
            {
                /* if this one matches our property, add it */
                if (entry->prop_ == prop_id)
                {
                    CTcConstVal str_val;
                    CTcPrsNode *ele;
                    
                    /* create a string element */
                    str_val.set_sstr(entry->txt_, entry->len_);
                    ele = new CTPNConst(&str_val);

                    /* add it to the list */
                    lst->add_element(ele);
                }
            }

            /* 
             *   Overwrite the original property value with the new list.
             *   If the list is empty, this object doesn't define or
             *   inherit any vocabulary of this property at all, so we can
             *   clear the property entirely. 
             */
            if (lst->get_count() == 0)
            {
                /* 
                 *   delete the property from the object by setting its
                 *   property ID to 'invalid' 
                 */
                CTPNStmObject::
                    set_stream_prop_id(G_os, stream_ofs_, i, VM_INVALID_PROP);
            }
            else
            {
                /* write the list value to the property */
                val.set_list(lst);
                G_cg->write_const_as_dh(G_os, prop_val_ofs, &val);
            }
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Symbol table entry routines for writing a symbol to the global symbol
 *   table in the debug records in the image file 
 */

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymFunc::write_to_image_file_global(class CVmImageWriter *image_writer)
{
    char buf[128];

    /* build our extra data buffer */
    oswp4(buf, get_code_pool_addr());
    oswp2(buf + 4, get_argc());
    buf[6] = (is_varargs() != 0);
    buf[7] = (has_retval() != 0);
    oswp2(buf + 8, get_opt_argc());
    
    /* write the data */
    image_writer->write_gsym_entry(get_sym(), get_sym_len(),
                                   (int)TC_SYM_FUNC, buf, 10);

    /* we wrote the symbol */
    return TRUE;
}

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymObj::write_to_image_file_global(class CVmImageWriter *image_writer)
{
    char buf[128];

    /* store our object ID in the extra data buffer */
    oswp4(buf, obj_id_);

    /* add our modifying object ID, if we have a modifying object */
    if (get_modifying_sym() != 0)
        oswp4(buf + 4, get_modifying_sym()->get_obj_id());
    else
        oswp4(buf + 4, 0);

    /* write the data */
    image_writer->write_gsym_entry(get_sym(), get_sym_len(),
                                   (int)TC_SYM_OBJ, buf, 8);

    /* we wrote the symbol */
    return TRUE;
}

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymProp::write_to_image_file_global(class CVmImageWriter *image_writer)
{
    char buf[128];

    /* build our extra data buffer */
    oswp2(buf, (uint)get_prop());

    /* build our flags byte */
    buf[2] = 0;
    if (vocab_)
        buf[2] |= 0x01;

    /* write the data */
    image_writer->write_gsym_entry(get_sym(), get_sym_len(),
                                   (int)TC_SYM_PROP, buf, 3);

    /* we wrote the symbol */
    return TRUE;
}

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymEnum::write_to_image_file_global(class CVmImageWriter *image_writer)
{
    char buf[128];

    /* build our extra data buffer */
    oswp4(buf, get_enum_id());

    /* build our flags */
    buf[4] = 0;
    if (is_token_)
        buf[4] |= 0x01;

    /* write the data */
    image_writer->write_gsym_entry(get_sym(), get_sym_len(),
                                   (int)TC_SYM_ENUM, buf, 5);

    /* we wrote the symbol */
    return TRUE;
}

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymMetaclass::
   write_to_image_file_global(class CVmImageWriter *image_writer)
{
    char buf[128];

    /* build our extra data buffer */
    oswp2(buf, meta_idx_);
    oswp4(buf + 2, class_obj_);

    /* write the data */
    image_writer->write_gsym_entry(get_sym(), get_sym_len(),
                                   (int)TC_SYM_METACLASS, buf, 6);

    /* we wrote the symbol */
    return TRUE;
}

/*
 *   Fix up the inheritance chain in the modifier objects 
 */
void CTcSymMetaclass::fix_mod_obj_sc_list()
{
    CTcSymObj *obj;
    CTcSymObj *obj_base;
    
    /* 
     *   go through our chain of modifier objects, and make sure the
     *   stream data for each object points to its correct superclass 
     */
    for (obj = mod_obj_ ; obj != 0 ; obj = obj_base)
    {
        CTcDataStream *str;

        /* get the correct data stream */
        str = obj->get_stream();

        /* get the base object for this symbol */
        obj_base = obj->get_mod_base_sym();

        /* 
         *   if there's no base object, there's no superclass entry to
         *   adjust for this object 
         */
        if (obj_base == 0)
            break;

        /* 
         *   set the superclass in this object to point to this base
         *   object 
         */
        CTPNStmObject::set_stream_sc(str, obj->get_stream_ofs(),
                                     0, obj_base->get_obj_id());
    }
}

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymBif::write_to_image_file_global(class CVmImageWriter *image_writer)
{
    char buf[128];

    /* build our extra data buffer */
    oswp2(buf, get_func_idx());
    oswp2(buf + 2, get_func_set_id());
    buf[4] = (has_retval() != 0);
    oswp2(buf + 5, get_min_argc());
    oswp2(buf + 7, get_max_argc());
    buf[9] = (is_varargs() != 0);

    /* write the data */
    image_writer->write_gsym_entry(get_sym(), get_sym_len(),
                                   (int)TC_SYM_BIF, buf, 10);

    /* we wrote the symbol */
    return TRUE;
}

/* 
 *   write the symbol to an image file's global symbol table 
 */
int CTcSymExtfn::write_to_image_file_global(class CVmImageWriter *iw)
{
    //$$$ to be implemented
    assert(FALSE);
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Object properties 
 */

/*
 *   Check locals 
 */
void CTPNObjProp::check_locals()
{
    /* check locals in our code body */
    if (code_body_ != 0)
        code_body_->check_locals();
}
