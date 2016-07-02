/* $Header$ */

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmgram.h - T3 grammar-production metaclass
Function
  
Notes
  
Modified
  02/15/00 MJRoberts  - Creation
*/

#ifndef VMGRAM_H
#define VMGRAM_H

#include <stdlib.h>
#include <string.h>

#include "os.h"
#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"

/* ------------------------------------------------------------------------ */
/*
 *   intrinsic function vector indices 
 */
enum vmobjgram_meta_fnset
{
    /* undefined function */
    VMOBJGRAM_UNDEF = 0,

    /* parseTokens(tokenList, dict) */
    VMOBJGRAM_PARSE = 1
};

/* ------------------------------------------------------------------------ */
/*
 *   Match types
 */
enum vmgram_match_type
{
    /* uninitialized */
    VMGRAM_MATCH_UNDEF = 0,
    
    /* production - matches a sub-production */
    VMGRAM_MATCH_PROD = 1,

    /* 
     *   part of speech - matches a word that appears in the dictionary
     *   under a particular part of speech 
     */
    VMGRAM_MATCH_SPEECH = 2,

    /* literal - matches a literal string */
    VMGRAM_MATCH_LITERAL = 3,

    /* token type - matches any token of a given type */
    VMGRAM_MATCH_TOKTYPE = 4,

    /* star - matches all remaining input tokens */
    VMGRAM_MATCH_STAR = 5,

    /* 
     *   N parts of speech - matches a word that appears in the dictionary
     *   under any of a set of N parts of speech 
     */
    VMGRAM_MATCH_NSPEECH = 6
};

/* ------------------------------------------------------------------------ */
/*
 *   Grammar production object - image file format
 *   
 *   UINT2 alt_count
 *.  alternative 1
 *.  alternative 2
 *.  etc
 *   
 *   Each alternative has the following structure:
 *   
 *.  INT2 score
 *.  INT2 badness
 *.  UINT4 processor_object_id
 *.  UINT2 token_count
 *.  token 1
 *.  token 2
 *.  etc
 *   
 *   Each token has this structure:
 *   
 *   UINT2 property_association
 *.  BYTE token_match_type (see below)
 *.  extra data depending on token_match_type (see below)
 *   
 *   The extra data for the token varies by match type:
 *   
 *   VMGRAM_MATCH_PROD - a UINT4 giving the production object ID
 *   
 *   VMGRAM_MATCH_SPEECH - a UINT2 giving the vocabulary property
 *   
 *   VMGRAM_MATCH_NSPEECH - a UINT2 giving a count, then that many
 *   additional UINT2's giving a list of vocabulary properties
 *   
 *   VMGRAM_MATCH_LITERAL - a UINT2 byte-length prefix followed by the
 *   UTF8-encoded bytes of the literal string
 *   
 *   VMGRAM_MATCH_TOKTYPE - a UINT4 giving the token enum's ID
 *   
 *   VMGRAM_MATCH_STAR - no additional data 
 */

/* property/match result enumeration entry */
struct vmgram_match_info
{
    vm_prop_id_t prop;
};

/*
 *   Grammar production object extension 
 */
struct vm_gram_ext
{
    /* pointer to load image data, if any */
    const char *image_data_;
    size_t image_data_size_;

    /* 
     *   The last comparator object we used to calculate hash values for
     *   literals.  Each time we need literal hash values, we'll check to see
     *   if we're using the same comparator we were last time; if so, we'll
     *   use the cached hash values, otherwise we'll recalculate them.  We
     *   reference this object weakly.  
     */
    vm_obj_id_t comparator_;

    /* flag: our rule list has been modified since load time */
    uint modified_ : 1;

    /* flag: we've cached hash values for our literals */
    uint hashes_cached_ : 1;

    /* 
     *   flag: there's at least one circular rule among my rules (i.e.,
     *   there's a rule whose first element is a self-reference
     *   subproduction) 
     */
    uint has_circular_alt : 1;

    /* private memory pool - we use this to make allocation cheaper */
    class CVmGramProdMem *mem_;

    /*
     *   Property list enumeration space.  We use this to build a list of
     *   properties for which a dictionary word is defined.  We'll expand
     *   this list as needed when we find we need more space. 
     */
    vmgram_match_info *prop_enum_arr_;
    size_t prop_enum_max_;

    /* array of rule alternatives */
    struct vmgram_alt_info **alts_;

    /* number of alternatives currently in use */
    size_t alt_cnt_;

    /* number of alternatives allocated */
    size_t alt_alo_;
};

/*
 *   Alternative object.  Each of these objects represents one of our rule
 *   alternatives. 
 */
struct vmgram_alt_info
{
    vmgram_alt_info(size_t ntoks);
    ~vmgram_alt_info();

    /* the alternative's score and badness values */
    int score;
    int badness;

    /* flag: this alternative is marked for deletion */
    int del;

    /* 
     *   the "processor object" for this alternative - this is the class we
     *   instantiate to represent a match to the alternative 
     */
    vm_obj_id_t proc_obj;

    /* array of token elements in the alternative */
    struct vmgram_tok_info *toks;
    size_t tok_cnt;
};

/*
 *   Grammar rule token entry.  This represents a token in a grammar rule. 
 */
struct vmgram_tok_info
{
    vmgram_tok_info() { typ = VMGRAM_MATCH_UNDEF; }
    ~vmgram_tok_info()
    {
        switch (typ)
        {
        case VMGRAM_MATCH_LITERAL:
            t3free(typinfo.lit.str);
            break;

        case VMGRAM_MATCH_NSPEECH:
            t3free(typinfo.nspeech.props);
            break;
        }
    }

    void set_literal(const char *str, size_t len)
    {
        set_literal(len);
        memcpy(typinfo.lit.str, str, len);
    }

    void set_literal(size_t len)
    {
        typ = VMGRAM_MATCH_LITERAL;
        typinfo.lit.len = len;
        typinfo.lit.hash = 0;
        typinfo.lit.str = (char *)t3malloc(len);
    }

    void set_nspeech(size_t cnt)
    {
        typ = VMGRAM_MATCH_NSPEECH;
        typinfo.nspeech.cnt = cnt;
        typinfo.nspeech.props = (vm_prop_id_t *)t3malloc(
            cnt * sizeof(vm_prop_id_t));
    }

    /* 
     *   property association - this is the property of the processor object
     *   that we'll set to point to the match object or input token if we
     *   match this rule token 
     */
    vm_prop_id_t prop;

    /* token type - this is a VMGRAM_MATCH_xxx value */
    uchar typ;

    /* extra data, depending on 'typ' */
    union
    {
        /* VMGRAM_MATCH_PROD - the sub-production object */
        vm_obj_id_t prod_obj;

        /* VMGRAM_MATCH_SPEECH - the part-of-speech property */
        vm_prop_id_t speech_prop;

        /* VMGRAM_MATCH_NSPEECH - an array of part-of-speech proeprties */
        struct
        {
            size_t cnt;
            vm_prop_id_t *props;
        } nspeech;

        /* VMGRAM_MATCH_LITERAL - the literal string to match */
        struct
        {
            /* the literal text and its length */
            char *str;
            size_t len;

            /* the cached hash value for the literal */
            uint hash;
        } lit;

        /* VMGRAM_MATCH_TOKTYPE - token type enum */
        uint32_t toktyp_enum;

    } typinfo;
};

/* ------------------------------------------------------------------------ */
/*
 *   Grammar-Production object interface 
 */
class CVmObjGramProd: public CVmObject
{
    friend class CVmMetaclassGramProd;
    friend struct CVmGramCompResults;

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

    /* determine if an object is a GrammarProduction object */
    static int is_gramprod_obj(VMG_ vm_obj_id_t obj)
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

    /* mark a reference in an undo record */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *rec);

    /* remove stale weak references from an undo record */
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }

    /* mark object references */
    void mark_refs(VMG_ uint state);

    /* remove weak references */
    void remove_stale_weak_refs(VMG0_);

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* restart/save/restore */
    void reset_to_image(VMG_ vm_obj_id_t self);
    void save_to_file(VMG_ class CVmFile *fp);
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* determine if the object has been changed since it was loaded */
    int is_changed_since_load() const { return FALSE; }

    /* 
     *   rebuild for image file - we can't change during execution, so our
     *   image file data never change 
     */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *,
                                       vm_obj_id_t) { }

protected:
    /* private constructor */
    CVmObjGramProd(VMG0_);

    /* clear the alternative list */
    void clear_alts();

    /* ensure that there's space for the given number of alternatives */
    void ensure_alts(size_t cnt, size_t margin);

    /* insert/delete an alternative, without keeping undo information */
    void insert_alt(vm_obj_id_t self, size_t idx, vmgram_alt_info *alt);
    void delete_alt(size_t idx);

    /* check the alternative list for circular references */
    void check_circular_refs(vm_obj_id_t self);

    /* build the grammarAltProps list for a match object */
    void build_alt_props(VMG_ vm_obj_id_t match_obj);
    size_t build_alt_props_list(VMG_ vm_obj_id_t match_obj,
                                class CVmObjList *lst);

    /* add an alternative from a parsed rule, keeping undo */
    void add_alt(VMG_ vm_obj_id_t self, class CTcGramProdAlt *alt,
                 vm_obj_id_t dict, vm_obj_id_t match);

    /* delete an alternative, keeping undo */
    void delete_alt_undo(VMG_ vm_obj_id_t self, int idx);

    /* delete a batch of alternatives - deletes each item marked with 'del' */
    void delete_marked_alts(VMG_ vm_obj_id_t self, vm_obj_id_t dict);

    /* find a literal in the surviving alternatives */
    int find_literal_in_alts(const char *str, size_t len);

    /* save to a data stream */
    void save_to_stream(VMG_ class CVmStream *dst);

    /* mark references in an alt list (for garbage collection tracing) */
    void mark_alt_refs(VMG_ const vmgram_alt_info *alt, uint state);

    /* load alternatives from a stream, in image file format */
    void load_alts(VMG_ vm_obj_id_t self,
                   class CVmStream *src, class CVmObjFixup *fixups);

    /* property evaluation - undefined property */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluation - parseTokens */
    int getp_parse(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluation - getGrammarInfo */
    int getp_get_gram_info(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - delete an alternative */
    int getp_deleteAlt(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - delete all alternatives */
    int getp_clearAlts(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - add an alternative */
    int getp_addAlt(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* get my extension, properly cast */
    vm_gram_ext *get_ext() const { return (vm_gram_ext *)ext_; }

    /* callback for dictionary word property enumeration */
    static void enum_props_cb(VMG_ void *ctx, vm_prop_id_t prop,
                              const vm_val_t *match_val);

    /* search a token for a match to the given vocabulary property */
    static int find_prop_in_tok(const struct vmgramprod_tok *tok,
                                vm_prop_id_t prop);

    /* enqueue our alternatives */
    void enqueue_alts(VMG_ class CVmGramProdMem *mem,
                      const struct vmgramprod_tok *tok,
                      size_t tok_cnt, size_t start_tok_pos,
                      struct CVmGramProdState *state,
                      struct CVmGramProdQueue *queues,
                      vm_obj_id_t self, int circ_only,
                      struct CVmGramProdMatch *circ_match,
                      class CVmObjDict *dict);

    /* create and enqueue a new state */
    static struct CVmGramProdState *
        enqueue_new_state(class CVmGramProdMem *mem,
                          size_t start_tok_pos,
                          struct CVmGramProdState *enclosing_state,
                          const vmgram_alt_info *altp, vm_obj_id_t self,
                          int *need_to_clone,
                          struct CVmGramProdQueue *queues,
                          int circular_alt);
    
    /* create a new state */
    static struct CVmGramProdState *
        create_new_state(class CVmGramProdMem *mem,
                         size_t start_tok_pos,
                         struct CVmGramProdState *enclosing_state,
                         const vmgram_alt_info *altp, vm_obj_id_t self,
                         int *need_to_clone, int circular_alt);
    
    /* enqueue a state */
    static void enqueue_state(struct CVmGramProdState *state,
                              struct CVmGramProdQueue *queues);

    /* process the work queue */
    static void process_work_queue(VMG_ CVmGramProdMem *mem,
                                   const struct vmgramprod_tok *tok,
                                   size_t tok_cnt,
                                   struct CVmGramProdQueue *queues,
                                   class CVmObjDict *dict);
                                   

    /* process the first work queue entry */
    static void process_work_queue_head(VMG_ CVmGramProdMem *mem,
                                        const struct vmgramprod_tok *tok,
                                        size_t tok_cnt,
                                        struct CVmGramProdQueue *queues,
                                        class CVmObjDict *dict);

    /* build a match tree */
    static void build_match_tree(VMG_ const struct CVmGramProdMatch *match,
                                 const vm_val_t *tok_list,
                                 const vm_val_t *tok_match_list,
                                 vm_val_t *retval,
                                 size_t *first_tok, size_t *last_tok);

    /* cache the hash values for the literal tokens in our alternatives */
    void cache_hashes(VMG_ CVmObjDict *dict);

    /* calculate the hash value for a literal string */
    static unsigned int calc_str_hash(VMG_ class CVmObjDict *dict,
                                      const vm_val_t *strval,
                                      const char *str, size_t len);

    /* check to see if a token matches a literal */
    static int tok_equals_lit(VMG_ const struct vmgramprod_tok *tok,
                              const char *lit, size_t lit_len,
                              class CVmObjDict *dict,
                              vm_val_t *match_result);

    /* property evaluation function table */
    static int (CVmObjGramProd::*func_table_[])(VMG_ vm_obj_id_t self,
                                                vm_val_t *retval, uint *argc);
};


/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassGramProd: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "grammar-production/030002"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjGramProd(vmg0_);
        G_obj_table->set_obj_gc_characteristics(id, TRUE, TRUE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjGramProd(vmg0_);
        G_obj_table->set_obj_gc_characteristics(id, TRUE, TRUE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjGramProd::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjGramProd::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};


#endif /* VMGRAM_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjGramProd)

