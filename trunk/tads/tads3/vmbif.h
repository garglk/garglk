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
    void clear();

    /* 
     *   Add an entry to the table, given the function set identifier (a
     *   string giving the universally unique name for the function set).
     *   Fills in the next available slot.  Throws an error if the
     *   function set is not present in the system.  A function set may
     *   not be present because it's a newer version than this
     *   implementation provides, or because this particular host
     *   application environment does not provide the function set.  
     */
    void add_entry(const char *func_set_id);

    /* get the total number of entries in the table */
    size_t get_count() const { return count_; }

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
    void add_entry_unresolved(const char *func_set_id);

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
    size_t count_;

    /* number of entries allocated for the table */
    size_t alloc_;
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

    /* 
     *   Function vector.  Each function has a common C interface, because
     *   the functions take arguments and return values on the VM stack.
     *   The order of the functions within the vector is defined by the
     *   function set specification; a function set which uses a
     *   particular universal identifier must always conform to the
     *   specification for that universal identifier.
     *   
     *   For each function, 'argc' is the number of actual parameters
     *   passed to the function by the caller.  The function receives its
     *   parameters from the VM stack; the first argument is at the top of
     *   the stack, the second argument is the next item on the stack, and
     *   so on.  The function must remove all of the arguments from the
     *   stack before returning, and must push a return value if
     *   appropriate.  
     */
    void (**func)(VMG_ uint argc);
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
    static int pop_long_val(VMG0_);

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

    /*
     *   Pop a null-terminated string into the given buffer.  If the
     *   string is too long for the buffer, we'll truncate it to the given
     *   size. 
     */
    static void pop_str_val_buf(VMG_ char *buf, size_t buflen);

    /*
     *   Pop a null-terminated string into the given buffer, converting
     *   the string to the filename character set.  Null-terminates the
     *   resulting string.  
     */
    static void pop_str_val_fname(VMG_ char *buf, size_t buflen);
    
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
};


#endif /* VMBIF_H */

