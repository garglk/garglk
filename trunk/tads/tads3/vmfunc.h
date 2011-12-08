/* $Header: d:/cvsroot/tads/tads3/vmfunc.h,v 1.3 1999/07/11 00:46:59 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmfunc.h - T3 VM Function Header definitions
Function
  Defines the layout of a function header, which is the block of data
  immediately preceding the first byte code instruction in every function.
  The function header is stored in binary portable format, so that image
  files can be loaded directly into memory and executed without translation.
Notes
  
Modified
  11/20/98 MJRoberts  - Creation
*/

#ifndef VMFUNC_H
#define VMFUNC_H

#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"


/* ------------------------------------------------------------------------ */
/*
 *   FUNCTION HEADER 
 */

/*   
 *   The function header is a packed array of bytes, with each element stored
 *   in a canonical binary format for binary portability.  No padding is
 *   present except where otherwise specified.  The fields in the function
 *   header, starting at offset zero, are:
 *   
 *   UBYTE argc - the number of parameters the function expects to receive.
 *   If the high-order bit is set (i.e., (argc & 0x80) != 0), then the
 *   function takes a variable number of parameters, with a minimum of (argc
 *   & 0x7f) and no maximum.  If the high-order bit is clear, argc gives the
 *   exact number of parameters required.
 *   
 *   UBYTE optional_argc - number of additional optional parameters the
 *   function can receive, beyond the fixed arguments given by the 'argc'
 *   field.
 *   
 *   UINT2 locals - the number of local variables the function uses.  This
 *   does not count the implicit argument counter local variable, which is
 *   always pushed by the VM after setting up a new activation frame.
 *   
 *   UINT2 total_stack - the total number of stack slots required by the
 *   function, including local variables, intermediate results of
 *   calculations, and actual parameters to functions invoked by this code.
 *   
 *   UINT2 exception_table_ofs - the byte offset from the start of this
 *   method header of the function's exception table.  This value is zero if
 *   the function has no exception table.
 *   
 *   UINT2 debug_ofs - the byte offset from the start of this method header
 *   of the function's debugger records.  This value is zero if the function
 *   has no debugger records.  
 */

/* minimum function header size supported by this version of the VM */
const size_t VMFUNC_HDR_MIN_SIZE = 10;

class CVmFuncPtr
{
public:
    CVmFuncPtr() { p_ = 0; }
    CVmFuncPtr(VMG_ pool_ofs_t ofs);
    CVmFuncPtr(const char *p) { p_ = (const uchar *)p; }
    CVmFuncPtr(const uchar *p) { p_ = p; }
    CVmFuncPtr(VMG_ const vm_val_t *val) { set(vmg_ val); }

    /* initialize with a pointer to the start of the function */
    void set(const uchar *p) { p_ = p; }

    /* set from a vm_val_t; returns true on success, false on failure */
    int set(VMG_ const vm_val_t *val);

    /* get the header pointer */
    const uchar *get() const { return p_; }

    /* copy from another function pointer */
    void copy_from(const CVmFuncPtr *fp) { p_ = fp->p_; }

    /* get a vm_val_t pointer to this function */
    int get_fnptr(VMG_ vm_val_t *v);

    /* get the minimum argument count */
    int get_min_argc() const
    {
        /* get the argument count, but mask out the varargs bit */
        return (int)(get_argc() & 0x7f);
    }

    /* get the maximum argument count, not counting varargs */
    int get_max_argc() const
    {
        return get_min_argc() + (int)get_opt_argc();
    }

    /* is this a varargs function? */
    int is_varargs() const { return ((get_argc() & 0x80) != 0); }

    /* 
     *   check an actual parameter count for correctness; returns true if
     *   the count is correct for this function, false if not 
     */
    int argc_ok(int argc) const
    {
        /* check for match to the min-max range */
        if (argc >= get_min_argc() && argc <= get_max_argc())
        {
            /* we have an exact match, so we're fine */
            return TRUE;
        }
        else if (is_varargs() && argc > get_min_argc())
        {
            /* 
             *   we have variable arguments, and we have at least the
             *   minimum, so we're okay 
             */
            return TRUE;
        }
        else
        {
            /* 
             *   either we don't have variable arguments, or we don't have
             *   the minimum varargs count - in either case, we have an
             *   argument count mistmatch 
             */
            return FALSE;
        }
    }

    /* 
     *   Get the internal argument count.  This has the high bit set for a
     *   varargs function, and the low-order seven bits give the nominal
     *   argument count.  If this is a varargs function, the nominal
     *   argument count is the minimum count: any actual number of arguments
     *   at least the nominal count is valid.  If this is not a varargs
     *   function, the nominal count is the exact count: the actual number
     *   of arguments must match the nominal count.  
     */
    uchar get_argc() const { return *(p_ + 0); }

    /* get the additional optional argument count */
    uint get_opt_argc() const { return *(p_ + 1); }

    /* get the number of locals */
    uint get_local_cnt() const { return osrp2(p_ + 2); }

    /* get the total stack slots required by the function */
    uint get_stack_depth() const { return osrp2(p_ + 4); }

    /* get the exception table offset */
    uint get_exc_ofs() const { return osrp2(p_ + 6); }

    /* get the debugger records table offset */
    uint get_debug_ofs() const { return osrp2(p_ + 8); }

    /* 
     *   Set up an exception table pointer for this function.  Returns
     *   true if successful, false if there's no exception table. 
     */
    int set_exc_ptr(class CVmExcTablePtr *exc_ptr) const;

    /*
     *   Set up a debug table pointer for this function.  Returns true if
     *   successful, false if there's no debug table. 
     */
    int set_dbg_ptr(class CVmDbgTablePtr *dbg_ptr) const;

private:
    /* pointer to the method header */
    const uchar *p_;
};

/* ------------------------------------------------------------------------ */
/*
 *   EXCEPTION TABLE 
 */

/*
 *   The exception table starts with a count indicating how many elements
 *   are in the table, followed by the table entries.  Each entry in the
 *   table specifies the handler for one protected range of code.  We
 *   search the table in forward order, so the handlers must be stored in
 *   order of precedence.
 *   
 *   Each table entry contains:
 *   
 *   UINT2 start_ofs - the starting offset (as a byte offset from the
 *   start of the function) of the protected range for this handler.
 *   
 *   UINT2 end_ofs - the ending offset (as a byte offset from the start of
 *   the function) of the protected range for this handler.  The range is
 *   inclusive of this offset, so a one-byte range would have start_ofs
 *   and end_ofs set to the same value.
 *   
 *   UINT4 exception_class - the object ID of the class of exception
 *   handled by this handler.
 *   
 *   UINT2 handler_ofs - the handler offset (as a byte offset from the
 *   start of the function).  This is the offset of the first instruction
 *   of the code to be invoked to handle this exception.  
 */

/*
 *   Exception Table Entry Pointer 
 */
class CVmExcEntryPtr
{
    /* let CVmExcTablePtr initialize us */
    friend class CVmExcTablePtr;

public:
    /* get the starting/ending offset from this entry */
    uint get_start_ofs() const { return osrp2(p_); }
    uint get_end_ofs() const { return osrp2(p_ + 2); }

    /* get the exception class's object ID */
    vm_obj_id_t get_exception() const { return (vm_obj_id_t)t3rp4u(p_ + 4); }

    /* get the handler byte code offset */
    uint get_handler_ofs() const { return osrp2(p_ + 8); }

    /* increment the pointer to the next entry in the table */
    void inc(VMG0_) { p_ += G_exc_entry_size; }

private:
    /* initialize with a pointer to the first byte of our entry */
    void set(const uchar *p) { p_ = p; }

    /* pointer to the first byte of our entry */
    const uchar *p_;
};

/*
 *   Exception Table Pointer 
 */
class CVmExcTablePtr
{
public:
    /* 
     *   Initialize with a pointer to the start of the function -- we'll
     *   read the exception table offset out of the method header.
     *   Returns true if the function has an exception table, false if
     *   there is no exception table defined in the function.  
     */
    int set(const uchar *p)
    {
        CVmFuncPtr func;

        /* set up the function pointer */
        func.set(p);

        /* if there's no exception table, simply return this information */
        if (func.get_exc_ofs() == 0)
            return FALSE;

        /* set up our pointer by reading from the header */
        p_ = p + func.get_exc_ofs();

        /* indicate that there is a valid exception table */
        return TRUE;
    }

    /* get the number of entries in the table */
    size_t get_count() const { return osrp2(p_); }

    /* initialize a CVmExcEntryPtr with the entry at the given index */
    void set_entry_ptr(VMG_ CVmExcEntryPtr *entry, size_t idx) const
        { entry->set(p_ + 2 + (idx * G_exc_entry_size)); }

private:
    /* pointer to the first byte of the exception table */
    const uchar *p_;
};

/* ------------------------------------------------------------------------ */
/*
 *   DEBUGGER RECORDS TABLE 
 */

/*
 *   The debugger table consists of three sections.  The first section is
 *   the header, with general information on the method or function.  The
 *   second section is the line records, which give the code boundaries of
 *   the source lines.  The third section is the frame records, giving the
 *   local symbol tables.  
 */

/*
 *   Debugger source line entry pointer
 */
class CVmDbgLinePtr
{
    /* let CVmDbgTablePtr initialize us */
    friend class CVmDbgTablePtr;

public:
    /* 
     *   get the byte-code offset (from method start) of the start of the
     *   byte-code for this line 
     */
    uint get_start_ofs() const { return osrp2(p_); }

    /* get the source file ID (an index into the global source file list) */
    uint get_source_id() const { return osrp2(p_ + 2); }

    /* get the line number in the source file */
    ulong get_source_line() const { return t3rp4u(p_ + 4); }

    /* get the frame ID (a 1-based index into our local frame table) */
    uint get_frame_id() const { return osrp2(p_ + 8); }

    /* increment the pointer to the next entry in the table */
    void inc(VMG0_) { p_ += G_line_entry_size; }

    /* set from another line pointer */
    void copy_from(const CVmDbgLinePtr *p) { p_ = p->p_; }

private:
    /* initialize with a pointer to the first byte of our entry */
    void set(const uchar *p) { p_ = p; }

    /* pointer to the first byte of our entry */
    const uchar *p_;
};

/*
 *   Debugger frame symbol entry pointer 
 */
class CVmDbgFrameSymPtr
{
    /* let CVmDbgFramePtr initialize us */
    friend class CVmDbgFramePtr;
    
public:
    /* get the local/parameter number */
    uint get_var_num() const { return osrp2(p_); }

    /* get the context array index (for context locals) */
    vm_prop_id_t get_ctx_arr_idx() const
        { return (vm_prop_id_t)osrp2(p_ + 4); }

    /* determine if I'm a local or a parameter */
    int is_local() const { return (get_flags() & 1) == 0; }
    int is_param() const
        { return (((get_flags() & 1) != 0) && !is_ctx_local()); }

    /* determine if I'm a context local */
    int is_ctx_local() const { return (get_flags() & 2) != 0; }

    /* get the length of my name string */
    uint get_sym_len(VMG0_) const { return osrp2(get_symptr(vmg0_)); }

    /* get a pointer to my name string - this is not null-terminated */
    const char *get_sym(VMG0_) const { return get_symptr(vmg0_) + 2; }

    /* increment this pointer to point to the next symbol in the frame */
    void inc(VMG0_)
    {
        /* skip the in-line symbol, or the pointer */
        if (is_sym_inline())
            p_ += get_sym_len(vmg0_) + 2;
        else
            p_ += 4;

        /* skip the header */
        p_ += G_dbg_lclsym_hdr_size;
    }

    /* is my symbol in-line or in the constant pool? */
    int is_sym_inline() const { return !(get_flags() & 0x0004); }

    /* 
     *   Set up a vm_val_t for the symbol.  For an in-line symbol, this
     *   creates a new string object; for a constant pool element, this
     *   returns a VM_SSTR pointer to it. 
     */
    void get_str_val(VMG_ vm_val_t *val) const;

private:
    /* get my flags value */
    uint get_flags() const { return osrp2(p_ + 2); }

    /* 
     *   get a pointer to my symbol data - this points to a UINT2 length
     *   prefix followed by the bytes of the UTF-8 string 
     */
    const char *get_symptr(VMG0_) const;
        
    /* initialize with a pointer to the first byte of our entry */
    void set(const uchar *p) { p_ = p; }

    /* pointer to the first byte of our entry */
    const uchar *p_;
};

/*
 *   Debugger frame entry pointer
 */
class CVmDbgFramePtr
{
    /* let CVmDbgTablePtr initialize us */
    friend class CVmDbgTablePtr;

public:
    /* copy from another frame pointer */
    void copy_from(const CVmDbgFramePtr *frame)
    {
        /* copy the original frame's pointer */
        p_ = frame->p_;
    }

    /* get the ID of the enclosing frame */
    uint get_enclosing_frame() const { return osrp2(p_); }

    /* get the number of symbols in the frame */
    uint get_sym_count() const { return osrp2(p_ + 2); }

    /* set up a pointer to the first symbol */
    void set_first_sym_ptr(VMG_ CVmDbgFrameSymPtr *entry)
        { entry->set(p_ + G_dbg_frame_size); }

    /* get the bytecode range covered by the frame */
    uint get_start_ofs(VMG0_)
        { return G_dbg_frame_size >= 8 ? osrp2(p_ + 4) : 0; }
    uint get_end_ofs(VMG0_)
        { return G_dbg_frame_size >= 8 ? osrp2(p_ + 6) : 0; }

    /* is this frame nested in the given frame? */
    int is_nested_in(VMG_ const class CVmDbgTablePtr *dp, int i);

private:
    /* initialize with a pointer to the first byte of our entry */
    void set(const uchar *p) { p_ = p; }

    /* pointer to the first byte of our entry */
    const uchar *p_;
};


/*
 *   Debugger Records Table Pointer 
 */
class CVmDbgTablePtr
{
public:
    /* 
     *   Initialize with a pointer to the start of the function -- we'll
     *   read the debugger table offset out of the method header.  Returns
     *   true if the function has debugger records, false if there is no
     *   debugger table defined in the function.  
     */
    int set(const uchar *p)
    {
        CVmFuncPtr func;

        /* if the pointer is null, there's obviously no function pointer */
        if (p == 0)
            return FALSE;

        /* set up the function pointer */
        func.set(p);

        /* if there's no debugger table, simply return this information */
        if (func.get_debug_ofs() == 0)
            return FALSE;

        /* set up our pointer by reading from the header */
        p_ = p + func.get_debug_ofs();

        /* indicate that there is a valid debugger records table */
        return TRUE;
    }

    /* copy from another debug table pointer */
    void copy_from(const CVmDbgTablePtr *table)
    {
        /* copy the other table's location */
        p_ = table->p_;
    }

    /* get the number of source line entries in the table */
    size_t get_line_count(VMG0_) const
        { return osrp2(p_ + G_dbg_hdr_size); }

    /* get the number of frame entries in the table */
    size_t get_frame_count(VMG0_) const
        {  return osrp2(p_ + get_frame_ofs(vmg0_)); }

    /* initialize a CVmDbgLinePtr with the entry at the given index */
    void set_line_ptr(VMG_ CVmDbgLinePtr *entry, size_t idx) const
        { entry->set(p_ + G_dbg_hdr_size + 2 + (idx * G_line_entry_size)); }

    /* initialize a CVmDbgFramePtr with the entry at the given index */
    void set_frame_ptr(VMG_ CVmDbgFramePtr *entry, size_t idx) const
    {
        size_t index_ofs;
        size_t frame_ofs;
        
        /* 
         *   Compute the location of the index table entry - note that
         *   'idx' is a one-based index value, so we must decrement it
         *   before performing our offset arithmetic.  Note also that we
         *   must add two bytes to get past the count field at the start
         *   of the frame table.  Each index entry is two bytes long.
         *   
         *   (If we were clever, we would distribute the multiply-by-two,
         *   which would yield a constant subtraction of two, which would
         *   cancel the constant addition of two.  Let's hope the C++
         *   catches on to this, because we would rather not be clever and
         *   instead write it explicitly for greater clarity.)  
         */
        index_ofs = get_frame_ofs(vmg0_) + 2 + (2 * (idx - 1));

        /* read the frame offset from the entry */
        frame_ofs = osrp2(p_ + index_ofs);

        /* 
         *   the frame offset in the table is relative to the location of
         *   the table location containing the entry, so add the index
         *   offset to the frame offset to get the actual location of the
         *   frame entry 
         */
        entry->set(p_ + index_ofs + frame_ofs);
    }

private:
    /* get the offset to the start of the frame table */
    size_t get_frame_ofs(VMG0_) const
    {
        /* 
         *   the frame table follows the line records, which follow the
         *   debug table header and the line counter, plus another two
         *   bytes for the post-frame offset pointer 
         */
        return (G_dbg_hdr_size
                + 2
                + (get_line_count(vmg0_) * G_line_entry_size)
                + 2);
    }
    
    /* pointer to the first byte of the debugger records table */
    const uchar *p_;
};

#endif /* VMFUNC_H */

