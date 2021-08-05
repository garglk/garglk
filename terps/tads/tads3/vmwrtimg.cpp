#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/vmwrtimg.cpp,v 1.4 1999/07/11 00:46:59 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmwrtimg.cpp - T3 Image File Writer utility functions
Function
  Provides functions to write an image file
Notes
  
Modified
  04/04/99 MJRoberts  - Creation
*/


#include <string.h>
#include <time.h>

#include "t3std.h"
#include "vmfile.h"
#include "vmwrtimg.h"
#include "vmimage.h"

/* ------------------------------------------------------------------------ */
/*
 *   initialize 
 */
CVmImageWriter::CVmImageWriter(CVmFile *fp)
{
    /* remember the underlying file */
    fp_ = fp;

    /* no block is currently open */
    block_start_ = 0;
}

/*
 *   delete 
 */
CVmImageWriter::~CVmImageWriter()
{
}

/* ------------------------------------------------------------------------ */
/*
 *   get the current seek position in the underlying file 
 */
long CVmImageWriter::get_pos() const
{
    return fp_->get_pos();
}

/* ------------------------------------------------------------------------ */
/*
 *   Prepare the file - write the header. 
 */
void CVmImageWriter::prepare(uint vsn, const char tool_data[4])
{
    /* write the signature */
    fp_->write_bytes(VMIMAGE_SIG, sizeof(VMIMAGE_SIG)-1);

    /* write the version number */
    fp_->write_uint2(vsn);

    /* write the 28 reserved bytes, setting all to zero */
    char buf[32];
    memset(buf, 0, 28);
    fp_->write_bytes(buf, 28);

    /* write the additional 4 bytes reserved for tool use */
    fp_->write_bytes(tool_data, 4);

    /* write the compilation timestamp */
    os_time_t timer = os_time(NULL);
    struct tm *tblock = os_localtime(&timer);
    fp_->write_bytes(asctime(tblock), 24);
}

/* ------------------------------------------------------------------------ */
/*
 *   Begin a block
 */
void CVmImageWriter::begin_block(const char *block_id, int mandatory)
{
    char buf[10];
    uint flags;
    
    /* if there's a block currently open, close it */
    end_block();

    /* remember the seek location of the start of the block */
    block_start_ = fp_->get_pos();

    /* store the type string */
    memcpy(buf, block_id, 4);

    /* store four bytes of zeros as a placeholder for the size */
    memset(buf+4, 0, 4);

    /* compute the flags */
    flags = 0;
    if (mandatory)
        flags |= VMIMAGE_DBF_MANDATORY;

    /* store the flags */
    oswp2(buf+8, flags);

    /* write the header */
    fp_->write_bytes(buf, 10);
}

/* ------------------------------------------------------------------------ */
/*
 *   End the current block.  If no block is open, this does nothing. 
 */
void CVmImageWriter::end_block()
{
    long end_pos;
    uint32_t siz;
    
    /* if there's no block open, there's nothing we need to do */
    if (block_start_ == 0)
        return;

    /* 
     *   note the current file position - this will let us compute the
     *   size of the block, and we'll need to seek back here when we're
     *   done updating the block header 
     */
    end_pos = fp_->get_pos();

    /*
     *   Since the block is finished, we can now compute its size.  The
     *   size of the data block is the end seek position minus the
     *   starting seek position.  'block_start_' contains the seek
     *   position of the block header, which takes up ten bytes; we want
     *   to store the size of the block's data, excluding the header,
     *   which is (end_pos - block_header_pos - 10).  
     */
    siz = (uint32_t)(end_pos - block_start_ - 10);

    /* 
     *   Seek back to the location of the size field in the block header;
     *   this is four bytes into the block header.  Then, update the size
     *   field with the size of the block's data.
     */
    fp_->set_pos(block_start_ + 4);
    fp_->write_uint4(siz);

    /* 
     *   seek back to the end of the block, so we can resume writing data
     *   following the block 
     */
    fp_->set_pos(end_pos);

    /* the block is now closed, so forget about it */
    block_start_ = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Write raw bytes to the file 
 */
void CVmImageWriter::write_bytes(const char *ptr, uint32_t siz)
{
    /* write in 64k chunks, to accommodate 16-bit platforms */
    while (siz != 0)
    {
        size_t cur;

        /* get the next 64k, or the remainder if less than 64k is left */
        cur = 65535;
        if (siz < cur)
            cur = (size_t)siz;

        /* write this chunk */
        fp_->write_bytes(ptr, cur);

        /* advance past this chunk */
        ptr += cur;
        siz -= cur;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Finish the file.  Closes the current block if one is open, and writes
 *   the end-of-file marker to the file. 
 */
void CVmImageWriter::finish()
{
    /* if there's a block open, close it */
    end_block();

    /* 
     *   write the EOF block - the block contains no data, so simply begin
     *   and end it 
     */
    begin_block("EOF ", TRUE);
    end_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   Write an entrypoint (ENTP) block
 */
void CVmImageWriter::write_entrypt(uint32_t entry_ofs, size_t method_hdr_size,
                                   size_t exc_entry_size,
                                   size_t line_entry_size,
                                   size_t dbg_hdr_size,
                                   size_t dbg_lclsym_hdr_size,
                                   size_t dbg_frame_hdr_size,
                                   int dbg_vsn_id)
{
    char buf[32];
    
    /* prepare the block's contents */
    oswp4(buf, entry_ofs);
    oswp2(buf+4, method_hdr_size);
    oswp2(buf+6, exc_entry_size);
    oswp2(buf+8, line_entry_size);
    oswp2(buf+10, dbg_hdr_size);
    oswp2(buf+12, dbg_lclsym_hdr_size);
    oswp2(buf+14, dbg_vsn_id);
    oswp2(buf+16, dbg_frame_hdr_size);

    /* open the block, write the data, and close the block */
    begin_block("ENTP", TRUE);
    fp_->write_bytes(buf, 18);
    end_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   Write a function set dependency block
 */
void CVmImageWriter::write_func_dep(const char **funcset_names, int count)
{
    /* write a FNSD block */
    write_dep_block("FNSD", funcset_names, count);
}

/* ------------------------------------------------------------------------ */
/*
 *   begin a metaclass dependency block 
 */
void CVmImageWriter::begin_meta_dep(int count)
{
    /* begin the dependency block */
    begin_dep_block("MCLD", count);

    /* we're not in a property list yet */
    mcld_propcnt_pos_ = 0;
}

/*
 *   Write a metaclass dependency block 
 */
void CVmImageWriter::write_meta_dep(const char **meta_names, int count)
{
    /* write a MCLD block */
    write_dep_block("MCLD", meta_names, count);
}

/*
 *   write a metaclass dependency block item 
 */
void CVmImageWriter::write_meta_dep_item(const char *metaclass_name)
{
    /* if we didn't end the previous item's property list, end it now */
    end_meta_prop_list();

    /* write a placeholder next record offset */
    mcld_ofs_pos_ = fp_->get_pos();
    fp_->write_uint2(0);
    
    /* write the metaclass name */
    write_dep_block_item(metaclass_name);

    /* write a placeholder property vector count */
    mcld_propcnt_pos_ = fp_->get_pos();
    fp_->write_uint2(0);

    /* write the property record size (2 bytes) */
    fp_->write_uint2(2);

    /* no properties yet */
    mcld_prop_cnt_ = 0;
}

/*
 *   write a metaclass dependency property list item 
 */
void CVmImageWriter::write_meta_item_prop(uint prop_id)
{
    /* write the property ID */
    fp_->write_uint2(prop_id);

    /* count it */
    ++mcld_prop_cnt_;
}

/*
 *   end a metaclass prop list 
 */
void CVmImageWriter::end_meta_prop_list()
{
    /* if we have a count pending, go write it */
    if (mcld_propcnt_pos_ != 0)
    {
        long pos;

        /* remember the current position */
        pos = fp_->get_pos();

        /* go back and write the property count */
        fp_->set_pos(mcld_propcnt_pos_);
        fp_->write_uint2(mcld_prop_cnt_);

        /* we no longer have a property count fixup to apply */
        mcld_propcnt_pos_ = 0;

        /* go back and write the next-record offset */
        fp_->set_pos(mcld_ofs_pos_);
        fp_->write_int2((int)(pos - mcld_ofs_pos_));

        /* go back to the end of the record */
        fp_->set_pos(pos);
    }
}

/*
 *   end a metaclass dependency block 
 */
void CVmImageWriter::end_meta_dep()
{
    /* end the last metaclass item */
    end_meta_prop_list();
    
    /* end the dependency block */
    end_dep_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   Begin a dependency block 
 */
void CVmImageWriter::begin_dep_block(const char *block_id, int count)
{
    char buf[4];
    
    /* open the block */
    begin_block(block_id, TRUE);

    /* write the number of entries */
    oswp2(buf, count);
    fp_->write_bytes(buf, 2);
}

/*
 *   Write a dependency block item 
 */
void CVmImageWriter::write_dep_block_item(const char *nm)
{
    size_t len;
    char buf[4];

    /* get the length of this name, and truncate to 255 bytes */
    len = strlen(nm);
    if (len > 255)
        len = 255;

    /* write the length, followed by the name */
    buf[0] = (char)(uchar)len;
    fp_->write_bytes(buf, 1);
    fp_->write_bytes(nm, len);
}

/*
 *   End a dependency block 
 */
void CVmImageWriter::end_dep_block()
{
    /* end the block */
    end_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   Write a generic dependency list block 
 */
void CVmImageWriter::write_dep_block(const char *block_id,
                                     const char **names, int count)
{
    /* open the block */
    begin_dep_block(block_id, count);

    /* write each entry */
    for ( ; count > 0 ; ++names, --count)
        write_dep_block_item(*names);

    /* end the block */
    end_dep_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   Write a constant pool definition block 
 */
void CVmImageWriter::write_pool_def(uint pool_id, uint32_t page_count,
                                    uint32_t page_size, int mandatory)
{
    char buf[16];
    
    /* prepare the block's data */
    oswp2(buf, pool_id);
    oswp4(buf+2, page_count);
    oswp4(buf+6, page_size);

    /* open the block, write the data, and end the block */
    begin_block("CPDF", mandatory);
    fp_->write_bytes(buf, 10);
    end_block();
}

/*
 *   Fix up a pool definition block with the actual page count 
 */
void CVmImageWriter::fix_pool_def(long def_seek_ofs, uint32_t page_count)
{
    long old_pos;
    char buf[4];

    /* note the file position at entry */
    old_pos = fp_->get_pos();
    
    /* 
     *   seek to the original definition block location, plus the size of
     *   the block header (10 bytes), plus the offset within the block of
     *   the pool page count (it starts 2 bytes into the block data) 
     */
    fp_->set_pos(def_seek_ofs + 10 + 2);

    /* write the page count */
    oswp4(buf, page_count);
    fp_->write_bytes(buf, 4);

    /* seek back to our location at entry */
    fp_->set_pos(old_pos);
}


/* ------------------------------------------------------------------------ */
/*
 *   Write a constant pool page 
 */
void CVmImageWriter::write_pool_page(uint pool_id, uint32_t page_index,
                                     const char *page_data,
                                     uint32_t page_data_size, int mandatory,
                                     uchar xor_mask)
{
    char buf[16];

    /* begin the block */
    begin_block("CPPG", mandatory);

    /* prepare the prefix */
    oswp2(buf, pool_id);
    oswp4(buf+2, page_index);
    buf[6] = xor_mask;

    /* write the prefix */
    fp_->write_bytes(buf, 7);

    /* write the page data, XOR'ing the data with the mask byte */
    xor_and_write_bytes(page_data, page_data_size, xor_mask);

    /* end the block */
    end_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   Begin writing a constant pool page.  This constructs the header and
 *   prepares for writing the bytes making up the page. 
 */
void CVmImageWriter::begin_pool_page(uint pool_id, uint32_t page_index,
                                     int mandatory, uchar xor_mask)
{
    char buf[16];

    /* begin the block */
    begin_block("CPPG", mandatory);

    /* prepare the prefix */
    oswp2(buf, pool_id);
    oswp4(buf+2, page_index);
    buf[6] = xor_mask;

    /* write the prefix */
    fp_->write_bytes(buf, 7);
}

/*
 *   write bytes to a pool page under construction 
 */
void CVmImageWriter::write_pool_page_bytes(const char *buf, uint32_t siz,
                                           uchar xor_mask)
{
    /* write the page data, XOR'ing the data with the mask byte */
    xor_and_write_bytes(buf, siz, xor_mask);
}

/*
 *   XOR and write a block of data - we will XOR each byte of the data
 *   with the given mask byte before writing it to the file 
 */
void CVmImageWriter::xor_and_write_bytes(const char *mem, uint32_t siz,
                                         uchar xor_mask)
{
    /* 
     *   if there's no mask, simply write the data directly - anything XOR
     *   zero equals the original value
     */
    if (xor_mask == 0)
    {
        /* write the data to the page */
        fp_->write_bytes(mem, siz);
    }
    else
    {
        /* 
         *   copy the data in chunks into our buffer, XOR it with the
         *   mask, and write the results 
         */
        while (siz != 0)
        {
            char buf[1024];
            size_t cur;
            size_t rem;
            char *dst;

            /* 
             *   limit this chunk to the buffer size or the remainder of
             *   the input, whichever is smaller 
             */
            cur = sizeof(buf);
            if (cur > siz)
                cur = (size_t)siz;

            /* copy this chunk, xor'ing each byte with the mask */
            for (dst = buf, rem = cur ; rem != 0 ; --rem, ++dst, ++mem)
                *dst = *mem ^ xor_mask;

            /* write out this chunk */
            fp_->write_bytes(buf, cur);

            /* subtract this chunk from the length remaining */
            siz -= cur;
        }
    }
}

/*
 *   finish writing a pool page 
 */
void CVmImageWriter::end_pool_page()
{
    /* end the block */
    end_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   Begin a symbolic names block
 */
void CVmImageWriter::begin_sym_block()
{
    char buf[4];

    /* begin the block */
    begin_block("SYMD", FALSE);

    /* remember where our placeholder goes */
    symd_prefix_ = fp_->get_pos();

    /* prepare the placeholder prefix, using a zero count for now */
    oswp2(buf, 0);

    /* write the prefix */
    fp_->write_bytes(buf, 2);

    /* we haven't written any symbolic name items yet */
    symd_cnt_ = 0;
}

/*
 *   write a symbolic name for an object ID 
 */
void CVmImageWriter::write_sym_item_objid(const char *nm, size_t len,
                                          ulong obj_id)
{
    vm_val_t val;

    /* set up the object ID value */
    val.set_obj((vm_obj_id_t)obj_id);

    /* write it out */
    write_sym_item(nm, len, &val);
}

/*
 *   write a symbolic name for a property ID 
 */
void CVmImageWriter::write_sym_item_propid(const char *nm, size_t len,
                                           uint prop_id)
{
    vm_val_t val;

    /* set up the property ID value */
    val.set_propid((vm_prop_id_t)prop_id);

    /* write it out */
    write_sym_item(nm, len, &val);
}

/*
 *   write a symbolic name for a function
 */
void CVmImageWriter::write_sym_item_func(const char *nm, size_t len,
                                         ulong code_ofs)
{
    vm_val_t val;

    /* set up the property ID value */
    val.set_fnptr((pool_ofs_t)code_ofs);

    /* write it out */
    write_sym_item(nm, len, &val);
}

/*
 *   write a symbolic name item 
 */
void CVmImageWriter::write_sym_item(const char *nm, size_t len,
                                    const vm_val_t *val)
{
    char buf[VMB_DATAHOLDER + 1];

    /* prepare the data holder in the prefix */
    vmb_put_dh(buf, val);

    /* limit the length to 255 bytes */
    if (len > 255)
        len = 255;

    /* add the length to the prefix */
    buf[VMB_DATAHOLDER] = (char)len;

    /* write the prefix */
    fp_->write_bytes(buf, VMB_DATAHOLDER + 1);

    /* write the string */
    fp_->write_bytes(nm, len);

    /* count it */
    ++symd_cnt_;
}

/*
 *   end a symbolic names block 
 */
void CVmImageWriter::end_sym_block()
{
    long old_pos;
    char buf[4];
    
    /* end the block */
    end_block();

    /* 
     *   Go back and fix the header with the number of items we wrote.
     *   First, remember our current position, then seek back to the count
     *   prefix. 
     */
    old_pos = fp_->get_pos();
    fp_->set_pos(symd_prefix_);

    /* prepare the prefix, and write it out */
    oswp2(buf, symd_cnt_);
    fp_->write_bytes(buf, 2);

    /* restore the file position */
    fp_->set_pos(old_pos);
}

/* ------------------------------------------------------------------------ */
/*
 *   Begin an object static data block
 */
void CVmImageWriter::begin_objs_block(uint metaclass_idx, int large_objects,
                                      int trans)
{
    char buf[16];
    uint flags;

    /* begin the block */
    begin_block("OBJS", TRUE);

    /* prepare the flags */
    flags = 0;
    if (large_objects)
        flags |= 1;
    if (trans)
        flags |= 2;

    /* remember where the prefix goes so we can fix it up later */
    objs_prefix_ = fp_->get_pos();

    /* 
     *   write a placeholder object count, the metaclass dependency table
     *   index, and the OBJS flags 
     */
    oswp2(buf, 0);
    oswp2(buf + 2, metaclass_idx);
    oswp2(buf + 4, flags);

    /* write the prefix */
    fp_->write_bytes(buf, 6);
}

/*
 *   Write bytes to an OBJS (object static data) block 
 */
void CVmImageWriter::write_objs_bytes(const char *buf, uint32_t siz)
{
    /* write the buffer */
    fp_->write_bytes(buf, siz);
}

/*
 *   end an object static data block
 */
void CVmImageWriter::end_objs_block(uint object_count)
{
    long pos;

    /* remember the current file write position for a moment */
    pos = fp_->get_pos();

    /* go back and fix up the object count in the header */
    fp_->set_pos(objs_prefix_);
    fp_->write_uint2(object_count);

    /* seek back to the original position */
    fp_->set_pos(pos);
    
    /* end the block */
    end_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   SRCF blocks - source file descriptors 
 */

/*
 *   begin a SRCF block 
 */
void CVmImageWriter::begin_srcf_block(int count)
{
    /* 
     *   begin the block - SRCF blocks are always optional, since they're
     *   purely for debugging purposes 
     */
    begin_block("SRCF", FALSE);

    /* write the number of entries */
    fp_->write_uint2(count);

    /* each source line record is 8 bytes in the current format */
    fp_->write_uint2(8);
}

/*
 *   begin a SRCF file entry 
 */
void CVmImageWriter::begin_srcf_entry(int orig_index, const char *fname)
{
    size_t len;

    /* remember where this entry starts, so we can fix up the size later */
    srcf_entry_pos_ = fp_->get_pos();

    /* write a placeholder size entry */
    fp_->write_uint4(0);
    
    /* write the original index */
    fp_->write_uint2(orig_index);

    /* write the length of the name */
    len = get_strlen(fname);
    fp_->write_uint2(len);

    /* write the filename */
    fp_->write_bytes(fname, len);

    /* we have no line record yet, so write a placeholder count */
    srcf_line_pos_ = fp_->get_pos();
    fp_->write_uint4(0);
    srcf_line_cnt_ = 0;
}

/*
 *   write a SRCF line record entry 
 */
void CVmImageWriter::write_srcf_line_entry(ulong linenum, ulong addr)
{
    /* write the line number and address */
    fp_->write_uint4(linenum);
    fp_->write_uint4(addr);

    /* count it */
    ++srcf_line_cnt_;
}

/*
 *   end a SRCF file entry 
 */
void CVmImageWriter::end_srcf_entry()
{
    ulong pos;

    /* go back and fix up the line record count */
    pos = fp_->get_pos();
    fp_->set_pos(srcf_line_pos_);
    fp_->write_uint4(srcf_line_cnt_);

    /* go back and fix up the total entry size record */
    fp_->set_pos(srcf_entry_pos_);
    fp_->write_uint4(pos - srcf_entry_pos_);

    /* seek back to the end of the block */
    fp_->set_pos(pos);

}

/*
 *   end a SRCF block 
 */
void CVmImageWriter::end_srcf_block()
{
    /* end the block using the generic mechanism */
    end_block();
}

/* ------------------------------------------------------------------------ */
/* 
 *   MACR blocks - global preprocess macro symbol table 
 */

/* 
 *   begin a MACR block 
 */
void CVmImageWriter::begin_macr_block()
{
    /* 
     *   write the header - it's an optional block since it's for the
     *   debugger's use only 
     */
    begin_block("MACR", FALSE);
}

/*
 *   end a MACR block 
 */
void CVmImageWriter::end_macr_block()
{
    /* end the block using the generic mechanism */
    end_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   GSYM blocks - global symbol table
 */

/*
 *   begin a GSYM block 
 */
void CVmImageWriter::begin_gsym_block()
{
    /* 
     *   begin the block - GSYM blocks are always optional, since they're
     *   purely for debugging purposes 
     */
    begin_block("GSYM", FALSE);

    /* remember where the prefix goes so we can fix it up later */
    gsym_prefix_ = fp_->get_pos();

    /* write a placehodler object count and the metaclass index */
    fp_->write_uint4(0);
}

/*
 *   write a GSYM entry 
 */
void CVmImageWriter::write_gsym_entry(const char *sym, size_t sym_len,
                                      int type_id,
                                      const char *dat, size_t dat_len)
{
    /* 
     *   write the length of the symbol, length of the extra data, and the
     *   symbol type 
     */
    fp_->write_uint2(sym_len);
    fp_->write_uint2(dat_len);
    fp_->write_uint2(type_id);

    /* write the symbol name */
    fp_->write_bytes(sym, sym_len);

    /* write the extra data */
    fp_->write_bytes(dat, dat_len);
}

/*
 *   end a GSYM block 
 */
void CVmImageWriter::end_gsym_block(ulong cnt)
{
    long pos;

    /* remember the current file write position for a moment */
    pos = fp_->get_pos();

    /* go back and fix up the count in the header */
    fp_->set_pos(gsym_prefix_);
    fp_->write_uint4(cnt);

    /* seek back to the original position */
    fp_->set_pos(pos);
    
    /* end the block using the generic mechanism */
    end_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   MHLS blocks - method header list
 */

/*
 *   begin an MHLS block
 */
void CVmImageWriter::begin_mhls_block()
{
    /* 
     *   begin the block - MHLS blocks are always optional, since they're
     *   purely for debugging purposes 
     */
    begin_block("MHLS", FALSE);

    /* remember where the count goes so we can fix it up later */
    mhls_cnt_pos_ = fp_->get_pos();

    /* write a placehodler count */
    fp_->write_uint4(0);

    /* there are no entries yet */
    mhls_cnt_ = 0;
}

/*
 *   write an MHLS entry 
 */
void CVmImageWriter::write_mhls_entry(ulong addr)
{
    /* write the address */
    fp_->write_uint4(addr);

    /* count the entry */
    ++mhls_cnt_;
}

/*
 *   end an MHLS block 
 */
void CVmImageWriter::end_mhls_block()
{
    long pos;

    /* remember the current file write position for a moment */
    pos = fp_->get_pos();

    /* go back and fix up the count in the header */
    fp_->set_pos(mhls_cnt_pos_);
    fp_->write_uint4(mhls_cnt_);

    /* seek back to the original position */
    fp_->set_pos(pos);

    /* end the block using the generic mechanism */
    end_block();
}

/* ------------------------------------------------------------------------ */
/*
 *   SINI block - static initializer list
 */

/*
 *   begin an SINI block
 */
void CVmImageWriter::begin_sini_block(ulong static_cs_ofs, ulong init_cnt)
{
    /* 
     *   begin the block - SINI blocks are mandatory, since the program
     *   depends upon static initializers being evaluated immediately
     *   after compilation 
     */
    begin_block("SINI", TRUE);

    /* 
     *   write the size of our header (including the size prefix); this
     *   serves as a simple versioning flag so we can tell if fields added
     *   at a later date are part of a given image file's data or not (if
     *   the header is too small to contain them, they're not present) 
     */
    fp_->write_uint4(12);

    /* write the starting static code segment offset */
    fp_->write_uint4(static_cs_ofs);

    /* write the initializer count */
    fp_->write_uint4(init_cnt);
}

/*
 *   end an SINI block 
 */
void CVmImageWriter::end_sini_block()
{
    /* end the block using the generic mechanism */
    end_block();
}

