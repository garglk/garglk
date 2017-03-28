/* $Header: d:/cvsroot/tads/tads3/VMTYPE.H,v 1.3 1999/05/17 02:52:29 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmtype.h - VM types
Function
  
Notes
  
Modified
  10/21/98 MJRoberts  - Creation
*/

#ifndef VMTYPE_H
#define VMTYPE_H

#include <string.h>
#include <stdlib.h>

#include "t3std.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmglob.h"


/*
 *   Constant pool/code offset.  This is an address of an object in the
 *   pool.  Pool offsets are 32-bit values.  
 */
typedef uint32_t pool_ofs_t;

/*
 *   Savepoint ID's are stored in a single byte; since we store many
 *   copies of savepoint ID's (since they need to be stored with each undo
 *   list head), we want to save some space on this type.  This limits us
 *   to 256 simultaneous savepoints, but this should be more than we
 *   actually want to keep around anyway, because of the amount of memory
 *   it would consume to try to keep more than that around.  
 */
typedef uchar vm_savept_t;
const vm_savept_t VM_SAVEPT_MAX = 255;

/* 
 *   Object ID type.  VM_INVALID_OBJ is a distinguished value that serves
 *   as an invalid object ID (a null pointer, effectively); no object can
 *   ever have this ID.  
 */
typedef uint32_t vm_obj_id_t;
const vm_obj_id_t VM_INVALID_OBJ = 0;

/*
 *   Property ID.  Property ID's are 16-bit values.  VM_INVALID_PROP is a
 *   distinguished value that serves as an invalid property ID, which can
 *   be used to indicate the absence of a property value.  
 */
typedef uint16_t vm_prop_id_t;
const vm_prop_id_t VM_INVALID_PROP = 0;

/*
 *   Maximum recursion depth for recursive equality tests and hash
 *   calculations.
 *   
 *   When we're comparing or hashing a tree of references by value, such as
 *   when we're comparing two vectors or hashing a vector, we'll keep track
 *   of the recursion depth of our tree traversal.  If we reach this depth,
 *   we'll throw an error on the assumption that the tree contains cycles and
 *   thus cannot be hashed or compared by value.  This depth is chosen to be
 *   large enough that it's unlikely we'll exceed it with acyclical trees,
 *   but small enough that we probably won't blow the C++ stack before we
 *   reach this depth.  
 */
const int VM_MAX_TREE_DEPTH_EQ = 256;

/*
 *   Datatypes
 */
enum vm_datatype_t
{
    /* nil - doubles as a null pointer and a boolean false */
    VM_NIL = 1,

    /* true - boolean true */
    VM_TRUE,

    /* 
     *   Stack pointer (this is used to store a pointer to the enclosing
     *   frame in a stack frame).  This is a native machine pointer.  
     */
    VM_STACK,

    /* 
     *   Code pointer (this is used to store a pointer to the return
     *   address in a stack frame, for example).  This is a native machine
     *   pointer.  This differs from VM_CODEOFS in that this is a native
     *   machine pointer.  
     */
    VM_CODEPTR,

    /* object reference */
    VM_OBJ,

    /* property ID */
    VM_PROP,

    /* 32-bit signed integer */
    VM_INT,
    
    /* 
     *   string constant value - the value is an offset into the constant
     *   pool of the string descriptor 
     */
    VM_SSTRING,
    
    /* 
     *   self-printing string value - the value is an offset into the
     *   constant pool of the string descriptor 
     */
    VM_DSTRING,

    /* 
     *   list constant - the value is an offset into the constant pool of
     *   the list descriptor 
     */
    VM_LIST,

    /* 
     *   byte-code constant offset - this is an offset into the byte-code
     *   pool.  This differs from VM_CODEPTR in that this is an offset in
     *   the byte-code constant pool rather than a native machine pointer.
     *   
     */
    VM_CODEOFS,

    /*
     *   function pointer - this is represented as an offset into the
     *   byte-code pool.  This differs from VM_CODEOFS in that the code
     *   referenced by a VM_CODEOFS value is generally invoked directly
     *   whenever the value is evaluated, whereas VM_FUNCPTR values are
     *   used to convey function pointers, so the underlying code is not
     *   executed implicitly on evaluation of such a value but must be
     *   explicitly invoked. 
     */
    VM_FUNCPTR,

    /*
     *   This is a special pseudo-type used to indicate that a value is
     *   not present.  This differs from nil, in that nil is a null
     *   reference or false value, whereas this indicates that there's no
     *   specified value at all.  This is used, for example, to indicate
     *   in an undo record that a property did not previously exist. 
     */
    VM_EMPTY,

    /*
     *   This is a special pseudo-type used to indicate that evaluating an
     *   expression requires executing system code.  The value stored is a
     *   pointer to a constant CVmNativeCodeDesc object, which describes a
     *   native code method.  
     */
    VM_NATIVE_CODE,

    /*
     *   Enumerated constant 
     */
    VM_ENUM,

    /*
     *   Built-in function pointer.  This is represented as an integer value
     *   with the BIF table entry index (the function set ID) in the high 16
     *   bits, and the function index (the index of the function within its
     *   function set) in the low 16 bits.  
     */
    VM_BIFPTR,

    /*
     *   Special internal pseudo-type for execute-on-evaluation objects.
     *   This is used when an anonymous object or dynamically created string
     *   is stored in an object property *as a method*, meaning that the
     *   object is executed upon evaluation rather than simply returned as an
     *   object value.  This is never stored in an image file, but it can be
     *   stored in a saved game file.  It's never used in a value that's
     *   visible to user code; it's only for internal storage within
     *   TadsObject property tables and the like.  
     */
    VM_OBJX,

    /* the equivalent of VM_OBJX for built-in function pointers */
    VM_BIFPTRX,

    /*
     *   First invalid type ID.  Tools (such as compilers and debuggers)
     *   can use this ID and any higher ID values to flag their own
     *   internal types.  
     */
    VM_FIRST_INVALID_TYPE
};

/* 
 *   Macro to create a private type constant for internal use in a tool.  The
 *   compiler uses this to create special non-VM values for its own use
 *   during compilation.  Types created with this macro must never appear in
 *   an image file, and must never be exposed to user bytecode via any APIs.
 */
#define VM_MAKE_INTERNAL_TYPE(idx) \
    ((vm_datatype_t)(((int)VM_FIRST_INVALID_TYPE) + (idx)))

/*
 *   Value container.  Local variables, stack locations, and other value
 *   holders use this structure to store a value and its type.  
 */
struct vm_val_t
{
    vm_datatype_t typ;
    union
    {
        /* stack/code pointer */
        const void *ptr;

        /* object reference */
        vm_obj_id_t obj;

        /* property ID */
        vm_prop_id_t prop;

        /* 32-bit integer */
        int32_t intval;

        /* enumerated constant */
        uint32_t enumval;

        /* sstring/dstring/list constant pool offset/pcode pool offset */
        pool_ofs_t ofs;

        /* native code descriptor */
        const class CVmNativeCodeDesc *native_desc;

        /* pointer to built-in function */
        struct
        {
            unsigned short set_idx;
            unsigned short func_idx;
        } bifptr;
    } val;

    /* set various types of values */
    void set_empty() { typ = VM_EMPTY; }
    void set_nil() { typ = VM_NIL; }
    void set_true() { typ = VM_TRUE; }
    void set_stack(void *ptr) { typ = VM_STACK; val.ptr = ptr; }
    void set_codeptr(const void *ptr) { typ = VM_CODEPTR; val.ptr = ptr; }
    void set_obj(vm_obj_id_t obj) { typ = VM_OBJ; val.obj = obj; }
    void set_propid(vm_prop_id_t prop) { typ = VM_PROP; val.prop = prop; }
    void set_int(int32_t intval) { typ = VM_INT; val.intval = intval; }
    void set_enum(uint32_t enumval) { typ = VM_ENUM; val.enumval = enumval; }
    void set_sstring(pool_ofs_t ofs) { typ = VM_SSTRING; val.ofs = ofs; }
    void set_dstring(pool_ofs_t ofs) { typ = VM_DSTRING; val.ofs = ofs; }
    void set_list(pool_ofs_t ofs) { typ = VM_LIST; val.ofs = ofs; }
    void set_codeofs(pool_ofs_t ofs) { typ = VM_CODEOFS; val.ofs = ofs; }
    void set_fnptr(pool_ofs_t ofs) { typ = VM_FUNCPTR; val.ofs = ofs; }
    void set_bifptr(ushort set_idx, ushort func_idx)
    {
        typ = VM_BIFPTR;
        val.bifptr.set_idx = set_idx;
        val.bifptr.func_idx = func_idx;
    }
    void set_native(const class CVmNativeCodeDesc *desc)
        { typ = VM_NATIVE_CODE; val.native_desc = desc; }

    /*
     *   Set nil, specifically for a value that might be interpreted as an
     *   object ID.  This sets the object ID field to invalid in addition to
     *   setting the value type to nil, so that callers that just look at the
     *   object ID will see an explicitly invalid object value rather than
     *   something random.  
     */
    void set_nil_obj() { typ = VM_NIL; val.obj = VM_INVALID_OBJ; }

    /* 
     *   set an object or nil value: if the object ID is VM_INVALID_OBJ,
     *   we'll set the type to nil 
     */
    void set_obj_or_nil(vm_obj_id_t obj)
    {
        /* set the object value initially */
        typ = VM_OBJ;
        val.obj = obj;

        /* if the object is invalid, set the type to nil */
        if (obj == VM_INVALID_OBJ)
            typ = VM_NIL;
    }

    /* get an object, or a null pointer if it's not an object */
    vm_obj_id_t get_as_obj() const { return typ == VM_OBJ ? val.obj : 0; }

    /* is this an object of the given class? */
    int is_instance_of(VMG_ vm_obj_id_t cls) const;

    /* set to an integer giving the datatype of the given value */
    void set_datatype(VMG_ const vm_val_t *val);

    /* set to nil if 'val' is zero, true if 'val' is non-zero */
    void set_logical(int v) { typ = (v != 0 ? VM_TRUE : VM_NIL); }

    /* determine if the value is logical (nil or true) */
    int is_logical() const { return (typ == VM_NIL || typ == VM_TRUE); }

    /* 
     *   Get a logical as numeric TRUE or FALSE.  This doesn't perform
     *   any type checking; the caller must ensure that the value is
     *   either true or nil, or this may return meaningless results.  
     */
    int get_logical() const { return (typ == VM_TRUE); }

    /* get as logical, checking type */
    int get_logical_only() const
    {
        if (typ == VM_TRUE)
            return TRUE;
        else if (typ == VM_NIL)
            return FALSE;
        else
        {
            err_throw(VMERR_BAD_TYPE_BIF);
            AFTER_ERR_THROW(return FALSE;)
        }
    }

    /*
     *   Get the underlying string constant value.  If the value does not
     *   have an underlying string constant (because it is of a type that
     *   does not store a string value), this will return null. 
     */
    const char *get_as_string(VMG0_) const;

    /*
     *   Cast the value to a string.  This attempts to reinterpret the value
     *   as a string.   
     */
    const char *cast_to_string(VMG_ vm_val_t *new_str) const;

    /*
     *   Get the underlying list constant value.  If the value does not
     *   have an underlying list constant (because it is of a type that
     *   does not store list data), this returns null. 
     */
    const char *get_as_list(VMG0_) const;

    /*
     *   Is the value indexable as a list?  This returns true for types that
     *   have contiguous integer indexes from 1..number of elements.  
     */
    int is_listlike(VMG0_) const;

    /* 
     *   For a list-like object, get the number of elements.  If this isn't a
     *   list-like object, returns -1. 
     */
    int ll_length(VMG0_) const;

    /* 
     *   For a list-like object, get an indexed element.  The index is
     *   1-based, per our usual conventions.
     */
    void ll_index(VMG_ vm_val_t *ret, const vm_val_t *idx) const;

    /* index with a 1-based integer */
    void ll_index(VMG_ vm_val_t *ret, int idx) const
    {
        vm_val_t vidx;
        vidx.set_int(idx);
        ll_index(vmg_ ret, &vidx);
    }

    /* get as a native code pointer value */
    const uchar *get_as_codeptr(VMG0_) const;

    /* 
     *   Is this a function pointer of some kind?  This returns true if it's
     *   a plain function pointer, a built-in function pointer, or an object
     *   with an invoker (which covers anonymous and dynamic functions). 
     */
    int is_func_ptr(VMG0_) const;

    /*
     *   Convert a numeric value to an integer value.  If the value isn't
     *   numeric, throws an error. 
     */
    void num_to_logical()
    {
        /* check the type */
        if (typ == VM_INT)
        {
            /* it's an integer - treat 0 as nil, all else as true */
            typ = (val.intval == 0 ? VM_NIL : VM_TRUE);
        }
        else
        {
            /* it's not a number - throw an error */
            err_throw(VMERR_NO_LOG_CONV);
        }
    }

    /* determine if the value is an integer */
    int is_int() const { return (typ == VM_INT); }

    /* get the value as an integer, throwing an error if it's any other type */
    int32_t get_as_int() const
    {
        /* check the type */
        if (typ == VM_INT)
        {
            /* it's an integer already - return the value directly */
            return val.intval;
        }
        else
        {
            /* not an integer - throw an error */
            err_throw(VMERR_NUM_VAL_REQD);
            AFTER_ERR_THROW(return 0;)
        }
    }

    /* 
     *   determine if the type is numeric - this returns true for integer and
     *   BigNumber values, and could match future types that are numeric
     */
    int is_numeric(VMG0_) const
        { return typ == VM_INT || nonint_is_numeric(vmg0_); }

    /*
     *   Promote an integer to this numeric type.  For example, if 'this' is
     *   a BigNumber, this creates a BigNumber representation of val.  This
     *   converts the value in place, replacing '*val' with the promoted
     *   value.
     */
    void promote_int(VMG_ vm_val_t *val) const;

    /* 
     *   Convert a numeric value to an integer.  This converts any type for
     *   which is_numeric() returns true, but doesn't convert non-numeric
     *   types.
     */
    int32_t num_to_int(VMG0_) const
        { return typ == VM_INT ? val.intval : nonint_num_to_int(vmg0_); }

    /*
     *   Convert a numeric value to a double.  This converts any type for
     *   which is_numeric() returns true, but doesn't convert non-numeric
     *   types.
     */
    double num_to_double(VMG0_) const
    {
        return typ == VM_INT
            ? (double)val.intval
            : nonint_num_to_double(vmg0_);
    }

    /* 
     *   Cast to an integer.  This is more aggressive than num_to_int(), in
     *   that we actively try to convert non-numeric types.
     */
    int32_t cast_to_int(VMG0_) const
        { return typ == VM_INT ? val.intval : nonint_cast_to_int(vmg0_); }

    /*
     *   Cast to a numeric type - integer or BigNumber.  If the value is
     *   already numeric, we'll return the value unchanged.  Otherwise, if an
     *   automatic conversion to a numeric type is possible, we'll return the
     *   converted value.  We'll return the "simplest" type that can
     *   precisely represent the value, meaning the type that uses the least
     *   memory and that's fastest for computations.  
     */
    void cast_to_num(VMG_ vm_val_t *val) const;

    /* 
     *   determine if the numeric value is zero; throws an error if the
     *   value is not numeric 
     */
    int num_is_zero() const
    {
        /* check the type */
        if (typ == VM_INT)
        {
            /* check the integer value to see if it's zero */
            return (val.intval == 0);
        }
        else
        {
            /* it's not a number */
            err_throw(VMERR_NUM_VAL_REQD);

            /* in case the compiler doesn't know we'll never get here */
            AFTER_ERR_THROW(return 0;)
        }
    }

    /*
     *   Determine if this value equals a given value.  The nature of the
     *   match depends on the type of this value:
     *   
     *   integers, property ID's, code offsets: the types and values must
     *   match exactly.
     *   
     *   string and list constants: the other value must either be the same
     *   type of constant, or an object that has an underlying value of the
     *   same type; and the contents of the strings or lists must match.
     *   
     *   objects: the match depends on the type of the object.  We invoke the
     *   object's virtual equals() routine to make this determination.
     *   
     *   'depth' has the same meaning as in calc_hash().  
     */
    int equals(VMG_ const vm_val_t *val, int depth = 0) const;

    /*
     *   Calculate a hash for the value.  The meaning of the hash varies by
     *   type, but is stable for a given value.  'depth' is a recursion depth
     *   counter, with the same meaning as in CVmObject::calc_hash().  
     */
    uint calc_hash(VMG_ int depth = 0) const;

    /*
     *   Compare this value to the given value.  Returns a value greater than
     *   zero if this value is greater than 'val', a value less than zero if
     *   this value is less than 'val', or 0 if the two values are equal.
     *   Throws an error if the two values are not comparable.
     *   
     *   By far the most common type of comparison is between integers, so we
     *   test in-line to see if we have two integer values, and if so, use a
     *   fast in-line comparison.  If we don't have two integers, we'll use
     *   our full out-of-line test, which will look at other more interesting
     *   type combinations.  
     */
    int compare_to(VMG_ const vm_val_t *b) const
    {
        if (typ == VM_INT && b->typ == VM_INT)
            return (val.intval > b->val.intval
                    ? 1 : val.intval < b->val.intval ? -1 : 0);
        else
            return gen_compare_to(vmg_ b);
    }

    /*
     *   relative value comparisons 
     */

    /* self > b */
    int is_gt(VMG_ const vm_val_t *b) const
    {
        if (typ == VM_INT && b->typ == VM_INT)
            return val.intval > b->val.intval;
        else
            return gen_compare_to(vmg_ b) > 0;
    }

    /* self >= b */
    int is_ge(VMG_ const vm_val_t *b) const
    {
        if (typ == VM_INT && b->typ == VM_INT)
            return val.intval >= b->val.intval;
        else
            return gen_compare_to(vmg_ b) >= 0;
    }

    /* self < b */
    int is_lt(VMG_ const vm_val_t *b) const
    {
        if (typ == VM_INT && b->typ == VM_INT)
            return val.intval < b->val.intval;
        else
            return gen_compare_to(vmg_ b) < 0;
    }

    /* self <= b */
    int is_le(VMG_ const vm_val_t *b) const
    {
        if (typ == VM_INT && b->typ == VM_INT)
            return val.intval <= b->val.intval;
        else
            return gen_compare_to(vmg_ b) <= 0;
    }

private:
    /* out-of-line comparison, used when we don't have two integers */
    int gen_compare_to(VMG_ const vm_val_t *val) const;

    /* 
     *   internal int conversion and casting - we separate these from
     *   num_to_int and cast_to_int so that cast_to_int can be inlined for
     *   the common case where the value is already an int 
     */
    int nonint_is_numeric(VMG0_) const;
    int32_t nonint_num_to_int(VMG0_) const;
    double nonint_num_to_double(VMG0_) const;
    int32_t nonint_cast_to_int(VMG0_) const;
};

/* ------------------------------------------------------------------------ */
/*
 *   Native code descriptor.  This describes a native method call of an
 *   intrinsic class object.  
 */
class CVmNativeCodeDesc
{
public:
    /* create a descriptor with an exact number of arguments */
    CVmNativeCodeDesc(int argc)
    {
        /* remember the parameters - there are no optional arguments */
        min_argc_ = argc;
        opt_argc_ = 0;
        varargs_ = FALSE;
    }

    /* create a descriptor with optional arguments (but not varargs) */
    CVmNativeCodeDesc(int min_argc, int opt_argc)
    {
        /* remember the parameters */
        min_argc_ = min_argc;
        opt_argc_ = opt_argc;
        varargs_ = FALSE;
    }

    /* create a descriptor with optional arguments and/or varargs */
    CVmNativeCodeDesc(int min_argc, int opt_argc, int varargs)
    {
        /* remember the parameters */
        min_argc_ = min_argc;
        opt_argc_ = opt_argc;
        varargs_ = varargs;
    }

    /* check the given number of arguments for validity */
    int args_ok(int argc) const
    {
        /* 
         *   the actual parameters must number at least the minimum, and
         *   cannot exceed the maximum (i.e., the minimum plus the
         *   optionals) unless we have varargs, in which case there is no
         *   maximum 
         */
        return (argc >= min_argc_
                && (varargs_ || argc <= min_argc_ + opt_argc_));
    }

    /* minimum argument count */
    int min_argc_;

    /* number of optional named arguments beyond the minimum */
    int opt_argc_;

    /* 
     *   true -> varargs: any number of arguments greater than or equal to
     *   min_argc_ are valid
     */
    int varargs_;
};

/* ------------------------------------------------------------------------ */
/*
 *   String handling - these routines are provided as covers to allow for
 *   easier adjustment for Unicode or other encodings.  Don't include these
 *   if we're compiling interface routines for the HTML TADS environment,
 *   since HTML TADS has its own similar definitions for these, and we don't
 *   need these for interface code.
 *   
 *   NOTE: The purpose of this macro is to indicate that we're being
 *   #included by an HTML TADS source file - that is, the current compile run
 *   is for an htmltads/xxx.cpp file.  Do NOT #define this macro when
 *   compiling tads3/xxx.cpp files.  The tads3/xxx.cpp files have absolutely
 *   no explicit dependencies on the htmltads subsystem and thus don't
 *   #include the htmltads headers that would provide the conflicting
 *   interface definitions for these functions.  The conflict is in the other
 *   direction: some of the htmltads source files explicitly depend on tads3
 *   headers, so they indirectly #include this header.  So, this macro needs
 *   to be defined only when compiling htmltads/xxx.cpp files.  
 */
#ifdef T3_COMPILING_FOR_HTML
#include "tadshtml.h"
#else

inline size_t get_strlen(const textchar_t *str) { return strlen(str); }
inline void do_strcpy(textchar_t *dst, const textchar_t *src)
    { strcpy(dst, src); }

#endif /* T3_COMPILING_FOR_HTML */

/* ------------------------------------------------------------------------ */
/*
 *   Portable Binary Representations.  When we store certain types of
 *   information in memory, we store it in a format that is identical to
 *   the format we use in portable binary files; using this format allows
 *   us to read and write binary files as byte images, without any
 *   interpretation, which greatly improves I/O performance in many cases.
 */

/*
 *   Portable binary LENGTH indicator.  This is used to store length
 *   prefixes for strings, lists, and similar objects.  We use a UINT2
 *   (16-bit unsigned integer) for this type of value.  
 */
const size_t VMB_LEN = 2;
inline void vmb_put_len(char *buf, size_t len) { oswp2(buf, len); }
inline size_t vmb_get_len(const char *buf) { return osrp2(buf); }

/*
 *   Portable binary unsigned 2-byte integer 
 */
const size_t VMB_UINT2 = 2;
inline void vmb_put_uint2(char *buf, uint16_t i) { oswp2(buf, i); }
inline uint16_t vmb_get_uint2(const char *buf) { return osrp2(buf); }

/*
 *   Portable binary unsigned 4-byte integer 
 */
const size_t VMB_UINT4 = 4;
inline void vmb_put_uint4(char *buf, uint32_t i) { oswp4(buf, i); }
inline uint32_t vmb_get_uint4(const char *buf) { return osrp4(buf); }

/*
 *   Portable binary signed 4-byte integer 
 */
const size_t VMB_INT4 = 4;
inline void vmb_put_int4(char *buf, int32_t i) { oswp4s(buf, i); }
inline int32_t vmb_get_int4(const char *buf) { return osrp4s(buf); }

/*
 *   Portable binary object ID. 
 */
const size_t VMB_OBJECT_ID = 4;
inline void vmb_put_objid(char *buf, vm_obj_id_t obj) { oswp4(buf, obj); }
inline vm_obj_id_t vmb_get_objid(const char *buf) { return t3rp4u(buf); }

/*
 *   Portable binary property ID 
 */
const size_t VMB_PROP_ID = 2;
inline void vmb_put_propid(char *buf, vm_obj_id_t prop) { oswp2(buf, prop); }
inline vm_prop_id_t vmb_get_propid(const char *buf) { return osrp2(buf); }

/*
 *   Portable data holder.  This is used to store varying-type data items;
 *   for example, this is used to store an element in a list, or the value
 *   of a property in an object.  This type of value stores a one-byte
 *   prefix indicating the type of the value, and a four-byte area in
 *   which the value is stored.  The actual use of the four-byte value
 *   area depends on the type.  
 */
const size_t VMB_DATAHOLDER = 5;

/* offset from a portable data holder pointer to the data value */
const size_t VMB_DH_DATAOFS = 1;

/* store a portable dataholder from a vm_val_t */
void vmb_put_dh(char *buf, const vm_val_t *val);

/* store a nil value in a portable dataholder */
inline void vmb_put_dh_nil(char *buf) { buf[0] = VM_NIL; }

/* store an object value in a portable dataholder */
inline void vmb_put_dh_obj(char *buf, vm_obj_id_t obj)
    { buf[0] = VM_OBJ; vmb_put_objid(buf + 1, obj); }

/* store a property value in a portable dataholder */
inline void vmb_put_dh_prop(char *buf, vm_prop_id_t prop)
    { buf[0] = VM_PROP; vmb_put_propid(buf + 1, prop); }

/* get the value portion of a vm_val_t from a portable dataholder */
void vmb_get_dh_val(const char *buf, vm_val_t *val);

/* get the type from a portable dataholder */
inline vm_datatype_t vmb_get_dh_type(const char *buf)
    { return (vm_datatype_t)buf[0]; }

/* get a vm_val_t from a portable dataholder */
inline void vmb_get_dh(const char *buf, vm_val_t *val)
    { val->typ = vmb_get_dh_type(buf); vmb_get_dh_val(buf, val); }

/* get an object value from a portable dataholder */
inline vm_obj_id_t vmb_get_dh_obj(const char *buf)
    { return (vm_obj_id_t)t3rp4u(buf+1); }

/* get an integer value from a portable dataholder */
inline int32_t vmb_get_dh_int(const char *buf)
    { return (int32_t)osrp4s(buf+1); }

/* get a property ID value from a portable dataholder */
inline vm_prop_id_t vmb_get_dh_prop(const char *buf)
    { return (vm_prop_id_t)osrp2(buf+1); }

/* get a constant offset value from a portable dataholder */
inline pool_ofs_t vmb_get_dh_ofs(const char *buf)
    { return (pool_ofs_t)t3rp4u(buf+1); }


#endif /* VMTYPE_H */

