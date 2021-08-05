/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmstrcmp.h - T3 String Comparator intrinsic class
Function
  Defines the String Comparator intrinsic class, which provides native
  code that performs complex, parameterized string comparisons.  We offer
  the following customizable options for our comparisons:

  - We can match exactly on case, or without regard to case.

  - We can optionally match a value to a truncated reference value
  (which allows user input to use abbreviated forms of dictionary words, for
  example).  The minimum truncation length is a settable option.

  - We can use equivalence mappings that allow a given character in a
  reference string to match different characters in value strings.  For
  example, we could specify that an "a" with an acute accent in a reference
  string matches an unaccented "a" in a value string.  Each such mapping can
  specify result flag bits, so a caller can determine if particular
  equivalence mappings were used in making a match.

  This class implements the generic "comparator" interface, by providing
  a hash value calculator method and a value comparison method, so a String
  Comparator instance can be used as a Dictionary's comparator object.

  StringComparator objects are immutable; all of our parameters are set
  in the constructor.  This is desirable because it allows the object to be
  installed in a Dictionary (or any other hash table-based structure)
  without any danger that the hash table will need to be rebuilt as long as
  the same comparator is installed.
Notes

Modified
  09/01/02 MJRoberts  - Creation
*/

#ifndef VMSTRCMP_H
#define VMSTRCMP_H

#include <stdlib.h>
#include <os.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmglob.h"


/* ------------------------------------------------------------------------ */
/*
 *   Our serialized data stream, in both the image file and a saved file,
 *   consists of:
 *   
 *   UINT2 truncation_length
 *.  UINT2 flags
 *.  UINT2 equivalence_mapping_count
 *.  UINT2 equivalence_total_value_chars
 *.  equivalence_mappings
 *   
 *   The 'flags' value consists of the following combination of bit fields:
 *   
 *   0x0001 - the match is case-sensitive
 *   
 *   The 'equivalence_total_value_chars' gives the sum total of the value
 *   string characters in ALL of the equivalence mappings.  This value is
 *   stored simply to make it easier to calculate the memory allocation
 *   needs when loading this object.
 *   
 *   Each 'equivalence_mapping' entry is arranged like this:
 *   
 *   UINT2 reference_char
 *.  UBYTE value_char_count
 *.  UINT4 uc_result_flags
 *.  UINT4 lc_result_flags
 *.  UINT2 value_char[value_char_count]
 *   
 *   Each character is given as a 16-bit Unicode value.  These values map
 *   directly to the corresponding vmobj_strcmp_equiv structure entries.  
 */

/* ------------------------------------------------------------------------ */
/*
 *   Our in-memory extension.
 */
struct vmobj_strcmp_ext
{
    /* 
     *   The truncation length for reference strings, or zero if no
     *   truncation is allowed.  This is the minimum length that we must
     *   match when the value string is shorter than the reference string.  
     */
    size_t trunc_len;

    /* 
     *   Case sensitivity.  If this is true, then our matches are sensitive
     *   to case, which means that we must match each character exactly on
     *   case.  If this is false, then our matches are insensitive to case,
     *   so we can match an upper-case letter to the corresponding
     *   lower-case letter.  
     */
    int case_sensitive;

    /*
     *   Equivalence mapping table, giving the mapping for each "reference"
     *   string character.  This is a two-tiered array: the first tier is
     *   indexed by the high-order 8 bits of a reference character, and
     *   gives a pointer to the second tier array, or a null pointer if
     *   there is no mapping for any character with the given high-order 8
     *   bits.  The second tier is indexed by the low-order 8 bits, and
     *   gives a pointer to the equivalence mapping structure for the
     *   character, or a null pointer if there is no mapping for the
     *   character.  
     */
    struct vmobj_strcmp_equiv **equiv[256];
};

/*
 *   Equivalence mapping entry.  Note that we don't store the reference
 *   character in a mapping structure, because we can only reach these
 *   mapping structures by indexing the mapping array with the reference
 *   character, and thus must always already know the reference character
 *   before we can even reach one of these.  
 */
struct vmobj_strcmp_equiv
{
    /* string of value characters matching this reference character */
    size_t val_ch_cnt;
    wchar_t *val_ch;

    /* 
     *   Additive result flags for upper-case input matches: this value is
     *   bitwise-OR'd into the result code when this equivalence mapping is
     *   used to match the value to an upper-case input letter.  
     */
    unsigned long uc_result_flags;

    /* additive result flags for lower-case input matches */
    unsigned long lc_result_flags;
};

/* ------------------------------------------------------------------------ */
/*
 *   String Comparator intrinsic class 
 */
class CVmObjStrComp: public CVmObject
{
    friend class CVmMetaclassStrComp;

public:
    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObject::is_of_metaclass(meta));
    }

    /* am I a StringComparator object? */
    static int is_strcmp_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop)
    {
        /* defer to our base class */
        return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* undo operations - we are immutable and hence keep no undo */
    void notify_new_savept() { }
    void apply_undo(VMG_ struct CVmUndoRecord *) { }
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }    

    /* we reference no other objects */
    void mark_refs(VMG_ uint) { }
    void remove_stale_weak_refs(VMG0_) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t, const char *ptr, size_t);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixup);

    /*
     *   Direct Interface.  These functions correspond to methods we expose
     *   through the get_prop() interface, but can be called directly from
     *   the C++ code of other intrinsic classes (such as Dictionary) to
     *   avoid the overhead of going through the get_prop() mechanism.
     *   These are virtual to allow derived intrinsic classes to override
     *   the implementation of the public VM-visible interface.  
     */

    /* calculate a hash value for a constant string */
    virtual unsigned int calc_str_hash(const char *str, size_t len);

    /* match two strings */
    virtual unsigned long match_strings(const char *valstr, size_t vallen,
                                        const char *refstr, size_t reflen);

    /* 
     *   Check for an approximation match to a given character.  This checks
     *   the given input string for a match to the approximation for a given
     *   reference character.  Returns the number of characters in the match,
     *   or zero if there's no match.  
     */
    virtual size_t match_chars(const wchar_t *valstr, size_t valcnt,
                               wchar_t refchar);

    /* truncation length, in characters; 0 means no truncation length */
    virtual size_t trunc_len() const;
    
protected:
    /* create with no extension */
    CVmObjStrComp() { ext_ = 0; }

    /* delete my extension */
    void delete_ext(VMG0_);

    /* get my extension data */
    vmobj_strcmp_ext *get_ext() const { return (vmobj_strcmp_ext *)ext_; }

    /* load from an abstact stream object */
    void load_from_stream(VMG_ class CVmStream *str);

    /* 
     *   Write to an abstract stream object.  Returns the number of bytes
     *   actually needed to store the object.
     *   
     *   If 'bytes_avail' is non-null, it indicates the maximum number of
     *   bytes available for writing; if we need more than this amount, we
     *   won't write anything at all, but will simply return the number of
     *   bytes we actually need.  
     */
    ulong write_to_stream(VMG_ class CVmStream *str, ulong *bytes_avail);
    
    /* allocate and initialize our extension */
    void alloc_ext(VMG_ size_t trunc_len, int case_sensitive,
                   size_t equiv_cnt, size_t total_chars,
                   class CVmObjStrCompMapReader *reader);

    /* count of equivalence mappings */
    void count_equiv_mappings(size_t *equiv_cnt, size_t *total_ch_cnt);

    /* property evaluator - undefined property */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluator - calculate a hash value */
    int getp_calc_hash(VMG_ vm_obj_id_t, vm_val_t *val, uint *argc);

    /* property evaluator - match two values */
    int getp_match_values(VMG_ vm_obj_id_t, vm_val_t *val, uint *argc);

    /* property evaluation function table */
    static int (CVmObjStrComp::*func_table_[])(VMG_ vm_obj_id_t self,
                                               vm_val_t *retval, uint *argc);
};

/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassStrComp: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "string-comparator/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjStrComp();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjStrComp();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjStrComp::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjStrComp::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMSTRCMP_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjStrComp)

