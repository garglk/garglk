/* $Header: d:/cvsroot/tads/tads3/VMSTR.H,v 1.2 1999/05/17 02:52:28 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmstr.h - VM dynamic string implementation
Function
  
Notes
  
Modified
  10/28/98 MJRoberts  - Creation
*/

#ifndef VMSTR_H
#define VMSTR_H

#include "vmglob.h"
#include "vmobj.h"


/*
 *   String object
 */
class CVmObjString: public CVmObject
{
    friend class CVmMetaclassString;

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

    /* is the given object reference a string object? */
    static int is_string_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* create from stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* reserve constant data */
    virtual void reserve_const_data(VMG_ class CVmConstMapper *mapper,
                                    vm_obj_id_t self);

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *mapper,
                                       vm_obj_id_t self);

    /* get my datatype when converted to constant data */
    virtual vm_datatype_t get_convert_to_const_data_type() const
      { return VM_SSTRING; }

    /* create a string with no initial contents */
    static vm_obj_id_t create(VMG_ int in_root_set);

    /* create a string to hold a string of the given byte length */
    static vm_obj_id_t create(VMG_ int in_root_set, size_t bytelen);

    /* create from a constant UTF-8 string */
    static vm_obj_id_t create(VMG_ int in_root_set,
                              const char *str, size_t bytelen);

    /* create from a wide unicode string */
    static vm_obj_id_t create(VMG_ int in_root_set,
                              const wchar_t *str, size_t charlen);

    /* create from a byte stream, treating the bytes as Latin-1 characters */
    static vm_obj_id_t create_latin1(VMG_ int in_root_set,
                                     class CVmDataSource *src);

    /* create from a string, mapping through the given character mapper */
    static vm_obj_id_t create(VMG_ int in_root_set,
                              const char *str, size_t bytelen,
                              CCharmapToUni *cmap);

    /* 
     *   For construction: get a pointer to the string's underlying
     *   buffer.  Returns a pointer into which the caller can write.  The
     *   buffer starts after the length prefix. 
     */
    char *cons_get_buf() const { return ext_ + 2; }

    /* get the current construction length in bytes */
    size_t cons_get_len() const { return vmb_get_len(ext_); }

    /*
     *   Ensure space for a string under construction.  This expands the
     *   buffer if necessary.  'ptr' is a pointer into our buffer, giving the
     *   current write position, and 'len' is the amount of data we want to
     *   add.  If we already have enough space, we'll simply return 'ptr'.
     *   If not, we'll reallocate the buffer with enough space for the added
     *   chunk plus the given overhead margin, and return a new pointer that
     *   points to the same offset in the new buffer as 'ptr' did in the
     *   original.  
     */
    char *cons_ensure_space(VMG_ char *ptr, size_t len, size_t margin);

    /* 
     *   Append a string to the buffer during construction, expanding the
     *   buffer if necessary.  'ptr' is a pointer into our buffer to where
     *   the string is to be appended.  Returns the updated pointer to the
     *   end of the appended data.  
     */
    char *cons_append(VMG_ char *ptr, const char *addstr, size_t addlen,
                      size_t margin);

    /* append a character to the buffer during construction */
    char *cons_append(VMG_ char *ptr, wchar_t ch, size_t margin);

    /*
     *   Shrink the buffer to the given actual size to finalize construction.
     *   'ptr' is a pointer into the construction buffer; we'll reallocate
     *   the buffer so that it's just big enough for the text up to the byte
     *   before 'ptr', and set the string length accordingly.
     *   
     *   If the buffer is within a reasonable margin of the actual used size,
     *   we'll ignore the reallocation request.  Reallocation takes some work
     *   (to find new memory and copy the string contents), so if the memory
     *   savings won't be substantial, it's more efficient just to waste the
     *   memory.  In all likelihood the string will be collected as garbage
     *   eventually anyway, so the extra space will probably only be tied up
     *   temporarily.
     *   
     *   The caller must not call cons_set_len() prior to calling this,
     *   because we take the current length from our internal length element.
     */
    void cons_shrink_buffer(VMG_ char *ptr);

    /* 
     *   For construction: set my length.  This can be used if the string
     *   stored is smaller than the buffer allocated.  This cannot be used to
     *   expand the buffer, since this merely writes the length prefix and
     *   does not reallocate the buffer.
     *   
     *   When used with a pointer, the pointer must point within our buffer;
     *   we'll set the length according to the offset of the pointer from the
     *   start of the buffer.  This can be used for dynamic construction with
     *   the final write pointer after filling in the buffer's contents.  
     */
    void cons_set_len(size_t len) { vmb_put_len(ext_, len); }
    void cons_set_len(char *ptr) { vmb_put_len(ext_, ptr - ext_ - 2); }

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* call a static property */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop);

    /* undo operations - strings are immutable and hence keep no undo */
    void notify_new_savept() { }
    void apply_undo(VMG_ struct CVmUndoRecord *) { };
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }

    /* reference operations - strings reference no other objects */
    void mark_refs(VMG_ uint state) { }
    void remove_stale_weak_refs(VMG0_) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t, const char *ptr, size_t)
        { ext_ = (char *)ptr; }

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* 
     *   add a value to the string -- this creates a new string by
     *   appending the value to this string 
     */
    int add_val(VMG_ vm_val_t *result,
                vm_obj_id_t self, const vm_val_t *val);

    /*
     *   Get a string representation of the object.  This is trivial for a
     *   string object - we simply return our extension, which contains
     *   the string in the required format.  
     */
    const char *cast_to_string(VMG_ vm_obj_id_t self,
                               vm_val_t *new_str) const
    {
        /* we are the string object */
        new_str->set_obj(self);
        
        /* return our extension directly */
        return ext_;
    }

    /* convert a value to string via reflection services in the bytecode */
    static const char *reflect_to_str(
        VMG_ vm_val_t *new_str, char *result_buf, size_t result_buf_size,
        const vm_val_t *val, const char *fmt, ...);

    /* get the underlying string */
    const char *get_as_string(VMG0_) const { return ext_; }

    /* cast to integer */
    virtual long cast_to_int(VMG0_) const;

    /* cast to number */
    virtual void cast_to_num(VMG_ vm_val_t *val, vm_obj_id_t self) const;

    /* parse a string as an integer value or (optionally) a BigNumber */
    static void parse_num_val(VMG_ vm_val_t *retval,
                              const char *str, size_t len,
                              int radix, int int_only);

    /*
     *   Static routine to add a value to a string constant.  Creates a
     *   new string by appending the given value to the given string
     *   constant.  The string constant must be stored in portable format:
     *   the first two bytes are the length prefix, in UINT2 format,
     *   giving the length of the string's contents not counting the
     *   prefix itself; immediately following the length prefix are the
     *   bytes of the string's contents.  
     */
    static void add_to_str(VMG_ vm_val_t *result,
                           const vm_val_t *self, const vm_val_t *val);

    /* 
     *   Check a value for equality.  We will match any constant string
     *   that contains the same text as our string, and any other string
     *   object with the same text. 
     */
    int equals(VMG_ vm_obj_id_t self, const vm_val_t *val, int depth) const;

    /*
     *   Compare the string to another value.  If the other value is a
     *   constant string or string object, we'll perform a lexical
     *   comparison of the string; other types are not comparable to
     *   strings, so we'll throw an error for any other type.  
     */
    int compare_to(VMG_ vm_obj_id_t self, const vm_val_t *val) const;

    /* calculate a hash */
    uint calc_hash(VMG_ vm_obj_id_t self, int depth) const;

    /*
     *   Convert a value to a string.  Throws an error if the value is not
     *   convertible to a string.
     *   
     *   The result is stored in the given buffer, if possible, in portable
     *   string format (with a portable UINT2 length prefix followed by the
     *   string's bytes).  If the buffer is not provided or is not large
     *   enough to contain the result, we will allocate a new string object
     *   and return its contents; since the string object will never be
     *   referenced by anyone, it will be deleted in the next garbage
     *   collection pass.  In any case, we will return a pointer to a buffer
     *   containing the result string.
     *   
     *   We'll fill in *new_obj with the new string object value, or nil if
     *   we don't create a new string; this allows the caller to protect the
     *   allocated object from garbage collection if necessary.
     *   
     *   'flags' is a combination of TOSTR_xxx values (see vmobj.h).
     */
    static const char *cvt_to_str(VMG_ vm_val_t *new_obj,
                                  char *result_buf, size_t result_buf_size,
                                  const vm_val_t *val,
                                  int radix, int flags);

    /*
     *   Convert an integer to a string, storing the result in the given
     *   buffer in portable string format (with length prefix).  The radix
     *   can be from 2 to 36.
     *   
     *   If is_signed is true, we'll show a hyphen as the first character.
     *   Otherwise we'll treat the value as unsigned.
     *   
     *   For efficiency, we store the number at the end of the buffer (this
     *   makes it easy to generate the number, since we need to generate
     *   numerals in reverse order).  We return a pointer to the result,
     *   which may not start at the beginning of the buffer.
     *   
     *   'flags' is a combination of TOSTR_xxx flags (see vmobj.h).  
     */
    static char *cvt_int_to_str(char *buf, size_t buflen,
                                int32_t inval, int radix, int flags);

    /*
     *   Allocate a string buffer large enough to hold a given value.
     *   We'll use the provided buffer if possible.
     *   
     *   If the provided buffer is null or is not large enough, we'll
     *   allocate a new string object with a large enough buffer to hold
     *   the value, and return the object's extension as the buffer.
     *   
     *   The buffer size and requested size are in bytes.
     *   
     *   If we allocate a new object, we'll set new_obj to the object
     *   value; otherwise we'll set new_obj to nil.  
     */
    static char *alloc_str_buf(VMG_ vm_val_t *new_obj,
                               char *buf, size_t buf_size,
                               size_t required_size);

    /*
     *   Constant string equality test routine.  Compares the given
     *   constant string (in portable format, with leading UINT2 length
     *   prefix followed by the string's text in UTF8 format) to the other
     *   value.  Returns true if the values are lexically identical, false
     *   if not or if the other value is not a string of some kind.  
     */
    static int const_equals(VMG_ const char *str, const vm_val_t *val);

    /*
     *   Constant string hash value calculation 
     */
    static uint const_calc_hash(const char *str);

    /*
     *   Constant string magnitude comparison routine.  Compares the given
     *   constant string (in portable format) to the other value.  Returns
     *   a positive value if the constant string is lexically greater than
     *   the other value, a negative value if the constant string is
     *   lexically less than the other value, or zero if the two values
     *   are identical.  Throws an error for any other type of value.  
     */
    static int const_compare(VMG_ const char *str, const vm_val_t *val);

    /*
     *   Find a substring within a string.  Returns a pointer to to the start
     *   of the substring within the string, or null if the substring isn't
     *   found.  If 'idxp' is non-null, we'll fill in *idxp with the
     *   character index, starting at zero for the first character, of the
     *   substring within the string.
     *   
     *   start_idx is the 1-based index where we start the search.  If this
     *   is negative, it's an index from the end of the string (-1 for the
     *   last character).
     *   
     *   Both strings are in standard constant string format, with UINT2
     *   length prefixes.  
     */
    static const char *find_substr(
        VMG_ const char *str, int32_t start_idx,
        const char *substr, size_t *idxp);

    /*
     *   Find a substring or RexPattern.  Returns a pointer to the matching
     *   substring, or null if no match is found.
     *   
     *   'basestr' is the base string we're searching, and 'str' and 'len'
     *   give the starting point and remaining length of the substring of
     *   'basestr' where the search actually begins.  'basestr' and 'str'
     *   point directly to the bytes to search, not a VMB_LEN prefix (which
     *   is why we have 'len' as a separate argument).  We won't actually
     *   search the portion of the base string before 'str', but we need to
     *   know where the overall string starts to properly handle certain
     *   regex assertions (e.g., '^').
     *   
     *   'substr' is the tads-style (VMB_LEN-prefixed) substring we're
     *   looking for; if 'substr' is null, 'pat' is the regular expression
     *   object to match, otherwise 'pat' is ignored.
     *   
     *   'match_idx' returns with the CHARACTER index from 'str' (starting at
     *   0 for the first byte of 'str') of the substring we found, if
     *   successful; otherwise this is undefined.
     *   
     *   'match_len' returns with the BYTE length of the substring we
     *   matched, if successful; otherwise this is undefined.
     */
    static const char *find_substr(
        VMG_ const vm_val_t *strval,
        const char *basestr, const char *str, size_t len,
        const char *substr, class CVmObjPattern *pat,
        int *match_idx, int *match_len);

    /* find the last matching substring or pattern */
    static const char *find_last_substr(
        VMG_ const vm_val_t *strval,
        const char *basestr, const char *str, size_t len,
        const char *substr, class CVmObjPattern *pat,
        int *match_idx, int *match_len);
                                   

    /*
     *   Evaluate a property of a constant string value.  Returns true if
     *   we successfully evaluated the property, false if the property is
     *   not one of the properties that the string class defines.  
     */
    static int const_get_prop(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                              const char *str, vm_prop_id_t prop,
                              vm_obj_id_t *srcobj, uint *argc);

    /* evaluate a property */
    virtual int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                         vm_obj_id_t self, vm_obj_id_t *source_obj,
                         uint *argc);

    /* property evaluator - undefined property */
    static int getp_undef(VMG_ vm_val_t *, const vm_val_t *,
                          const char *, uint *)
        { return FALSE; }

    /* property evaluator - get the length */
    static int getp_len(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                        const char *str, uint *argc);

    /* property evaluator - extract a substring */
    static int getp_substr(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                           const char *str, uint *argc);

    /* property evaluator - toUpper */
    static int getp_upper(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                          const char *str, uint *argc);

    /* property evaluator - toLower */
    static int getp_lower(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                          const char *str, uint *argc);

    /* property evaluator - toTitleCase */
    static int getp_toTitleCase(
        VMG_ vm_val_t *retval, const vm_val_t *self_val,
        const char *str, uint *argc);

    /* property evaluator - toFoldedCase */
    static int getp_toFoldedCase(VMG_ vm_val_t *retval,
                                 const vm_val_t *self_val,
                                 const char *str, uint *argc);

    /* common handler for case conversions (toUpper, toLower, etc) */
    static int gen_getp_case_conv(VMG_ vm_val_t *retval,
                                  const vm_val_t *self_val,
                                  const char *str, uint *argc,
                                  const wchar_t *(*conv)(wchar_t));
    
    /* property evaluator - find substring */
    static int getp_find(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                         const char *str, uint *argc);

    /* common handler for find() and findLast() */
    template<int dir> static inline int find_common(
        VMG_ vm_val_t *retval,
        const vm_val_t *self_val, const char *str,
        uint *argc);

    /* property evaluator - convert to unicode */
    static int getp_to_uni(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                           const char *str, uint *argc);

    /* property evaluator - htmlify */
    static int getp_htmlify(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                            const char *str, uint *argc);

    /* property evaluator - startsWith */
    static int getp_starts_with(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val,
                                const char *str, uint *argc);

    /* property evaluator - endsWith */
    static int getp_ends_with(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *str, uint *argc);

    /* property evaluator - mapToByteArray */
    static int getp_to_byte_array(VMG_ vm_val_t *retval,
                                  const vm_val_t *self_val,
                                  const char *str, uint *argc);

    /* property evaluator - findReplace() - replace substring */
    static int getp_replace(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                            const char *str, uint *argc);

    /* property evaluator - splice */
    static int getp_splice(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                           const char *str, uint *argc);

    /* property evaluator - split */
    static int getp_split(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                          const char *str, uint *argc);

    /* property evaluator - specialsToHtml */
    static int getp_specialsToHtml(VMG_ vm_val_t *retval,
                                   const vm_val_t *self_val,
                                   const char *str, uint *argc);

    /* property evaluator - specialsToText */
    static int getp_specialsToText(VMG_ vm_val_t *retval,
                                   const vm_val_t *self_val,
                                   const char *str, uint *argc);

    /* property evaluator - urlEncode */
    static int getp_urlEncode(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *str, uint *argc);
    
    /* property evaluator - urlDecode */
    static int getp_urlDecode(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *str, uint *argc);

    /* property evaluator - sha256 hash */
    static int getp_sha256(VMG_ vm_val_t *retval,
                           const vm_val_t *self_val,
                           const char *str, uint *argc);

    /* property evaluator - md5 hash */
    static int getp_md5(VMG_ vm_val_t *retval,
                        const vm_val_t *self_val,
                        const char *str, uint *argc);

    /* property evaluator - packBytes */
    static int getp_packBytes(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *str, uint *argc)
        { return static_getp_packBytes(vmg_ retval, argc); }

    /* static property - packBytes */
    static int static_getp_packBytes(VMG_ vm_val_t *retval, uint *argc);

    /* property evaluator - unpackBytes */
    static int getp_unpackBytes(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val,
                                const char *str, uint *argc);

    /* property evaluator - compareTo */
    static int getp_compareTo(VMG_ vm_val_t *retval,
                              const vm_val_t *self_val,
                              const char *str, uint *argc);

    /* property evaluator - compareIgnoreCase */
    static int getp_compareIgnoreCase(VMG_ vm_val_t *retval,
                                      const vm_val_t *self_val,
                                      const char *str, uint *argc);

    /* property evaluator - findLast */
    static int getp_findLast(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                             const char *str, uint *argc);

    /* property evaluator - findAll */
    static int getp_findAll(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                            const char *str, uint *argc);

    /* property evaluator - match */
    static int getp_match(VMG_ vm_val_t *retval, const vm_val_t *self_val,
                          const char *str, uint *argc);


protected:
    /* create a string with no initial contents */
    CVmObjString() { ext_ = 0; }

    /* create with a given buffer size in bytes */
    CVmObjString(VMG_ size_t bytelen);

    /* create from a constant UTF-8 string */
    CVmObjString(VMG_ const char *str, size_t bytelen);

    /*
     *   Set the length of the string.  This can be used after a string is
     *   constructed to set the size of the actual stored string. 
     */
    void set_length(size_t bytelen) { vmb_put_len(ext_, bytelen); }

    /* copy bytes into the string buffer */
    void copy_into_str(const char *str, size_t bytelen)
        { memcpy(ext_ + VMB_LEN, str, bytelen); }

    /* copy bytes into the string buffer starting at the given byte offset */
    void copy_into_str(size_t ofs, const char *str, size_t bytelen)
        { memcpy(ext_ + VMB_LEN + ofs, str, bytelen); }

    /* 
     *   Copy bytes from the byte array into the string buffer, treating the
     *   bytes as Latin-1 characters.  Returns the number of bytes used in
     *   the output buffer.  If the output exceeds the available length in
     *   'len', we won't store any bytes past 'len', but we'll still
     *   calculate the full needed length and return it.  Call with len == 0
     *   to scan the string and get the required allocation length.  
     */
    static size_t copy_latin1_to_string(char *str, size_t len,
                                        class CVmDataSource *src);

    /* common handler for specialsToText and specialsToHtml */
    static int specialsTo(VMG_ vm_val_t *retval,
                          const vm_val_t *self_val,
                          const char *str, uint *argc, int html);
    
    /* property evaluation function table */
    static int (*func_table_[])(VMG_ vm_val_t *retval,
                                const vm_val_t *self_val,
                                const char *str, uint *argc);
};

/* ------------------------------------------------------------------------ */
/*
 *   A constant string is exactly like an ordinary string, except that our
 *   contents come from the constant pool.  We store a pointer directly to
 *   our constant pool data rather than making a separate copy.  The only
 *   thing we have to do differently from an ordinary string is that we don't
 *   delete our extension when we're deleted, since our extension is really
 *   just a pointer into the constant pool.  
 */
class CVmObjStringConst: public CVmObjString
{
public:
    /* notify of deletion */
    void notify_delete(VMG_ int /*in_root_set*/)
    {
        /* 
         *   do nothing, since our extension is just a pointer into the
         *   constant pool 
         */
    }

    /* create from constant pool data */
    static vm_obj_id_t create(VMG_ const char *const_ptr);

protected:
    /* construct from constant pool data */
    CVmObjStringConst(VMG_ const char *const_ptr)
    {
        /* point our extension directly to the constant pool data */
        ext_ = (char *)const_ptr;
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassString: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "string/030008"; }
    
    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjString();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjString();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjString::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjString::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMSTR_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjString)

