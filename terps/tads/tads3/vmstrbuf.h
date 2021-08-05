/* $Header$ */

/* 
 *   Copyright (c) 2010 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmstrbuf.h - StringBuffer object
Function
  
Notes
  
Modified
  12/13/09 MJRoberts  - Creation
*/

#ifndef VMSTRBUF_H
#define VMSTRBUF_H

#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"


/* ------------------------------------------------------------------------ */
/*
 *   The image file data block is arranged as follows:
 *   
 *.  UINT4 buffer_length
 *.  UINT4 buffer_increment
 *.  UINT4 string_length
 *.  UINT2 string[string_length]
 *   
 *   buffer_length is the allocated size of the buffer.
 *   
 *   buffer_increment is the incremental allocation size; when we need to
 *   expand the buffer, we'll allocate in multiples of this size.
 *   
 *   string_length is the length of the string data; this is the amount of
 *   buffer space actually used.
 *   
 *   string is the string data, as 16-bit Unicode character values.  (Note
 *   that this is not stored in UTF-8 encoding as most other VM strings are.
 *   This is instead an array of 16-bit character values.) 
 */


/* ------------------------------------------------------------------------ */
/*
 *   Our in-memory extension data structure, which mimics the image file
 *   structure but uses native types.  
 */
struct vm_strbuf_ext
{
    /* allocate the structure */
    static vm_strbuf_ext *alloc_ext(VMG_ class CVmObjStringBuffer *self,
                                    int32_t alo, int32_t inc);

    /* expand an existing extension */
    static vm_strbuf_ext *expand_ext(VMG_ class CVmObjStringBuffer *self,
                                     vm_strbuf_ext *old_ext,
                                     int32_t new_len);

    /* the length of the string data (the part of the buffer in use) */
    int32_t len;

    /* the allocated buffer size */
    int32_t alo;

    /* the incremental allocation size */
    int32_t inc;

    /* 
     *   The string data, as 16-bit unicode character values.  We
     *   overallocate the structure to make room for an actual array length
     *   of [alo] elements here.  
     */
    wchar_t buf[1];
};


/* ------------------------------------------------------------------------ */
/* 
 *   undo action codes 
 */
enum strbuf_undo_action
{
    /* we inserted text into the buffer */
    STRBUF_UNDO_INS,

    /* we deleted text from the buffer */
    STRBUF_UNDO_DEL,

    /* we replaced text in the buffer */
    STRBUF_UNDO_REPL
};


/* ------------------------------------------------------------------------ */
/*
 *   StringBuffer metaclass 
 */

class CVmObjStringBuffer: public CVmObject
{
    friend class CVmMetaclassStringBuffer;
    
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

    /* is this a StringBuffer object? */
    static int is_string_buffer_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* create */
    static vm_obj_id_t create(VMG_ int in_root_set, int32_t alo, int32_t inc);

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
        return CVmObject::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* reserve constant data */
    virtual void reserve_const_data(VMG_ class CVmConstMapper *mapper,
                                    vm_obj_id_t self)
    {
        /* we cannot be converted to constant data */
    }
    
    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *,
                                       vm_obj_id_t)
    {
        /* we don't reference anything */
    }

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* 
     *   receive savepoint notification - we don't keep any
     *   savepoint-relative records, so we don't need to do anything here 
     */
    void notify_new_savept() { }
    
    /* apply an undo record */
    void apply_undo(VMG_ struct CVmUndoRecord *rec);
    
    /* discard an undo record */
    void discard_undo(VMG_ struct CVmUndoRecord *);
    
    /* we don't reference anything */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }

    /* we don't reference anything */
    void mark_refs(VMG_ uint) { }

    /* we don't keep any references */
    void remove_stale_weak_refs(VMG0_) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* reload from an image file */
    void reload_from_image(VMG_ vm_obj_id_t self,
                           const char *ptr, size_t siz);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* 
     *   determine if we've been changed since loading - assume we have (if
     *   we haven't, the only harm is the cost of unnecessarily reloading or
     *   saving) 
     */
    int is_changed_since_load() const { return TRUE; }

    /* get a value by index */
    int index_val_q(VMG_ vm_val_t *result,
                    vm_obj_id_t self,
                    const vm_val_t *index_val);
    
    /* set a value by index */
    int set_index_val_q(VMG_ vm_val_t *new_container,
                        vm_obj_id_t self,
                        const vm_val_t *index_val,
                        const vm_val_t *new_val);
    
    /* add an entry - does not generate undo */
    void add_entry(VMG_ const vm_val_t *key, const vm_val_t *val);

    /* set or add an entry - does not generate undo */
    void set_or_add_entry(VMG_ const vm_val_t *key, const vm_val_t *val);

    /* cast to string */
    virtual const char *cast_to_string(VMG_ vm_obj_id_t self,
                                       vm_val_t *new_str) const;

    /* equals - we equal a string or string buffer with the same text */
    virtual int equals(VMG_ vm_obj_id_t self, const vm_val_t *val, int) const;

    /* test for equality to a constant string */
    int equals_str(const char *str) const;

    /* compare - we can compare to a string or string buffer */
    virtual int compare_to(VMG_ vm_obj_id_t self, const vm_val_t *val) const;

    /* compare to a constant string - strcmp semantics */
    int compare_str(const char *str) const;

    /* calculate my hash value */
    virtual uint calc_hash(VMG_ vm_obj_id_t self, int /*depth*/) const;

    /* calculate the length in bytes of the contents in UTF-8 format */
    int32_t utf8_length() const;

    /*
     *   Copy a portion of the string to a buffer as UTF-8 bytes.
     *   
     *   'idx' is an in-out variable.  On input, it's the starting character
     *   index requested for the copy.  On output, it's updated to the index
     *   of the next character after the last character copied.  This can be
     *   used for piecewise copies, since it's updated to point to the start
     *   of the next piece after copying each piece.
     *   
     *   'bytelen' is an in-out variable.  On input, this is the number of
     *   bytes requested to copy to the buffer.  On output, it's the actual
     *   number of bytes copied.  This might be smaller than the request
     *   size, because (a) we won't copy past the end of the string, and (b)
     *   we'll only copy whole, well-formed character sequences, so if the
     *   requested number of bytes would copy a fractional character, we'll
     *   omit that fractional character and stop at the previous whole
     *   character.  
     */
    void to_utf8(char *buf, int32_t &idx, int32_t &bytelen);

    /* ensure there's enough space for the given buffer length */
    void ensure_space(VMG_ int32_t len);

    /* ensure there's enough space for the given ADDED buffer length */
    void ensure_added_space(VMG_ int32_t len)
        { ensure_space(vmg_ get_ext()->len + len); }

    /* 
     *   Get my string buffer.  Use with caution; the underlying buffer
     *   pointer can change if we modify the contents. 
     */
    const wchar_t *get_buf() const { return get_ext()->buf; }

    /* get the current character length */
    size_t get_len() const { return get_ext()->len; }

protected:
    /* get my extension data */
    vm_strbuf_ext *get_ext() const { return (vm_strbuf_ext *)ext_; }

    /* create a string object from a substring of the buffer */
    const char *substr_to_string(
        VMG_ vm_val_t *new_str, int32_t idx, int32_t len) const;

    /* adjust arguments to make sure they're within the buffer */
    void adjust_args(int32_t *idx, int32_t *len, int32_t *ins) const;

    /* move the tail of the buffer for a splice operation */
    void splice_move(VMG_ int32_t idx, int32_t del, int32_t ins);

    /* load or reload image data */
    void load_image_data(VMG_ const char *ptr, size_t siz);

    /* create a with no initial contents */
    CVmObjStringBuffer() { ext_ = 0; }

    /* create a string buffer with the given buffer size */
    CVmObjStringBuffer(VMG_ int32_t alo, int32_t inc);

    /* insert text into the buffer */
    void insert_text(VMG_ vm_obj_id_t self,
                     int32_t idx, const wchar_t *src, int32_t chars, int undo)
        { splice_text(vmg_ self, idx, 0, src, chars, undo); }

    /* insert a character */
    void insert_char(VMG_ vm_obj_id_t self,
                     int32_t idx, wchar_t ch, int undo)
        { splice_text(vmg_ self, idx, 0, &ch, 1, undo); }

    /* insert UTF-8 text into the buffer */
    void insert_text(VMG_ vm_obj_id_t self,
                     int32_t idx, const char *src, int32_t bytes, int undo)
        { splice_text(vmg_ self, idx, 0, src, bytes, undo); }

    /* delete text from the buffer */
    void delete_text(VMG_ vm_obj_id_t self,
                     int32_t idx, int32_t chars, int undo)
        { splice_text(vmg_ self, idx, chars, (wchar_t *)0, 0, undo); }

    /* splice text into the buffer */
    void splice_text(VMG_ vm_obj_id_t self,
                     int32_t idx, int32_t del_chars,
                     const wchar_t *src, int32_t ins_chars, int undo);
    void splice_text(VMG_ vm_obj_id_t self,
                     int32_t idx, int32_t del_chars,
                     const char *src, int32_t ins_bytes, int undo);

    /* property evaluator - undefined function */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluator - get length */
    int getp_len(VMG_ vm_obj_id_t, vm_val_t *, uint *);

    /* property evaluator - get a character at an index position */
    int getp_char_at(VMG_ vm_obj_id_t, vm_val_t *, uint *);

    /* property evaluator - append */
    int getp_append(VMG_ vm_obj_id_t, vm_val_t *, uint *);

    /* property evaluator - copyChars */
    int getp_copyChars(VMG_ vm_obj_id_t, vm_val_t *, uint *);

    /* property evaluator - insert */
    int getp_insert(VMG_ vm_obj_id_t, vm_val_t *, uint *);

    /* property evaluator - delete */
    int getp_delete(VMG_ vm_obj_id_t, vm_val_t *, uint *);

    /* property evaluator - splice */
    int getp_splice(VMG_ vm_obj_id_t, vm_val_t *, uint *);

    /* property evaluator - substr */
    int getp_substr(VMG_ vm_obj_id_t, vm_val_t *, uint *);

    /* add a record to the global undo stream */
    void add_undo_rec(VMG_ vm_obj_id_t self,
                      enum strbuf_undo_action action,
                      int32_t idx, int32_t old_len, int32_t new_len);

    /* property evaluation function table */
    static int (CVmObjStringBuffer::*func_table_[])(VMG_ vm_obj_id_t self,
        vm_val_t *retval, uint *argc);
};


/* ------------------------------------------------------------------------ */
/*
 *   StringBuffer registration table object 
 */
class CVmMetaclassStringBuffer: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "stringbuffer/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjStringBuffer();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjStringBuffer();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjStringBuffer::create_from_stack(vmg_ pc_ptr, argc); }
    
    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjStringBuffer::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMSTRBUF_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjStringBuffer)
