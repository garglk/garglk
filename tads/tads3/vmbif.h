/* $Header: d:/cvsroot/tads/tads3/vmbif.h,v 1.2 1999/05/17 02:52:29 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmbif.h - built-in function interface
Function
  Provides the interface to built-in function sets.

  The host application environment may provide more than one set of
  built-in functions.  Each function set is identified by a universally
  unique identifier; the program image specifies one or more function
  sets using these identifiers and maps each set to an image-specific
  index.  At run-time, the byte-code program calls built-in functions
  by specifying a function set index and a function index within the
  set.

  A particular function set may use more than one universal identifier,
  because of version evolution.  If new functions are added to a function
  set, the new function set can continue to use its original universal
  identifier, as long as the function set continues to provide the
  original set of functions as a subset of its new vector, and as long
  as the functions in the original set are at the same index positions
  in the modified function set.
Notes
  
Modified
  12/05/98 MJRoberts  - Creation
*/

#ifndef VMBIF_H
#define VMBIF_H

#include "t3std.h"
#include "vmglob.h"
#include "vmtype.h"
#include "vmfunc.h"


/* ------------------------------------------------------------------------ */
/*
 *   Image function set table.  We maintain one global function set table,
 *   which we create when we load the image file. 
 */

class CVmBifTable
{
public:
    /* 
     *   Create a table with a given number of initial entries.  The table
     *   may be expanded in the future if necessary, but if the caller can
     *   predict the maximum number of entries required, we can
     *   preallocate the table at its final size and thus avoid the
     *   overhead and memory fragmentation of expanding the table.  
     */
    CVmBifTable(size_t init_entries);

    /* delete the table */
    ~CVmBifTable();

    /* clear all entries from the table */
    void clear(VMG0_);

    /* 
     *   Add an entry to the table, given the function set identifier (a
     *   string giving the universally unique name for the function set).
     *   Fills in the next available slot.  Throws an error if the
     *   function set is not present in the system.  A function set may
     *   not be present because it's a newer version than this
     *   implementation provides, or because this particular host
     *   application environment does not provide the function set.  
     */
    void add_entry(VMG_ const char *func_set_id);

    /* get the total number of entries in the table */
    size_t get_count() const { return count_; }

    /* get the descriptor for a function; returns null if it's invalid */
    const struct vm_bif_desc *get_desc(uint set_index, uint func_index);

    /* validate an entry */
    int validate_entry(uint set_index, uint func_index);

    /* get the entry at the given index */
    const struct vm_bif_entry_t *get_entry(uint idx)
        { return idx < count_ ? table_[idx] : 0; }
    
    /* 
     *   Look up an entry by function set name.  If the name has a "/nnnnnn"
     *   version suffix, we'll return null if the loaded version is older
     *   than the requested version. 
     */
    const struct vm_bif_entry_t *get_entry(const char *name);

    /* call the given function from the given function set */
    void call_func(VMG_ uint set_index, uint func_index, uint argc);

private:
    /* 
     *   Ensure we have space for a given number of entries, allocating
     *   more if necessary.  If we must allocate more space, we'll
     *   increase the current allocation size by at least the given
     *   increment (more if necessary to bring it up to the required
     *   size).  
     */
    void ensure_space(size_t entries, size_t increment);

    /*
     *   Add an unresolved function set to the table, or throw an error if
     *   this isn't allowed.  If we require resolution of function sets at
     *   load time, this should simply throw an error; otherwise, this
     *   should make a null entry in the function set table so that we can
     *   recognize the missing function set if one of its functions is
     *   ever invoked.  
     */
    void add_entry_unresolved(VMG_ const char *func_set_id);

    /* the table array - we keep an array of pointers */
    struct vm_bif_entry_t **table_;

    /* 
     *   Name array - this is a list of the global function set names;
     *   each entry corresponds to the entry of the table_ array at the
     *   same index.  We use this only for call-time resolution, so that
     *   we can report the name of an unavailable function set when a
     *   function from the unavailable set is invoked.  
     */
    char **names_;

    /* number of entries defined in the table */
    ushort count_;

    /* number of entries allocated for the table */
    size_t alloc_;
};

/* ------------------------------------------------------------------------ */
/*
 *   Built-in function descriptor.  This tells us how to call a function, and
 *   provides reflection data on the function. 
 */
struct vm_bif_desc
{
    /*
     *   The function entrypoint.
     *   
     *   Each function has a common C interface, because the functions take
     *   arguments and return values on the VM stack.  The order of the
     *   functions within the vector is defined by the function set
     *   specification; a function set which uses a particular universal
     *   identifier must always conform to the specification for that
     *   universal identifier.
     *   
     *   For each function, 'argc' is the number of actual parameters passed
     *   to the function by the caller.  The function receives its parameters
     *   from the VM stack; the first argument is at the top of the stack,
     *   the second argument is the next item on the stack, and so on.  The
     *   function must remove all of the arguments from the stack before
     *   returning, and must push a return value if appropriate.  
     */
    void (*func)(VMG_ uint argc);

    /* minimum number of arguments required */
    int min_argc;

    /* additional optional arguments */
    int opt_argc;

    /* are additional variadic arguments allowed? */
    int varargs;

    /*
     *   VM_BIFPTR value for this function.  During image file loading, we
     *   fill this in for each function, so that the function can create a
     *   pointer to itself during execution as needed.  
     */
    vm_val_t bifptr;

    /* 
     *   Synthetic method header.  We build this during the initialization
     *   process to mimic a byte-code method header.  This makes bif pointers
     *   fit in with the rest of the type system by letting CVmFuncPtr pull
     *   out reflection information using the same format as for byte-code
     *   methods.  
     */
    char synth_hdr[VMFUNC_HDR_MIN_SIZE];

    /* fill in the synthetic header */
    void init_synth_hdr()
    {
        /* set everything to zeros to start with */
        memset(synth_hdr, 0, sizeof(synth_hdr));

        /* 
         *   Fill in the base arguments field.  This is an 8-bit field
         *   containing the minimum number of arguments, with the high bit
         *   (0x80) set if it's varargs.  
         */
        synth_hdr[0] = (char)(min_argc | (varargs ? 0x80 : 0));

        /* set the optional argument count field */
        synth_hdr[1] = (char)opt_argc;

        /*
         *   We can leave all of the other fields zeroed out:
         *   
         *   - locals, total_stack: the caller doesn't allocate a standard
         *   stack frame for a built-in, so these are unused
         *   
         *   - exception_table_ofs: we don't have any bytecode, so we
         *   obviously don't have a bytecode exception table; leave it zeroed
         *   out to indicate the absence of a table
         *   
         *   - debug_ofs: we don't have any debugger information since we
         *   don't have any bytecode; zero indicates no records 
         */
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Function set table entry.  This contains information on the function
 *   set at one index in the table. 
 */
struct vm_bif_entry_t
{
    /* 
     *   Function set identifier - a string of 7-bit ASCII characters, one
     *   byte per character, in the range 32 to 126 inclusive, of length 1
     *   to 255 characters, null-terminated. 
     */
    const char *func_set_id;

    /* number of functions in the function set */
    size_t func_count;

    /* the function vector */
    vm_bif_desc *func;

    /* "attach" function for the set - perform static initialization */
    void (*attach)(VMG0_);

    /* "detach" function for the set - perform static cleanup */
    void (*detach)(VMG0_);

    /* link to the image file */
    void link_to_image(VMG_ ushort set_idx)
    {
        /* initialize the function set */
        (*attach)(vmg0_);

        /* link each function entry */
        for (ushort i = 0 ; i < func_count ; ++i)
        {
            /* set up the bifptr value for the function */
            func[i].bifptr.set_bifptr(set_idx, i);

            /* generate the synthetic method header for the function */
            func[i].init_synth_hdr();
        }
    }

    /* termination */
    void unload_image(VMG_ int set_idx)
    {
        /* perform static cleanup */
        (*detach)(vmg0_);
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Base class for function set collections.  This class provides some
 *   utility functions that intrinsics might find useful. 
 */
class CVmBif
{
public:
    /*
     *   Attach to the function set.  The image file loader calls this during
     *   program load for each function set linked into the image file.  This
     *   gives the function set a chance to to any static initialization.  We
     *   only call this when the function set is actually going to be used,
     *   which avoids unnecessarily allocation resources when the function
     *   set isn't needed.
     *   
     *   Note that this is a static function, but it's kind of like a "class
     *   virtual" in that the loader explicitly calls the suitable
     *   Subclass::attach() and Subclass::detach() for each CVmBif subclass
     *   linked from the image file.  If you don't define an overriding
     *   static in a subclass, the compiler will simply link to this base
     *   class method.  
     */
    static void attach(VMG0_) { }
    
    /*
     *   Detach from the function set.  The system calls this during program
     *   termination to let the function set do any static cleanup of
     *   resources allocated during attach(). 
     */
    static void detach(VMG0_) { }

    /* 
     *   check arguments; throws an error if the argument count doesn't
     *   match the given value 
     */
    static void check_argc(VMG_ uint argc, uint needed_argc);

    /* 
     *   check arguments; throws an error if the argument count is outside
     *   of the given range 
     */
    static void check_argc_range(VMG_ uint argc,
                                 uint argc_min, uint argc_max);

    /* pop an integer/long value */
    static int pop_int_val(VMG0_);
    static int32_t pop_long_val(VMG0_);

    /* pop an integer value, or use a default if the argument is nil */
    static int pop_int_or_nil(VMG_ int defval);

    /* pop a true/nil logical value */
    static int pop_bool_val(VMG0_);

    /* pop an object ID value */
    static vm_obj_id_t pop_obj_val(VMG0_);

    /* pop a property ID value */
    static vm_prop_id_t pop_propid_val(VMG0_);

    /*
     *   Pop a string or list value, returning a pointer to the
     *   string/list data.  If the value is a constant string or constant
     *   list, as appropriate, we'll return the constant pool pointer; if
     *   the value is an object of metaclass String or List, as
     *   appropriate, we'll return the metaclass data.  Throws an error if
     *   the value is of any other type.  
     */
    static const char *pop_str_val(VMG0_);
    static const char *pop_list_val(VMG0_);

    /* get a value as a string */
    static const char *get_str_val(VMG_ const vm_val_t *val);

    /*
     *   Pop a null-terminated string into the given buffer.  If the
     *   string is too long for the buffer, we'll truncate it to the given
     *   size. 
     */
    static void pop_str_val_buf(VMG_ char *buf, size_t buflen);
    static void get_str_val_buf(VMG_ char *buf, size_t buflen,
                                const vm_val_t *val);

    /*
     *   Pop a string value, returning the result as a filename in the given
     *   buffer.  The returned filename is null-terminated and converted to
     *   the local filename character set.  The source value must be a
     *   string.
     */
    static void pop_str_val_fname(VMG_ char *buf, size_t buflen);
    static void get_str_val_fname(VMG_ char *buf, size_t buflen,
                                  const char *strp);

    /*
     *   Pop a filename value, returning the result as a filename in the
     *   given buffer.  The returned filename is null-terminated and
     *   converted to the local filename character set.  The source value can
     *   be a string or FileName object. 
     */
    static void pop_fname_val(VMG_ char *buf, size_t buflen);
    static void get_fname_val(VMG_ char *buf, size_t buflen,
                              const vm_val_t *val);
    
    /*
     *   Pop a null-terminated string into the given buffer, converting the
     *   string to the UI character set.  Null-terminates the resulting
     *   string.  If the given buffer is null, we'll allocate a buffer with
     *   t3malloc() and return it; the caller is responsible for freeing the
     *   buffer with t3free().  In any case, returns the buffer into which we
     *   store the results.  
     */
    static char *pop_str_val_ui(VMG_ char *buf, size_t buflen);

    /* create a string object from a C-style string in the UI character set */
    static vm_obj_id_t str_from_ui_str(VMG_ const char *str);
    static vm_obj_id_t str_from_ui_str(VMG_ const char *str, size_t len);

    /* 
     *   Get/pop a CharacterSet argument from the stack.  If the argument is
     *   an object, it must be a CharacterSet object.  If it's a string,
     *   we'll create a CharacterSet object based on the given character set
     *   name.  We'll also allow nil if desired.  
     */
    static vm_obj_id_t pop_charset_obj(VMG0_);
    static vm_obj_id_t get_charset_obj(VMG_ int stk_idx);

    /* get a character set object from a value */
    static vm_obj_id_t get_charset_obj(VMG_ const vm_val_t *val);
    
    /*
     *   Return a value 
     */
    static void retval(VMG_ const struct vm_val_t *val);
    static void retval_nil(VMG0_);
    static void retval_true(VMG0_);
    static void retval_bool(VMG_ int val);
    static void retval_int(VMG_ long val);
    static void retval_obj(VMG_ vm_obj_id_t obj);
    static void retval_prop(VMG_ vm_prop_id_t prop);
    static void retval_str(VMG_ const char *str);
    static void retval_str(VMG_ const char *str, size_t len);
    static void retval_ui_str(VMG_ const char *str);
    static void retval_ui_str(VMG_ const char *str, size_t len);
    static void retval_fnptr(VMG_ pool_ofs_t func);
    static void retval_bifptr(VMG_ ushort set_idx, ushort func_idx);

    /* return the value at top of stack */
    static void retval_pop(VMG0_);
};


#endif /* VMBIF_H */

