/* $Header$ */

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmdict.h - VM 'Dictionary' metaclass
Function
  
Notes
  
Modified
  01/24/00 MJRoberts  - Creation
*/

#ifndef VMDICT_H
#define VMDICT_H

#include <stdlib.h>
#include <string.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"
#include "vmhash.h"
#include "utf8.h"


/* forward-declare the class */
class CVmObjDict;

/* ------------------------------------------------------------------------ */
/* 
 *   undo action codes 
 */
enum dict_undo_action
{
    /* we added this word to the dictionary (undo by deleting it) */
    DICT_UNDO_ADD,

    /* we deleted this word from the dictionary (undo by adding it back) */
    DICT_UNDO_DEL,

    /* we change the comparator object */
    DICT_UNDO_COMPARATOR
};


/* ------------------------------------------------------------------------ */
/*
 *   The image file data block is arranged as follows:
 *   
 *.  UINT4 comparator_object_id
 *.  UINT2 load_image_entry_count
 *.  entry 1
 *.  entry 2
 *.  ...
 *.  entry N
 *   
 *   Each entry has the following structure:
 *   
 *.  UCHAR key_string_byte_length
 *.  key_string (UTF-8 characters, not null terminated, XOR'ed with 0xBD)
 *.  UINT2 number of sub-entries
 *.  sub-entry 1
 *.  sub-entry 2
 *.  etc
 *   
 *   Each sub-entry is structured like this:
 *   
 *   UINT4 associated_object_id
 *.  UINT2 defining_property_id
 *   
 *   Note that each byte of the key string is XOR'ed with the arbitrary
 *   byte value 0xBD.  This is simply to provide a minimal level of
 *   obfuscation in the image file to prevent casual browsing of the image
 *   contents.  
 */
/*   
 *   Separately, we maintain a hash table and entries in the hash table.  We
 *   don't attempt to keep the hash table in a portable format, which means
 *   that we have to rebuild the hash table on restoring a saved state file.
 */

/* comparator object types */
enum vm_dict_comp_type
{
    VMDICT_COMP_NONE,                               /* no comparator object */
    VMDICT_COMP_STRCOMP,                                /* StringComparator */
    VMDICT_COMP_GENERIC                        /* generic comparator object */
};

/*
 *   Dictionary object extension - we use this memory (in the variable
 *   heap) to keep track of our image data and our hash table.  
 */
struct vm_dict_ext
{
    /* pointer to load image data, if any */
    const char *image_data_;
    size_t image_data_size_;

    /* pointer to our hash table (in the system heap) */
    class CVmHashTable *hashtab_;

    /* flag: the table has been changed since image load */
    ulong modified_;

    /* our comparator object, if any */
    vm_obj_id_t comparator_;
    CVmObject *comparator_obj_;

    /* type of comparator */
    vm_dict_comp_type comparator_type_;

    /* Trie of our entries, for spelling correction */
    struct vmdict_TrieNode *trie_;
};


/* ------------------------------------------------------------------------ */
/*
 *   For spelling correction, we maintain a Trie on the dictionary in
 *   parallel to the hash table.  
 */

/* 
 *   Trie Node - a node in the trie tree.
 */
struct vmdict_TrieNode
{
    vmdict_TrieNode(vmdict_TrieNode *nxt, wchar_t c)
    {
        /* remember the transition character from our parent to us */
        ch = c;

        /* we don't have any words in this node yet */
        word_cnt = 0;

        /* remember our next sibling link */
        this->nxt = nxt;

        /* we don't have any children yet */
        chi = 0;
    }

    ~vmdict_TrieNode()
    {
        /* delete the subtree */
        while (chi != 0)
        {
            /* unlink this child */
            vmdict_TrieNode *n = chi;
            chi = chi->nxt;

            /* delete it */
            delete n;
        }
    }

    /* add a word */
    void add_word(const char *str, size_t len);

    /* find a word */
    vmdict_TrieNode *find_word(const char *str, size_t len);

    /* delete a word */
    void del_word(const char *str, size_t len);

    /* 
     *   Transition character from parent to this node.  Append this
     *   character to the string that reached the parent node to get the
     *   string that reaches the child node. 
     */
    wchar_t ch;

    /* 
     *   Number of words at this node.  Since we use the Trie only for
     *   spelling correction, we don't care about any of the other data
     *   stored per word; we can look that up through the hash table.  All we
     *   care about is whether or not a given node has any words associated
     *   with it.  Since the same word can be entered in the dictionary
     *   several times, we need to keep a count - that way, we can track
     *   dynamic removal of words by matching deletions against insertions.  
     */
    int word_cnt;

    /* the head of the list of child nodes */
    vmdict_TrieNode *chi;

    /* our next sibling */
    vmdict_TrieNode *nxt;
};


/* ------------------------------------------------------------------------ */
/*
 *   Dictionary object interface 
 */
class CVmObjDict: public CVmObject
{
    friend class CVmMetaclassDict;

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
        { return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop); }

    /* determine if an object is a Dictionary object */
    static int is_dictionary_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* receive notification of a new undo savepoint */
    void notify_new_savept() { }

    /* apply undo */
    void apply_undo(VMG_ struct CVmUndoRecord *rec);

    /* discard additional information associated with an undo record */
    void discard_undo(VMG_ struct CVmUndoRecord *rec);

    /* mark a reference in undo */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *rec);

    /* remove stale weak references from an undo record */
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *rec);

    /* mark references */
    void mark_refs(VMG_ uint);

    /* remove weak references */
    void remove_stale_weak_refs(VMG0_);

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t, const char *ptr, size_t siz);

    /* perform post-load initialization */
    void post_load_init(VMG_ vm_obj_id_t self);

    /* restore to image file state */
    void reset_to_image(VMG_ vm_obj_id_t /*self*/);

    /* determine if the object has been changed since it was loaded */
    int is_changed_since_load() const;

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *mapper,
                                       vm_obj_id_t self);

    /* enumerate the properties for which a word is defined */
    void enum_word_props(VMG_ void (*cb_func)(VMG_ void *, vm_prop_id_t,
                                              const vm_val_t *),
                         void *cb_ctx, const vm_val_t *strval,
                         const char *strp, size_t strl);

    /* get/set my comparator object */
    vm_obj_id_t get_comparator() const { return get_ext()->comparator_; }
    void set_comparator(VMG_ vm_obj_id_t obj);

    /* 
     *   Match a pair of strings.
     *   
     *   'valstrval' is the vm_val_t with the string value, if one is
     *   available; if not, this can be given as null and we'll synthesize a
     *   new string object from 'valstr' and 'vallen' if one is needed.  We
     *   don't require the caller to synthesize a string object because we
     *   might not need one; we'll create one only if one is actually needed.
     *   
     *   'valstr' and 'vallen' directly give the text of the value string,
     *   and 'refstr' and 'reflen' likewise directly give the text of the
     *   reference string.  
     */
    int match_strings(VMG_ const vm_val_t *valstrval,
                      const char *valstr, size_t vallen,
                      const char *refstr, size_t reflen,
                      vm_val_t *result_val);

    /* 
     *   Calculate the hash value for a string.  As in match_strings(),
     *   'valstrval' can be passed as null if no vm_val_t for the string text
     *   is available, in which case we'll synthesize a new string object
     *   from 'valstr' and 'vallen' if one is needed. 
     */
    unsigned int calc_str_hash(VMG_ const vm_val_t *valstrval,
                               const char *valstr, size_t vallen);

    /* add a string/obj/prop association from a VMB_LEN+buffer string */
    void add_word(VMG_ vm_obj_id_t self, const char *str,
                  vm_obj_id_t obj, vm_prop_id_t prop)
        { add_word(vmg_ self, str + VMB_LEN, vmb_get_len(str), obj, prop); }

    /* add a string/obj/prop association */
    void add_word(VMG_ vm_obj_id_t self, const char *str, size_t len,
                  vm_obj_id_t obj, vm_prop_id_t voc_prop);

    /* remove a string/obj/prop association, given a VMB_LEN+buffer string */
    void del_word(VMG_ vm_obj_id_t self, const char *str,
                  vm_obj_id_t obj, vm_prop_id_t prop)
        { del_word(vmg_ self, str + VMB_LEN, vmb_get_len(str), obj, prop); }

    /* remove a string/obj/prop association */
    void del_word(VMG_ vm_obj_id_t self, const char *str, size_t len,
                  vm_obj_id_t obj, vm_prop_id_t voc_prop);

protected:
    CVmObjDict(VMG0_);

    /* set the comparator type */
    void set_comparator_type(VMG_ vm_obj_id_t obj);

    /* create or re-create the hash table */
    void create_hash_table(VMG0_);

    /* fill the hash table with entries from the image data */
    void build_hash_from_image(VMG0_);

    /* build the Trie from the hash table */
    void build_trie(VMG0_);

    /* property evaluation - undefined property */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* enumeration callback for getp_find */
    static void find_cb(void *ctx, class CVmHashEntry *entry);

    /* enumeration callback for getp_isdef */
    static void isdef_cb(void *ctx, class CVmHashEntry *entry);

    /* enumeration callback for enum_word_props */
    static void enum_word_props_cb(void *ctx, class CVmHashEntry *entry);

    /* property evaluation - set the comparator object */
    int getp_set_comparator(VMG_ vm_obj_id_t, vm_val_t *val, uint *argc);

    /* property evaluation - findWord */
    int getp_find(VMG_ vm_obj_id_t, vm_val_t *val, uint *argc);

    /* property evaluation - addWord */
    int getp_add(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluation - delWord */
    int getp_del(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluation - isWordDefined */
    int getp_is_defined(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluation - forEachWord */
    int getp_for_each_word(VMG_ vm_obj_id_t self, vm_val_t *retval,
                           uint *argc);

    /* property evaluation - find corrections for a misspelled word */
    int getp_correct(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* get my extension, properly cast */
    vm_dict_ext *get_ext() const { return (vm_dict_ext *)ext_; }

    /* 
     *   add an entry; returns true if successful, false if the entry
     *   already exists, in which case no new entry is made 
     */
    int add_hash_entry(VMG_ const char *p, size_t len, int copy,
                       vm_obj_id_t obj, vm_prop_id_t prop, int from_image);

    /* delete an entry */
    int del_hash_entry(VMG_ const char *p, size_t len,
                       vm_obj_id_t obj, vm_prop_id_t prop);

    /* callback for hash table enumeration - delete stale weak refs */
    static void remove_weak_ref_cb(void *ctx, class CVmHashEntry *entry);

    /* callback for hash table enumeration - save file */
    static void save_file_cb(void *ctx, class CVmHashEntry *entry);
    
    /* callback for hash table enumeration - convert to constant data */
    static void cvt_const_cb(void *ctx, class CVmHashEntry *entry);

    /* callback for hash table enumeration - rebuild image, phase 1 */
    static void rebuild_cb_1(void *ctx, class CVmHashEntry *entry);

    /* callback for hash table enumeration - rebuild image, phase 2 */
    static void rebuild_cb_2(void *ctx, class CVmHashEntry *entry);

    /* allocate an undo record */
    struct dict_undo_rec *alloc_undo_rec(enum dict_undo_action action,
                                         const char *txt, size_t len);

    /* add a record to the global undo stream */
    void add_undo_rec(VMG_ vm_obj_id_t self, struct dict_undo_rec *rec);

    /* function table */
    static int (CVmObjDict::*func_table_[])(VMG_ vm_obj_id_t self,
                                            vm_val_t *retval, uint *argc);
};


/* ------------------------------------------------------------------------ */
/*
 *   hash table list entry - each hash entry has a list of associated
 *   objects; this structure represents an entry in one of these lists 
 */
struct vm_dict_entry
{
    vm_dict_entry(vm_obj_id_t obj, vm_prop_id_t prop, int from_image)
    {
        obj_ = obj;
        prop_ = prop;
        from_image_ = from_image;
        nxt_ = 0;
    }

    /* object ID of this entry */
    vm_obj_id_t obj_;

    /* defining property ID */
    vm_prop_id_t prop_;

    /* next entry in the list */
    vm_dict_entry *nxt_;

    /* flag: entry is from the image file */
    int from_image_;
};

/*
 *   Dictionary object hash table entry 
 */
class CVmHashEntryDict: public CVmHashEntryCS
{
public:
    CVmHashEntryDict(const char *str, size_t len, int copy, int from_image)
        : CVmHashEntryCS(str, len, copy || from_image)
    {
        /* nothing in our item list yet */
        list_ = 0;
    }

    ~CVmHashEntryDict()
    {
        /* delete all of our entries */
        while (list_ != 0)
        {
            vm_dict_entry *nxt;

            /* note the next entry */
            nxt = list_->nxt_;

            /* delete the current entry */
            delete list_;

            /* move on to the next entry */
            list_ = nxt;
        }
    }

    /* get the first entry in our list */
    vm_dict_entry *get_head() const { return list_; }

    /* 
     *   add an entry to our list - returns true if we added the entry,
     *   false if an entry with the same object ID was already present, in
     *   which case we will ignore the addition 
     */
    int add_entry(vm_obj_id_t obj, vm_prop_id_t prop, int from_image)
    {
        vm_dict_entry *entry;
        vm_dict_entry *prv;

        /* 
         *   check to see if this entry is already in our list - if it is,
         *   don't bother adding the redundant definition 
         */
        for (prv = 0, entry = list_ ; entry != 0 ; entry = entry->nxt_)
        {
            /* 
             *   if this entry matches the object ID and property ID,
             *   ignore the addition 
             */
            if (entry->obj_ == obj && entry->prop_ == prop)
                return FALSE;

            /* 
             *   if this entry matches the property, remember it as the
             *   insertion point, so that we keep all definitions for the
             *   same word with the same property together in the list 
             */
            if (entry->prop_ == prop)
                prv = entry;
        }

        /* create a list entry */
        entry = new vm_dict_entry(obj, prop, from_image);

        /* 
         *   link it in after the insertion point, or at the head of our
         *   list if we didn't find any other insertion point 
         */
        if (prv != 0)
        {
            /* we found an insertion point - link it in */
            entry->nxt_ = prv->nxt_;
            prv->nxt_ = entry;
        }
        else
        {
            /* no insertion point - link it at the head of our list */
            entry->nxt_ = list_;
            list_ = entry;
        }

        /* we added the entry */
        return TRUE;
    }

    /* 
     *   Delete all entries matching a given object ID from our list.
     *   Returns true if any entries were deleted, false if not. 
     */
    int del_entry(CVmHashTable *table, vm_obj_id_t obj, vm_prop_id_t prop)
    {
        vm_dict_entry *cur;
        vm_dict_entry *nxt;
        vm_dict_entry *prv;
        int found;

        /* find the entry in our list */
        for (found = FALSE, prv = 0, cur = list_ ; cur != 0 ;
             prv = cur, cur = nxt)
        {
            /* remember the next entry */
            nxt = cur->nxt_;

            /* if this is our entry, delete it */
            if (cur->obj_ == obj && cur->prop_ == prop)
            {
                /* unlink this entry */
                if (prv != 0)
                    prv->nxt_ = nxt;
                else
                    list_ = nxt;

                /* delete this entry */
                delete cur;

                /* note that we found at least one entry to delete */
                found = TRUE;
            }
        }

        /* if our list is now empty, delete myself from the table */
        if (list_ == 0)
        {
            /* remove myself from the table */
            table->remove(this);

            /* delete myself */
            delete this;
        }

        /* tell the caller whether we found anything to delete */
        return found;
    }

protected:
    /* list of associated objects */
    vm_dict_entry *list_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassDict: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "dictionary2/030001"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
        { new (vmg_ id) CVmObjDict(vmg0_); }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
        { new (vmg_ id) CVmObjDict(vmg0_); }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjDict::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjDict::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMDICT_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjDict)
