/* $Header: d:/cvsroot/tads/tads3/vmimage.h,v 1.3 1999/07/11 00:46:59 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmimage.h - VM image file loader
Function
  Loads an image file for execution.

  The VM loads and executes an image by creating an appropriate
  concrete CVmImageFile subclass, then creating a CVmImageLoader using
  the CVmImageFile object.  CVmImageLoader parses the input file, sets
  up page mappings for constant pools, and initializes objects found in
  the file.

  The lifespan of the CVmImageLoader object is the lifespan of the
  loaded image.  As long as the VM is executing the image, it must keep
  the CVmImageLoader object in existence.  The CVmImageLoader object
  can be deleted after the VM terminates execution of the image.

  The loader comes in two varieties: an external file loader, and a
  memory-mapped file loader.  The external file loader reads data from
  a disk file, allocating memory for the information read from the file.
  The memory-mapped file loader takes a chunk of memory that's already
  been loaded, and initializes the VM to use the pre-allocated memory,
  rather than allocating a separate copy of the image data.

  The memory-mapped loader is meant for systems with no external storage,
  such as hand-held devices.  It can also be used on systems with large,
  flat address spaces to speed up loading by isolating all disk access
  into a single bulk load of the image into memory.

  The external file loader is useful for systems with smaller address
  spaces, and can be used with a swapping pool implementation to allow
  the VM to operate when available memory is smaller than the image
  file.
Notes
  
Modified
  12/12/98 MJRoberts  - Creation
*/

#ifndef VMIMAGE_H
#define VMIMAGE_H

#include <memory.h>

#include "vmpool.h"
#include "vmglob.h"
#include "vmtype.h"
#include "vmfile.h"


/* ------------------------------------------------------------------------ */
/*
 *   Image file constants 
 */

/* 
 *   signature - this is at the beginning of every image file so that we
 *   can easily detect a completely invalid file 
 */
#define VMIMAGE_SIG "T3-image\015\012\032"


/* ------------------------------------------------------------------------ */
/*
 *   Data Block Flags 
 */

/* 
 *   Mandatory: this block must be recognized and loaded.  When new types
 *   of blocks are added in future versions, the compiler can use this
 *   flag to indicate whether or not an old VM can safely ignore the new
 *   block type.  An old VM will not be able to load a new block, since
 *   that new block won't have been part of the spec the VM was designed
 *   for; in some cases, an image file can be successfully loaded and used
 *   even when a particular block is ignored (possibly with the loss of
 *   the new feature enabled by the data in the block).  When a block can
 *   be ignored by an old VM without losing the ability to correctly load
 *   the image for the old VM's functionality, the mandatory flag will be
 *   set to 0; when a new block carries information without which the
 *   image cannot be properly loaded, the mandatory flag will be set to 1. 
 */
#define VMIMAGE_DBF_MANDATORY    0x0001


/* ------------------------------------------------------------------------ */
/*
 *   Constant pool image tracking structure.  For each constant pool, we
 *   maintain information on the locations in the image file of the pages
 *   in the pool.  
 */

/* number of entries in each subarray */
const size_t VMIMAGE_POOL_SUBARRAY_SIZE = 4096;

/* page information structure */
struct CVmImagePool_pg
{
    /* seek offset of the page */
    long seek_pos;

    /* size of the page */
    size_t page_size;

    /* XOR mask for the page */
    uchar xor_mask;
};

class CVmImagePool: public CVmPoolBackingStore
{
public:
    CVmImagePool();
    ~CVmImagePool();

    /* 
     *   Initialize the pool with a given number of pages and page size.
     *   This can only be called once, and must be called before any page
     *   locations can be established. 
     */
    void init(class CVmImageFile *fp, ulong page_count, ulong page_size);

    /* set a page's information */
    void set_page_info(ulong page_idx, long seek_pos, size_t page_size,
                       uchar xor_mask);

    /* apply an XOR mask to a block of bytes */
    static void apply_xor_mask(char *p, size_t len, uchar xor_mask)
    {
        /* 
         *   apply the mask only if it's non-zero - xor'ing zero with
         *   anything yields the original value, so we can avoid a lot of
         *   pointless memory traversal by checking this first 
         */
        if (xor_mask != 0)
        {
            /* xor each byte with the xor mask */
            for ( ; len != 0 ; --len, ++p)
                *p ^= xor_mask;
        }
    }

    /* -------------------------------------------------------------------- */
    /*
     *   CVmPoolBackingStore implementation 
     */

    /* get the total number of pages */
    size_t vmpbs_get_page_count() { return page_count_; }

    /* get the common page size */
    size_t vmpbs_get_common_page_size() { return page_size_; }

    /* get the size of a given page */
    size_t vmpbs_get_page_size(pool_ofs_t ofs, size_t page_size)
    {
        return get_page_info_ofs(ofs)->page_size;
    }

    /* allocate and load a given page */
    const char *vmpbs_alloc_and_load_page(pool_ofs_t ofs, size_t page_size,
                                          size_t load_size);

    /* free a page */
    void vmpbs_free_page(const char *mem, pool_ofs_t ofs, size_t page_size);

    /* load a page into a given memory block */
    void vmpbs_load_page(pool_ofs_t ofs, size_t page_size,
                         size_t load_size, char *mem);

    /* determine if the backing store pages are writable */
    int vmpbs_is_writable();
    
    
    /* -------------------------------------------------------------------- */

private:
    /* compute the number of subarray pages we have */
    size_t get_subarray_count() const
    {
        return (size_t)((page_count_ + VMIMAGE_POOL_SUBARRAY_SIZE - 1)
                        / VMIMAGE_POOL_SUBARRAY_SIZE);
    }

    /* given a page index, get the information structure at the index */
    CVmImagePool_pg *get_page_info(ulong idx) const
    {
        return &(page_info_[idx / VMIMAGE_POOL_SUBARRAY_SIZE]
                 [idx % VMIMAGE_POOL_SUBARRAY_SIZE]);
    }

    /* given a pool offset, get the information structure for the page */
    CVmImagePool_pg *get_page_info_ofs(pool_ofs_t ofs) const
        { return get_page_info(ofs / page_size_); }

    /* 
     *   Given a pool offset, seek to the image file data for the page, in
     *   preparation for loading the data from the image file into memory. 
     */
    void seek_page_ofs(pool_ofs_t ofs);
    
    /* number of pages in the pool */
    ulong page_count_;

    /* page size - each page in the pool has a common size */
    ulong page_size_;

    /* 
     *   Page seek array.  To accommodate 16-bit platforms, we keep this as a
     *   set of arrays, with each subarray smaller than 64k.  
     */
    CVmImagePool_pg **page_info_;

    /* underlying image file */
    class CVmImageFile *fp_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Image loader.  This takes an image file interface object (see below),
 *   and loads the underlying image data into memory.  
 */
class CVmImageLoader
{
public:
    /* initialize with an image file interface */
    CVmImageLoader(class CVmImageFile *fp, const char *fname, long base_ofs);

    /* destruction */
    ~CVmImageLoader();

    /* 
     *   Load the image.
     */
    void load(VMG0_);

    /* 
     *   Load a resource-only image file.  'fileno' is the file number
     *   assigned by the host application (via the add_resfile()
     *   interface) to the resource file. 
     */
    void load_resource_file(class CVmImageLoaderMres *res_ifc);

    /* load resources from a file handle at the current seek location */
    static void load_resources_from_fp(osfildef *fp, const char *fname,
                                       class CVmHostIfc *hostifc);

    /* load resources from a file handle at the current seek location */
    static void load_resources_from_fp(osfildef *fp, const char *fname,
                                       class CVmImageLoaderMres *res_ifc);

    /* 
     *   Run the image.  This transfers control to the entrypoint defined in
     *   the image file.  This function doesn't return until the program
     *   defined in the image terminates its execution by returning from the
     *   entrypoint function or throwing an unhandled exception.
     *   
     *   If 'saved_state' is not null, it gives a null-terminated character
     *   string with the name of a saved state file to be restored
     *   immediately.  We'll pass this information to the program's
     *   entrypoint so it can handle the restore appropriately.
     *   
     *   The caller must create the code and constant pools before invoking
     *   this.  We'll set up the pools with their backing stores as loaded
     *   from the image file.
     *   
     *   The global_symtab argument optionally provides the global symbol
     *   table; if this is null, we'll use our own global symbol table that
     *   we loaded from the debug records, if we found any.
     *   
     *   If an unhandled exception is thrown, this function throws
     *   VMERR_UNHANDLED_EXC, with the exception object as the first
     *   parameter.  
     */
    void run(VMG_ const char *const *argv, int argc,
             class CVmRuntimeSymbols *global_symtab,
             class CVmRuntimeSymbols *macro_symtab,
             const char *saved_state);

    /* run all static initializers */
    void run_static_init(VMG0_);

    /*
     *   Unload the image.  This should be called after execution is
     *   finished to disactivate the pools, which must be done before the
     *   image file is deleted.  
     */
    void unload(VMG0_);

    /* 
     *   Create a global LookupTable to hold the symbols in the global
     *   symbol table.
     */
    void create_global_symtab_lookup_table(VMG0_);

    /* create a global LookupTable to hold the symbols in the macro table */
    void create_macro_symtab_lookup_table(VMG0_);

    /* determine if the given block type identifiers match */
    static int block_type_is(const char *type1, const char *type2)
    {
        /* compare the four-byte identifiers to see if they're identical */
        return (memcmp(type1, type2, 4) == 0);
    }

    /* 
     *   Get the image file's timestamp.  This is a 24-byte array in the
     *   format "Sun Aug 01 17:05:20 1999".  The purpose of the image file
     *   timestamp is to provide a reasonably unique identifier that can
     *   be stored in a saved state file and then checked upon loading the
     *   file to ensure that it was created by the identical version of
     *   the image file.  
     */
    const char *get_timestamp() const { return &timestamp_[0]; }

    /* get the filename of the loaded image */
    const char *get_filename() const { return fname_; }

    /* get the fully-qualified, absolute directory path of the loaded image */
    const char *get_path() const { return path_; }

    /* 
     *   check to see if the image file has a global symbol table (GSYM
     *   block) 
     */
    int has_gsym() const { return has_gsym_ != 0; }

    /* 
     *   get the object ID of the LookupTable with the global symbol table
     *   for reflection purposes 
     */
    vm_obj_id_t get_reflection_symtab() const { return reflection_symtab_; }

    /* get the object ID of the LookupTable with the macro table */
    vm_obj_id_t get_reflection_macros() const { return reflection_macros_; }

    /*
     *   perform dynamic linking after loading, resetting, or restoring 
     */
    void do_dynamic_link(VMG0_);

    /* 
     *   delete all synthesized exports - this must be called just prior to
     *   resetting to image file state or loading a saved state 
     */
    void discard_synth_exports();

    /* allocate a new property ID */
    vm_prop_id_t alloc_new_prop(VMG0_);

    /* get the last property ID currently in use */
    vm_prop_id_t get_last_prop(VMG0_);

    /* save/restore the synthesized export table */
    void save_synth_exports(VMG_ class CVmFile *fp);
    int restore_synth_exports(VMG_ class CVmFile *fp,
                              class CVmObjFixup *fixups);

    /* get the starting offset of static initializers in the code pool */
    ulong get_static_cs_ofs() const { return static_cs_ofs_; }

    /* get the entrypoint function's code pool offset */
    uint32_t get_entrypt() const { return entrypt_; }

private:
    /* load external resource files associated with an image file */
    void load_ext_resfiles(VMG0_);

    /* read and verify an image file header */
    void read_image_header();
    
    /* load an Entrypoint block */
    void load_entrypt(VMG_ ulong siz);

    /* load a Static Object (OBJS) block */
    void load_static_objs(VMG_ ulong siz);

    /* load a Constant Pool Definition (CPDF) block */
    void load_const_pool_def(ulong siz);

    /* load a Constant Pool Page (CPPG) block */
    void load_const_pool_page(ulong siz);

    /* load a Multimedia Resource (MRES) block */
    void load_mres(ulong siz, class CVmImageLoaderMres *res_ifc);

    /* load a Multimedia Resource Link (MREL) block */
    void load_mres_link(ulong size, class CVmImageLoaderMres *res_ifc);

    /* load a Metaclass Dependency block */
    void load_meta_dep(VMG_ ulong siz);

    /* load a Function Set Dependency block */
    void load_funcset_dep(VMG_ ulong siz);

    /* load a Symbolic Names block */
    void load_sym_names(VMG_ ulong siz);

    /* load a Source Filenames block */
    void load_srcfiles(VMG_ ulong siz);

    /* load a Global Symbols block */
    void load_gsym(VMG_ ulong siz);

    /* load a Global Symbols (GSYM) block into the runtime symbol table */
    void load_runtime_symtab_from_gsym(VMG_ ulong siz);

    /* load a Macro Symbols (MACR) block into the runtime symbol table */
    void load_runtime_symtab_from_macr(VMG_ ulong siz);

    /* load a Macro Symbols (MACR) block */
    void load_macros(VMG_ ulong siz);

    /* load a Method Header List block */
    void load_mhls(VMG_ ulong siz);

    /* load a Static Initializer List block */
    void load_sini(VMG_ ulong siz);

    /* 
     *   Fix up the debugging global symbol table's object entries with the
     *   correct metaclass IDs.  This has to wait until the whole image file
     *   is loaded so that we're sure we have all of the objects loaded
     *   already. 
     */
    void fix_gsym_meta(VMG0_);

    /* 
     *   Copy data from the file into a buffer, decrementing a size
     *   counter.  We'll throw a BLOCK_TOO_SMALL error if the read length
     *   exceeds the remaining size. 
     */
    void read_data(char *buf, size_t read_len, ulong *remaining_size);

    /* skip data */
    void skip_data(size_t skipo_len, ulong *remaining_size);

    /* 
     *   Allocate memory for data and read the data from the file,
     *   decrementing the amount read from a size counter.  Throws
     *   BLOCK_TOO_SMALL if the read length exceeds the remaining size. 
     */
    const char *alloc_and_read(size_t read_len, ulong *remaining_size);

    /* add a resource to our resource table */
    void add_resource(class CVmResource *res);

    /*
     *   Add a symbol to the list of synthesized exports.  Each time we
     *   synthesize a value because we didn't find the associated symbol
     *   exported from the image file, we must add an entry to this table.
     *   On saving state, we'll save these symbols to the saved state file
     *   so they will be restored on load. 
     */
    void add_synth_export_obj(const char *nm, vm_obj_id_t val);
    void add_synth_export_prop(const char *nm, vm_prop_id_t val);

    /* set the last property ID value */
    void set_last_prop(VMG_ vm_prop_id_t last_prop);

    /* callback for synthesized export enumeration: save to file */
    static void save_synth_export_cb(void *ctx, class CVmHashEntry *entry);

    /* underlying image file interface */
    class CVmImageFile *fp_;

    /* image filename */
    char *fname_;

    /* 
     *   fully-qualified, absolute directory path to the file (this is just
     *   the directory path, sans the filename) 
     */
    char *path_;

    /* the base seek offset of the image stream within the image file */
    long base_seek_ofs_;

    /* image file version number */
    uint ver_;

    /* image file timestamp */
    char timestamp_[24];

    /* code pool offset of entrypoint function */
    uint32_t entrypt_;

    /* pool tracking objects */
    class CVmImagePool *pools_[2];

    /* 
     *   The image's exported symbols.  These are the symbols that the
     *   program explicitly exported for dynamic linking from the VM. 
     */
    class CVmHashTable *exports_;

    /*
     *   List of exports synthesized after loading by the VM.  These exports
     *   are not in the image file, so they must be saved in the saved state
     *   file so that we can reattach to the same objects and properties on
     *   restore.  
     */
    class CVmHashTable *synth_exports_;

    /*
     *   The runtime global symbol table, if we have one.  We'll build this
     *   from the debug records if we find any, or from the records passed
     *   in from the compiler when we run preinitialization.
     *   
     *   Note that the runtime global symbols are not the same as the
     *   exported symbols.  The exports are the symbols explicitly exported
     *   for dynamic linking, so that the VM can attach to particular
     *   objects defined in the image file.  The runtime globals are all of
     *   the compile-time global symbols as reflected in the debugging
     *   records, and are used for reflection-type operations.  
     */
    class CVmRuntimeSymbols *runtime_symtab_;

    /* 
     *   The runtime macro definitions table, if we have one.  As with the
     *   runtime global symbol table, we build this from the debug records,
     *   or from the records passed in from the compiler during preinit. 
     */
    class CVmRuntimeSymbols *runtime_macros_;

    /* object ID of LookupTable containing the global symbol table */
    vm_obj_id_t reflection_symtab_;

    /* object ID of LookupTable containing the macro symbol table */
    vm_obj_id_t reflection_macros_;

    /* head/tail of list of static initializer pages */
    class CVmStaticInitPage *static_head_;
    class CVmStaticInitPage *static_tail_;

    /* 
     *   starting offset in code pool of static initializer code - the
     *   compiler groups all static initializer code, and only static
     *   initializer code, above this point, so after preinit, we can omit
     *   all code above this point from the rewritten image file 
     */
    ulong static_cs_ofs_;

    /* flag: metaclass dependency table loaded */
    uint loaded_meta_dep_ : 1;

    /* flag: function set dependency table loaded */
    uint loaded_funcset_dep_ : 1;

    /* flag: entrypoint loaded */
    uint loaded_entrypt_ : 1;

    /* 
     *   flag: the image file has a GSYM (global symbol table), which
     *   implies that it was compiled for debugging 
     */
    uint has_gsym_ : 1;
};


/* ------------------------------------------------------------------------ */
/*
 *   Static initializer page.  Each page contains a fixed number of
 *   initializers. 
 */
const size_t VM_STATIC_INIT_PAGE_MAX = 1000;
class CVmStaticInitPage
{
public:
    CVmStaticInitPage(size_t cnt)
    {
        /* remember the number of records */
        cnt_ = cnt;

        /* no data yet */
        data_ = 0;

        /* not in a list yet */
        nxt_ = 0;
    }

    /* get an object/property ID given the index of a record */
    vm_obj_id_t get_obj_id(size_t idx)
        { return vmb_get_objid(get_rec(idx)); }
    vm_prop_id_t get_prop_id(size_t idx)
        { return vmb_get_propid(get_rec(idx) + VMB_OBJECT_ID); }

    /* get a raw record pointer given an index */
    const char *get_rec(size_t idx) { return data_ + (idx * 6); }

    /* next page in list */
    CVmStaticInitPage *nxt_;

    /* number of records in the page */
    size_t cnt_;

    /* 
     *   The data of the page.  Each record is six bytes long, in portable
     *   format: a UINT4 for the object ID, and a UINT2 for the property
     *   ID. 
     */
    const char *data_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Image file resource loader interface.  This is an abstract class
 *   interface that must be provided to load_resource_file() to provide
 *   per-resource loading.  
 */
class CVmImageLoaderMres
{
public:
    virtual ~CVmImageLoaderMres() { }

    /* load a resource */
    virtual void add_resource(uint32_t seek_ofs, uint32_t siz,
                              const char *res_name, size_t res_name_len) = 0;

    /* load a resource link */
    virtual void add_resource(const char *fname, size_t fnamelen,
                              const char *res_name, size_t res_name_len) = 0;
};


/* ------------------------------------------------------------------------ */
/*
 *   Image file interface.  This is an abstract interface that provides
 *   access to the data in an image file independently of the location of
 *   the data.  
 */
class CVmImageFile
{
public:
    /* delete the loader */
    virtual ~CVmImageFile() { }

    /* duplicate the image file interface, a la stdio freopen() */
    virtual CVmImageFile *dup(const char *mode) = 0;
    
    /* 
     *   Copy data from the image file to the caller's buffer.  Reads from
     *   the current file position. 
     */
    virtual void copy_data(char *buf, size_t len) = 0;

    /* 
     *   Allocate memory for and load data from the image file.  Reads from
     *   the current file position.  Returns a pointer to the allocated data.
     *   
     *   'remaining_in_page' is the amount of space remaining in the current
     *   block being read from the image, including the space being read
     *   here; this can be used as an upper bound for a new allocation if
     *   the concrete subclass wishes to allocate blocks for suballocation.  
     */
    virtual const char *alloc_and_read(size_t len, uchar xor_mask,
                                       ulong remaining_in_page) = 0;

    /*
     *   Determine if memory read with alloc_and_read() is writable.
     *   Returns true if so, false if not.  If the memory that
     *   alloc_and_read() returns is a copy of the external file (rather
     *   than mapped to the original external data, as might be the case
     *   on a small machine without external storage, such as a palm-top
     *   computer), this should return true. 
     */
    virtual int allow_write_to_alloc() = 0;

    /* free memory previously allocated by alloc_and_read */
    virtual void free_mem(const char *mem) = 0;

    /* seek to a new file position, as an offset from the start of the file */
    virtual void seek(long pos) = 0;

    /* get the current seek position */
    virtual long get_seek() const = 0;

    /* skip the given number of bytes */
    virtual void skip_ahead(long len) = 0;
};


/*
 *   Implementation of the generic stream interface for an image file block.
 *   This will limit reading to the data in the block.  
 */
class CVmImageFileStream: public CVmStream
{
public:
    CVmImageFileStream(CVmImageFile *fp, size_t len)
    {
        /* 
         *   remember the underlying image file, and the amount of space in
         *   our data block 
         */
        fp_ = fp;
        len_ = len;
    }

    CVmStream *clone(VMG_ const char *mode)
    {
        /* duplicate our file handle */
        CVmImageFile *fpdup = fp_->dup(mode);
        if (fpdup == 0)
            return 0;

        /* create a new image file stream wrapper for the duplicate handle */
        return new CVmImageFileStream(fpdup, len_);
    }

    /* read bytes into a buffer */
    virtual void read_bytes(char *buf, size_t len);
    virtual size_t read_nbytes(char *buf, size_t len);

    /* read a line (not used for this object) */
    virtual char *read_line(char *buf, size_t len) { return 0; }

    /* write bytes */
    virtual void write_bytes(const char *, size_t);

    /* get/set the seek position */
    virtual long get_seek_pos() const { return fp_->get_seek(); }
    virtual void set_seek_pos(long pos) { fp_->seek(pos); }

    virtual long get_len() { return len_; }

private:
    /* our underlying image file reader */
    CVmImageFile *fp_;

    /* remaining data length in the underlying block */
    size_t len_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Image file interface - external disk file 
 */
class CVmImageFileExt: public CVmImageFile
{
public:
    /* delete the image file loader */
    ~CVmImageFileExt();
    
    /* initialize with an underlying file */
    CVmImageFileExt(class CVmFile *fp)
    {
        /* remember our file */
        fp_ = fp;

        /* we don't have any suballocation blocks yet */
        mem_head_ = mem_tail_ = 0;
    }

    /* duplicate the file interface */
    virtual CVmImageFile *dup(const char *mode)
    {
        /* duplicate our file handle */
        CVmFile *fpdup = fp_->dup(mode);
        if (fpdup == 0)
            return 0;

        /* return a new wrapper object for the duplicate handle */
        return new CVmImageFileExt(fpdup);
    }

    /* 
     *   CVmImageFile interface implementation 
     */

    /* copy data to the caller's buffer */
    void copy_data(char *buf, size_t len);

    /* allocate memory for and read data */
    const char *alloc_and_read(size_t len, uchar xor_mask,
                               ulong remaining_in_page);

    /* allow writing to alloc_and_read blocks */
    virtual int allow_write_to_alloc() { return TRUE; }

    /* 
     *   Free memory previously allocated with alloc_and_read.  We don't
     *   need to do anything here; once we allocate and load a block, we
     *   keep it in memory until the entire load image is deleted, at
     *   which time we free all of the associated memory.  
     */
    void free_mem(const char *) { }

    /* seek to a new file position */
    void seek(long pos);

    /* get the current seek position */
    long get_seek() const;

    /* skip the given number of bytes */
    void skip_ahead(long len);

private:
    /* allocate memory for loading data */
    char *alloc_mem(size_t siz, ulong remaining_in_page);
    
    /* the underlying file */
    class CVmFile *fp_;

    /* 
     *   Memory block list.  We keep a set of memory blocks for loading
     *   data via the alloc_and_read() method.  Rather than allocating an
     *   individual "malloc" block for each alloc_and_read() call, we
     *   allocate a large block, and suballocate memory out of the large
     *   block.  Since we don't know exactly how much memory we'll need in
     *   advance, we take a guess at how large a block we need,
     *   suballocate from the block until we fill it up, then allocate
     *   another block and fill it up, then allocate another, and so on.
     *   We keep all of the blocks we allocate in a linked list.
     */
    class CVmImageFileExt_blk *mem_head_;
    class CVmImageFileExt_blk *mem_tail_;
};

/* 
 *   aggregate allocation block size - use a size that should be reasonably
 *   safe for 16-bit platforms (not over 64k, and a bit less to allow for
 *   some malloc overhead) 
 */
const size_t VMIMAGE_EXT_BLK_SIZE = 65000;

/*
 *   Memory tracking structure for external file reader.  For each large
 *   memory block we allocate (for suballocation), we allocate one of
 *   these structures.  
 */
class CVmImageFileExt_blk
{
public:
    /* create the block, allocating the given number of bytes for the block */
    CVmImageFileExt_blk(size_t siz);

    /* delete the block */
    ~CVmImageFileExt_blk();
    
    /* suballocate memory out of the current block */
    char *suballoc(size_t siz);

    /* next block in the list */
    CVmImageFileExt_blk *nxt_;

    /* previous block in the list */
    CVmImageFileExt_blk *prv_;

    /* pointer to the start of the block */
    char *block_ptr_;

    /* number of bytes remaining in the block for future suballocations */
    size_t rem_;

    /* pointer to next free byte of the block */
    char *free_ptr_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Image file interface - memory-mapped implementation.  This
 *   implementation assumes that the file is loaded into memory in a
 *   contiguous chunk, which can be addressed linearly.  
 */
class CVmImageFileMem: public CVmImageFile
{
public:
    ~CVmImageFileMem() { }
    
    /* initialize with an underlying block of pre-loaded data */
    CVmImageFileMem(const char *mem, long len)
    {
        /* remember where our data are */
        mem_ = mem;
        len_ = len;

        /* start at the beginning of the data */
        pos_ = 0;
    }

    /* duplicate the file interface */
    CVmImageFile *dup(const char *mode)
    {
        return new CVmImageFileMem(mem_, len_);
    }

    /* 
     *   CVmImageFile interface implementation 
     */

    /* copy data to the caller's buffer */
    void copy_data(char *buf, size_t len);

    /* allocate memory for and read data */
    const char *alloc_and_read(size_t len, uchar xor_mask,
                               ulong remaining_in_page);

    /* 
     *   do not allow writing to alloc_and_read blocks, since we map these
     *   blocks directly to the underlying in-memory data 
     */
    virtual int allow_write_to_alloc() { return FALSE; }

    /* 
     *   Free memory allocated by alloc_and_read.  Since our underlying
     *   file is entirely in memory to start with, we don't actually ever
     *   allocate any memory; hence, we don't actually need to free any
     *   memory here.
     */
    void free_mem(const char *) { }

    /* seek to a new file position */
    void seek(long pos) { pos_ = pos; }

    /* get the current seek position */
    long get_seek() const { return pos_; }
    
    /* skip the given number of bytes */
    void skip_ahead(long len) { pos_ += len; }

private:
    /* the underlying memory block */
    const char *mem_;

    /* size in bytes of the underlying memory block */
    ulong len_;

    /* current offset within the memory block */
    long pos_;
};


#endif /* VMIMAGE_H */

