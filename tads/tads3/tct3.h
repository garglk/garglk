/* $Header: d:/cvsroot/tads/tads3/TCT3.H,v 1.5 1999/07/11 00:46:55 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tct3.h - TADS 3 compiler - T3 Virtual Machine Code Generator
Function
  
Notes
  
Modified
  04/30/99 MJRoberts  - Creation
*/

#ifndef TCT3_H
#define TCT3_H

#include "t3std.h"
#include "tcprs.h"
#include "vmop.h"
#include "vmtype.h"
#include "tct3ty.h"

/* ------------------------------------------------------------------------ */
/*
 *   include the T3-specific CVmRuntimeSymbols class definition 
 */
#include "vmrunsym.h"


/* ------------------------------------------------------------------------ */
/* 
 *   include the T3-specific final parse node classes 
 */
#include "tct3drv.h"


/* ------------------------------------------------------------------------ */
/*
 *   Define some internal compiler datatypes - any type
 *   VM_FIRST_INVALID_TYPE or higher is not used by the VM and can thus be
 *   used for our own internal types. 
 */
const vm_datatype_t VM_VOCAB_LIST = VM_MAKE_INTERNAL_TYPE(0);


/* ------------------------------------------------------------------------ */
/*
 *   Data structure sizes.  These are the sizes of various data structures
 *   that we write to the image file; these values are global to the
 *   entire image file, and are constants of the current file format.  
 */

/* method header size */
#define TCT3_METHOD_HDR_SIZE   10

/* exception table entry size */
#define TCT3_EXC_ENTRY_SIZE    10

/* debugger line record entry size */
#define TCT3_LINE_ENTRY_SIZE   10

/* debug table header size */
#define TCT3_DBG_HDR_SIZE      0

/* debugger local symbol record size */
#define TCT3_DBG_LCLSYM_HDR_SIZE  6

/* debugger frame record header size */
#define TCT3_DBG_FRAME_SIZE    8

/* debugger record format version */
#define TCT3_DBG_FMT_VSN       2

/* ------------------------------------------------------------------------ */
/*
 *   Object file header flags.
 */
#define TCT3_OBJHDR_DEBUG   0x0001


/* ------------------------------------------------------------------------ */
/*
 *   Object Stream prefix flags.  Each object we write to the object
 *   stream starts with a prefix byte that we use to store some extra flag
 *   information about the object.  
 */

/* object has been replaced - do not write to image file */
#define TCT3_OBJ_REPLACED  0x0001

/* object has been modified */
#define TCT3_OBJ_MODIFIED  0x0002

/* object is transient */
#define TCT3_OBJ_TRANSIENT 0x0004


/* ------------------------------------------------------------------------ */
/*
 *   T3 metaclass object stream header sizes 
 */

/* 
 *   internal header size - this is an extra header the compiler adds for
 *   each object in an object stream
 *   
 *   UINT2 compiler_flags
 */
const size_t TCT3_OBJ_INTERNHDR_SIZE = 2;

/* offset of the internal header from the start of the object data */
const size_t TCT3_OBJ_INTERNHDR_OFS = 0;

/* offset of internal header flags */
const size_t TCT3_OBJ_INTERNHDR_FLAGS_OFS = TCT3_OBJ_INTERNHDR_OFS;

/* 
 *   T3 generic metaclass header - this is the header on every metaclass
 *   in a T3 image file 'OBJS' block.
 *   
 *   UINT4 object_id
 *.  UINT2 metaclass_specific_byte_count
 */
const size_t TCT3_META_HEADER_SIZE = 6;

/* 
 *   large metaclass header size - this is the same as the standard
 *   header, but uses a 32-bit size field rather than the 16-bit size
 *   field 
 */
const size_t TCT3_LARGE_META_HEADER_SIZE = 8;

/* offset of generic metaclass header from the start of an object's data */
const size_t TCT3_META_HEADER_OFS =
    TCT3_OBJ_INTERNHDR_OFS + TCT3_OBJ_INTERNHDR_SIZE;

/* ------------------------------------------------------------------------ */
/*
 *   tads-object metaclass object stream header sizes
 */

/* 
 *   tads-object header - each object with metaclass tads-object defines
 *   this header
 *   
 *.  UINT2 superclass_count
 *.  UINT2 property_count
 *.  UINT2 object_flags 
 */
const size_t TCT3_TADSOBJ_HEADER_SIZE = 6;

/* offset of the tads-object header from the start of an object's data */
const size_t TCT3_TADSOBJ_HEADER_OFS =
    TCT3_META_HEADER_OFS + TCT3_META_HEADER_SIZE;

/* 
 *   offset to the superclass table, which immediately follows the
 *   tads-object header 
 */
const size_t TCT3_TADSOBJ_SC_OFS =
    TCT3_TADSOBJ_HEADER_OFS + TCT3_TADSOBJ_HEADER_SIZE;

/* 
 *   size of a property table entry
 *   
 *.  UINT2 property_ID
 *.  DATAHOLDER value 
 */
const size_t TCT3_TADSOBJ_PROP_SIZE = 2 + VMB_DATAHOLDER;

/*
 *   T3 object flags 
 */

/* class flag - object is a class, not an instance */
#define TCT3_OBJFLG_CLASS 0x0001


/* ------------------------------------------------------------------------ */
/*
 *   Metaclass List Entry.  This list keeps track of the metaclasses that
 *   the image file is dependent upon, and the dynamic link mapping
 *   between metaclass ID in the image file and the universally unique
 *   metaclass name.  
 */
struct tc_meta_entry
{
    /* next entry in the list */
    tc_meta_entry *nxt;

    /* 
     *   metaclass symbol object, if present - we get the property list
     *   from the metaclass symbol 
     */
    class CTcSymMetaclass *sym;

    /* external (universally unique) metaclass name */
    char nm[1];
};

/*
 *   Fixed System Metaclasses.  The compiler must generate code for these
 *   metaclasses directly, so it pre-loads the metaclass dependency table
 *   with these metaclasses at initialization.  Because these entries are
 *   always loaded into the table in the same order, they have fixed table
 *   indices that we can define as constants here.
 */

/* TADS Object Metaclass */
const int TCT3_METAID_TADSOBJ = 0;

/* list metaclass */
const int TCT3_METAID_LIST = 1;

/* dictionary metaclass */
const int TCT3_METAID_DICT = 2;

/* grammar production metaclass */
const int TCT3_METAID_GRAMPROD = 3;

/* vector metaclass */
const int TCT3_METAID_VECTOR = 4;

/* anonymous function pointer */
const int TCT3_METAID_ANONFN = 5;

/* intrinsic class modifiers */
const int TCT3_METAID_ICMOD = 6;

/* lookup table */
const int TCT3_METAID_LOOKUP_TABLE = 7;

/*
 *   IMPORTANT!!!  When adding new entries to this list of pre-defined
 *   metaclasses, you must:
 *   
 *   - update the 'last' constant below
 *   
 *   - add the new entry to CTcGenTarg::load_image_file_meta_table()
 */

/* last metaclass ID - adjust when new entries are added */
const int TCT3_METAID_LAST = 7;

/* ------------------------------------------------------------------------ */
/*
 *   Function set dependency list entry 
 */
struct tc_fnset_entry
{
    /* next entry in the list */
    tc_fnset_entry *nxt;

    /* external (universally unique) function set name */
    char nm[1];
};

/* ------------------------------------------------------------------------ */
/*
 *   Exception Table builder.  This object keeps track of the entries in
 *   an exception table under construction, so that the exception table
 *   for a function can be written to the code stream after all of the
 *   code in the function has been generated.  
 */
class CTcT3ExcTable
{
public:
    CTcT3ExcTable();
    
    ~CTcT3ExcTable()
    {
        /* if we've allocated a table, delete it */
        if (table_ != 0)
            t3free(table_);
    }

    /* 
     *   Set the current function or method's start offset.  The code
     *   generator for the function body should set this to the code
     *   stream offset of the start of the method header; this allows us
     *   to calculate the offsets of protected code and 'catch' blocks.
     *   
     *   Important: this is the code stream offset (G_cs->get_ofs()), not
     *   the final code pool address.  We need only relative offsets, so
     *   the code stream offset suffices (and is available much earlier in
     *   the code generation process).  
     */
    void set_method_ofs(ulong ofs) { method_ofs_ = ofs; }

    /* 
     *   Add a 'catch' entry.  The offsets are all code stream offsets
     *   (G_cs->get_ofs() values). 
     */
    void add_catch(ulong protected_start_ofs, ulong protected_end_ofs,
                   ulong exc_obj_id, ulong catch_block_ofs);

    /* 
     *   write our exception table to the code stream - writes to the G_cs
     *   global code stream object 
     */
    void write_to_code_stream();

    /* get the number of entries */
    size_t get_entry_count() const { return exc_used_; }

    /* clear the exception table - remove all entries */
    void clear_table() { exc_used_ = 0; }

protected:
    /* the starting offset of this method header */
    ulong method_ofs_;

    /* exception table */
    struct CTcT3ExcEntry *table_;

    /* number of entries used/allocated in our table */
    size_t exc_used_;
    size_t exc_alloced_;
};

/*
 *   Exception table entry 
 */
struct CTcT3ExcEntry
{
    /* start/end offset (from start of method header) of protected code */
    ulong start_ofs;
    ulong end_ofs;

    /* object ID of exception class caught */
    ulong exc_obj_id;

    /* 'catch' block starting offset (from start of method header) */
    ulong catch_ofs;
};

/* ------------------------------------------------------------------------ */
/*
 *   Data Stream Page Layout Manager.  This works with a CTcDataStream
 *   object (such as the constant pool or the code pool) to divide the
 *   stream into pages for the image file.  
 */
class CTcStreamLayout
{
public:
    CTcStreamLayout()
    {
        /* we don't know anything about our layout yet */
        page_size_ = 0;
        page_cnt_ = 0;
    }
    
    /* 
     *   Calculate my layout, given the maximum object size.  This can be
     *   called once the entire stream has been generated, hence the size
     *   of the largest indivisible item in the stream is known.  This
     *   will apply all fixups throughout the stream.
     *   
     *   If this is the first stream for this layout, is_first is true.
     *   If we're adding more pages, is_first is false, and max_len is
     *   ignored (so the caller must ensure that the max_len provided on
     *   laying out the first stream for this page set is adequate for all
     *   streams added to this layout).  
     */
    void calc_layout(class CTcDataStream *ds, ulong max_len, int is_first);

    /*
     *   Write the stream(s) to an image file.  We'll write the pool
     *   definition block and the pool pages.  This cannot be called until
     *   after calc_layout() has been called for all streams, because we
     *   must apply all fixups throughout the entire image before we can
     *   write out anything.  
     */
    void write_to_image(class CTcDataStream **ds_array, size_t ds_cnt,
                        class CVmImageWriter *image_writer, int pool_id,
                        uchar xor_mask);

    /* page size */
    ulong page_size_;

    /* number of pages used */
    size_t page_cnt_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Debug line list page.  We keep a linked list of these pages, and
 *   allocate new entries out of the last page.  We keep going until the
 *   last page is filled up, then allocate a new page.  
 */
const size_t TCT3_DEBUG_LINE_PAGE_SIZE = 1024;
const size_t TCT3_DEBUG_LINE_REC_SIZE = 5;
struct tct3_debug_line_page
{
    /* next page in list */
    tct3_debug_line_page *nxt;

    /* 
     *   Entries on this page (each entry is a debug line record offset in
     *   the code stream).  Each entry consists of one byte for the code
     *   stream identifier (TCGEN_xxx_STREAM) and four bytes for a
     *   portable UINT4 with the offset in the stream.  
     */
    uchar line_ofs[TCT3_DEBUG_LINE_PAGE_SIZE * TCT3_DEBUG_LINE_REC_SIZE];
};


/* ------------------------------------------------------------------------ */
/*
 *   T3-specific code generator helper class.  This class provides a set
 *   of static functions that are useful for T3 code generation.  
 */
class CTcGenTarg
{
public:
    /* initialize the code generator */
    CTcGenTarg();

    /* destroy the code generator object */
    ~CTcGenTarg();

    /*
     *   Set the run-time metaclass dependency table index for a given
     *   metaclass, identified by 'name' (a string of length 'len').  'idx'
     *   is the run-time metaclass dependency index.
     *   
     *   When we're operating as part of an interactive debugger, the image
     *   loader must call this for each entry in the metaclass dependency
     *   table loaded from the image file.  This allows us to fix up our
     *   internal notion of the metaclass indices so that we generate code
     *   compatible with the actual loaded image file.
     *   
     *   The protocol is as follows: call start_image_file_meta_table(), then
     *   call load_image_file_meta_table() on each entry in the table, then
     *   call end_image_file_meta_table().  
     */
    void start_image_file_meta_table();
    void load_image_file_meta_table(const char *nm, size_t len, int idx);
    void end_image_file_meta_table();

    /*
     *   Allocate a new global property ID. 
     */
    tctarg_prop_id_t new_prop_id() { return next_prop_++; }

    /*
     *   Allocate a new global object ID. 
     */
    tctarg_obj_id_t new_obj_id() { return next_obj_++; }

    /* 
     *   add a metaclass to the dependency table - returns the index of
     *   the metaclass in the table 
     */
    int add_meta(const char *meta_extern_name, size_t len,
                 class CTcSymMetaclass *sym);
    int add_meta(const char *nm, class CTcSymMetaclass *sym)
        { return add_meta(nm, strlen(nm), sym); }
    int add_meta(const char *nm)
        { return add_meta(nm, strlen(nm), 0); }

    /* 
     *   Find a metaclass entry, adding it if it's not already there.  If
     *   the metaclass is already defined, and it has an associated
     *   symbol, we will not change the associated symbol - this will let
     *   the caller detect that the metaclass has been previously defined
     *   for a different symbol, which is usually an error. 
     */
    int find_or_add_meta(const char *nm, size_t len,
                         class CTcSymMetaclass *sym);

    /* 
     *   Get the property ID for the given method table index in the given
     *   metaclass.  The metaclass ID is given as the internal metaclass ID
     *   (without the "/version" suffix), not as the external class name.
     *   Returns a parse node for the property, or null if it's not found.  
     */
    CTcPrsNode *get_metaclass_prop(const char *name, ushort idx) const;

    /* get a metaclass symbol by the metaclass's global identifier */
    class CTcSymMetaclass *find_meta_sym(const char *nm, size_t len);

    /* 
     *   Find the metaclass table entry for a given global identifier.  If
     *   update_vsn is true, we'll update the entry stored in the table to
     *   the given version number if the given name's version number is
     *   higher than the one in the table.  If we find an entry, we'll
     *   fill in *entry_idx with the entry's index.  
     */
    tc_meta_entry *find_meta_entry(const char *nm, size_t len,
                                   int update_vsn, int *entry_idx);

    /* get/set the symbol for a given metaclass */
    class CTcSymMetaclass *get_meta_sym(int meta_idx);
    void set_meta_sym(int meta_idx, class CTcSymMetaclass *sym);

    /* get the number of metaclasses */
    int get_meta_cnt() const { return meta_cnt_; }

    /* get the external (universally unique) name for the given metaclass */
    const char *get_meta_name(int idx) const;

    /*
     *   Get the dependency table index for the given pre-defined metaclass,
     *   specified by the TCT3_METAID_xxx value. 
     */
    int get_predef_meta_idx(int id) const { return predef_meta_idx_[id]; }

    /*
     *   Add a function set to the dependency table - returns the index of
     *   the function set in the table 
     */
    ushort add_fnset(const char *fnset_extern_name, size_t len);
    ushort add_fnset(const char *fnset_extern_name)
        { return add_fnset(fnset_extern_name, strlen(fnset_extern_name)); }

    /* get the name of a function set given its index */
    const char *get_fnset_name(int idx) const;

    /* get the number of defined function sets */
    int get_fnset_cnt() const { return fnset_cnt_; }

    /*
     *   Notify the code generator that parsing is finished.  This should
     *   be called after parsing and before code generation begins.  
     */
    void parsing_done();

    /*
     *   Note a string value's length.  This should be invoked during the
     *   parsing phase for each constant string value.  We'll keep track
     *   of the largest constant data in the file, so that after parsing
     *   is finished, we'll know the minimum size we need for each
     *   constant pool page.  This doesn't actually allocate any space in
     *   the constant pool; this merely keeps track of the longest string
     *   we'll eventually need to store.  
     */
    void note_str(size_t len);

    /*
     *   Note number of elements in a constant list value.  This is the
     *   list equivalent of note_str().  
     */
    void note_list(size_t element_count);

    /*
     *   Note the length of a code block's byte code.  This should be
     *   invoked during code generation for each code block; we'll keep
     *   track of the longest byte code block, so that after code
     *   generation is complete, we'll know the minimum size we need for
     *   each code pool page.  
     */
    void note_bytecode(ulong len);

    /*
     *   Notify the code generator that we're replacing an object (via the
     *   "replace" statement) at the given stream offset.  We'll mark the
     *   data in the stream as deleted so that we don't write it to the
     *   image file.  
     */
    void notify_replace_object(ulong stream_ofs);

    /*
     *   Write to an object file.  The compiler calls this after all
     *   parsing and code generation are completed to write an object
     *   file, which can then be linked with other object files to create
     *   an image file.
     */
    void write_to_object_file(class CVmFile *object_fp,
                              class CTcMake *make_obj);

    /*
     *   Load an object file.  Returns zero on success, non-zero on error.
     */
    int load_object_file(CVmFile *fp, const textchar_t *fname);

    /*
     *   Write the image file.  The compiler calls this after all parsing
     *   and code generation are completed to write an image file.  We
     *   must apply all fixups, assign the code and constant pool layouts,
     *   and write the data to the image file.  
     */
    void write_to_image(class CVmFile *image_fp, uchar data_xor_mask,
                        const char tool_data[4]);

    /* generate synthesized code during linking */
    void build_synthesized_code();

    /* generate code for a dictionary object */
    void gen_code_for_dict(class CTcDictEntry *dict);

    /* generate code for a grammar production object */
    void gen_code_for_gramprod(class CTcGramProdEntry *prod);

    /* get the maximum string/list/bytecode lengths */
    size_t get_max_str_len() const { return max_str_len_; }
    size_t get_max_list_cnt() const { return max_list_cnt_; }
    size_t get_max_bytecode_len() const { return max_bytecode_len_; }

    /*
     *   Add a debug line record.  If we're in debug mode, this will clear
     *   the peephole optimizer to ensure that the line record doesn't get
     *   confused due to compression of opcodes.  
     */
    void add_line_rec(class CTcTokFileDesc *file, long linenum);

    /* write an opcode to the output stream */
    void write_op(uchar opc);

    /* write a CALLPROP instruction */
    void write_callprop(int argc, int varargs, vm_prop_id_t prop);

    /* 
     *   Determine if we can skip an opcode for peephole optimization.
     *   We'll look at the previous opcode to determine if this opcode is
     *   reachable, and we'll indicate that we should suppress the new
     *   opcode if not.  
     */
    int can_skip_op();

    /* 
     *   Add a string to the constant pool, and create a fixup for the
     *   item for a reference from the given stream at the given offset.  
     */
    void add_const_str(const char *str, size_t len,
                       class CTcDataStream *ds, ulong ofs);

    /* 
     *   Add a list to the constant pool, and create a fixup for the item
     *   for a reference from the given stream at the given offset.  
     */
    void add_const_list(class CTPNList *lst,
                        class CTcDataStream *ds, ulong ofs);

    /*
     *   Write a constant value (in the compiler's internal
     *   representation, a CTcConstVal structure) to a given buffer in T3
     *   image file DATA_HOLDER format.  Write at a given offset, or at
     *   the current write offset.  
     */
    void write_const_as_dh(class CTcDataStream *ds, ulong ofs,
                           const class CTcConstVal *src);
    void write_const_as_dh(class CTcDataStream *ds,
                           const class CTcConstVal *src);

    /*
     *   Clear the peephole optimizer state.  This must be invoked
     *   whenever a jump label is defined.  We can't combine an
     *   instruction at a jump destination with anything previous: the
     *   instruction at a jump destination must be generated as-is, rather
     *   than being combined with the preceding instruction, since someone
     *   could jump directly to it.  
     */
    void clear_peephole()
    {
        last_op_ = OPC_NOP;
        second_last_op_ = OPC_NOP;
    }

    /* get the last opcode we generated */
    uchar get_last_op() const { return last_op_; }

    /*
     *   Remove the last JMP instruction.  This is used when we detect
     *   that we just generated a JMP ahead to the very next instruction,
     *   in which case we can eliminate the JMP, since it has no effect. 
     */
    void remove_last_jmp();

    /*
     *   Stack depth counting.  While we're generating code for a code
     *   block (a function or method), we'll keep track of our stack push
     *   and pop operations, so that we can monitor the maximum stack
     *   depth.  In order for the stack depth to be calculable at compile
     *   time, the code generator must take care that each individual
     *   statement is stack-neutral (i.e, the stack comes out of each
     *   statement at the same depth as when it entered the statement), so
     *   that jumps, iterations, and other variables we can't analyze
     *   statically can be ignored.
     */

    /* 
     *   reset the stack depth counters - call this at the start
     *   generating of each code block 
     */
    void reset_sp_depth() { sp_depth_ = max_sp_depth_ = 0; }

    /* 
     *   get the maximum stack depth for the current function - use this
     *   when finished generating a code block to determine the maximum
     *   stack space needed by the code block 
     */
    int get_max_sp_depth() const { return max_sp_depth_; }

    /* get the current stack depth */
    int get_sp_depth() const { return sp_depth_; }

    /* record a push - increments the current stack depth */
    void note_push() { note_push(1); }

    /* record a push of a given number of stack elements */
    void note_push(int cnt)
    {
        sp_depth_ += cnt;
        if (sp_depth_ > max_sp_depth_)
            max_sp_depth_ = sp_depth_;
    }

    /* record a pop - decrements the current stack depth */
    void note_pop() { note_pop(1); }

    /* record a pop of a given number of stack elements */
    void note_pop(int cnt) { sp_depth_ -= cnt; }

    /* record a full stack reset back to function entry conditions */
    void note_rst() { sp_depth_ = 0; }

    /* 
     *   do post-call cleanup: generate a named argument table pointer if
     *   needed, remove the named arguments from the stack
     */
    void post_call_cleanup(const struct CTcNamedArgs *named_args);

    /*
     *   Open/close a method/function.  "Open" generates a placeholder method
     *   header and sets up our generator globals to prepare for a new
     *   method.  "Close" goes back and fills in the final method header
     *   based on the code generated since "Open".
     */
    void open_method(class CTcCodeStream *stream,
                     class CTcSymbol *fixup_owner_sym,
                     struct CTcAbsFixup **fixup_list_head,
                     class CTPNCodeBody *code_body,
                     class CTcPrsSymtab *goto_tab,
                     int argc, int opt_argc, int varargs,
                     int is_constructor, int is_op_overload,
                     int is_self_available,
                     struct tct3_method_gen_ctx *ctx);
    void close_method(int local_cnt,
                      class CTcPrsSymtab *local_symtab,
                      class CTcTokFileDesc *end_desc, long end_linenum,
                      struct tct3_method_gen_ctx *ctx,
                      struct CTcNamedArgTab *named_arg_tab_head);
    void close_method_cleanup(struct tct3_method_gen_ctx *ctx);

    /*
     *   Generate a TadsObject header to a data stream 
     */
    void open_tadsobj(struct tct3_tadsobj_ctx *ctx,
                      CTcDataStream *stream,
                      vm_obj_id_t obj_id, int sc_cnt, int prop_cnt,
                      unsigned int internal_flags, unsigned int vm_flags);
    void close_tadsobj(struct tct3_tadsobj_ctx *ctx);

    /*
     *   Linker support: ensure that the given intrinsic class has a modifier
     *   object.  If there's no modifier, we'll create one and add code for
     *   it to the intrinsic class modifier stream. 
     */
    void linker_ensure_mod_obj(CTcSymMetaclass *mc_sym);
    void linker_ensure_mod_obj(const char *name, size_t len);

    /* 
     *   get my exception table object - this is used to construct a
     *   method's exception table during code generation, and to write the
     *   table to the code stream 
     */
    CTcT3ExcTable *get_exc_table() { return &exc_table_; }

    /* determine if we're compiling a constructor */
    int is_in_constructor() const { return in_constructor_; }
    void set_in_constructor(int f) { in_constructor_ = f; }

    /* determine if we're compiling an operator overload method */
    int is_in_op_overload() const { return in_op_overload_; }
    void set_in_op_overload(int f) { in_op_overload_ = f; }

    /*   
     *   set the method offset - the code body object calls this when it's
     *   about to start generating code to let us know the offset of the
     *   current method 
     */
    void set_method_ofs(ulong ofs);

    /*
     *   Add a debug line table to our list.  We keep track of all of the
     *   debug line record tables in the program, so that we can store the
     *   list in the object file.  We need this information in the object
     *   file because each debug line record table in an object file must
     *   be fixed up at link time after loading the object file. 
     */
    void add_debug_line_table(ulong ofs);

    /* 
     *   Set dynamic (run-time) compilation mode.  This mode must be used for
     *   code compiled during program execution, such as for an "eval()"
     *   facility.  In dynamic compilation, all pool addresses are already
     *   resolved from the loaded program, and we can't add anything to any
     *   of the constant or code pools.  
     */
    void set_dyn_eval() { eval_for_dyn_ = TRUE; }

    /*
     *   Set debug evaluation mode.  If 'speculative' is true, it means
     *   that we're generating an expression for speculative evaluation,
     *   in which case the evaluation must fail if it would have any side
     *   effects (such as calling a method, displaying a string, or
     *   assigning a value).  'stack_level' is the enclosing stack level
     *   at which to evaluate the expression; 0 is the last active
     *   non-debug stack level, 1 is the first enclosing level, and so on.
     */
    void set_debug_eval(int speculative, int level)
    {
        /* note that we're evaluating for the debugger */
        eval_for_debug_ = TRUE;

        /* this is a special type of run-time dynamic compilation */
        eval_for_dyn_ = TRUE;

        /* note the speculative mode */
        speculative_ = speculative;

        /* note the stack level */
        debug_stack_level_ = level;
    }

    /* set normal evaluation mode */
    void set_normal_eval() { eval_for_debug_ = eval_for_dyn_ = FALSE; }

    /* determine if we're in dynamic/debugger evaluation mode */
    int is_eval_for_dyn() const { return eval_for_dyn_; }
    int is_eval_for_debug() const { return eval_for_debug_; }

    /* determine if we're in speculative evaluation mode */
    int is_speculative() const { return eval_for_debug_ && speculative_; }

    /* get the active debugger stack level */
    int get_debug_stack_level() const { return debug_stack_level_; }

    /* 
     *   Generate a BigNumber object, returning the object ID.  The input
     *   text gives the source representation of the number. 
     */
    vm_obj_id_t gen_bignum_obj(const char *txt, size_t len, int promoted);

    /* generate a RexPattern object */
    vm_obj_id_t gen_rexpat_obj(const char *txt, size_t len);

private:
    /* eliminate jump-to-jump sequences */
    void remove_jumps_to_jumps(class CTcCodeStream *str, ulong start_ofs);

    /*
     *   Calculate pool layouts.  This is called after all code generation
     *   is completed; at this point, the T3 code generator can determine
     *   how the code pages will be laid out, since we now know the size
     *   of the largest single chunk of code.
     *   
     *   We'll fill in *first_static_page with the page number in the code
     *   pool of the first page of code containing static initializers.
     *   We group all of the static initializer code together at the end
     *   of the code pool to allow the pre-initialization re-writer to
     *   omit all of the static code pages from the final image file.  
     */
    void calc_pool_layouts(size_t *first_static_page);

    /* 
     *   Write a TADS object stream to the image file.  This routine will
     *   fix up the property table in each object to put the table in
     *   sorted order.  
     */
    void write_tads_objects_to_image(class CTcDataStream *obj_stream,
                                     class CVmImageWriter *image_writer,
                                     int metaclass_idx);

    /* 
     *   write the TADS objects of one particular type - transient or
     *   persistent - to the image file 
     */
    void write_tads_objects_to_image(CTcDataStream *os,
        CVmImageWriter *image_writer, int meta_idx, int trans);

    /* 
     *   Write an object stream of non-TADS objects to the image file.
     *   This writes the objects as-is, without looking into their
     *   contents at all.  
     */
    void write_nontads_objs_to_image(class CTcDataStream *obj_stream,
                                     class CVmImageWriter *image_writer,
                                     int metaclass_idx, int large_obs);

    /* 
     *   Sort an object's property table, and compress the table to remove
     *   deleted properties.  Returns the final size of the object data to
     *   write to the image file, which could differ from the original
     *   size, because we might remove property slots from the property
     *   data.  If we do change the size of the property table, we'll
     *   update the stream data to reflect the new property count and
     *   metaclass data size.  
     */
    size_t sort_object_prop_table(class CTcDataStream *obj_stream,
                                  ulong start_ofs);

    /* write the function-set dependency table to an object file */
    void write_funcdep_to_object_file(class CVmFile *fp);

    /* write the metaclass dependency table to an object file */
    void write_metadep_to_object_file(class CVmFile *fp);

    /* load the function set dependency table from an object file */
    void load_funcdep_from_object_file(class CVmFile *fp,
                                       const textchar_t *fname);

    /* load the metaclass dependency table from an object file */
    void load_metadep_from_object_file(class CVmFile *fp,
                                       const textchar_t *fname);

    /* look up a required or optional property by name */
    vm_prop_id_t look_up_prop(const char *propname, int required,
                              int err_if_undef, int err_if_not_prop);

    /* build the IntrinsicClass instances */
    void build_intrinsic_class_objs(CTcDataStream *str);

    /* build the source file line maps */
    void build_source_line_maps();

    /* build the local symbol records */
    void build_local_symbol_records(class CTcCodeStream *cs,
                                    class CVmHashTable *tab);

    /* build the multi-method initializer list */
    void build_multimethod_initializers();

    /* symbol table enumerator callback for the multi-method initializers */
    static void multimethod_init_cb(void *ctx, CTcSymbol *sym);

    /* symbol table enumerator callback for the multi-method stubs */
    static void multimethod_stub_cb(void *ctx, CTcSymbol *sym);

    /* write an overloaded operator property export */
    void write_op_export(CVmImageWriter *image_writer,
                         class CTcSymProp *prop);

    /* write the static initializer list to the image file */
    void write_static_init_list(CVmImageWriter *image_writer,
                                ulong main_cs_size);

    /* write the list of source file descriptors to an image file */
    void write_sources_to_image(class CVmImageWriter *image_writer);

    /* write the global symbol table to an object file */
    void write_global_symbols_to_image(class CVmImageWriter *image_writer);

    /* write the method header list to the image file */
    void write_method_list_to_image(class CVmImageWriter *image_writer);

    /* write macro definitions to the image file */
    void write_macros_to_image(class CVmImageWriter *image_writer);

    /* write the list of source file descriptors to an object file */
    void write_sources_to_object_file(class CVmFile *fp);

    /* 
     *   read the list of sources from an object file, adding the sources
     *   to the tokenizer's internal list 
     */
    void read_sources_from_object_file(class CVmFile *fp);

    /* load debug records from an object file */
    void load_debug_records_from_object_file(class CVmFile *fp,
                                             const textchar_t *fname,
                                             ulong main_cs_start_ofs,
                                             ulong static_cs_start_ofs);

    /* fix up a debug line record table for the object file */
    void fix_up_debug_line_table(class CTcCodeStream *cs,
                                 ulong line_table_ofs, int first_filedesc);

    /* hash table enumerator callback - generate dictionary code */
    static void enum_dict_gen_cb(void *ctx, class CVmHashEntry *entry);
        

    /* most recent opcodes we've written, for peephole optimization */
    uchar last_op_;
    uchar second_last_op_;

    /* maximum constant string length seen during parsing */
    size_t max_str_len_;

    /* maximum list element count seen during parsing */
    size_t max_list_cnt_;

    /* maximum byte code block generated during code generation */
    size_t max_bytecode_len_;

    /* head and tail of metaclass list */
    tc_meta_entry *meta_head_;
    tc_meta_entry *meta_tail_;

    /* number of entries in metaclass list so far */
    int meta_cnt_;

    /* head and tail of function set list */
    tc_fnset_entry *fnset_head_;
    tc_fnset_entry *fnset_tail_;

    /* number of function sets in the list */
    int fnset_cnt_;

    /* next available global property ID */
    vm_prop_id_t next_prop_;

    /* next available global object ID */
    vm_obj_id_t next_obj_;

    /* current stack depth */
    int sp_depth_;

    /* maximum stack depth in current code block */
    int max_sp_depth_;

    /* exception table for current code block */
    CTcT3ExcTable exc_table_;

    /* constant pool layout manager */
    CTcStreamLayout const_layout_;

    /* code pool layout manager */
    CTcStreamLayout code_layout_;

    /* first/last page of debug line list */
    tct3_debug_line_page *debug_line_head_;
    tct3_debug_line_page *debug_line_tail_;

    /* total number of debug line list entries used so far */
    ulong debug_line_cnt_;

    /*
     *   Object ID of the multi-method static initializer object.  This will
     *   be set by build_multimethod_initializers() if we end up creating any
     *   registration code.  
     */
    vm_obj_id_t mminit_obj_;

    /* 
     *   property sorting buffer - this is space we allocate to copy an
     *   object's property table for sorting 
     */
    char *sort_buf_;
    size_t sort_buf_size_;

    /* flag: we're currently compiling a constructor */
    uint in_constructor_ : 1;

    /* flag: we're currently compiling an operator overload method */
    uint in_op_overload_ : 1;

    /* flag: we're generating code for dynamic (run-time) compilation */
    uint eval_for_dyn_ : 1;

    /* flag: we're generating an expression for debugger use */
    uint eval_for_debug_ : 1;

    /* flag: we're generating a debugger speculative evaluation expression */
    uint speculative_ : 1;

    /* 
     *   debugger active stack context level - valid when eval_for_debug_
     *   is true 
     */
    int debug_stack_level_;

    /* 
     *   String interning hash table.  This is a table of short string
     *   literals that we've generated into the data segment.  Whenever we
     *   generate a new string literal, we'll check this table to see if
     *   we've previously stored the identical string.  If so, we'll simply
     *   use the original string rather than generating a new copy.  This can
     *   reduce the size of the object file by eliminating duplication of
     *   common short strings.
     *   
     *   Note that string interning has the potential to change run-time
     *   semantics in some other languages, but not in TADS.  There are two
     *   common issues in other languages.  First, in C/C++, a string
     *   constant is technically a writable buffer.  This means that two
     *   distinct string literals in the source code really need to be
     *   treated as distinct memory locations, even if they have identical
     *   contents, because the program might write to one buffer and expect
     *   the other to remain unchanged.  This doesn't apply to TADS because
     *   string literals are strictly read-only.  Second, strings in many
     *   languages are objects with reference semantics for comparisons; two
     *   distinct source-code strings must therefore have distinct reference
     *   identities.  String comparisons in TADS are always by value, so
     *   there's no need for distinct references if two strings have
     *   identical contents.  
     */
    class CVmHashTable *strtab_;

    /*
     *   Static table of image file metaclass dependency indices for the
     *   pre-defined metaclasses (i.e., the metaclasses that the compiler
     *   specifically generates code for).  When we compile a program, these
     *   are determined by the compiler simply according to the order in
     *   which it builds its own initial table of the known metaclasses.
     *   When we're debugging, we need to get these values from the image
     *   file.
     *   
     *   This is a simple translation table - we translate from our internal
     *   index (a TCT3_METAID_xxx value) to the corresponding dependency
     *   index in the actual image file.  So, we just need as many of these
     *   as there TCT3_METAID_xxx indices.  We never need these for any
     *   additional metaclasses that might exist - we only care about the
     *   ones that the compiler specifically knows about in advance.  
     */
    int predef_meta_idx_[TCT3_METAID_LAST + 1];
};

/* ------------------------------------------------------------------------ */
/*
 *   Method generator context 
 */
struct tct3_method_gen_ctx
{
    /* output code stream */
    class CTcCodeStream *stream;

    /* stream anchor */
    struct CTcStreamAnchor *anchor;

    /* method header offset in stream */
    ulong method_ofs;

    /* starting and ending code offset in stream */
    ulong code_start_ofs;
    ulong code_end_ofs;

    /* enclosing code body */
    class CTPNCodeBody *old_code_body;
};

/* ------------------------------------------------------------------------ */
/*
 *   TadsObject header writer context 
 */
struct tct3_tadsobj_ctx
{
    /* start of object header in data stream */
    ulong obj_ofs;

    /* data stream to which we're writing the object */
    CTcDataStream *stream;
};

/* ------------------------------------------------------------------------ */
/*
 *   Named arguments information.  This keeps track of the named arguments
 *   for a generated call.  
 */
struct CTcNamedArgs
{
    /* number of named arguments */
    int cnt;

    /* argument list */
    class CTPNArglist *args;
};


#endif /* TCT3_H */

