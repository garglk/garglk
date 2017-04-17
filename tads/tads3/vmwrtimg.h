/* $Header: d:/cvsroot/tads/tads3/vmwrtimg.h,v 1.4 1999/07/11 00:46:59 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmwrtimg.h - T3 Image File Writer utility functions
Function
  
Notes
  
Modified
  04/04/99 MJRoberts  - Creation
*/

#ifndef VMWRTIMG_H
#define VMWRTIMG_H

#include "t3std.h"
#include "vmtype.h"


class CVmImageWriter
{
public:
    /* create an image writer with a given output file */
    CVmImageWriter(class CVmFile *fp);

    /* delete the image writer */
    ~CVmImageWriter();

    /* get the current seek location in the underlying file */
    long get_pos() const;

    /* 
     *   Prepare the file: write out the fixed header information.  This
     *   should be called once, before doing any other writing.  'vsn' is
     *   the image file version number to store in the file.
     */
    void prepare(uint vsn, const char tool_data[4]);

    /*
     *   Begin a block with the specified ID (a four-byte string).  Writes
     *   the block header, and remembers where the block starts so that
     *   the size prefix can be stored when the block is finished.  Blocks
     *   cannot be nested; starting a new block automatically ends the
     *   previous block.  
     */
    void begin_block(const char *block_id, int mandatory);

    /*
     *   End the current block. 
     */
    void end_block();

    /* write raw bytes to the image file */
    void write_bytes(const char *ptr, uint32_t siz);

    /* 
     *   Write a complete entrypoint block, given the code offset of the
     *   entrypoint and the various table entry sizes.  If a block is in
     *   progress, this will terminate it.  
     */
    void write_entrypt(uint32_t entry_ofs, size_t method_header_size,
                       size_t exc_entry_size, size_t line_entry_size,
                       size_t dbg_hdr_size, size_t dbg_lclsym_hdr_size,
                       size_t dbg_frame_hdr_size,
                       int dbg_vsn_id);

    /*
     *   Write a complete function set dependency block, given an array of
     *   function set names.  If a block is in progress, this will
     *   terminate it.
     */
    void write_func_dep(const char **funcset_names, int count);

    /*
     *   Write a complete metaclass dependency block, given an array of
     *   metaclass names.  If a block is in progress, this will terminate
     *   it. 
     */
    void write_meta_dep(const char **metaclass_names, int count);

    /*
     *   Write a metaclass dependency block in pieces.  Call
     *   begin_meta_dep() with the number of metaclasses, then call
     *   write_meta_dep_item() to write each item, and finally call
     *   end_meta_dep() to finish the block.
     */
    void begin_meta_dep(int count);
    void write_meta_dep_item(const char *metaclass_name);
    void write_meta_item_prop(uint prop_id);
    void end_meta_prop_list();
    void end_meta_dep();

    /*
     *   Write a function set dependency block in pieces 
     */
    void begin_func_dep(int count)
        { begin_dep_block("FNSD", count); }
    void write_func_dep_item(const char *funcset_name)
        { write_dep_block_item(funcset_name); }
    void end_func_dep()
        { end_dep_block(); }

    /*
     *   Write a constant pool definition block.
     */
    void write_pool_def(uint pool_id, uint32_t page_count, uint32_t page_size,
                        int mandatory);

    /*
     *   Fix up a constant pool definition block with the actual number of
     *   pages.  This can be used, if desired, to wait to determine the
     *   actual number of pages in a given constant pool until after
     *   writing the pages.  Since the pool definition block must precede
     *   the pool's first page block in the image file, it's impossible to
     *   wait until after writing all of the pages to write the definition
     *   block.  Instead, the caller must write the pool definition block
     *   first, using a temporary placeholder value for the page_count (0
     *   will suffice), then write the pool pages, then use this to fix up
     *   the pool definition block with the actual number of pages.  The
     *   caller must note the seek position prior to writing the pool
     *   definition block, so that we can seek back to that position to
     *   fix up the definition block.
     *   
     *   Callers need not use this function if they know the actual number
     *   of pages in the pool when writing the original pool definition
     *   block.  
     */
    void fix_pool_def(long def_seek_ofs, uint32_t page_count);

    /*
     *   Write a constant/code pool page.  This writes the entire page
     *   with its header and data in a single operation; this is the
     *   easiest way to write a page when the page has been fully
     *   constructed in a single memory block in advance.  
     */
    void write_pool_page(uint pool_id, uint32_t page_index,
                         const char *page_data, uint32_t page_data_size,
                         int mandatory, uchar xor_mask);

    /*
     *   Write a constant/code pool page in pieces.  These routines can be
     *   used when the data in the page are not contiguous in memory and
     *   must be written in pieces.  Start by calling begin_pool_page(),
     *   then call write_pool_page_bytes() for each item; the items are
     *   written contiguously to the page.  Finish by calling
     *   end_pool_page().  
     */
    void begin_pool_page(uint pool_id, uint32_t page_index, int mandatory,
                         uchar xor_mask);
    void write_pool_page_bytes(const char *buf, uint32_t siz, uchar xor_mask);
    void end_pool_page();

    /*
     *   Write items in the symbolic names block.  Start with
     *   begin_sym_block(), then call the write_sym_item_xxx() functions
     *   to write the names.  Finally, call end_sym_block() when done.
     */
    void begin_sym_block();

    void write_sym_item_objid(const char *nm, size_t len, ulong obj_id);
    void write_sym_item_propid(const char *nm, size_t len, uint prop_id);
    void write_sym_item_func(const char *nm, size_t len, ulong code_ofs);
    void write_sym_item(const char *nm, size_t nmlen,
                        const struct vm_val_t *val);

    void write_sym_item_objid(const char *nm, ulong obj_id)
        { write_sym_item_objid(nm, get_strlen(nm), obj_id); }
    void write_sym_item_propid(const char *nm, uint prop_id)
        { write_sym_item_propid(nm, get_strlen(nm), prop_id); }
    void write_sym_item(const char *nm, const struct vm_val_t *val)
        { write_sym_item(nm, get_strlen(nm), val); }

    void end_sym_block();


    /*
     *   Write items in an OBJS (static object data) block.  Start with
     *   begin_objs_block(), then call write_objs_bytes() repeatedly to
     *   write the bytes.  Finally, call end_objs_block() when done.
     *   
     *   If the 'large_objects' field is set, the objects in the block use
     *   32-bit size fields; otherwise the objects use 16-bit size fields.
     *   
     *   If 'trans' is true, the objects in this block are transient;
     *   otherwise, the objects are non-transient (i.e., persistent).  
     */
    void begin_objs_block(uint metaclass_idx, int large_objects, int trans);
    void write_objs_bytes(const char *buf, uint32_t siz);
    void end_objs_block(uint object_count);

    /*
     *   Write the items in a SRCF (source file descriptor) block.  Start
     *   with begin_srcf_block(), then write the file entries.  For each
     *   entry, call begin_src_entry(), then call write_src_line_entry()
     *   for each source line debug record, then call end_srcf_entry().
     *   Call end_srcf_block() when done with all file entries.  
     */
    void begin_srcf_block(int count);
    void begin_srcf_entry(int orig_index, const char *fname);
    void write_srcf_line_entry(ulong linenum, ulong addr);
    void end_srcf_entry();
    void end_srcf_block();

    /* 
     *   Write the items in a GSYM (global symbol table) block.  Start
     *   with begin_gsym_block(), then call write_gsym_entry() repeatedly
     *   to write the entries.  Call end_gsym_block() when done. 
     */
    void begin_gsym_block();
    void write_gsym_entry(const char *sym, size_t sym_len,
                          int type_id, const char *dat, size_t dat_len);
    void end_gsym_block(ulong count);

    /*
     *   Begin MHLS block, write an item, and end the block 
     */
    void begin_mhls_block();
    void write_mhls_entry(ulong code_addr);
    void end_mhls_block();

    /*
     *   Begin/end SINI block.  static_cs_ofs is the offset in the code
     *   segment of the first static initializer; this is useful in the
     *   image file because we can delete all of the code pages starting
     *   at this point after pre-initialization is complete.  
     */
    void begin_sini_block(ulong static_cs_ofs, ulong init_cnt);
    void end_sini_block();

    /* begin/end a MACR (macro symbols) block */
    void begin_macr_block();
    void end_macr_block();

    /*
     *   Finish the file.  Automatically ends the current block if a block
     *   is open, and writes the end-of-file marker. 
     */
    void finish();

    /* 
     *   get the underlying file object; for some types of blocks, it's
     *   simplest for the caller to write the data directly to the underlying
     *   file stream without any help from us 
     */
    class CVmFile *get_fp() const { return fp_; }

private:
    /* write a generic dependency (function set, metaclass) block */
    void write_dep_block(const char *block_id, const char **names, int count);

    /* write a dependency block in pieces */
    void begin_dep_block(const char *block_id, int count);
    void write_dep_block_item(const char *nm);
    void end_dep_block();

    /* XOR a block of bytes with a mask and write the results to the file */
    void xor_and_write_bytes(const char *p, uint32_t len, uchar xor_mask);
  
    /* underlying file */
    class CVmFile *fp_;

    /* 
     *   Seek position of start of current block.  If this is zero, no
     *   block is currently open. 
     */
    long block_start_;

    /* count of symbolic names written so far (for writing SYMD block) */
    int symd_cnt_;

    /* location of metaclass entry next-record offset */
    long mcld_ofs_pos_;

    /* location of metaclass property list count prefix */
    long mcld_propcnt_pos_;

    /* count of properties writeen in a metaclass prop list so far */
    int mcld_prop_cnt_;

    /* seek location of SYMD count prefix, for fixing up at end of block */
    long symd_prefix_;

    /* seek location of OBJS count prefix, for fixup up at end of block */
    long objs_prefix_;

    /* seek location of GSYM count prefix, for fixup at end of block */
    long gsym_prefix_;

    /* start of current SRCF file entry */
    long srcf_entry_pos_;

    /* count of SRCF line entries */
    long srcf_line_cnt_;

    /* position of SRCF line entry for the current file */
    long srcf_line_pos_;

    /* position of MHLS block count entry, and MHLS entry count so far */
    long mhls_cnt_pos_;
    ulong mhls_cnt_;
};

#endif /* VMWRTIMG_H */

